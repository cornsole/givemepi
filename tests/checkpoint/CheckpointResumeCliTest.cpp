#include <chrono>
#include <csignal>
#include <cstdint>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace
{

using namespace std::chrono_literals;

class TemporaryDirectory
{
public:
    TemporaryDirectory()
        : path_(std::filesystem::temp_directory_path()
            / ("givemepi-cli-resume-" + std::to_string(::getpid())))
    {
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

struct ChildProcess
{
    pid_t pid = -1;

    ~ChildProcess()
    {
        if (pid <= 0) return;
        if (::waitpid(pid, nullptr, WNOHANG) == 0)
        {
            ::kill(pid, SIGKILL);
            static_cast<void>(::waitpid(pid, nullptr, 0));
        }
    }

    ChildProcess(const ChildProcess&) = delete;
    ChildProcess& operator=(const ChildProcess&) = delete;
    ChildProcess() = default;
    ChildProcess(ChildProcess&& other) noexcept : pid(other.pid) { other.pid = -1; }
    ChildProcess& operator=(ChildProcess&& other) noexcept
    {
        if (this != &other)
        {
            pid = other.pid;
            other.pid = -1;
        }
        return *this;
    }
};

struct RunPaths
{
    std::filesystem::path directory;
    std::filesystem::path checkpoints;
    std::filesystem::path quarantine;
    std::filesystem::path storage;
    std::filesystem::path output;
    std::filesystem::path stdoutLog;
    std::filesystem::path stderrLog;
};

RunPaths makePaths(const std::filesystem::path& root, std::string_view name)
{
    RunPaths paths;
    paths.directory = root / name;
    paths.checkpoints = paths.directory / "checkpoints";
    paths.quarantine = paths.directory / "quarantine";
    paths.storage = paths.directory / "storage";
    paths.output = paths.directory / "pi.txt";
    paths.stdoutLog = paths.directory / "stdout.log";
    paths.stderrLog = paths.directory / "stderr.log";
    std::filesystem::create_directories(paths.directory);
    return paths;
}

std::vector<std::string> jsonResumeOptions(const RunPaths& paths, bool resume)
{
    return {
        "--digits", "1000000",
        "--threads", "2",
        "--checkpoint",
        resume ? "--resume" : "--no-resume",
        "--checkpoint-directory", paths.checkpoints.string(),
        "--checkpoint-quarantine-directory", paths.quarantine.string(),
        "--checkpoint-interval", "3600",
        "--storage-directory", paths.storage.string(),
        "--progress",
        "--progress-interval", "1",
        "--progress-format", "json",
        "--verify",
        "--bbp-samples", "1",
        "--output", paths.output.string()
    };
}

/// Starts pi-engine directly (no shell) inside the run directory with stdin
/// from /dev/null and stdout/stderr captured to regular files. `options`
/// excludes argv[0]; an empty vector runs the engine with no arguments.
ChildProcess startProcess(
    const std::filesystem::path& executable,
    const std::vector<std::string>& options,
    const RunPaths& paths)
{
    const std::string program = executable.string();
    std::vector<char*> argv;
    argv.reserve(options.size() + 2);
    argv.push_back(const_cast<char*>(program.c_str()));
    for (const std::string& option : options)
        argv.push_back(const_cast<char*>(option.c_str()));
    argv.push_back(nullptr);

    const pid_t pid = ::fork();
    if (pid < 0) throw std::runtime_error("fork failed");
    if (pid == 0)
    {
        const int stdinFd = ::open("/dev/null", O_RDONLY);
        const int stdoutFd = ::open(
            paths.stdoutLog.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0600);
        const int stderrFd = ::open(
            paths.stderrLog.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0600);
        if (stdinFd < 0 || stdoutFd < 0 || stderrFd < 0
            || ::dup2(stdinFd, STDIN_FILENO) < 0
            || ::dup2(stdoutFd, STDOUT_FILENO) < 0
            || ::dup2(stderrFd, STDERR_FILENO) < 0
            || ::chdir(paths.directory.c_str()) != 0)
        {
            ::_exit(126);
        }
        ::close(stdinFd);
        ::close(stdoutFd);
        ::close(stderrFd);
        ::execv(program.c_str(), argv.data());
        ::_exit(127);
    }
    ChildProcess child;
    child.pid = pid;
    return child;
}

std::optional<int> tryWait(ChildProcess& child)
{
    int status = 0;
    const pid_t result = ::waitpid(child.pid, &status, WNOHANG);
    if (result < 0) throw std::runtime_error("waitpid failed");
    if (result == 0) return std::nullopt;
    child.pid = -1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 255;
}

int waitForExit(ChildProcess& child, std::chrono::seconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (const auto status = tryWait(child)) return *status;
        std::this_thread::sleep_for(10ms);
    }
    ::kill(child.pid, SIGKILL);
    int status = 0;
    static_cast<void>(::waitpid(child.pid, &status, 0));
    child.pid = -1;
    throw std::runtime_error("pi-engine timed out");
}

std::size_t countNonEmptyFiles(
    const std::filesystem::path& directory,
    std::string_view extension)
{
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error)) return 0;
    std::size_t count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
        if (entry.path().extension() == extension
            && entry.is_regular_file()
            && entry.file_size() > 0)
        {
            ++count;
        }
    }
    return count;
}

