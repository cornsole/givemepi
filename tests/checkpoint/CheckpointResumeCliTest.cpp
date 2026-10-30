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

std::vector<std::string> arguments(
    const std::filesystem::path& executable,
    const RunPaths& paths,
    bool resume)
{
    return {
        executable.string(),
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

ChildProcess startProcess(
    const std::filesystem::path& executable,
    const RunPaths& paths,
    bool resume)
{
    const std::vector<std::string> values = arguments(executable, paths, resume);
    const pid_t pid = ::fork();
    if (pid < 0) throw std::runtime_error("fork failed");
    if (pid == 0)
    {
        const int stdoutFd = ::open(
            paths.stdoutLog.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0600);
        const int stderrFd = ::open(
            paths.stderrLog.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0600);
        if (stdoutFd < 0 || stderrFd < 0
            || ::dup2(stdoutFd, STDOUT_FILENO) < 0
            || ::dup2(stderrFd, STDERR_FILENO) < 0
            || ::chdir(paths.directory.c_str()) != 0)
        {
            ::_exit(126);
        }
        ::close(stdoutFd);
        ::close(stderrFd);

        std::vector<char*> argv;
        argv.reserve(values.size() + 1);
        for (const std::string& value : values)
            argv.push_back(const_cast<char*>(value.c_str()));
        argv.push_back(nullptr);
        ::execv(executable.c_str(), argv.data());
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

bool hasPublishedManifest(const std::filesystem::path& directory)
{
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error)) return false;
    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
        if (entry.path().extension() == ".manifest"
            && entry.is_regular_file()
            && entry.file_size() > 0)
        {
            return true;
        }
    }
    return false;
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
        throw std::runtime_error("missing numeric JSON field " + std::string(label));
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
    const RunPaths& paths,
    bool resume)
{
    ChildProcess child = startProcess(executable, paths, resume);
    const int status = waitForExit(child, 180s);
    if (status != 0)
    {
        throw std::runtime_error(
            "pi-engine failed with status=" + std::to_string(status)
            + " stderr=" + readFile(paths.stderrLog));
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
        const RunPaths resumed = makePaths(temporary.path(), "resumed");
        ChildProcess interrupted = startProcess(executable, resumed, false);
        interruptAfterFirstManifest(interrupted, resumed);

        requireSuccessfulRun(executable, resumed, true);
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
        requireSuccessfulRun(executable, fresh, false);
        const std::string freshOutput = readFile(fresh.output);
        const std::string freshStdout = readFile(fresh.stdoutLog);
        requireLiveTelemetry(readFile(fresh.stderrLog));

        if (resumedOutput != freshOutput)
            throw std::runtime_error("resumed output differs from fresh output");
        if (verificationHash(resumedStdout) != verificationHash(freshStdout))
            throw std::runtime_error("resumed verification hash differs");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "CheckpointResumeCliTest: " << error.what() << '\n';
        return 1;
    }
}
