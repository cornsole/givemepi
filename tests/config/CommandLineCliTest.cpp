// Real-process regression for the pi-engine command line: help aliases,
// usage errors, exit status, and supported overrides. Each case runs the
// executable directly in its own working directory with stdout and stderr
// captured outside that directory, so any file the engine creates is visible.

#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
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

/// An invalid TOML document: loading it is a configuration failure (exit 1).
constexpr std::string_view invalidConfig = "digits = [unterminated\n";
constexpr std::string_view usageHint = "Try 'pi-engine --help' for usage.\n";

class TemporaryDirectory
{
public:
    TemporaryDirectory()
        : path_(std::filesystem::temp_directory_path()
            / ("givemepi-cli-options-" + std::to_string(::getpid())))
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

struct RunResult
{
    int status = -1;
    std::string out;
    std::string err;
};

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot read " + path.string());
    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

void writeFile(const std::filesystem::path& path, std::string_view content)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << content;
    if (!output) throw std::runtime_error("cannot write " + path.string());
}

/// Names of the entries directly inside `directory`.
std::set<std::string> entries(const std::filesystem::path& directory)
{
    std::set<std::string> names;
    for (const auto& entry : std::filesystem::directory_iterator(directory))
        names.insert(entry.path().filename().string());
    return names;
}

void require(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

/// Runs pi-engine without a shell inside `workingDirectory`, with stdin from
/// /dev/null; stdout and stderr go to files under `logDirectory`.
RunResult run(
    const std::filesystem::path& executable,
    const std::vector<std::string>& options,
    const std::filesystem::path& workingDirectory,
    const std::filesystem::path& logDirectory,
    std::chrono::seconds timeout)
{
    const std::filesystem::path outLog = logDirectory / "stdout.log";
    const std::filesystem::path errLog = logDirectory / "stderr.log";
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
            outLog.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0600);
        const int stderrFd = ::open(
            errLog.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0600);
        if (stdinFd < 0 || stdoutFd < 0 || stderrFd < 0
            || ::dup2(stdinFd, STDIN_FILENO) < 0
            || ::dup2(stdoutFd, STDOUT_FILENO) < 0
            || ::dup2(stderrFd, STDERR_FILENO) < 0
            || ::chdir(workingDirectory.c_str()) != 0)
        {
            ::_exit(126);
        }
        ::close(stdinFd);
        ::close(stdoutFd);
        ::close(stderrFd);
        ::execv(program.c_str(), argv.data());
        ::_exit(127);
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    int status = 0;
    while (true)
    {
        const pid_t waited = ::waitpid(pid, &status, WNOHANG);
        if (waited < 0) throw std::runtime_error("waitpid failed");
        if (waited == pid) break;
        if (std::chrono::steady_clock::now() >= deadline)
        {
            ::kill(pid, SIGKILL);
            static_cast<void>(::waitpid(pid, &status, 0));
            throw std::runtime_error("pi-engine timed out");
        }
        std::this_thread::sleep_for(10ms);
    }

    RunResult result;
    if (WIFEXITED(status)) result.status = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) result.status = 128 + WTERMSIG(status);
    result.out = readFile(outLog);
    result.err = readFile(errLog);
    return result;
}

/// Fixture: a fresh working directory containing only `config.toml` and a
/// separate log directory.
struct Case
{
    std::filesystem::path work;
    std::filesystem::path logs;
};

Case makeCase(
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view config)
{
    Case value{root / name / "work", root / name / "logs"};
    std::filesystem::create_directories(value.work);
    std::filesystem::create_directories(value.logs);
    writeFile(value.work / "config.toml", config);
    return value;
}

std::string describe(const std::vector<std::string>& options)
{
    std::string text = "pi-engine";
    for (const std::string& option : options) text += ' ' + option;
    return text;
}

/// Help must exit 0 with help on stdout, nothing on stderr, and no change to
/// the working directory, even though config.toml there is invalid.
std::string requireHelp(
    const std::filesystem::path& executable,
    const std::filesystem::path& root,
    std::string_view name,
    const std::vector<std::string>& options)
{
    const Case fixture = makeCase(root, name, invalidConfig);
    const RunResult result = run(
        executable, options, fixture.work, fixture.logs, 30s);
    const std::string label = describe(options);
    require(result.status == 0,
        label + " exited with " + std::to_string(result.status)
            + ", stderr: " + result.err);
    require(result.err.empty(), label + " wrote to stderr: " + result.err);
    require(result.out.rfind("Usage: pi-engine [options]\n", 0) == 0,
        label + " did not print usage");
    require(entries(fixture.work) == std::set<std::string>{"config.toml"},
        label + " created files in the working directory");
    return result.out;
}

/// A usage error must exit 2 with the exact diagnostic on stderr, nothing on
/// stdout, and no files created; it is detected before config.toml is read.
void requireUsageError(
    const std::filesystem::path& executable,
    const std::filesystem::path& root,
    std::string_view name,
    const std::vector<std::string>& options,
    const std::string& expectedStderr)
{
    const Case fixture = makeCase(root, name, invalidConfig);
    const RunResult result = run(
        executable, options, fixture.work, fixture.logs, 30s);
    const std::string label = describe(options);
    require(result.status == 2,
        label + " exited with " + std::to_string(result.status)
            + ", stderr: " + result.err);
    require(result.out.empty(), label + " wrote to stdout: " + result.out);
    require(result.err == expectedStderr,
        label + " stderr mismatch: " + result.err);
    require(entries(fixture.work) == std::set<std::string>{"config.toml"},
        label + " created files in the working directory");
}