bool hasPublishedManifest(const std::filesystem::path& directory)
{
    return countNonEmptyFiles(directory, ".manifest") > 0;
}

void interruptAfterFirstManifest(ChildProcess& child, const RunPaths& paths)
{
    const auto deadline = std::chrono::steady_clock::now() + 120s;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (const auto status = tryWait(child))
        {
            throw std::runtime_error(
                "pi-engine exited before interruption, status="
                + std::to_string(*status));
        }
        if (hasPublishedManifest(paths.checkpoints))
        {
            if (::kill(child.pid, SIGTERM) != 0)
                throw std::runtime_error("failed to terminate pi-engine");
            const int status = waitForExit(child, 30s);
            if (status != 128 + SIGTERM)
            {
                throw std::runtime_error(
                    "interrupted pi-engine returned unexpected status="
                    + std::to_string(status));
            }
            return;
        }
        std::this_thread::sleep_for(1ms);
    }
    throw std::runtime_error("checkpoint manifest was not published in time");
}

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot read " + path.string());
    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

std::uint64_t parseUnsigned(
    std::string_view text,
    std::size_t& position,
    std::string_view label)
{
    if (position >= text.size() || text[position] < '0' || text[position] > '9')
        throw std::runtime_error("missing numeric field " + std::string(label));
    std::uint64_t value = 0;
    while (position < text.size() && text[position] >= '0' && text[position] <= '9')
    {
        value = value * 10 + static_cast<unsigned>(text[position] - '0');
        ++position;
    }
    return value;
}

void requireLiveTelemetry(std::string_view log)
{
    constexpr std::string_view prefix =
        "\"checkpoint_blocks\":{\"completed\":";
    std::size_t position = log.rfind(prefix);
    if (position == std::string_view::npos)
        throw std::runtime_error("JSON checkpoint telemetry is absent");
    position += prefix.size();
    const std::uint64_t completed = parseUnsigned(log, position, "completed");

    constexpr std::string_view totalLabel = ",\"total\":";
    if (log.substr(position, totalLabel.size()) != totalLabel)
        throw std::runtime_error("JSON checkpoint total is absent");
    position += totalLabel.size();
    const std::uint64_t total = parseUnsigned(log, position, "total");

    constexpr std::string_view durableLabel = ",\"durable\":";
    if (log.substr(position, durableLabel.size()) != durableLabel)
        throw std::runtime_error("JSON durable checkpoint count is absent");
    position += durableLabel.size();
    const std::uint64_t durable = parseUnsigned(log, position, "durable");

    constexpr std::string_view bytesLabel = "\"checkpoint_bytes\":";
    position = log.rfind(bytesLabel);
    if (position == std::string_view::npos)
        throw std::runtime_error("JSON checkpoint bytes are absent");
    position += bytesLabel.size();
    const std::uint64_t bytes = parseUnsigned(log, position, "checkpoint_bytes");

    if (completed == 0 || completed > total || durable == 0 || bytes == 0)
        throw std::runtime_error("JSON checkpoint telemetry stayed empty");
}

