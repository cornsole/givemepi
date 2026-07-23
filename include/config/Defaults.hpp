#pragma once

#include <cstdint>
#include <string_view>

namespace pi::config::defaults
{

// Precision
inline constexpr std::uint64_t DIGITS = 1'000'000;

// Threading
// 0 = Auto (use all available hardware threads)
inline constexpr std::uint32_t THREADS = 0;

// Checkpoint
inline constexpr bool CHECKPOINT_ENABLED = true;
inline constexpr std::uint32_t CHECKPOINT_INTERVAL_SECONDS = 300;

// Memory
inline constexpr bool HUGE_PAGES_ENABLED = true;

// Out-of-Core Storage
inline constexpr bool OUT_OF_CORE_ENABLED = true;
inline constexpr std::string_view COMPRESSION = "lz4";
inline constexpr std::string_view STORAGE_DIRECTORY = "storage";
inline constexpr std::uint64_t STORAGE_MEMORY_BUDGET_BYTES = 512ULL * 1024ULL * 1024ULL;
inline constexpr std::uint64_t STORAGE_TARGET_CHUNK_SIZE_BYTES = 64ULL * 1024ULL * 1024ULL;
inline constexpr std::uint32_t STORAGE_MAX_CONCURRENT_IO = 1;

// Progress
inline constexpr bool PROGRESS_ENABLED = true;
inline constexpr std::uint32_t PROGRESS_INTERVAL_MS = 500;
inline constexpr std::string_view PROGRESS_FORMAT = "text";

// Output
inline constexpr std::string_view OUTPUT_FILE = "pi.txt";

// Final verification
inline constexpr bool VERIFICATION_ENABLED = true;
inline constexpr std::uint32_t BBP_SAMPLE_COUNT = 8;

// Resume
inline constexpr bool RESUME_ENABLED = true;

} // namespace pi::config::defaults
