#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "config/Defaults.hpp"

namespace pi::storage { struct StoragePolicy; }

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
    std::string storage_directory = std::string(defaults::STORAGE_DIRECTORY);
    std::uint64_t storage_memory_budget_bytes =
        defaults::STORAGE_MEMORY_BUDGET_BYTES;
    std::uint64_t storage_target_chunk_size_bytes =
        defaults::STORAGE_TARGET_CHUNK_SIZE_BYTES;
    std::uint32_t storage_max_concurrent_io =
        defaults::STORAGE_MAX_CONCURRENT_IO;

    // Progress
    bool progress_enabled = defaults::PROGRESS_ENABLED;
    std::uint32_t progress_interval_ms =
        defaults::PROGRESS_INTERVAL_MS;
    ProgressFormat progress_format = ProgressFormat::text;

    // Output
    std::string output_file = std::string(defaults::OUTPUT_FILE);

    // Final verification
    bool verification_enabled = defaults::VERIFICATION_ENABLED;
    std::uint32_t bbp_sample_count = defaults::BBP_SAMPLE_COUNT;
    std::string verification_manifest_file;

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

/// Converts validated configuration into the storage subsystem's typed policy.
[[nodiscard]]
pi::storage::StoragePolicy makeStoragePolicy(const Config& config);

} // namespace pi::config
