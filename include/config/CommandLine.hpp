#pragma once

#include "config/Config.hpp"

#include <cstdint>
#include <iosfwd>
#include <optional>
#include <stdexcept>
#include <string>

namespace pi::config
{

/// Malformed command line: an unknown option, a missing option value, or a
/// value that cannot be parsed as the option's type or accepted spelling.
///
/// The CLI reports this error with a pointer to `--help` and exit status 2.
/// Constraints evaluated on the merged configuration (for example
/// `--bbp-samples 0`) are configuration validation failures, not usage errors.
class CommandLineUsageError : public std::invalid_argument
{
public:
    using std::invalid_argument::invalid_argument;
};


/// Command-line request parsed before `config.toml` is read.
///
/// Each engaged field overrides the matching `Config` field; disengaged
/// fields keep the default or configuration-file value. When an option
/// repeats, the last occurrence wins.
struct CommandLineOptions
{
    /// `-h` or `--help` was given; no other option after it was parsed.
    bool help_requested = false;

    // Calculation
    std::optional<std::uint64_t> digits;
    std::optional<std::uint32_t> threads;

    // Output
    std::optional<std::string> output_file;

    // Checkpoint and resume
    std::optional<bool> checkpoint_enabled;
    std::optional<std::uint32_t> checkpoint_interval_seconds;
    std::optional<std::string> checkpoint_directory;
    std::optional<std::string> checkpoint_quarantine_directory;
    std::optional<bool> resume_enabled;

    // Progress
    std::optional<bool> progress_enabled;
    std::optional<std::uint32_t> progress_interval_ms;
    std::optional<ProgressFormat> progress_format;

    // Out-of-core storage
    std::optional<std::string> storage_directory;
    std::optional<std::uint64_t> storage_memory_budget_bytes;
    std::optional<std::uint64_t> storage_target_chunk_size_bytes;
    std::optional<std::uint32_t> storage_max_concurrent_io;

    // Final verification
    std::optional<bool> verification_enabled;
    std::optional<std::uint32_t> bbp_sample_count;
    std::optional<std::string> verification_manifest_file;
};


class CommandLine
{
public:
    CommandLine() = delete;

    /// Parse the command line without touching configuration files or the
    /// filesystem.
    ///
    /// Arguments are read left to right; every value is the next separate
    /// argument (`--digits 1000`, not `--digits=1000`). Parsing stops at the
    /// first `-h` or `--help` and returns `help_requested = true`.
    /// The supported options are exactly those listed by `writeHelp`.
    ///
    /// Time complexity: O(total argument length). Memory: O(path lengths).
    ///
    /// @param argc Argument count, including the program name.
    /// @param argv Argument vector; `argv[0]` is the program name.
    /// @return Parsed overrides.
    /// @throws CommandLineUsageError for unknown options, missing values, or
    ///         malformed values.
    [[nodiscard]]
    static CommandLineOptions parse(int argc, const char* const argv[]);

    /// Apply parsed overrides to a configuration and validate the result.
    ///
    /// @param config Configuration loaded from defaults and `config.toml`.
    /// @param options Overrides returned by `parse`.
    /// @throws std::exception subclasses from `validateConfig` when the
    ///         merged configuration is invalid.
    static void applyOverrides(
        Config& config,
        const CommandLineOptions& options
    );

    /// Write the `--help` text: grouped options, built-in defaults from
    /// `config/Defaults.hpp`, accepted values, configuration precedence,
    /// path resolution, exit status, and examples.
    ///
    /// @param output Destination stream.
    static void writeHelp(std::ostream& output);
};

} // namespace pi::config
