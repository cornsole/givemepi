#include "config/CommandLine.hpp"

#include <cstring>
#include <string>

namespace pi::config
{

void CommandLine::applyOverrides(
    Config& config,
    int argc,
    char* argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg{argv[i]};

        if (arg == "--digits" && (i + 1) < argc)
        {
            config.digits = std::stoull(argv[++i]);
        }
        else if (arg == "--threads" && (i + 1) < argc)
        {
            config.threads = static_cast<std::uint32_t>(
                std::stoul(argv[++i]));
        }
        else if (arg == "--output" && (i + 1) < argc)
        {
            config.output_file = argv[++i];
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

        // Unknown arguments are ignored for now.
        // Future versions may report them as warnings or errors.
    }
}

} // namespace pi::config