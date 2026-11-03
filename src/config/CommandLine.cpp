#include "config/CommandLine.hpp"

#include <charconv>
#include <cstdint>
#include <limits>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>

namespace pi::config
{
namespace
{

[[noreturn]]
void throwInvalidValue(
    std::string_view option,
    std::string_view value,
    std::string_view expected)
{
    throw CommandLineUsageError(
        "invalid value for " + std::string(option) + ": '"
        + std::string(value) + "' (expected " + std::string(expected) + ')'
    );
}


template<typename Unsigned>
Unsigned parseUnsigned(std::string_view option, std::string_view value)
{
    Unsigned parsed = 0;
    const auto result = std::from_chars(
        value.data(),
        value.data() + value.size(),
        parsed
    );
    if (value.empty()
        || result.ec != std::errc{}
        || result.ptr != value.data() + value.size())
    {
        throwInvalidValue(
            option,
            value,
            "a non-negative integer up to "
                + std::to_string(std::numeric_limits<Unsigned>::max())
        );
    }
    return parsed;
}


ProgressFormat parseFormat(std::string_view option, std::string_view value)
{
    if (value == toString(ProgressFormat::text))
    {
        return ProgressFormat::text;
    }
    if (value == toString(ProgressFormat::json))
    {
        return ProgressFormat::json;
    }
    throwInvalidValue(option, value, "text or json");
}


/// Returns the value following `option` and advances `index` past it.
std::string_view requireValue(
    std::string_view option,
    int& index,
    int argc,
    const char* const argv[])
{
    if ((index + 1) >= argc)
    {
        throw CommandLineUsageError(
            "missing value for " + std::string(option)
        );
    }
    return argv[++index];
}


std::string_view enabledText(bool enabled) noexcept
{
    return enabled ? "enabled" : "disabled";
}


template<typename Value, typename Target>
void applyIfSet(const std::optional<Value>& value, Target& target)
{
    if (value)
    {
        target = *value;
    }
}

} // namespace


CommandLineOptions CommandLine::parse(int argc, const char* const argv[])
{
    CommandLineOptions options;

    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg{argv[i]};

        if (arg == "-h" || arg == "--help")
        {
            options.help_requested = true;
            return options;
        }
        else if (arg == "--digits")
        {
            options.digits = parseUnsigned<std::uint64_t>(
                arg, requireValue(arg, i, argc, argv));
        }
        else if (arg == "--threads")
        {
            options.threads = parseUnsigned<std::uint32_t>(
                arg, requireValue(arg, i, argc, argv));
        }
        else if (arg == "--output")
        {
            options.output_file = std::string(requireValue(arg, i, argc, argv));
        }
        else if (arg == "--checkpoint")
        {
            options.checkpoint_enabled = true;
        }
        else if (arg == "--no-checkpoint")
        {
            options.checkpoint_enabled = false;
        }
        else if (arg == "--checkpoint-interval")
        {
            options.checkpoint_interval_seconds = parseUnsigned<std::uint32_t>(
                arg, requireValue(arg, i, argc, argv));
        }
        else if (arg == "--checkpoint-directory")
        {
            options.checkpoint_directory =
                std::string(requireValue(arg, i, argc, argv));
        }
        else if (arg == "--checkpoint-quarantine-directory")
        {
            options.checkpoint_quarantine_directory =
                std::string(requireValue(arg, i, argc, argv));
        }
        else if (arg == "--resume")
        {
            options.resume_enabled = true;
        }
        else if (arg == "--no-resume")
        {
            options.resume_enabled = false;
        }
        else if (arg == "--progress")
        {
            options.progress_enabled = true;
        }
        else if (arg == "--no-progress")
        {
            options.progress_enabled = false;
        }
        else if (arg == "--progress-interval")
        {
            options.progress_interval_ms = parseUnsigned<std::uint32_t>(
                arg, requireValue(arg, i, argc, argv));
        }
        else if (arg == "--progress-format")
        {
            options.progress_format = parseFormat(
                arg, requireValue(arg, i, argc, argv));
        }
        else if (arg == "--storage-directory")
        {
            options.storage_directory =
                std::string(requireValue(arg, i, argc, argv));
        }
        else if (arg == "--storage-memory-budget-bytes")
        {
            options.storage_memory_budget_bytes = parseUnsigned<std::uint64_t>(
                arg, requireValue(arg, i, argc, argv));
        }
        else if (arg == "--storage-chunk-size-bytes")
        {
            options.storage_target_chunk_size_bytes =
                parseUnsigned<std::uint64_t>(
                    arg, requireValue(arg, i, argc, argv));
        }
        else if (arg == "--storage-max-concurrent-io")
        {
            options.storage_max_concurrent_io = parseUnsigned<std::uint32_t>(
                arg, requireValue(arg, i, argc, argv));
        }
        else if (arg == "--verify")
        {
            options.verification_enabled = true;
        }
        else if (arg == "--no-verify")
        {
            options.verification_enabled = false;
        }
        else if (arg == "--bbp-samples")
        {
            options.bbp_sample_count = parseUnsigned<std::uint32_t>(
                arg, requireValue(arg, i, argc, argv));
        }
        else if (arg == "--verification-manifest")
        {
            options.verification_manifest_file =
                std::string(requireValue(arg, i, argc, argv));
        }
        else
        {
            throw CommandLineUsageError(
                "unknown option: " + std::string(arg)
            );
        }
    }

