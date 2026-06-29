#pragma once

#include <cstdint>
#include <string>

#include "config/Defaults.hpp"

namespace pi::config
{

struct Config
{
    // Precision
    std::uint64_t digits = defaults::DIGITS;

    // Threading
    std::uint32_t threads = defaults::THREADS;

    // Checkpoint
    bool checkpoint_enabled = defaults::CHECKPOINT_ENABLED;
    std::uint32_t checkpoint_interval_seconds =
        defaults::CHECKPOINT_INTERVAL_SECONDS;

    // Memory
    bool huge_pages_enabled = defaults::HUGE_PAGES_ENABLED;

    // Out-of-Core Storage
    bool out_of_core_enabled = defaults::OUT_OF_CORE_ENABLED;
    std::string compression = std::string(defaults::COMPRESSION);

    // Progress
    std::uint32_t progress_interval_ms =
        defaults::PROGRESS_INTERVAL_MS;

    // Output
    std::string output_file = std::string(defaults::OUTPUT_FILE);

    // Resume
    bool resume_enabled = defaults::RESUME_ENABLED;
};

} // namespace pi::config