/// Final record of a non-TTY text progress log, parsed from its key=value
/// fields.
struct TextProgressRecord
{
    std::string phase;
    std::uint64_t completedTerms = 0;
    std::uint64_t totalTerms = 0;
    std::uint64_t completedCheckpoints = 0;
    std::uint64_t totalCheckpoints = 0;
    std::uint64_t durableCheckpoints = 0;
    bool checkpointBytesNonZero = false;
};

/// Returns the position just past `key=` where the key starts the record or
/// follows a space; throws if absent.
std::size_t fieldStart(std::string_view record, std::string_view key)
{
    const std::string needle = std::string(key) + '=';
    if (record.substr(0, needle.size()) == needle) return needle.size();
    const std::size_t position = record.find(' ' + needle);
    if (position == std::string_view::npos)
        throw std::runtime_error("text progress field is absent: " + needle);
    return position + 1 + needle.size();
}

std::string_view fieldToken(std::string_view record, std::string_view key)
{
    const std::size_t start = fieldStart(record, key);
    const std::size_t end = record.find(' ', start);
    return record.substr(
        start, end == std::string_view::npos ? std::string_view::npos : end - start);
}

void parseFraction(
    std::string_view record,
    std::string_view key,
    std::uint64_t& completed,
    std::uint64_t& total)
{
    const std::string_view token = fieldToken(record, key);
    std::size_t position = 0;
    completed = parseUnsigned(token, position, key);
    if (position >= token.size() || token[position] != '/')
        throw std::runtime_error("text progress fraction is malformed: " + std::string(key));
    ++position;
    total = parseUnsigned(token, position, key);
    if (position != token.size())
        throw std::runtime_error("text progress fraction is malformed: " + std::string(key));
}

std::uint64_t parseWholeUnsigned(std::string_view record, std::string_view key)
{
    const std::string_view token = fieldToken(record, key);
    std::size_t position = 0;
    const std::uint64_t value = parseUnsigned(token, position, key);
    if (position != token.size())
        throw std::runtime_error("text progress number is malformed: " + std::string(key));
    return value;
}

/// Parses a human byte count such as `0 B` or `12.34 MiB`.
bool parseByteCountNonZero(std::string_view record, std::string_view key)
{
    const std::size_t start = fieldStart(record, key);
    const std::size_t space = record.find(' ', start);
    if (space == std::string_view::npos)
        throw std::runtime_error("text byte count lacks a unit: " + std::string(key));
    const std::string_view amount = record.substr(start, space - start);
    const std::size_t unitEnd = record.find(' ', space + 1);
    const std::string_view unit = record.substr(
        space + 1,
        unitEnd == std::string_view::npos ? std::string_view::npos : unitEnd - space - 1);
    if (unit != "B" && unit != "KiB" && unit != "MiB" && unit != "GiB")
        throw std::runtime_error("text byte count unit is malformed: " + std::string(key));

    bool nonZero = false;
    bool sawDigit = false;
    bool sawPoint = false;
    for (const char character : amount)
    {
        if (character >= '0' && character <= '9')
        {
            sawDigit = true;
            nonZero = nonZero || character != '0';
        }
        else if (character == '.' && !sawPoint && sawDigit)
        {
            sawPoint = true;
        }
        else
        {
            throw std::runtime_error("text byte count is malformed: " + std::string(key));
        }
    }
    if (!sawDigit || amount.back() == '.')
        throw std::runtime_error("text byte count is malformed: " + std::string(key));
    return nonZero;
}