    return options;
}


void CommandLine::applyOverrides(
    Config& config,
    const CommandLineOptions& options)
{
    applyIfSet(options.digits, config.digits);
    applyIfSet(options.threads, config.threads);
    applyIfSet(options.output_file, config.output_file);
    applyIfSet(options.checkpoint_enabled, config.checkpoint_enabled);
    applyIfSet(
        options.checkpoint_interval_seconds,
        config.checkpoint_interval_seconds
    );
    applyIfSet(options.checkpoint_directory, config.checkpoint_directory);
    applyIfSet(
        options.checkpoint_quarantine_directory,
        config.checkpoint_quarantine_directory
    );
    applyIfSet(options.resume_enabled, config.resume_enabled);
    applyIfSet(options.progress_enabled, config.progress_enabled);
    applyIfSet(options.progress_interval_ms, config.progress_interval_ms);
    applyIfSet(options.progress_format, config.progress_format);
    applyIfSet(options.storage_directory, config.storage_directory);
    applyIfSet(
        options.storage_memory_budget_bytes,
        config.storage_memory_budget_bytes
    );
    applyIfSet(
        options.storage_target_chunk_size_bytes,
        config.storage_target_chunk_size_bytes
    );
    applyIfSet(
        options.storage_max_concurrent_io,
        config.storage_max_concurrent_io
    );
    applyIfSet(options.verification_enabled, config.verification_enabled);
    applyIfSet(options.bbp_sample_count, config.bbp_sample_count);
    applyIfSet(
        options.verification_manifest_file,
        config.verification_manifest_file
    );

    validateConfig(config);
}


