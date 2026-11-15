#include "progress/ProgressMetrics.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <stdexcept>


namespace
{

using namespace std::chrono_literals;
using pi::progress::EtaBasis;
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


std::chrono::nanoseconds fraction(long double nanoseconds) noexcept
{
    return std::chrono::nanoseconds(
        static_cast<std::int64_t>(nanoseconds)
    );
}


ProgressMetrics calculate(const ProgressSnapshotData& data)
{
    return ProgressMetricsCalculator::calculate(ProgressSnapshot(data));
}


/// Lets `duration` pass without any counter change.
void stall(ProgressSnapshotData& data, std::chrono::nanoseconds duration)
{
    data.elapsed += duration;
    data.phaseElapsed += duration;
    data.sinceLastPhaseUnit += duration;
}


/// Fresh splitting run: 40 of 100 terms in 8 s of splitting after 2 s of
/// initialization; the latest term landed just now. Costs: splitting 100,
/// merging 50, writing 10.
///
/// Phase: 60 terms at 40 per 8 s = 12 s. Calibration from the current
/// phase: 8 s per 40 cost = 0.2 s per cost unit, so the 60 later cost
/// units take 12 s and the total is 24 s.
ProgressSnapshotData splitting()
{
    ProgressSnapshotData data;
    data.phase = ProgressPhase::splitting;
    data.targetDigits = 1'000;
    data.totalTerms = 100;
    data.completedTerms = 40;
    data.totalCheckpointBlocks = 10;
    data.completedCheckpointBlocks = 2;
    data.elapsed = 10s;
    data.phaseElapsed = 8s;
    data.closedPhaseDurations[phaseIndex(ProgressPhase::initializing)] = 2s;
    data.phaseCosts[phaseIndex(ProgressPhase::splitting)] = 100;
    data.phaseCosts[phaseIndex(ProgressPhase::merging)] = 50;
    data.phaseCosts[phaseIndex(ProgressPhase::writingOutput)] = 10;
    return data;
}


/// Merging after a closed splitting phase that took 20 s for its 100 cost
/// units (0.2 s per cost unit): 50 of 200 merge units in 4 s.
ProgressSnapshotData merging()
{
    ProgressSnapshotData data = splitting();
    data.phase = ProgressPhase::merging;
    data.completedTerms = 100;
    data.closedPhaseDurations[phaseIndex(ProgressPhase::splitting)] = 20s;
    data.closedPhaseUnits[phaseIndex(ProgressPhase::splitting)] = 100;
    data.phaseUnits[phaseIndex(ProgressPhase::merging)] =
        PhaseWorkUnits{200, 50};
    data.phaseElapsed = 4s;
    data.sinceLastPhaseUnit = 0s;
    data.elapsed = 26s;
    return data;
}


void testMeasuredPhaseAndTotal()
{
    const ProgressMetrics metrics = calculate(splitting());
    assert(metrics.completionRatio.has_value());
    assert(near(*metrics.completionRatio, 0.4));
    assert(metrics.termsPerSecond.has_value());
    assert(near(*metrics.termsPerSecond, 4.0));
    assert(metrics.estimatedPhaseTimeRemaining == 12s);
    assert(!metrics.phaseOverdue);
    assert(metrics.estimatedTotalTimeRemaining == 24s);
    assert(metrics.totalBasis == EtaBasis::measured);
}


void testStallDecreasesByElapsed()
{
    // With fixed counters both estimates fall by exactly the time passed.
    for (const std::chrono::nanoseconds wait :
         std::initializer_list<std::chrono::nanoseconds>{
             0ns, 1ns, 1s, 3s, 11s, 12s})
    {
        ProgressSnapshotData data = splitting();
        stall(data, wait);
        const ProgressMetrics metrics = calculate(data);
        assert(metrics.estimatedPhaseTimeRemaining == 12s - wait);
        assert(metrics.estimatedTotalTimeRemaining == 24s - wait);
        assert(metrics.totalBasis == EtaBasis::measured);
        assert(!metrics.phaseOverdue);
    }

    // The same holds before the first phase sample, on the calibrated plan.
    ProgressSnapshotData finalizing = merging();
    finalizing.phase = ProgressPhase::finalizing;
    finalizing.phaseUnits[phaseIndex(ProgressPhase::merging)] =
        PhaseWorkUnits{200, 200};
    finalizing.closedPhaseDurations[phaseIndex(ProgressPhase::merging)] = 10s;
    finalizing.closedPhaseUnits[phaseIndex(ProgressPhase::merging)] = 200;
    finalizing.phaseCosts[phaseIndex(ProgressPhase::finalizing)] = 30;
    finalizing.phaseUnits[phaseIndex(ProgressPhase::finalizing)] =
        PhaseWorkUnits{10, 0};
    finalizing.phaseElapsed = 0s;
    finalizing.elapsed = 32s;
    const auto before = calculate(finalizing).estimatedTotalTimeRemaining;
    stall(finalizing, 1500ms);
    const auto after = calculate(finalizing).estimatedTotalTimeRemaining;
    assert(before.has_value() && after.has_value());
    assert(*before - *after == 1500ms);
}


void testOverdue()
{
    // Thirteen seconds after the last term the 12 s prediction has passed:
    // the phase is overdue rather than zero, and later work keeps its full
    // calibrated price.
    ProgressSnapshotData data = splitting();
    stall(data, 13s);
    const ProgressMetrics metrics = calculate(data);
    assert(!metrics.estimatedPhaseTimeRemaining.has_value());
    assert(metrics.phaseOverdue);
    assert(metrics.estimatedTotalTimeRemaining == 12s);
    assert(metrics.totalBasis == EtaBasis::measured);

    // Without later work the total is overdue too, never zero.
    ProgressSnapshotData last = data;
    last.phaseCosts[phaseIndex(ProgressPhase::merging)] = 0;
    last.phaseCosts[phaseIndex(ProgressPhase::writingOutput)] = 0;
    const ProgressMetrics lastMetrics = calculate(last);
    assert(lastMetrics.phaseOverdue);
    assert(!lastMetrics.estimatedPhaseTimeRemaining.has_value());
    assert(!lastMetrics.estimatedTotalTimeRemaining.has_value());
    assert(lastMetrics.totalBasis == EtaBasis::overdue);

    // Publishing the final unit can precede the phase transition. That gap
    // has no current-phase work left and therefore is not overdue.
    ProgressSnapshotData finished = splitting();
    finished.completedTerms = finished.totalTerms;
    finished.sinceLastPhaseUnit = 2s;
    const ProgressMetrics finishedMetrics = calculate(finished);
    assert(finishedMetrics.estimatedPhaseTimeRemaining == 0s);
    assert(!finishedMetrics.phaseOverdue);
    assert(nearDuration(
        finishedMetrics.estimatedTotalTimeRemaining,
        fraction(3'600'000'000.0L)
    ));
    assert(finishedMetrics.totalBasis == EtaBasis::measured);
}


void testSlowdownStep()
{
    // Stalled for 12 s, the phase estimate has run down to zero.
    ProgressSnapshotData data = splitting();
    stall(data, 12s);
    assert(calculate(data).estimatedPhaseTimeRemaining == 0s);

    // The next term lands: the measured rate is now 41 terms per 20 s, so
    // the estimates step up once, on the counter change only.
    data.completedTerms = 41;
    data.sinceLastPhaseUnit = 0s;
    const ProgressMetrics stepped = calculate(data);
    assert(nearDuration(
        stepped.estimatedPhaseTimeRemaining,
        fraction(59.0L * 20'000'000'000.0L / 41.0L)
    ));
    // Later work is repriced at 20 s per 41 cost units.
    assert(nearDuration(
        stepped.estimatedTotalTimeRemaining,
        fraction(119.0L * 20'000'000'000.0L / 41.0L)
    ));
    assert(stepped.totalBasis == EtaBasis::measured);

    // Fractional estimates then fall by the stall to within rounding.
    stall(data, 2s);
    const ProgressMetrics later = calculate(data);
    assert(nearDuration(
        later.estimatedPhaseTimeRemaining,
        *stepped.estimatedPhaseTimeRemaining - 2s
    ));
    assert(nearDuration(
        later.estimatedTotalTimeRemaining,
        *stepped.estimatedTotalTimeRemaining - 2s
    ));
}


void testCalibrationIndependentOfElapsed()
{
    // Phase: 150 merge units at 50 per 4 s = 12 s. Later writing costs 10
    // at the closed-phase calibration of 0.2 s per cost unit = 2 s.
    const ProgressMetrics metrics = calculate(merging());
    assert(metrics.estimatedPhaseTimeRemaining == 12s);
    assert(metrics.estimatedTotalTimeRemaining == 14s);
    assert(metrics.totalBasis == EtaBasis::measured);

    // A long unplanned checkpoint validation changes the run's elapsed time
    // but not the calibration.
    ProgressSnapshotData slowStart = merging();
    slowStart.closedPhaseDurations[
        phaseIndex(ProgressPhase::validatingCheckpoints)] = 100s;
    slowStart.elapsed += 100s;
    assert(calculate(slowStart).estimatedTotalTimeRemaining == 14s);

    // A slower current phase changes only its own remainder: closed phases
    // keep pricing later work, so it stays 2 s.
    ProgressSnapshotData slowMerge = merging();
    slowMerge.phaseElapsed = 8s;
    slowMerge.elapsed = 30s;
    const ProgressMetrics slow = calculate(slowMerge);
    assert(slow.estimatedPhaseTimeRemaining == 24s);
    assert(slow.estimatedTotalTimeRemaining == 26s);
}


ProgressSnapshotData finalizingBeforeSample()
{
    ProgressSnapshotData data = merging();
    data.phase = ProgressPhase::finalizing;
    data.phaseUnits[phaseIndex(ProgressPhase::merging)] =
        PhaseWorkUnits{200, 200};
    data.closedPhaseDurations[phaseIndex(ProgressPhase::merging)] = 10s;
    data.closedPhaseUnits[phaseIndex(ProgressPhase::merging)] = 200;
    data.phaseCosts[phaseIndex(ProgressPhase::finalizing)] = 30;
    data.phaseUnits[phaseIndex(ProgressPhase::finalizing)] =
        PhaseWorkUnits{10, 0};
    data.phaseElapsed = 2s;
    data.sinceLastPhaseUnit = 2s;
    data.elapsed = 34s;
    return data;
}


void testFinalizingBeforeFirstSample()
{
    // Calibration: 30 s for 150 closed cost units = 0.2 s per unit. The
    // phase costs 30 (6 s) of which 2 s are spent; writing adds 2 s.
    const ProgressMetrics metrics = calculate(finalizingBeforeSample());
    assert(!metrics.estimatedPhaseTimeRemaining.has_value());
    assert(!metrics.phaseOverdue);
    assert(metrics.estimatedTotalTimeRemaining == 6s);
    assert(metrics.totalBasis == EtaBasis::calibratedPlan);

    // An undeclared phase is priced at its whole cost.
    ProgressSnapshotData undeclared = finalizingBeforeSample();
    undeclared.phaseUnits[phaseIndex(ProgressPhase::finalizing)] = {};
    assert(calculate(undeclared).estimatedTotalTimeRemaining == 6s);

    // A unit below the sampling span is not a sample and is not subtracted
    // twice: the entry cost minus phase time still applies.
    ProgressSnapshotData early = finalizingBeforeSample();
    early.phaseUnits[phaseIndex(ProgressPhase::finalizing)] =
        PhaseWorkUnits{10, 1};
    early.sinceLastPhaseUnit = 1500ms;
    const ProgressMetrics earlyMetrics = calculate(early);
    assert(!earlyMetrics.estimatedPhaseTimeRemaining.has_value());
    assert(earlyMetrics.estimatedTotalTimeRemaining == 6s);
    assert(earlyMetrics.totalBasis == EtaBasis::calibratedPlan);

    // Past its planned 6 s the phase contributes nothing; writing remains.
    ProgressSnapshotData late = finalizingBeforeSample();
    stall(late, 5s);
    const ProgressMetrics lateMetrics = calculate(late);
    assert(lateMetrics.estimatedTotalTimeRemaining == 2s);
    assert(lateMetrics.totalBasis == EtaBasis::calibratedPlan);

    // As the last planned phase, it becomes overdue.
    late.phaseCosts[phaseIndex(ProgressPhase::writingOutput)] = 0;
    const ProgressMetrics lastMetrics = calculate(late);
    assert(!lastMetrics.estimatedTotalTimeRemaining.has_value());
    assert(lastMetrics.totalBasis == EtaBasis::overdue);
}


void testResumeExcluded()
{
    // 50 restored terms, 25 new terms in 5 s of splitting.
    ProgressSnapshotData data = splitting();
    data.completedTerms = 75;
    data.restoredTerms = 50;
    data.phaseBaselineUnits = 50;
    data.restoredCheckpointBlocks = 1;
    data.closedPhaseDurations = {};
    data.elapsed = 5s;
    data.phaseElapsed = 5s;
    const ProgressMetrics metrics = calculate(data);
    assert(metrics.termsPerSecond.has_value());
    assert(near(*metrics.termsPerSecond, 5.0));
    assert(metrics.checkpointBlocksPerSecond.has_value());
    assert(near(*metrics.checkpointBlocksPerSecond, 0.2));
    // Phase: 25 terms at 25 per 5 s. Calibration: 5 s per 25 new cost
    // units; restored cost would have cut later work to 4 s.
    assert(metrics.estimatedPhaseTimeRemaining == 5s);
    assert(metrics.estimatedTotalTimeRemaining == 17s);

    // After splitting closes, only its 50 new terms calibrate: 10 s per 50
    // cost units. Merging (50 cost) and writing (10) then take 12 s, minus
    // the 0.5 s already spent merging.
    ProgressSnapshotData resumedMerge = data;
    resumedMerge.phase = ProgressPhase::merging;
    resumedMerge.completedTerms = 100;
    resumedMerge.phaseBaselineUnits = 0;
    resumedMerge.closedPhaseDurations[phaseIndex(ProgressPhase::splitting)] =
        10s;
    resumedMerge.closedPhaseUnits[phaseIndex(ProgressPhase::splitting)] = 50;
    resumedMerge.phaseUnits[phaseIndex(ProgressPhase::merging)] =
        PhaseWorkUnits{200, 0};
    resumedMerge.phaseElapsed = 500ms;
    resumedMerge.sinceLastPhaseUnit = 500ms;
    resumedMerge.elapsed = 10500ms;
    const ProgressMetrics merged = calculate(resumedMerge);
    assert(merged.estimatedTotalTimeRemaining == 11500ms);
    assert(merged.totalBasis == EtaBasis::calibratedPlan);

    // Restored-only work is never a usable sample.
    ProgressSnapshotData restoredOnly = data;
    restoredOnly.completedTerms = 50;
    restoredOnly.sinceLastPhaseUnit = 5s;
    const ProgressMetrics restoredMetrics = calculate(restoredOnly);
    assert(!restoredMetrics.termsPerSecond.has_value());
    assert(!restoredMetrics.estimatedPhaseTimeRemaining.has_value());
    assert(!restoredMetrics.estimatedTotalTimeRemaining.has_value());
    assert(restoredMetrics.totalBasis == EtaBasis::unknown);
}


/// Level-scaled units of one pair merge: span * bit_width(span) divided by
/// min(workers, pairs), rounded up, as BinarySplitter publishes them.
std::uint64_t levelPairUnits(
    std::uint64_t span,
    std::uint64_t pairs,
    std::uint64_t workers
)
{
    const std::uint64_t units =
        span * static_cast<std::uint64_t>(std::bit_width(span));
    const std::uint64_t concurrency = std::min(workers, pairs);
    return (units + concurrency - 1) / concurrency;
}


void testMergeRateConstantAcrossLevels()
{
    // Eight frontier nodes of 1000 terms merge on four workers in levels
    // of 4, 2, and 1 pairs. A pair takes 100 us per unscaled unit on one
    // thread, so a level lasts ceil(pairs / workers) pair durations.
    constexpr std::uint64_t workers = 4;
    constexpr long double nanosecondsPerUnit = 100'000.0L;
    std::uint64_t totalUnits = 0;
    long double totalTime = 0.0L;
    struct Level
    {
        std::uint64_t units;
        long double time;
    };
    Level levels[3]{};
    std::uint64_t pairs = 4;
    std::uint64_t span = 2'000;
    for (Level& level : levels)
    {
        const std::uint64_t unscaled =
            span * static_cast<std::uint64_t>(std::bit_width(span));
        level.units = pairs * levelPairUnits(span, pairs, workers);
        level.time = static_cast<long double>(
            (pairs + workers - 1) / workers * unscaled
        ) * nanosecondsPerUnit;
        totalUnits += level.units;
        totalTime += level.time;
        pairs /= 2;
        span *= 2;
    }

    ProgressSnapshotData data = merging();
    std::uint64_t completed = 0;
    long double spent = 0.0L;
    for (const Level& level : levels)
    {
        completed += level.units;
        spent += level.time;
        if (completed == totalUnits)
        {
            break;
        }
        data.phaseUnits[phaseIndex(ProgressPhase::merging)] =
            PhaseWorkUnits{totalUnits, completed};
        data.phaseElapsed = fraction(spent);
        data.sinceLastPhaseUnit = 0s;
        data.elapsed = 22s + data.phaseElapsed;
        // Every level boundary predicts the true remaining merge time.
        assert(nearDuration(
            calculate(data).estimatedPhaseTimeRemaining,
            fraction(totalTime - spent)
        ));
    }
}


void testVerificationAndCompletionEdges()
{
    ProgressSnapshotData verifying = merging();
    verifying.phase = ProgressPhase::verifyingOutput;
    verifying.phaseUnits[phaseIndex(ProgressPhase::merging)] =
        PhaseWorkUnits{200, 200};
    verifying.phaseCosts[phaseIndex(ProgressPhase::verifyingOutput)] = 20;
    verifying.phaseUnits[phaseIndex(ProgressPhase::verifyingOutput)] =
        PhaseWorkUnits{1'000, 400};
    verifying.phaseElapsed = 2s;
    verifying.elapsed = 24s;
    const ProgressMetrics verifyingMetrics = calculate(verifying);
    // The total estimate continues through verification.
    assert(verifyingMetrics.estimatedPhaseTimeRemaining == 3s);
    assert(verifyingMetrics.estimatedTotalTimeRemaining == 3s);
    assert(verifyingMetrics.totalBasis == EtaBasis::measured);

    // Verification disabled: writing is the last planned phase.
    ProgressSnapshotData writing = verifying;
    writing.phase = ProgressPhase::writingOutput;
    writing.phaseCosts[phaseIndex(ProgressPhase::verifyingOutput)] = 0;
    writing.phaseUnits[phaseIndex(ProgressPhase::verifyingOutput)] = {};
    writing.phaseUnits[phaseIndex(ProgressPhase::writingOutput)] =
        PhaseWorkUnits{100, 50};
    const ProgressMetrics written = calculate(writing);
    assert(written.estimatedPhaseTimeRemaining == 2s);
    assert(written.estimatedTotalTimeRemaining == 2s);
    stall(writing, 3s);
    const ProgressMetrics overdue = calculate(writing);
    assert(overdue.phaseOverdue);
    assert(!overdue.estimatedTotalTimeRemaining.has_value());
    assert(overdue.totalBasis == EtaBasis::overdue);

    // Without a cost plan only the phase estimate is available.
    ProgressSnapshotData unplanned = splitting();
    unplanned.phaseCosts = {};
    const ProgressMetrics unplannedMetrics = calculate(unplanned);
    assert(unplannedMetrics.estimatedPhaseTimeRemaining == 12s);
    assert(!unplannedMetrics.estimatedTotalTimeRemaining.has_value());
    assert(unplannedMetrics.totalBasis == EtaBasis::unknown);

    // Huge remaining work saturates instead of overflowing.
    ProgressSnapshotData huge = verifying;
    huge.phaseUnits[phaseIndex(ProgressPhase::verifyingOutput)] =
        PhaseWorkUnits{std::numeric_limits<std::uint64_t>::max(), 1};
    huge.phaseCosts[phaseIndex(ProgressPhase::verifyingOutput)] = 1;
    const ProgressMetrics hugeMetrics = calculate(huge);
    assert(hugeMetrics.estimatedPhaseTimeRemaining
        == std::chrono::nanoseconds::max());
    assert(hugeMetrics.estimatedTotalTimeRemaining
        == std::chrono::nanoseconds::max());

    ProgressSnapshotData completed = verifying;
    completed.phase = ProgressPhase::completed;
    completed.phaseElapsed = 0ns;
    completed.sinceLastPhaseUnit = 0ns;
    completed.phaseBaselineUnits = 0;
    const ProgressMetrics done = calculate(completed);
    assert(done.estimatedTotalTimeRemaining == 0ns);
    assert(done.estimatedPhaseTimeRemaining == 0ns);
    assert(done.totalBasis == EtaBasis::measured);
    assert(!done.phaseOverdue);

    ProgressSnapshotData failed = completed;
    failed.phase = ProgressPhase::failed;
    const ProgressMetrics failedMetrics = calculate(failed);
    assert(failedMetrics.termsPerSecond.has_value());
    assert(!failedMetrics.estimatedTotalTimeRemaining.has_value());
    assert(!failedMetrics.estimatedPhaseTimeRemaining.has_value());
    assert(failedMetrics.totalBasis == EtaBasis::unknown);
    assert(!failedMetrics.phaseOverdue);

    // Initialization has no rate evidence yet.
    ProgressSnapshotData initializing = splitting();
    initializing.phase = ProgressPhase::initializing;
    initializing.completedTerms = 0;
    initializing.closedPhaseDurations = {};
    initializing.phaseElapsed = 10s;
    initializing.sinceLastPhaseUnit = 10s;
    const ProgressMetrics starting = calculate(initializing);
    assert(!starting.estimatedPhaseTimeRemaining.has_value());
    assert(!starting.estimatedTotalTimeRemaining.has_value());
    assert(starting.totalBasis == EtaBasis::unknown);

    ProgressSnapshotData unknownTotal;
    unknownTotal.targetDigits = 1;
    const ProgressMetrics unknown = calculate(unknownTotal);
    assert(!unknown.completionRatio.has_value());
    assert(!unknown.termsPerSecond.has_value());
    assert(!unknown.estimatedTotalTimeRemaining.has_value());
    assert(!unknown.estimatedPhaseTimeRemaining.has_value());
}


void testMinimumSamples()
{
    // The latest unit landed 500 ms into the phase: too short a span.
    ProgressSnapshotData shortSpan = splitting();
    shortSpan.sinceLastPhaseUnit = 7500ms;
    const ProgressMetrics shortMetrics = calculate(shortSpan);
    assert(!shortMetrics.estimatedPhaseTimeRemaining.has_value());
    assert(!shortMetrics.phaseOverdue);
    assert(!shortMetrics.estimatedTotalTimeRemaining.has_value());
    assert(shortMetrics.totalBasis == EtaBasis::unknown);

    ProgressSnapshotData noNewPhaseWork = splitting();
    noNewPhaseWork.phaseBaselineUnits = 40;
    assert(!calculate(noNewPhaseWork).estimatedPhaseTimeRemaining);

    ProgressSnapshotData early = splitting();
    early.elapsed = 500ms;
    early.phaseElapsed = 500ms;
    early.closedPhaseDurations = {};
    const ProgressMetrics unknown = calculate(early);
    assert(unknown.completionRatio.has_value());
    assert(!unknown.termsPerSecond.has_value());
    assert(!unknown.checkpointBlocksPerSecond.has_value());
    assert(!unknown.estimatedPhaseTimeRemaining.has_value());
    assert(!unknown.estimatedTotalTimeRemaining.has_value());

    ProgressMetricPolicy strict;
    strict.minimumElapsed = 1s;
    strict.minimumCompletedTerms = 60;
    strict.minimumCompletedCheckpointBlocks = 3;
    strict.minimumCompletedPhaseUnits = 41;
    const auto insufficient = ProgressMetricsCalculator::calculate(
        ProgressSnapshot(splitting()),
        strict
    );
    assert(!insufficient.termsPerSecond.has_value());
    assert(!insufficient.checkpointBlocksPerSecond.has_value());
    assert(!insufficient.estimatedPhaseTimeRemaining.has_value());
}


void testPolicyValidation()
{
    const ProgressSnapshot snapshot(splitting());
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

    assert(pi::progress::toString(EtaBasis::measured) == "measured");
    assert(pi::progress::toString(EtaBasis::calibratedPlan)
        == "calibrated_plan");
    assert(pi::progress::toString(EtaBasis::overdue) == "overdue");
    assert(pi::progress::toString(EtaBasis::unknown) == "unknown");
}

} // namespace


int main()
{
    testMeasuredPhaseAndTotal();
    testStallDecreasesByElapsed();
    testOverdue();
    testSlowdownStep();
    testCalibrationIndependentOfElapsed();
    testFinalizingBeforeFirstSample();
    testResumeExcluded();
    testMergeRateConstantAcrossLevels();
    testVerificationAndCompletionEdges();
    testMinimumSamples();
    testPolicyValidation();
    return 0;
}
