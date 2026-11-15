#pragma once

#include "progress/ProgressSnapshot.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string_view>


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


/// Evidence behind the end-to-end ETA.
enum class EtaBasis
{
    /// The current phase is priced from its own measured rate (or is
    /// finished), later phases from calibrated plan costs.
    measured,
    /// No current-phase rate sample exists yet: the current phase is priced
    /// from its calibrated plan cost minus the time already spent in it.
    calibratedPlan,
    /// All remaining planned work should already have finished; the value
    /// is unavailable rather than zero.
    overdue,
    /// Not enough evidence for an estimate.
    unknown
};


/// Returns the stable schema spelling of an ETA basis:
/// `measured`, `calibrated_plan`, `overdue`, or `unknown`.
///
/// Time complexity: O(1). Memory complexity: O(1).
[[nodiscard]]
std::string_view toString(EtaBasis basis) noexcept;


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
    /// Evidence behind estimatedTotalTimeRemaining; `overdue` and `unknown`
    /// always pair with an empty total estimate.
    EtaBasis totalBasis = EtaBasis::unknown;
    /// The current phase has outlived its measured-rate prediction; the
    /// phase estimate is then empty rather than zero.
    bool phaseOverdue = false;

    [[nodiscard]]
    bool operator==(const ProgressMetrics&) const noexcept = default;
};


/// Derives rate and completion estimates without changing authoritative data.
///
/// Rates exclude resume-restored work. Both ETAs are evidence driven: while
/// no counter changes they fall by exactly the wall time that passes.
///
/// Phase ETA: the current phase's rate is measured up to its latest unit,
/// r = (u - u0) / (phi - s) for u completed units, u0 the phase-entry
/// baseline, phi the phase elapsed time, and s the time since the latest
/// unit. The remaining time is (U - u) / r - s. The rate needs a measured
/// span phi - s of at least minimumElapsed and minimumCompletedPhaseUnits
/// new units. A negative result marks the phase overdue with no value.
///
/// Calibration: k = sum of closed phase wall durations / sum of the planned
/// cost newly completed in those phases (restored and pre-entry units
/// excluded). Before any planned phase closes, the current phase's measured
/// span divided by its newly completed planned cost is used instead.
///
/// Total ETA: R_phase + k * W_later, where W_later is the remaining planned
/// cost of later phases; an overdue current phase contributes zero. Without
/// a phase sample the current phase contributes k * W_entry - phi, where
/// W_entry is its planned cost from the entry baseline; a negative
/// contribution counts as zero. When no later work remains, a negative
/// current-phase remainder makes the total overdue. Phases already left
/// behind and zero-cost phases contribute nothing. Completed snapshots
/// report zero for both ETAs; failed snapshots report neither.
class ProgressMetricsCalculator
{
public:
    /// Validates a sampling policy.
    ///
    /// Throws std::invalid_argument when minimum elapsed time is not positive
    /// or a minimum completed-work threshold is zero. Time and memory: O(1).
    static void validatePolicy(const ProgressMetricPolicy& policy);

    /// Calculates metrics from one snapshot using measured phase rates and
    /// calibrated plan costs.
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
