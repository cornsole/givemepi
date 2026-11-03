#include "config/CommandLine.hpp"

#include <cassert>
#include <cctype>
#include <initializer_list>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>


namespace
{

using pi::config::CommandLine;
using pi::config::CommandLineOptions;
using pi::config::CommandLineUsageError;

struct SupportedOption
{
    std::string_view name;
    const char* sampleValue;
};

/// Every option the CLI accepts, with a value that parses successfully.
constexpr SupportedOption supportedOptions[] = {
    {"--digits", "1000"},
    {"--threads", "2"},
    {"--output", "out.txt"},
    {"--checkpoint", nullptr},
    {"--no-checkpoint", nullptr},
    {"--checkpoint-interval", "60"},
    {"--checkpoint-directory", "cp"},
    {"--checkpoint-quarantine-directory", "cq"},
    {"--resume", nullptr},
    {"--no-resume", nullptr},
    {"--progress", nullptr},
    {"--no-progress", nullptr},
    {"--progress-interval", "250"},
    {"--progress-format", "json"},
    {"--storage-directory", "st"},
    {"--storage-memory-budget-bytes", "1048576"},
    {"--storage-chunk-size-bytes", "4096"},
    {"--storage-max-concurrent-io", "2"},
    {"--verify", nullptr},
    {"--no-verify", nullptr},
    {"--bbp-samples", "4"},
    {"--verification-manifest", "out.verify"},
};


CommandLineOptions parse(std::initializer_list<const char*> values)
{
    std::vector<const char*> arguments{"pi-engine"};
    arguments.insert(arguments.end(), values.begin(), values.end());
    return CommandLine::parse(
        static_cast<int>(arguments.size()),
        arguments.data()
    );
}


/// Returns the usage error message, or an empty string if parsing succeeded.
std::string usageError(std::initializer_list<const char*> values)
{
    try
    {
        static_cast<void>(parse(values));
    }
    catch (const CommandLineUsageError& error)
    {
        return error.what();
    }
    return {};
}


std::string helpText()
{
    std::ostringstream output;
    CommandLine::writeHelp(output);
    return output.str();
}


/// Extracts option tokens (`-h`, `--name`) that start a word in the help text.
std::set<std::string> helpOptionTokens(std::string_view help)
{
    std::set<std::string> tokens;
    for (std::size_t i = 0; i < help.size(); ++i)
    {
        const bool wordStart = i == 0 || help[i - 1] == ' '
            || help[i - 1] == '\n' || help[i - 1] == '(';
        if (!wordStart || help[i] != '-')
        {
            continue;
        }
        std::size_t end = i;
        while (end < help.size()
               && (help[end] == '-'
                   || std::islower(static_cast<unsigned char>(help[end]))))
        {
            ++end;
        }
        if (end - i > 1)
        {
            tokens.emplace(help.substr(i, end - i));
        }
        i = end;
    }
    return tokens;
}


void testNoArgumentsKeepConfiguration()
{
    const CommandLineOptions options = parse({});
    assert(!options.help_requested);
    assert(!options.digits && !options.threads && !options.output_file);
    assert(!options.checkpoint_enabled && !options.resume_enabled);
    assert(!options.progress_enabled && !options.progress_format);
    assert(!options.storage_directory && !options.verification_enabled);

    pi::config::Config config;
    config.digits = 777;
    config.output_file = "from-config.txt";
    CommandLine::applyOverrides(config, parse({"--threads", "3"}));
    assert(config.digits == 777);
    assert(config.output_file == "from-config.txt");
    assert(config.threads == 3);
}


void testHelpAliases()
{
    assert(parse({"-h"}).help_requested);
    assert(parse({"--help"}).help_requested);
    assert(parse({"--digits", "5", "--help"}).help_requested);
    // Parsing stops at help, so later arguments are not inspected.
    assert(parse({"-h", "--not-an-option"}).help_requested);
    // Arguments are parsed left to right; an earlier error wins.
    assert(usageError({"--not-an-option", "--help"})
        == "unknown option: --not-an-option");
}


void testUnknownOptionsAreRejected()
{
    assert(usageError({"--digit", "1000"}) == "unknown option: --digit");
    assert(usageError({"--digits=1000"}) == "unknown option: --digits=1000");
    assert(usageError({"--version"}) == "unknown option: --version");
    assert(usageError({"-x"}) == "unknown option: -x");
    assert(usageError({"1000"}) == "unknown option: 1000");
    assert(usageError({"--no-progress", "--help-me"})
        == "unknown option: --help-me");
}


void testMalformedValuesAreUsageErrors()
{
    assert(usageError({"--digits"}) == "missing value for --digits");
    assert(usageError({"--output"}) == "missing value for --output");
    assert(usageError({"--progress-format"})
        == "missing value for --progress-format");

    for (const char* value : {"-1", "1e6", "", " 5", "+5", "12abc"})
    {
        const std::string message = usageError({"--digits", value});
        assert(message.rfind("invalid value for --digits: '", 0) == 0);
    }
    assert(usageError({"--digits", "18446744073709551616"}).rfind(
        "invalid value for --digits: '18446744073709551616'", 0) == 0);
    assert(usageError({"--threads", "4294967296"}).rfind(
        "invalid value for --threads: '4294967296'", 0) == 0);
    assert(usageError({"--progress-format", "xml"})
        == "invalid value for --progress-format: 'xml' (expected text or json)");
    assert(usageError({"--bbp-samples", "eight"}).rfind(
        "invalid value for --bbp-samples:", 0) == 0);
}


void testConfigurationConstraintsAreNotUsageErrors()
{
    // Well-formed values are parsed; range rules apply to the merged config.
    const CommandLineOptions options = parse({"--bbp-samples", "0"});
    pi::config::Config config;
    bool rejected = false;
    try
    {
        CommandLine::applyOverrides(config, options);
    }
    catch (const CommandLineUsageError&)
    {
        assert(false && "validation must not be reported as usage error");
    }
    catch (const std::runtime_error&)
    {
        rejected = true;
    }
    assert(rejected);
}


void testSupportedOverrides()
{
    pi::config::Config config;
    CommandLine::applyOverrides(config, parse({
        "--digits", "10", "--digits", "2500",
        "--threads", "4",
        "--output", "result.txt",
        "--checkpoint", "--no-checkpoint",
        "--checkpoint-interval", "45",
        "--checkpoint-directory", "cp",
        "--checkpoint-quarantine-directory", "cq",
        "--no-resume",
        "--no-progress",
        "--progress-interval", "125",
        "--progress-format", "json",
        "--storage-directory", "st",
        "--storage-memory-budget-bytes", "2097152",
        "--storage-chunk-size-bytes", "524288",
        "--storage-max-concurrent-io", "3",
        "--no-verify",
        "--bbp-samples", "12",
        "--verification-manifest", "result.verify",
    }));
    assert(config.digits == 2500);
    assert(config.threads == 4);
    assert(config.output_file == "result.txt");
    assert(!config.checkpoint_enabled);
    assert(config.checkpoint_interval_seconds == 45);
    assert(config.checkpoint_directory == "cp");
    assert(config.checkpoint_quarantine_directory == "cq");
    assert(!config.resume_enabled);
    assert(!config.progress_enabled);
    assert(config.progress_interval_ms == 125);
    assert(config.progress_format == pi::config::ProgressFormat::json);
    assert(config.storage_directory == "st");
    assert(config.storage_memory_budget_bytes == 2097152);
    assert(config.storage_target_chunk_size_bytes == 524288);
    assert(config.storage_max_concurrent_io == 3);
    assert(!config.verification_enabled);
    assert(config.bbp_sample_count == 12);
    assert(config.verification_manifest_file == "result.verify");

    CommandLine::applyOverrides(config, parse({
        "--digits", "18446744073709551615",
        "--checkpoint", "--resume", "--progress", "--verify",
        "--progress-format", "text",
    }));
    assert(config.digits == 18446744073709551615ULL);
    assert(config.checkpoint_enabled && config.resume_enabled);
    assert(config.progress_enabled && config.verification_enabled);
    assert(config.progress_format == pi::config::ProgressFormat::text);
}


void testHelpListsExactlySupportedOptions()
{
    const std::string help = helpText();
    std::set<std::string> expected{"-h", "--help"};
    for (const SupportedOption& option : supportedOptions)
    {
        expected.emplace(option.name);
        if (option.sampleValue == nullptr)
        {
            static_cast<void>(parse({option.name.data()}));
        }
        else
        {
            static_cast<void>(parse({option.name.data(), option.sampleValue}));
        }
    }
    assert(helpOptionTokens(help) == expected);
    assert(help.find("--version") == std::string::npos);
}


void testHelpContent()
{
    namespace defaults = pi::config::defaults;
    const std::string help = helpText();
    assert(help.rfind("Usage: pi-engine [options]\n", 0) == 0);
    assert(help.back() == '\n');

    std::size_t previous = 0;
    for (std::string_view heading : {
             "\nCalculation:\n",
             "\nOutput:\n",
             "\nCheckpoint and resume:\n",
             "\nProgress:\n",
             "\nOut-of-core storage",
             "\nFinal verification:\n",
             "\nGeneral:\n",
             "\nConfiguration precedence",
             "\nPaths:\n",
             "\nExit status:\n",
             "\nExamples:\n"})
    {
        const std::size_t position = help.find(heading);
        assert(position != std::string::npos && position > previous);
        previous = position;
    }

    const auto hasDefault = [&help](const std::string& value) {
        return help.find("Default: " + value + '.') != std::string::npos;
    };
    assert(hasDefault(std::to_string(defaults::DIGITS)));
    assert(hasDefault(std::to_string(defaults::THREADS)));
    assert(hasDefault(std::string(defaults::OUTPUT_FILE)));
    assert(hasDefault(std::to_string(defaults::CHECKPOINT_INTERVAL_SECONDS)));
    assert(hasDefault(std::string(defaults::CHECKPOINT_DIRECTORY)));
    assert(hasDefault(std::string(defaults::CHECKPOINT_QUARANTINE_DIRECTORY)));
    assert(hasDefault(std::to_string(defaults::PROGRESS_INTERVAL_MS)));
    assert(hasDefault(std::string(defaults::PROGRESS_FORMAT)));
    assert(hasDefault(std::string(defaults::STORAGE_DIRECTORY)));
    assert(hasDefault(std::to_string(defaults::STORAGE_MEMORY_BUDGET_BYTES)));
    assert(hasDefault(
        std::to_string(defaults::STORAGE_TARGET_CHUNK_SIZE_BYTES)));
    assert(hasDefault(std::to_string(defaults::STORAGE_MAX_CONCURRENT_IO)));
    assert(hasDefault(std::to_string(defaults::BBP_SAMPLE_COUNT)));
    assert(hasDefault("<output>.verify"));
    assert(help.find("compression " + std::string(defaults::COMPRESSION))
        != std::string::npos);

    for (std::string_view text : {
             "<text|json>",
             "integer from 1 to 32",
             "0 uses all hardware threads, maximum 1024",
             "<milliseconds>",
             "<seconds>",
             "<bytes>",
             "built-in defaults, then config.toml in the current working",
             "then command-line options",
             "resolve against the current working",
             "2  command-line usage error"})
    {
        assert(help.find(text) != std::string::npos);
    }
}

} // namespace


int main()
{
    testNoArgumentsKeepConfiguration();
    testHelpAliases();
    testUnknownOptionsAreRejected();
    testMalformedValuesAreUsageErrors();
    testConfigurationConstraintsAreNotUsageErrors();
    testSupportedOverrides();
    testHelpListsExactlySupportedOptions();
    testHelpContent();
    return 0;
}
