#include "progress/ProgressMetrics.hpp"

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>


namespace
{

using namespace std::chrono_literals;
using pi::progress::PhaseWorkUnits;
using pi::progress::ProgressMetricPolicy;
using pi::progress::ProgressMetrics;
using pi::progress::ProgressMetricsCalculator;
using pi::progress::ProgressPhase;
using pi::progress::ProgressSnapshot;
using pi::progress::ProgressSnapshotData;
using pi::progress::phaseIndex;


template<typename Exception, typename Function>
bool throws(Function&& function)
{
    try
    {
        function();
    }
    catch (const Exception&)
    {
        return true;
    }
    return false;
}


bool near(double value, double expected) noexcept
{
    return std::abs(value - expected) < 1e-12;
}


bool nearDuration(
    const std::optional<std::chrono::nanoseconds>& value,
    std::chrono::nanoseconds expected
) noexcept
{
    return value.has_value()
        && std::abs((*value - expected).count()) <= 1;
}


ProgressMetrics calculate(const ProgressSnapshotData& data)
{
    return ProgressMetricsCalculator::calculate(ProgressSnapshot(data));
}


/// Resumed splitting snapshot: 5 restored terms, 20 new terms in 4 s of
/// splitting and 5 s overall. Costs: splitting 100, merging 50, writing 10.
ProgressSnapshotData resumedSplitting()
{
    ProgressSnapshotData data;
    data.phase = ProgressPhase::splitting;
    data.targetDigits = 1'000;
    data.totalTerms = 100;
    data.completedTerms = 25;
    data.restoredTerms = 5;
    data.totalCheckpointBlocks = 10;
    data.completedCheckpointBlocks = 2;
    data.restoredCheckpointBlocks = 1;
    data.elapsed = 5s;
    data.phaseElapsed = 4s;
    data.phaseBaselineUnits = 5;
    data.phaseCosts[phaseIndex(ProgressPhase::splitting)] = 100;
    data.phaseCosts[phaseIndex(ProgressPhase::merging)] = 50;
    data.phaseCosts[phaseIndex(ProgressPhase::writingOutput)] = 10;
    return data;
}


void testResumeExcludedFromRates()
{
    const ProgressMetrics metrics = calculate(resumedSplitting());
    assert(metrics.completionRatio.has_value());
    assert(near(*metrics.completionRatio, 0.25));
    // 20 new terms and 1 new block in 5 s; restored work is excluded.
    assert(metrics.termsPerSecond.has_value());
    assert(near(*metrics.termsPerSecond, 4.0));
    assert(metrics.checkpointBlocksPerSecond.has_value());
    assert(near(*metrics.checkpointBlocksPerSecond, 0.2));
    // Phase: 75 remaining terms at 20 terms per 4 s.
    assert(metrics.estimatedPhaseTimeRemaining == 15s);
    // Total: phase ETA plus 60 later cost units at 20 per 5 s.
    assert(metrics.estimatedTotalTimeRemaining == 30s);
    assert(*metrics.estimatedTotalTimeRemaining
        >= *metrics.estimatedPhaseTimeRemaining);
}


void testMinimumSamples()
{
    ProgressSnapshotData earlyPhase = resumedSplitting();
    earlyPhase.phaseElapsed = 500ms;
    const ProgressMetrics phaseUnknown = calculate(earlyPhase);
    assert(!phaseUnknown.estimatedPhaseTimeRemaining.has_value());
    // Total falls back to all remaining cost (135) at the observed rate.
    assert(phaseUnknown.estimatedTotalTimeRemaining == 33750ms);

    ProgressSnapshotData noNewPhaseWork = resumedSplitting();
    noNewPhaseWork.phaseBaselineUnits = 25;
    assert(!calculate(noNewPhaseWork).estimatedPhaseTimeRemaining);

    ProgressSnapshotData early = resumedSplitting();
    early.elapsed = 500ms;
    early.phaseElapsed = 500ms;
    const ProgressMetrics unknown = calculate(early);
    assert(unknown.completionRatio.has_value());
    assert(!unknown.termsPerSecond.has_value());
    assert(!unknown.checkpointBlocksPerSecond.has_value());
    assert(!unknown.estimatedPhaseTimeRemaining.has_value());
    assert(!unknown.estimatedTotalTimeRemaining.has_value());

    // Restored-only work is never a usable rate sample.
    ProgressSnapshotData restoredOnly = resumedSplitting();
    restoredOnly.completedTerms = 5;
    restoredOnly.phaseBaselineUnits = 5;
    const ProgressMetrics restoredMetrics = calculate(restoredOnly);
    assert(!restoredMetrics.termsPerSecond.has_value());
    assert(!restoredMetrics.estimatedPhaseTimeRemaining.has_value());
    assert(!restoredMetrics.estimatedTotalTimeRemaining.has_value());

    ProgressMetricPolicy strict;
    strict.minimumElapsed = 1s;
    strict.minimumCompletedTerms = 60;
    strict.minimumCompletedCheckpointBlocks = 3;
    strict.minimumCompletedPhaseUnits = 21;
    const auto insufficient = ProgressMetricsCalculator::calculate(
        ProgressSnapshot(resumedSplitting()),
        strict
    );
    assert(!insufficient.termsPerSecond.has_value());
    assert(!insufficient.checkpointBlocksPerSecond.has_value());
    assert(!insufficient.estimatedPhaseTimeRemaining.has_value());
}


void testWeightedMergeAndLaterPhases()
{
    ProgressSnapshotData data = resumedSplitting();
    data.phase = ProgressPhase::merging;
    data.completedTerms = 100;
    data.phaseUnits[phaseIndex(ProgressPhase::merging)] =
        PhaseWorkUnits{200, 50};
    data.phaseBaselineUnits = 0;
    data.phaseElapsed = 2s;
    data.elapsed = 10s;
    const ProgressMetrics metrics = calculate(data);
    // Phase: 150 remaining merge units at 50 per 2 s.
    assert(metrics.estimatedPhaseTimeRemaining == 6s);
    // New normalized work: 95 splitting + 12.5 merging in 10 s; writing
    // cost 10 remains after the phase.
    const auto later = std::chrono::nanoseconds(static_cast<std::int64_t>(
        10.0L * 10'000'000'000.0L / 107.5L
    ));
    assert(nearDuration(metrics.estimatedTotalTimeRemaining, 6s + later));
}


void testSkippedPhaseLeftBehind()
{
    ProgressSnapshotData data = resumedSplitting();
    data.phase = ProgressPhase::finalizing;
    data.completedTerms = 100;
    data.phaseCosts[phaseIndex(ProgressPhase::finalizing)] = 30;
    data.phaseUnits[phaseIndex(ProgressPhase::finalizing)] =
        PhaseWorkUnits{10, 0};
    data.phaseBaselineUnits = 0;
    data.phaseElapsed = 2s;
    data.elapsed = 10s;
    const ProgressMetrics metrics = calculate(data);
    assert(!metrics.estimatedPhaseTimeRemaining.has_value());
    // Merging was never entered and is excluded: 30 + 10 remaining cost at
    // 95 new cost units per 10 s.
    const auto expected = std::chrono::nanoseconds(static_cast<std::int64_t>(
        40.0L * 10'000'000'000.0L / 95.0L
    ));
    assert(nearDuration(metrics.estimatedTotalTimeRemaining, expected));
}


void testVerificationAndCompletionEdges()
{
    ProgressSnapshotData verifying = resumedSplitting();
    verifying.phase = ProgressPhase::verifyingOutput;
    verifying.completedTerms = 100;
    verifying.phaseCosts[phaseIndex(ProgressPhase::verifyingOutput)] = 20;
    verifying.phaseUnits[phaseIndex(ProgressPhase::verifyingOutput)] =
        PhaseWorkUnits{1'000, 400};
    verifying.phaseBaselineUnits = 0;
    verifying.phaseElapsed = 2s;
    verifying.elapsed = 10s;
    const ProgressMetrics verifyingMetrics = calculate(verifying);
    // The total estimate continues through verification.
    assert(verifyingMetrics.estimatedPhaseTimeRemaining == 3s);
    assert(verifyingMetrics.estimatedTotalTimeRemaining == 3s);

    // Verification disabled: writing is the last planned phase.
    ProgressSnapshotData writing = resumedSplitting();
    writing.phase = ProgressPhase::writingOutput;
    writing.completedTerms = 100;
    writing.phaseUnits[phaseIndex(ProgressPhase::writingOutput)] =
        PhaseWorkUnits{100, 100};
    writing.phaseBaselineUnits = 0;
    writing.phaseElapsed = 100ms;
    writing.elapsed = 10s;
    const ProgressMetrics written = calculate(writing);
    assert(written.estimatedPhaseTimeRemaining == 0ns);
    assert(written.estimatedTotalTimeRemaining == 0ns);

    // Without a cost plan only the phase estimate is available.
    ProgressSnapshotData unplanned = resumedSplitting();
    unplanned.phaseCosts = {};
    const ProgressMetrics unplannedMetrics = calculate(unplanned);
    assert(unplannedMetrics.estimatedPhaseTimeRemaining == 15s);
    assert(!unplannedMetrics.estimatedTotalTimeRemaining.has_value());

    // Huge remaining work saturates instead of overflowing.
    ProgressSnapshotData huge = resumedSplitting();
    huge.phase = ProgressPhase::verifyingOutput;
    huge.completedTerms = 100;
    huge.phaseUnits[phaseIndex(ProgressPhase::verifyingOutput)] =
        PhaseWorkUnits{std::numeric_limits<std::uint64_t>::max(), 1};
    huge.phaseCosts[phaseIndex(ProgressPhase::verifyingOutput)] = 1;
    huge.phaseBaselineUnits = 0;
    huge.phaseElapsed = 1s;
    huge.elapsed = 10s;
    const ProgressMetrics hugeMetrics = calculate(huge);
    assert(hugeMetrics.estimatedPhaseTimeRemaining
        == std::chrono::nanoseconds::max());
    assert(hugeMetrics.estimatedTotalTimeRemaining
        == std::chrono::nanoseconds::max());

    ProgressSnapshotData completed = resumedSplitting();
    completed.phase = ProgressPhase::completed;
    completed.completedTerms = 100;
    completed.phaseElapsed = 0ns;
    completed.phaseBaselineUnits = 0;
    const ProgressMetrics done = calculate(completed);
    assert(done.estimatedTotalTimeRemaining == 0ns);
    assert(done.estimatedPhaseTimeRemaining == 0ns);

    ProgressSnapshotData failed = resumedSplitting();
    failed.phase = ProgressPhase::failed;
    failed.phaseElapsed = 0ns;
    failed.phaseBaselineUnits = 0;
    const ProgressMetrics failedMetrics = calculate(failed);
    assert(failedMetrics.termsPerSecond.has_value());
    assert(!failedMetrics.estimatedTotalTimeRemaining.has_value());
    assert(!failedMetrics.estimatedPhaseTimeRemaining.has_value());

    ProgressSnapshotData unknownTotal;
    unknownTotal.targetDigits = 1;
    const ProgressMetrics unknown = calculate(unknownTotal);
    assert(!unknown.completionRatio.has_value());
    assert(!unknown.termsPerSecond.has_value());
    assert(!unknown.estimatedTotalTimeRemaining.has_value());
    assert(!unknown.estimatedPhaseTimeRemaining.has_value());
}


void testPolicyValidation()
{
    const ProgressSnapshot snapshot(resumedSplitting());
    ProgressMetricPolicy invalid;
    invalid.minimumElapsed = 0ns;
    assert(throws<std::invalid_argument>([&invalid, &snapshot]() {
        static_cast<void>(ProgressMetricsCalculator::calculate(
            snapshot,
            invalid
        ));
    }));
    invalid.minimumElapsed = 1s;
    invalid.minimumCompletedTerms = 0;
    assert(throws<std::invalid_argument>([&invalid]() {
        ProgressMetricsCalculator::validatePolicy(invalid);
    }));
    invalid.minimumCompletedTerms = 1;
    invalid.minimumCompletedCheckpointBlocks = 0;
    assert(throws<std::invalid_argument>([&invalid]() {
        ProgressMetricsCalculator::validatePolicy(invalid);
    }));
    invalid.minimumCompletedCheckpointBlocks = 1;
    invalid.minimumCompletedPhaseUnits = 0;
    assert(throws<std::invalid_argument>([&invalid]() {
        ProgressMetricsCalculator::validatePolicy(invalid);
    }));
}

} // namespace


int main()
{
    testResumeExcludedFromRates();
    testMinimumSamples();
    testWeightedMergeAndLaterPhases();
    testSkippedPhaseLeftBehind();
    testVerificationAndCompletionEdges();
    testPolicyValidation();
    return 0;
}
