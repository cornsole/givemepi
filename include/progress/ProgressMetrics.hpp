#pragma once

#include "progress/ProgressSnapshot.hpp"

#include <chrono>
#include <cstdint>
#include <optional>


namespace pi::progress
{

/// Sampling thresholds for derived progress metrics.
struct ProgressMetricPolicy
{
    std::chrono::nanoseconds minimumElapsed = std::chrono::seconds(1);
    std::uint64_t minimumCompletedTerms = 1;
    std::uint64_t minimumCompletedCheckpointBlocks = 1;
};


/// Presentation-independent values derived from one immutable snapshot.
struct ProgressMetrics
{
    std::optional<double> completionRatio;
    std::optional<double> termsPerSecond;
    std::optional<double> checkpointBlocksPerSecond;
    std::optional<std::chrono::nanoseconds> estimatedTimeRemaining;

    [[nodiscard]]
    bool operator==(const ProgressMetrics&) const noexcept = default;
};


/// Derives rate and completion estimates without changing authoritative data.
class ProgressMetricsCalculator
{
public:
    /// Validates a sampling policy.
    ///
    /// Throws std::invalid_argument when minimum elapsed time is not positive
    /// or a minimum completed-work threshold is zero. Time and memory: O(1).
    static void validatePolicy(const ProgressMetricPolicy& policy);

    /// Calculates metrics from one snapshot using lifetime-average rates.
    ///
    /// Input: an immutable raw snapshot and validated sampling thresholds.
    /// Output: optional derived values; unavailable estimates remain empty.
    /// Time complexity: O(1). Memory complexity: O(1).
    [[nodiscard]]
    static ProgressMetrics calculate(
        const ProgressSnapshot& snapshot,
        const ProgressMetricPolicy& policy = {}
    );
};

} // namespace pi::progress