/// Validates that `log` is a full non-TTY text progress log (timestamped
/// newline-terminated records, no terminal control) and parses its final
/// record, which must be the last line of stderr.
TextProgressRecord finalTextRecord(std::string_view log)
{
    if (log.find('\r') != std::string_view::npos
        || log.find('\x1b') != std::string_view::npos)
    {
        throw std::runtime_error("non-TTY text progress log contains terminal control");
    }
    if (log.empty() || log.back() != '\n')
        throw std::runtime_error("text progress log is empty or unterminated");

    std::string_view last;
    std::size_t start = 0;
    while (start < log.size())
    {
        const std::size_t end = log.find('\n', start);
        const std::string_view line = log.substr(start, end - start);
        start = end + 1;
        if (!line.empty()) last = line;
    }
    const std::size_t close = last.find("] ");
    if (last.empty() || last.front() != '[' || close == std::string_view::npos)
        throw std::runtime_error("final stderr line is not a text progress record");
    const std::string_view record = last.substr(close + 2);
    if (record.substr(0, 6) != "phase=")
        throw std::runtime_error("final stderr line is not a text progress record");

    TextProgressRecord parsed;
    parsed.phase = std::string(fieldToken(record, "phase"));
    parseFraction(record, "terms", parsed.completedTerms, parsed.totalTerms);
    parseFraction(
        record, "checkpoints", parsed.completedCheckpoints, parsed.totalCheckpoints);
    parsed.durableCheckpoints = parseWholeUnsigned(record, "durable");
    parsed.checkpointBytesNonZero =
        parseByteCountNonZero(record, "checkpoint_bytes");
    return parsed;
}

std::string verificationHash(std::string_view output)
{
    constexpr std::string_view marker = "sha256 ";
    const std::size_t start = output.rfind(marker);
    if (start == std::string_view::npos)
        throw std::runtime_error("verification SHA-256 is absent");
    const std::size_t hashStart = start + marker.size();
    const std::size_t end = output.find(')', hashStart);
    if (end == std::string_view::npos || end - hashStart != 64)
        throw std::runtime_error("verification SHA-256 is malformed");
    return std::string(output.substr(hashStart, 64));
}

void requireSuccessfulRun(
    const std::filesystem::path& executable,
    const std::vector<std::string>& options,
    const RunPaths& paths,
    std::chrono::seconds timeout)
{
    ChildProcess child = startProcess(executable, options, paths);
    const int status = waitForExit(child, timeout);
    if (status != 0)
    {
        throw std::runtime_error(
            "pi-engine failed with status=" + std::to_string(status)
            + " stderr=" + readFile(paths.stderrLog));
    }
}

void requireNonEmptyFile(const std::filesystem::path& path)
{
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)
        || std::filesystem::file_size(path, error) == 0 || error)
    {
        throw std::runtime_error("expected non-empty file: " + path.string());
    }
}

/// Runs pi-engine with no arguments and no config.toml in an otherwise empty
/// directory, so every setting comes from built-in defaults. stderr is a
/// regular file, so text progress uses its full log mode.
std::string requireDefaultRun(
    const std::filesystem::path& executable,
    const std::filesystem::path& root)
{
    const RunPaths paths = makePaths(root, "defaults");
    if (std::filesystem::exists(paths.directory / "config.toml"))
        throw std::runtime_error("default run directory is not isolated");

    requireSuccessfulRun(executable, {}, paths, 180s);

    const TextProgressRecord last = finalTextRecord(readFile(paths.stderrLog));
    if (last.phase != "completed")
        throw std::runtime_error("default run final phase is " + last.phase);
    if (last.totalTerms == 0 || last.completedTerms != last.totalTerms)
        throw std::runtime_error("default run did not finish all terms");
    if (last.totalCheckpoints == 0
        || last.completedCheckpoints != last.totalCheckpoints)
    {
        throw std::runtime_error("default run did not finish all checkpoint blocks");
    }
    if (last.durableCheckpoints == 0)
        throw std::runtime_error("default run reported no durable checkpoints");
    if (!last.checkpointBytesNonZero)
        throw std::runtime_error("default run reported zero checkpoint bytes");

    const std::filesystem::path output = paths.directory / "pi.txt";
    requireNonEmptyFile(output);
    requireNonEmptyFile(paths.directory / "pi.txt.verify");
    const std::filesystem::path checkpoints = paths.directory / "checkpoints";
    if (countNonEmptyFiles(checkpoints, ".manifest") == 0)
        throw std::runtime_error("default run published no checkpoint manifest");
    if (countNonEmptyFiles(checkpoints, ".checkpoint") == 0)
        throw std::runtime_error("default run published no checkpoint block");

    static_cast<void>(verificationHash(readFile(paths.stdoutLog)));
    return readFile(output);
}

