#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "config/Defaults.hpp"

namespace pi::config
{

enum class ProgressFormat
{
    text,
    json
};


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
    bool progress_enabled = defaults::PROGRESS_ENABLED;
    std::uint32_t progress_interval_ms =
        defaults::PROGRESS_INTERVAL_MS;
    ProgressFormat progress_format = ProgressFormat::text;

    // Output
    std::string output_file = std::string(defaults::OUTPUT_FILE);

    // Resume
    bool resume_enabled = defaults::RESUME_ENABLED;
};


/// Returns the stable configuration spelling of a progress format.
[[nodiscard]]
std::string_view toString(ProgressFormat format) noexcept;

/// Parses text or json, rejecting unsupported progress formats.
[[nodiscard]]
ProgressFormat parseProgressFormat(std::string_view value);

/// Runs shared validation after defaults, TOML, or CLI overrides.
void validateConfig(Config& config);

} // namespace pi::config