void requireConfigurationFailures(
    const std::filesystem::path& executable,
    const std::filesystem::path& root)
{
    const Case broken = makeCase(root, "invalid-config", invalidConfig);
    const RunResult brokenRun = run(
        executable, {}, broken.work, broken.logs, 30s);
    require(brokenRun.status == 1,
        "invalid config.toml exited with " + std::to_string(brokenRun.status));
    require(brokenRun.err.rfind("pi-engine: Config TOML parse error", 0) == 0,
        "invalid config.toml diagnostic mismatch: " + brokenRun.err);
    require(brokenRun.err.find("--help") == std::string::npos,
        "configuration failure was reported as a usage error");

    const Case range = makeCase(root, "invalid-range", "");
    const RunResult rangeRun = run(
        executable, {"--bbp-samples", "33"}, range.work, range.logs, 30s);
    require(rangeRun.status == 1,
        "--bbp-samples 33 exited with " + std::to_string(rangeRun.status));
    require(rangeRun.err
            == "pi-engine: Config validation failed: "
               "BBP samples must be between 1 and 32.\n",
        "--bbp-samples 33 diagnostic mismatch: " + rangeRun.err);
    require(entries(range.work) == std::set<std::string>{"config.toml"},
        "--bbp-samples 33 created files in the working directory");
}

/// CLI options override config.toml, and config.toml values remain in effect
/// for options that are not given.
void requireSupportedOverrides(
    const std::filesystem::path& executable,
    const std::filesystem::path& root)
{
    const Case fixture = makeCase(root, "overrides",
        "digits = 50\n"
        "output = \"from-config.txt\"\n"
        "progress = true\n"
        "checkpoint = false\n"
        "resume = false\n");
    const std::vector<std::string> options{
        "--digits", "1000",
        "--threads", "1",
        "--output", "cli-pi.txt",
        "--no-progress",
        "--verify",
        "--bbp-samples", "2",
        "--verification-manifest", "cli-pi.verify",
    };
    const RunResult result = run(
        executable, options, fixture.work, fixture.logs, 120s);
    require(result.status == 0,
        "override run exited with " + std::to_string(result.status)
            + ", stderr: " + result.err);
    require(result.err.empty(),
        "--no-progress run wrote to stderr: " + result.err);
    require(result.out.rfind("Calculated 1000 digits to cli-pi.txt (verified;"
            " manifest cli-pi.verify; sha256 ", 0) == 0,
        "override run stdout mismatch: " + result.out);

    const std::string digits = readFile(fixture.work / "cli-pi.txt");
    require(digits.rfind("3.14159265358979323846", 0) == 0,
        "override output does not start with pi");
    require(std::filesystem::file_size(fixture.work / "cli-pi.verify") > 0,
        "verification manifest override was not written");

    const std::set<std::string> created = entries(fixture.work);
    require(!created.contains("from-config.txt") && !created.contains("pi.txt")
            && !created.contains("cli-pi.txt.verify"),
        "output overrides were not honored");
    require(!created.contains("checkpoints")
            && !created.contains("checkpoint-quarantine"),
        "config.toml checkpoint=false/resume=false was not honored");
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
        const std::filesystem::path& root = temporary.path();

        const std::string longHelp =
            requireHelp(executable, root, "help-long", {"--help"});
        const std::string shortHelp =
            requireHelp(executable, root, "help-short", {"-h"});
        require(longHelp == shortHelp, "-h and --help differ");
        require(requireHelp(executable, root, "help-after-options",
                    {"--digits", "5", "--output", "x.txt", "--help"})
                == longHelp,
            "help after options differs");

        requireUsageError(executable, root, "unknown-option",
            {"--digit", "1000"},
            "pi-engine: unknown option: --digit\n" + std::string(usageHint));
        requireUsageError(executable, root, "unknown-version",
            {"--version"},
            "pi-engine: unknown option: --version\n" + std::string(usageHint));
        requireUsageError(executable, root, "missing-value",
            {"--no-progress", "--digits"},
            "pi-engine: missing value for --digits\n" + std::string(usageHint));
        requireUsageError(executable, root, "invalid-number",
            {"--threads", "four"},
            "pi-engine: invalid value for --threads: 'four' (expected a "
            "non-negative integer up to 4294967295)\n"
                + std::string(usageHint));
        requireUsageError(executable, root, "invalid-format",
            {"--progress-format", "xml"},
            "pi-engine: invalid value for --progress-format: 'xml' "
            "(expected text or json)\n" + std::string(usageHint));

        requireConfigurationFailures(executable, root);
        requireSupportedOverrides(executable, root);
    }
    catch (const std::exception& error)
    {
        std::cerr << "CommandLineCliTest failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