/// Resume-only run without accepted blocks: no checkpoint work is planned and
/// nothing is written to the (default) checkpoint directory.
void requireResumeOnlyRun(
    const std::filesystem::path& executable,
    const std::filesystem::path& root)
{
    const RunPaths paths = makePaths(root, "resume-only");
    requireSuccessfulRun(
        executable,
        {
            "--digits", "1000",
            "--threads", "2",
            "--no-checkpoint",
            "--resume",
            "--progress",
            "--progress-interval", "1",
            "--progress-format", "text"
        },
        paths,
        60s);

    const TextProgressRecord last = finalTextRecord(readFile(paths.stderrLog));
    if (last.phase != "completed")
        throw std::runtime_error("resume-only final phase is " + last.phase);
    if (last.totalTerms == 0 || last.completedTerms != last.totalTerms)
        throw std::runtime_error("resume-only run did not finish all terms");
    if (last.completedCheckpoints != 0 || last.totalCheckpoints != 0)
    {
        throw std::runtime_error(
            "resume-only run reported checkpoints="
            + std::to_string(last.completedCheckpoints) + '/'
            + std::to_string(last.totalCheckpoints));
    }
    if (last.durableCheckpoints != 0 || last.checkpointBytesNonZero)
        throw std::runtime_error("resume-only run reported durable checkpoints");

    requireNonEmptyFile(paths.directory / "pi.txt");
    if (std::filesystem::exists(paths.directory / "checkpoints"))
        throw std::runtime_error("resume-only run created a checkpoint directory");
    for (const auto& entry
         : std::filesystem::recursive_directory_iterator(paths.directory))
    {
        const auto extension = entry.path().extension();
        if (extension == ".checkpoint" || extension == ".manifest")
        {
            throw std::runtime_error(
                "resume-only run wrote checkpoint data: " + entry.path().string());
        }
    }
}

} // namespace

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 2)
            throw std::invalid_argument("expected the pi-engine path");
        const std::filesystem::path executable =
            std::filesystem::absolute(argv[1]);
        if (!std::filesystem::is_regular_file(executable))
            throw std::invalid_argument("pi-engine path is not a file");

        TemporaryDirectory temporary;
        const std::string defaultOutput =
            requireDefaultRun(executable, temporary.path());
        requireResumeOnlyRun(executable, temporary.path());

        const RunPaths resumed = makePaths(temporary.path(), "resumed");
        ChildProcess interrupted = startProcess(
            executable, jsonResumeOptions(resumed, false), resumed);
        interruptAfterFirstManifest(interrupted, resumed);

        requireSuccessfulRun(
            executable, jsonResumeOptions(resumed, true), resumed, 180s);
        const std::string resumedOutput = readFile(resumed.output);
        const std::string resumedStdout = readFile(resumed.stdoutLog);
        const std::string resumedStderr = readFile(resumed.stderrLog);
        requireLiveTelemetry(resumedStderr);
        if (resumedStderr.find("\"last_validated_checkpoint\":{")
            == std::string::npos)
        {
            throw std::runtime_error("resume validation telemetry is absent");
        }
        if (resumedStderr.find("\"last_committed_checkpoint\":{")
            == std::string::npos)
        {
            throw std::runtime_error("live commit telemetry is absent");
        }

        const RunPaths fresh = makePaths(temporary.path(), "fresh");
        requireSuccessfulRun(
            executable, jsonResumeOptions(fresh, false), fresh, 180s);
        const std::string freshOutput = readFile(fresh.output);
        const std::string freshStdout = readFile(fresh.stdoutLog);
        requireLiveTelemetry(readFile(fresh.stderrLog));

        if (resumedOutput != freshOutput)
            throw std::runtime_error("resumed output differs from fresh output");
        if (verificationHash(resumedStdout) != verificationHash(freshStdout))
            throw std::runtime_error("resumed verification hash differs");
        if (defaultOutput != freshOutput)
            throw std::runtime_error("default-run output differs from fresh output");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "CheckpointResumeCliTest: " << error.what() << '\n';
        return 1;
    }
}
