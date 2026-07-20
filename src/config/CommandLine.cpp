#include "config/CommandLine.hpp"

#include <charconv>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace pi::config
{
namespace
{

std::uint32_t parseUint32(std::string_view option, std::string_view value)
{
    std::uint64_t parsed = 0;
    const auto result = std::from_chars(
        value.data(),
        value.data() + value.size(),
        parsed
    );
    if (result.ec != std::errc{}
        || result.ptr != value.data() + value.size()
        || parsed > std::numeric_limits<std::uint32_t>::max())
    {
        throw std::invalid_argument(
            "Invalid value for " + std::string(option) + ": "
            + std::string(value)
        );
    }
    return static_cast<std::uint32_t>(parsed);
}


const char* requireValue(const std::string& option, int& index, int argc, char* argv[])
{
    if ((index + 1) >= argc)
    {
        throw std::invalid_argument("Missing value for " + option);
    }
    return argv[++index];
}

} // namespace

void CommandLine::applyOverrides(
    Config& config,
    int argc,
    char* argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg{argv[i]};

        if (arg == "--digits")
        {
            config.digits = std::stoull(requireValue(arg, i, argc, argv));
        }
        else if (arg == "--threads")
        {
            config.threads = parseUint32(
                arg,
                requireValue(arg, i, argc, argv)
            );
        }
        else if (arg == "--output")
        {
            config.output_file = requireValue(arg, i, argc, argv);
        }
        else if (arg == "--checkpoint")
        {
            config.checkpoint_enabled = true;
        }
        else if (arg == "--no-checkpoint")
        {
            config.checkpoint_enabled = false;
        }
        else if (arg == "--resume")
        {
            config.resume_enabled = true;
        }
        else if (arg == "--no-resume")
        {
            config.resume_enabled = false;
        }
        else if (arg == "--progress")
        {
            config.progress_enabled = true;
        }
        else if (arg == "--no-progress")
        {
            config.progress_enabled = false;
        }
        else if (arg == "--progress-interval")
        {
            config.progress_interval_ms = parseUint32(
                arg,
                requireValue(arg, i, argc, argv)
            );
        }
        else if (arg == "--progress-format")
        {
            config.progress_format = parseProgressFormat(
                requireValue(arg, i, argc, argv)
            );
        }

        // Unknown arguments are ignored for now.
        // Future versions may report them as warnings or errors.
    }

    validateConfig(config);
}

} // namespace pi::config