void CommandLine::writeHelp(std::ostream& output)
{
    output
        << "Usage: pi-engine [options]\n"
        << "\n"
        << "Calculate decimal digits of pi with the Chudnovsky formula and\n"
        << "write them to a file. Every option value is a separate argument\n"
        << "(--digits 1000, not --digits=1000).\n"
        << "\n"
        << "Calculation:\n"
        << "  --digits <count>\n"
        << "      Decimal digits to calculate; integer >= 1.\n"
        << "      Default: " << defaults::DIGITS << ".\n"
        << "  --threads <count>\n"
        << "      Worker threads; 0 uses all hardware threads, maximum 1024.\n"
        << "      Default: " << defaults::THREADS << ".\n"
        << "\n"
        << "Output:\n"
        << "  --output <path>\n"
        << "      Decimal result file; must not be empty. The file is\n"
        << "      overwritten. Default: " << defaults::OUTPUT_FILE << ".\n"
        << "\n"
        << "Checkpoint and resume:\n"
        << "  --checkpoint | --no-checkpoint\n"
        << "      Write durable checkpoint blocks during the calculation.\n"
        << "      Default: " << enabledText(defaults::CHECKPOINT_ENABLED)
        << ".\n"
        << "  --checkpoint-interval <seconds>\n"
        << "      Seconds between checkpoint commits; integer >= 1 when\n"
        << "      checkpoints are enabled. Default: "
        << defaults::CHECKPOINT_INTERVAL_SECONDS << ".\n"
        << "  --checkpoint-directory <path>\n"
        << "      Checkpoint blocks and manifest; must not be empty when\n"
        << "      checkpoint or resume is enabled. Default: "
        << defaults::CHECKPOINT_DIRECTORY << ".\n"
        << "  --checkpoint-quarantine-directory <path>\n"
        << "      Destination for rejected checkpoint files; must not be\n"
        << "      empty when checkpoint or resume is enabled.\n"
        << "      Default: " << defaults::CHECKPOINT_QUARANTINE_DIRECTORY
        << ".\n"
        << "  --resume | --no-resume\n"
        << "      Reuse verified checkpoint blocks of the same computation.\n"
        << "      Default: " << enabledText(defaults::RESUME_ENABLED) << ".\n"
        << "\n"
        << "Progress:\n"
        << "  --progress | --no-progress\n"
        << "      Report progress on stderr. Default: "
        << enabledText(defaults::PROGRESS_ENABLED) << ".\n"
        << "  --progress-interval <milliseconds>\n"
        << "      Reporting interval; integer >= 1. Default: "
        << defaults::PROGRESS_INTERVAL_MS << ".\n"
        << "  --progress-format <text|json>\n"
        << "      text, or json for JSON Lines records. Default: "
        << defaults::PROGRESS_FORMAT << ".\n"
        << "\n"
        << "Out-of-core storage (used when out_of_core is enabled in\n"
        << "config.toml; default: "
        << enabledText(defaults::OUT_OF_CORE_ENABLED)
        << ", compression " << defaults::COMPRESSION << "):\n"
        << "  --storage-directory <path>\n"
        << "      Spilled chunks and index metadata; must not be empty.\n"
        << "      Default: " << defaults::STORAGE_DIRECTORY << ".\n"
        << "  --storage-memory-budget-bytes <bytes>\n"
        << "      Resident memory budget; integer >= 1.\n"
        << "      Default: " << defaults::STORAGE_MEMORY_BUDGET_BYTES << ".\n"
        << "  --storage-chunk-size-bytes <bytes>\n"
        << "      Target chunk size; integer from 1 to the memory budget.\n"
        << "      Default: " << defaults::STORAGE_TARGET_CHUNK_SIZE_BYTES
        << ".\n"
        << "  --storage-max-concurrent-io <count>\n"
        << "      Concurrent storage I/O operations; integer >= 1.\n"
        << "      Default: " << defaults::STORAGE_MAX_CONCURRENT_IO << ".\n"
        << "\n"
        << "Final verification:\n"
        << "  --verify | --no-verify\n"
        << "      Verify the written result (hash, BBP samples, known\n"
        << "      digits) and save a verification manifest. Default: "
        << enabledText(defaults::VERIFICATION_ENABLED) << ".\n"
        << "  --bbp-samples <count>\n"
        << "      BBP spot-check samples; integer from 1 to 32.\n"
        << "      Default: " << defaults::BBP_SAMPLE_COUNT << ".\n"
        << "  --verification-manifest <path>\n"
        << "      Verification manifest file. Default: <output>.verify.\n"
        << "\n"
        << "General:\n"
        << "  -h | --help\n"
        << "      Print this help and exit without reading config.toml or\n"
        << "      creating files.\n"
        << "\n"
        << "Configuration precedence (later wins):\n"
        << "  built-in defaults, then config.toml in the current working\n"
        << "  directory (optional), then command-line options. A repeated\n"
        << "  option uses its last value. out_of_core, compression, and\n"
        << "  huge_pages are set only in config.toml.\n"
        << "\n"
        << "Paths:\n"
        << "  config.toml and relative output, checkpoint, quarantine,\n"
        << "  storage, and manifest paths resolve against the current working\n"
        << "  directory, not the executable location. Resume from the same\n"
        << "  working directory or use absolute paths.\n"
        << "\n"
        << "Exit status:\n"
        << "  0  success, or help printed\n"
        << "  1  configuration, calculation, I/O, or verification failure\n"
        << "  2  command-line usage error\n"
        << "\n"
        << "Examples:\n"
        << "  pi-engine --digits 1000000 --threads 0\n"
        << "  pi-engine --output /data/pi/pi.txt --bbp-samples 16\n"
        << "  pi-engine --progress-format json 2>progress.jsonl\n"
        << "  pi-engine --no-checkpoint --no-resume --digits 100000\n";
}

} // namespace pi::config
