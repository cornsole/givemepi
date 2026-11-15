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
    /// Newly completed units required inside the current phase before the
    /// phase rate is trusted.
    std::uint64_t minimumCompletedPhaseUnits = 1;
};


/// Presentation-independent values derived from one immutable snapshot.
struct ProgressMetrics
{
    std::optional<double> completionRatio;
    std::optional<double> termsPerSecond;
    std::optional<double> checkpointBlocksPerSecond;
    /// End-to-end time until the whole pipeline completes.
    std::optional<std::chrono::nanoseconds> estimatedTotalTimeRemaining;
    /// Time until the current phase completes.
    std::optional<std::chrono::nanoseconds> estimatedPhaseTimeRemaining;

    [[nodiscard]]
    bool operator==(const ProgressMetrics&) const noexcept = default;
};


/// Derives rate and completion estimates without changing authoritative data.
///
/// Rates exclude resume-restored work. Phase ETA uses only units completed
/// since the current phase was entered and the time spent in that phase:
/// remaining = (total - completed) * phaseElapsed / (completed - baseline).
/// Total ETA normalizes every phase's units by its planned cost. The observed
/// end-to-end rate is newly completed normalized work divided by elapsed
/// time. Total remaining time is the phase ETA, when known, plus the
/// normalized cost of later phases divided by that rate; otherwise all
/// remaining normalized work divided by that rate. Consequently a known
/// total ETA is never below a known phase ETA. Phases already left behind
/// contribute no remaining work, and zero-cost phases contribute nothing.
/// Completed snapshots report zero for both ETAs; failed snapshots report
/// neither.
class ProgressMetricsCalculator
{
public:
    /// Validates a sampling policy.
    ///
    /// Throws std::invalid_argument when minimum elapsed time is not positive
    /// or a minimum completed-work threshold is zero. Time and memory: O(1).
    static void validatePolicy(const ProgressMetricPolicy& policy);

    /// Calculates metrics from one snapshot using observed average rates.
    ///
    /// Input: an immutable raw snapshot and validated sampling thresholds.
    /// Output: optional derived values; unavailable estimates remain empty.
    /// Estimates saturate at std::chrono::nanoseconds::max().
    /// Time complexity: O(p) for p progress phases. Memory complexity: O(1).
    [[nodiscard]]
    static ProgressMetrics calculate(
        const ProgressSnapshot& snapshot,
        const ProgressMetricPolicy& policy = {}
    );
};

} // namespace pi::progress
