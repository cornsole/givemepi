// Real-process regression for PR-0041 cross-precision checkpoint promotion.
//
// A lower-precision run publishes a source checkpoint frontier. Promotion
// into a higher-precision target must produce byte-identical output to a cold
// target run, publish only target-identity checkpoint files, never modify the
// source, and reject same-directory, conflicting, corrupt, and non-lower
// sources before any target checkpoint work.

#include <chrono>
#include <csignal>
#include <cstdint>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
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

constexpr std::string_view sourceDigits = "100000";
constexpr std::string_view targetDigits = "200000";

class TemporaryDirectory
{
public:
    TemporaryDirectory()
        : path_(std::filesystem::temp_directory_path()
            / ("givemepi-cli-promotion-" + std::to_string(::getpid())))
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

struct RunPaths
{
    std::filesystem::path directory;
    std::filesystem::path checkpoints;
    std::filesystem::path output;
    std::filesystem::path stdoutLog;
    std::filesystem::path stderrLog;
};

RunPaths makePaths(const std::filesystem::path& root, std::string_view name)
{
    RunPaths paths;
    paths.directory = root / name;
    paths.checkpoints = paths.directory / "checkpoints";
    paths.output = paths.directory / "pi.txt";
    paths.stdoutLog = paths.directory / "stdout.log";
    paths.stderrLog = paths.directory / "stderr.log";
    std::filesystem::create_directories(paths.directory);
    return paths;
}

/// Runs pi-engine without a shell inside the run directory, captures stdout
/// and stderr to files, and returns the exit status.
int run(
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
        ::execv(program.c_str(), argv.data());
        ::_exit(127);
    }

    const auto deadline = std::chrono::steady_clock::now() + 180s;
    int status = 0;
    while (true)
    {
        const pid_t result = ::waitpid(pid, &status, WNOHANG);
        if (result < 0) throw std::runtime_error("waitpid failed");
        if (result == pid) break;
        if (std::chrono::steady_clock::now() >= deadline)
        {
            ::kill(pid, SIGKILL);
            static_cast<void>(::waitpid(pid, &status, 0));
            throw std::runtime_error("pi-engine timed out");
        }
        std::this_thread::sleep_for(10ms);
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 255;
}

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot read " + path.string());
    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

void writeFile(const std::filesystem::path& path, const std::string& bytes)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!output) throw std::runtime_error("cannot write " + path.string());
}

/// Relative path to exact bytes for every regular file below a directory.
std::map<std::string, std::string> snapshot(const std::filesystem::path& root)
{
    std::map<std::string, std::string> files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
    {
        if (entry.is_regular_file())
        {
            files.emplace(
                std::filesystem::relative(entry.path(), root).string(),
                readFile(entry.path()));
        }
    }
    return files;
}

std::vector<std::filesystem::path> filesWithExtension(
    const std::filesystem::path& directory,
    std::string_view extension)
{
    std::vector<std::filesystem::path> files;
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error)) return files;
    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
        if (entry.is_regular_file() && entry.path().extension() == extension)
            files.push_back(entry.path());
    }
    return files;
}

std::filesystem::path onlyManifest(const std::filesystem::path& directory)
{
    const auto manifests = filesWithExtension(directory, ".manifest");
    if (manifests.size() != 1)
    {
        throw std::runtime_error(
            "expected one manifest in " + directory.string() + ", found "
            + std::to_string(manifests.size()));
    }
    return manifests.front();
}

/// Term count encoded as `-n<terms>` in a deterministic manifest filename.
std::uint64_t manifestTermCount(const std::filesystem::path& manifest)
{
    const std::string name = manifest.stem().string();
    const std::size_t marker = name.rfind("-n");
    if (marker == std::string::npos)
        throw std::runtime_error("manifest name has no term count: " + name);
    return std::stoull(name.substr(marker + 2));
}

std::vector<std::string> options(
    std::string_view digits,
    const RunPaths& paths,
    std::optional<std::filesystem::path> promoteFrom = std::nullopt,
    std::optional<std::filesystem::path> checkpointDirectory = std::nullopt)
{
    std::vector<std::string> result{
        "--digits", std::string(digits),
        "--threads", "2",
        "--checkpoint",
        "--resume",
        "--checkpoint-directory",
        checkpointDirectory.value_or(paths.checkpoints).string(),
        "--checkpoint-quarantine-directory",
        (paths.directory / "quarantine").string(),
        "--storage-directory", (paths.directory / "storage").string(),
        "--progress",
        "--progress-interval", "1000",
        "--progress-format", "json",
        "--verify",
        "--bbp-samples", "1",
        "--output", paths.output.string()
    };
    if (promoteFrom)
    {
        result.push_back("--promote-checkpoint-manifest");
        result.push_back(promoteFrom->string());
    }
    return result;
}

void requireStatus(
    int status,
    int expected,
    const RunPaths& paths,
    std::string_view scenario)
{
    if (status != expected)
    {
        throw std::runtime_error(
            std::string(scenario) + ": status=" + std::to_string(status)
            + " expected=" + std::to_string(expected)
            + " stderr=" + readFile(paths.stderrLog));
    }
}

void requireStderrContains(
    const RunPaths& paths,
    std::string_view text,
    std::string_view scenario)
{
    if (readFile(paths.stderrLog).find(text) == std::string::npos)
    {
        throw std::runtime_error(
            std::string(scenario) + ": stderr lacks '" + std::string(text)
            + "': " + readFile(paths.stderrLog));
    }
}

void requireUnchanged(
    const std::map<std::string, std::string>& before,
    const std::filesystem::path& root,
    std::string_view scenario)
{
    if (snapshot(root) != before)
        throw std::runtime_error(std::string(scenario) + ": source changed");
}

