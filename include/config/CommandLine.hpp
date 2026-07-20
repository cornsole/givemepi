#pragma once

#include "config/Config.hpp"

namespace pi::config
{

class CommandLine
{
public:
    CommandLine() = delete;

    /// Apply command-line overrides to an existing configuration.
    ///
    /// Supported options (initial implementation):
    ///   --digits <value>
    ///   --threads <value>
    ///   --output <path>
    ///   --checkpoint
    ///   --no-checkpoint
    ///   --resume
    ///   --no-resume
    ///   --progress
    ///   --no-progress
    ///   --progress-interval <milliseconds>
    ///   --progress-format <text|json>
    ///
    /// Unknown options are ignored for now.
    ///
    /// @param config Configuration to modify.
    /// @param argc Argument count.
    /// @param argv Argument vector.
    static void applyOverrides(
        Config& config,
        int argc,
        char* argv[]
    );
};

} // namespace pi::config