/// The first JSON record is published immediately after progress restore,
/// before split work starts, so it shows the promoted term baseline.
std::uint64_t firstRecordCompletedTerms(const RunPaths& paths)
{
    const std::string log = readFile(paths.stderrLog);
    constexpr std::string_view key = "\"terms\":{\"completed\":";
    const std::size_t start = log.find(key);
    if (start == std::string::npos)
        throw std::runtime_error("no JSON terms record: " + log);
    return std::stoull(log.substr(start + key.size()));
}

void requireRejected(
    const std::filesystem::path& executable,
    const RunPaths& paths,
    const std::vector<std::string>& arguments,
    std::string_view message,
    const std::map<std::string, std::string>& sourceBefore,
    const std::filesystem::path& sourceRoot,
    std::string_view scenario)
{
    requireStatus(run(executable, arguments, paths), 1, paths, scenario);
    requireStderrContains(paths, message, scenario);
    requireUnchanged(sourceBefore, sourceRoot, scenario);
    if (std::filesystem::exists(paths.output))
        throw std::runtime_error(std::string(scenario) + ": wrote output");
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
        TemporaryDirectory temporary;
        const auto& root = temporary.path();

        // Source: one complete lower-precision checkpoint frontier.
        const RunPaths source = makePaths(root, "source");
        requireStatus(
            run(executable, options(sourceDigits, source), source),
            0, source, "source run");
        const std::filesystem::path sourceManifest =
            onlyManifest(source.checkpoints);
        const std::uint64_t sourceTerms = manifestTermCount(sourceManifest);
        const auto sourceBefore = snapshot(source.checkpoints);

        // Cold target reference.
        const RunPaths cold = makePaths(root, "cold");
        requireStatus(
            run(executable, options(targetDigits, cold), cold),
            0, cold, "cold run");

        // Promotion succeeds, matches the cold run, and leaves the source intact.
        const RunPaths promoted = makePaths(root, "promoted");
        requireStatus(
            run(executable,
                options(targetDigits, promoted, sourceManifest), promoted),
            0, promoted, "promoted run");
        requireUnchanged(sourceBefore, source.checkpoints, "promoted run");
        if (readFile(promoted.output) != readFile(cold.output))
            throw std::runtime_error("promoted output differs from cold output");
        if (firstRecordCompletedTerms(promoted) < sourceTerms)
            throw std::runtime_error("promoted terms were not seeded");

        // Only target-identity checkpoint files are published.
        const std::filesystem::path targetManifest =
            onlyManifest(promoted.checkpoints);
        const std::string targetTag = std::string("-d") + std::string(targetDigits) + "-";
        if (targetManifest.filename().string().find(targetTag) == std::string::npos)
            throw std::runtime_error("target manifest has the wrong identity");
        const auto targetBlocks =
            filesWithExtension(promoted.checkpoints, ".checkpoint");
        if (targetBlocks.empty())
            throw std::runtime_error("promoted run published no target block");
        for (const auto& block : targetBlocks)
        {
            if (block.filename().string().find(targetTag) == std::string::npos)
                throw std::runtime_error("source-identity block in target: "
                    + block.filename().string());
        }

        // Re-running promotion over accepted target blocks is a conflict.
        requireStatus(
            run(executable,
                options(targetDigits, promoted, sourceManifest), promoted),
            1, promoted, "conflict run");
        requireStderrContains(
            promoted, "conflicts with accepted target checkpoints", "conflict run");
        requireUnchanged(sourceBefore, source.checkpoints, "conflict run");

        // Same source and target directory is rejected before the target
        // resume planner can quarantine source files.
        const RunPaths same = makePaths(root, "same");
        requireRejected(
            executable, same,
            options(targetDigits, same, sourceManifest, source.checkpoints),
            "directories must differ", sourceBefore, source.checkpoints,
            "same-directory run");
        if (std::filesystem::exists(same.directory / "quarantine"))
            throw std::runtime_error("same-directory run created quarantine");

        // A bare manifest filename resolves against the working directory.
        const RunPaths bare = makePaths(root, "bare");
        for (const auto& entry : sourceBefore)
            writeFile(bare.directory / entry.first, entry.second);
        requireStatus(
            run(executable,
                options(targetDigits, bare, sourceManifest.filename()), bare),
            0, bare, "bare-filename run");
        if (readFile(bare.output) != readFile(cold.output))
            throw std::runtime_error("bare-filename output differs");

        // A corrupt source block rejects the whole promotion, unchanged.
        const RunPaths corrupt = makePaths(root, "corrupt");
        const std::filesystem::path corruptSource = corrupt.directory / "source";
        std::filesystem::create_directories(corruptSource);
        for (const auto& entry : sourceBefore)
        {
            std::string bytes = entry.second;
            if (std::filesystem::path(entry.first).extension() == ".checkpoint")
                bytes.back() = static_cast<char>(bytes.back() ^ 0x01);
            writeFile(corruptSource / entry.first, bytes);
        }
        const auto corruptBefore = snapshot(corruptSource);
        requireRejected(
            executable, corrupt,
            options(targetDigits, corrupt,
                corruptSource / sourceManifest.filename()),
            "promotion source block", corruptBefore, corruptSource,
            "corrupt-source run");
        if (!filesWithExtension(corrupt.checkpoints, ".manifest").empty())
            throw std::runtime_error("corrupt-source run published a manifest");

        // A source that is not strictly lower precision is rejected.
        const RunPaths equal = makePaths(root, "equal");
        requireRejected(
            executable, equal,
            options(sourceDigits, equal, sourceManifest),
            "fewer digits and terms", sourceBefore, source.checkpoints,
            "equal-precision run");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "CheckpointPromotionCliTest: " << error.what() << '\n';
        return 1;
    }
}
