#include "progress/ProgressTracker.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>


namespace
{

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


void testPhaseWork()
{
    using pi::progress::PhaseWorkUnits;
    using pi::progress::ProgressPhase;
    using pi::progress::ProgressPhaseCosts;
    using pi::progress::ProgressTracker;
    using pi::progress::ProgressWorkPlan;
    using pi::progress::phaseIndex;

    ProgressPhaseCosts invalidCosts{};
    invalidCosts[phaseIndex(ProgressPhase::completed)] = 1;
    assert(throws<std::invalid_argument>([&invalidCosts]() {
        static_cast<void>(ProgressTracker(
            ProgressWorkPlan{1, 1, 0, invalidCosts}
        ));
    }));

    ProgressPhaseCosts costs{};
    costs[phaseIndex(ProgressPhase::splitting)] = 40;
    costs[phaseIndex(ProgressPhase::merging)] = 30;
    costs[phaseIndex(ProgressPhase::writingOutput)] = 20;
    costs[phaseIndex(ProgressPhase::verifyingOutput)] = 10;
    ProgressTracker tracker(ProgressWorkPlan{100, 50, 4, costs});

    // Restored work is published once, before splitting, as restored work.
    assert(!tracker.restoreCompletedWork(51, 0, 0));
    assert(!tracker.restoreCompletedWork(0, 5, 0));
    assert(tracker.transitionTo(ProgressPhase::validatingCheckpoints));
    assert(tracker.restoreCompletedWork(10, 2, 300));
    assert(!tracker.restoreCompletedWork(10, 2, 300));
    assert(tracker.transitionTo(ProgressPhase::splitting));
    assert(!tracker.restoreCompletedWork(0, 0, 0));
    auto snapshot = tracker.snapshot();
    assert(snapshot.restoredTerms() == 10);
    assert(snapshot.completedTerms() == 10);
    assert(snapshot.restoredCheckpointBlocks() == 2);
    assert(snapshot.completedCheckpointBlocks() == 2);
    assert(snapshot.durableCheckpointBlocks() == 2);
    assert(snapshot.checkpointBytes() == 300);
    assert(snapshot.phaseBaselineUnits() == 10);
    assert(snapshot.phaseElapsed() <= snapshot.elapsed());
    assert(snapshot.phaseCost(ProgressPhase::merging) == 30);

    assert(tracker.addCompletedTerms(20));
    assert((tracker.snapshot().phaseWork(ProgressPhase::splitting)
        == PhaseWorkUnits{50, 30}));
    // A repeated transition keeps the phase clock and baseline.
    assert(tracker.transitionTo(ProgressPhase::splitting));
    assert(tracker.snapshot().phaseBaselineUnits() == 10);

    assert(!tracker.declarePhaseUnits(ProgressPhase::splitting, 1));
    assert(!tracker.declarePhaseUnits(ProgressPhase::initializing, 1));
    assert(!tracker.declarePhaseUnits(ProgressPhase::completed, 1));
    assert(!tracker.declarePhaseUnits(
        ProgressPhase::validatingCheckpoints, 1));
    assert(!tracker.addPhaseUnits(ProgressPhase::merging, 1));
    assert(!tracker.addPhaseUnits(ProgressPhase::splitting, 1));

    assert(tracker.declarePhaseUnits(ProgressPhase::merging, 100));
    assert(!tracker.declarePhaseUnits(ProgressPhase::merging, 200));
    assert(tracker.addPhaseUnits(ProgressPhase::merging, 60));
    assert(!tracker.addPhaseUnits(ProgressPhase::merging, 41));
    assert((tracker.snapshot().phaseWork(ProgressPhase::merging)
        == PhaseWorkUnits{100, 60}));

    assert(tracker.transitionTo(ProgressPhase::merging));
    snapshot = tracker.snapshot();
    assert(snapshot.phaseBaselineUnits() == 60);
    assert(snapshot.phaseElapsed() <= snapshot.elapsed());

    // A zero total removes the phase from the end-to-end plan.
    assert(tracker.declarePhaseUnits(ProgressPhase::writingOutput, 0));
    assert(tracker.snapshot().phaseCost(ProgressPhase::writingOutput) == 0);
    assert(tracker.snapshot().phaseCost(ProgressPhase::verifyingOutput) == 10);

    assert(tracker.transitionTo(ProgressPhase::finalizing));
    assert(tracker.snapshot().phaseBaselineUnits() == 0);
    assert(!tracker.declarePhaseUnits(ProgressPhase::merging, 1));

    assert(tracker.markFailed("stopped"));
    snapshot = tracker.snapshot();
    assert(snapshot.phaseElapsed() == std::chrono::nanoseconds::zero());
    assert(snapshot.phaseBaselineUnits() == 0);
    assert(!tracker.addPhaseUnits(ProgressPhase::merging, 1));
    assert(!tracker.declarePhaseUnits(ProgressPhase::verifyingOutput, 1));

    ProgressTracker completing(ProgressWorkPlan{10, 10, 0});
    assert(completing.transitionTo(ProgressPhase::splitting));
    assert(completing.transitionTo(ProgressPhase::finalizing));
    assert(completing.transitionTo(ProgressPhase::completed));
    assert(completing.snapshot().phaseElapsed()
        == std::chrono::nanoseconds::zero());

    // Concurrent bounded phase units never exceed the declared total.
    ProgressTracker concurrent(ProgressWorkPlan{10, 10, 0});
    assert(concurrent.declarePhaseUnits(ProgressPhase::merging, 4'000));
    std::vector<std::thread> workers;
    for (int worker = 0; worker < 4; ++worker)
    {
        workers.emplace_back([&concurrent]() {
            for (int unit = 0; unit < 1'000; ++unit)
            {
                static_cast<void>(
                    concurrent.addPhaseUnits(ProgressPhase::merging, 1));
                static_cast<void>(concurrent.snapshot());
            }
        });
    }
    for (std::thread& worker : workers)
    {
        worker.join();
    }
    assert((concurrent.snapshot().phaseWork(ProgressPhase::merging)
        == PhaseWorkUnits{4'000, 4'000}));
    assert(!concurrent.addPhaseUnits(ProgressPhase::merging, 1));
}


std::chrono::nanoseconds closedTotal(
    const pi::progress::ProgressSnapshot& snapshot
)
{
    std::chrono::nanoseconds total = std::chrono::nanoseconds::zero();
    for (std::size_t index = 0; index < pi::progress::PROGRESS_PHASE_COUNT;
         ++index)
    {
        total += snapshot.closedPhaseDuration(
            static_cast<pi::progress::ProgressPhase>(index)
        );
    }
    return total;
}


void testPhaseTiming()
{
    using namespace std::chrono_literals;
    using pi::progress::ProgressPhase;
    using pi::progress::ProgressTracker;
    using pi::progress::ProgressWorkPlan;

    ProgressTracker tracker(ProgressWorkPlan{100, 50, 0});
    assert(tracker.transitionTo(ProgressPhase::validatingCheckpoints));
    assert(tracker.restoreCompletedWork(10, 0, 0));
    assert(tracker.transitionTo(ProgressPhase::splitting));

    // Without a unit in the phase, the reference is the phase start.
    std::this_thread::sleep_for(5ms);
    auto snapshot = tracker.snapshot();
    assert(snapshot.sinceLastPhaseUnit() == snapshot.phaseElapsed());

    assert(tracker.addCompletedTerms(20));
    std::this_thread::sleep_for(20ms);
    snapshot = tracker.snapshot();
    assert(snapshot.sinceLastPhaseUnit() >= 20ms);
    assert(snapshot.phaseElapsed() - snapshot.sinceLastPhaseUnit() >= 5ms);
    // Closed phases plus the current one cover the whole run exactly.
    assert(closedTotal(snapshot) + snapshot.phaseElapsed()
        == snapshot.elapsed());

    // Units published before a phase is entered predate the phase.
    assert(tracker.declarePhaseUnits(ProgressPhase::merging, 100));
    assert(tracker.addPhaseUnits(ProgressPhase::merging, 40));
    std::this_thread::sleep_for(5ms);
    assert(tracker.transitionTo(ProgressPhase::merging));
    snapshot = tracker.snapshot();
    assert(snapshot.sinceLastPhaseUnit() == snapshot.phaseElapsed());
    // Splitting closed with only its 20 newly computed terms.
    assert(snapshot.closedPhaseUnits(ProgressPhase::splitting) == 20);
    assert(snapshot.closedPhaseDuration(ProgressPhase::splitting) >= 25ms);
    assert(snapshot.closedPhaseUnits(ProgressPhase::merging) == 0);
    assert(snapshot.closedPhaseDuration(ProgressPhase::merging) == 0ns);

    assert(tracker.addPhaseUnits(ProgressPhase::merging, 30));
    assert(tracker.transitionTo(ProgressPhase::finalizing));
    snapshot = tracker.snapshot();
    assert(snapshot.closedPhaseUnits(ProgressPhase::merging) == 30);
    assert(closedTotal(snapshot) + snapshot.phaseElapsed()
        == snapshot.elapsed());

    assert(tracker.transitionTo(ProgressPhase::completed));
    snapshot = tracker.snapshot();
    assert(snapshot.sinceLastPhaseUnit() == 0ns);
    assert(closedTotal(snapshot) == snapshot.elapsed());

    ProgressTracker failing(ProgressWorkPlan{100, 50, 0});
    assert(failing.transitionTo(ProgressPhase::splitting));
    assert(failing.addCompletedTerms(5));
    assert(failing.markFailed("stopped"));
    snapshot = failing.snapshot();
    assert(snapshot.closedPhaseUnits(ProgressPhase::splitting) == 5);
    assert(snapshot.sinceLastPhaseUnit() == 0ns);
    assert(closedTotal(snapshot) == snapshot.elapsed());
}

} // namespace


int main()
{
    using pi::progress::CheckpointProgressUpdate;
    using pi::progress::CheckpointRangeProgress;
    using pi::progress::ProgressPhase;
    using pi::progress::ProgressTracker;
    using pi::progress::ProgressWorkPlan;

    assert(throws<std::invalid_argument>([]() {
        static_cast<void>(ProgressTracker(ProgressWorkPlan{0, 1, 0}));
    }));
    assert(throws<std::invalid_argument>([]() {
        static_cast<void>(ProgressTracker(ProgressWorkPlan{1, 0, 0}));
    }));

    ProgressTracker tracker(ProgressWorkPlan{1'000, 80'000, 8});
    assert(tracker.transitionTo(ProgressPhase::splitting));
    assert(!tracker.transitionTo(ProgressPhase::initializing));
    assert(tracker.setCurrentMergeLevel(0));
    tracker.setTaskCounts(8, 32);
    tracker.setMemoryBytes(4096);
    tracker.setStorageProgress(8192, 16384, 3);
    tracker.setStorageIoProgress(2, 1, 7, 1, 3, 2, 9, 2);
    assert(tracker.recordValidatedCheckpoint(
        CheckpointRangeProgress{0, 10'000, 0}
    ));
    assert(!tracker.recordValidatedCheckpoint(
        CheckpointRangeProgress{80'000, 80'001, 0}
    ));

    // Invalid committed ranges are rejected without mutation.
    assert(!tracker.publishCheckpointProgress(CheckpointProgressUpdate{
        1, 1, 10, CheckpointRangeProgress{5, 5, 0}
    }));
    assert(!tracker.publishCheckpointProgress(CheckpointProgressUpdate{
        1, 1, 10, CheckpointRangeProgress{0, 80'001, 0}
    }));
    {
        const auto unchanged = tracker.snapshot();
        assert(unchanged.completedCheckpointBlocks() == 0);
        assert(unchanged.durableCheckpointBlocks() == 0);
        assert(unchanged.checkpointBytes() == 0);
        assert(!unchanged.lastCommittedCheckpoint().has_value());
    }

    // Initial frontier: work and durable count rise together.
    assert(tracker.publishCheckpointProgress(CheckpointProgressUpdate{
        6, 6, 600, CheckpointRangeProgress{70'000, 80'000, 0}
    }));
    // Merge: durable gauge and bytes shrink, work stays monotonic.
    assert(tracker.publishCheckpointProgress(CheckpointProgressUpdate{
        0, 3, 450, CheckpointRangeProgress{40'000, 80'000, 1}
    }));
    // Reuse-only publication keeps the previous last committed range.
    assert(tracker.publishCheckpointProgress(CheckpointProgressUpdate{
        0, 3, 450, std::nullopt
    }));
    {
        const auto merged = tracker.snapshot();
        assert(merged.completedCheckpointBlocks() == 6);
        assert(merged.durableCheckpointBlocks() == 3);
        assert(merged.checkpointBytes() == 450);
        const CheckpointRangeProgress expectedCommit{40'000, 80'000, 1};
        assert(merged.lastCommittedCheckpoint() == expectedCommit);
        const CheckpointRangeProgress expectedValidated{0, 10'000, 0};
        assert(merged.lastValidatedCheckpoint() == expectedValidated);
    }
    // Work beyond the fixed total is clamped to it.
    assert(tracker.publishCheckpointProgress(CheckpointProgressUpdate{
        5, 2, 400, std::nullopt
    }));
    assert(tracker.snapshot().completedCheckpointBlocks() == 8);

    constexpr std::uint64_t threadCount = 8;
    constexpr std::uint64_t updatesPerThread = 10'000;
    std::atomic<bool> begin{false};
    std::atomic<bool> stopSnapshots{false};

    std::thread observer([&tracker, &stopSnapshots]() {
        std::uint64_t previous = 0;
        while (!stopSnapshots.load(std::memory_order_acquire))
        {
            const auto snapshot = tracker.snapshot();
            assert(snapshot.completedTerms() >= previous);
            assert(snapshot.completedTerms() <= snapshot.totalTerms());
            previous = snapshot.completedTerms();
        }
    });

    std::vector<std::thread> workers;
    workers.reserve(threadCount);
    for (std::uint64_t thread = 0; thread < threadCount; ++thread)
    {
        workers.emplace_back([&tracker, &begin]() {
            while (!begin.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }
            for (std::uint64_t update = 0;
                 update < updatesPerThread;
                 ++update)
            {
                assert(tracker.addCompletedTerms(1));
            }
        });
    }

    begin.store(true, std::memory_order_release);
    for (std::thread& worker : workers)
    {
        worker.join();
    }
    stopSnapshots.store(true, std::memory_order_release);
    observer.join();

    assert(!tracker.addCompletedTerms(1));

    // Concurrent snapshots observe each publication as one coherent unit.
    {
        ProgressTracker coherent(ProgressWorkPlan{1'000, 1'000, 1'000});
        std::atomic<bool> stop{false};
        std::thread reader([&coherent, &stop]() {
            std::uint64_t previousCompleted = 0;
            while (!stop.load(std::memory_order_acquire))
            {
                const auto snapshot = coherent.snapshot();
                assert(snapshot.completedCheckpointBlocks()
                    >= previousCompleted);
                assert(snapshot.completedCheckpointBlocks()
                    <= snapshot.totalCheckpointBlocks());
                assert(snapshot.checkpointBytes()
                    == snapshot.durableCheckpointBlocks() * 100);
                assert(snapshot.durableCheckpointBlocks()
                    == snapshot.completedCheckpointBlocks());
                if (snapshot.durableCheckpointBlocks() != 0)
                {
                    assert(snapshot.lastCommittedCheckpoint()
                        == (CheckpointRangeProgress{
                            snapshot.durableCheckpointBlocks() - 1,
                            snapshot.durableCheckpointBlocks(),
                            0
                        }));
                }
                previousCompleted = snapshot.completedCheckpointBlocks();
            }
        });
        for (std::uint64_t block = 1; block <= 1'000; ++block)
        {
            assert(coherent.publishCheckpointProgress(CheckpointProgressUpdate{
                1, block, block * 100,
                CheckpointRangeProgress{block - 1, block, 0}
            }));
        }
        stop.store(true, std::memory_order_release);
        reader.join();
    }

    assert(tracker.transitionTo(ProgressPhase::finalizing));
    assert(tracker.clearCurrentMergeLevel());
    assert(tracker.transitionTo(ProgressPhase::completed));
    const auto completed = tracker.snapshot();
    const auto completedAgain = tracker.snapshot();
    assert(completed.completedTerms() == 80'000);
    assert(completed.completedCheckpointBlocks() == 8);
    assert(completed.durableCheckpointBlocks() == 2);
    assert(completed.activeTasks() == 8);
    assert(completed.queuedTasks() == 32);
    assert(completed.memoryBytes() == 4096);
    assert(completed.checkpointBytes() == 400);
    assert(completed.storageResidentBytes() == 8192);
    assert(completed.storageStoredBytes() == 16384);
    assert(completed.storageChunkCount() == 3);
    assert(completed.storageQueuedWrites() == 2);
    assert(completed.storageActiveWrites() == 1);
    assert(completed.storageCompletedWrites() == 7);
    assert(completed.storageFailedWrites() == 1);
    assert(completed.storageQueuedReads() == 3);
    assert(completed.storageActiveReads() == 2);
    assert(completed.storageCompletedReads() == 9);
    assert(completed.storageFailedReads() == 2);
    assert(completed.lastValidatedCheckpoint().has_value());
    assert(completed.lastCommittedCheckpoint().has_value());
    assert(completed.elapsed() == completedAgain.elapsed());
    assert(!tracker.addCompletedTerms(0));
    assert(!tracker.setCurrentMergeLevel(4));
    assert(!tracker.publishCheckpointProgress(CheckpointProgressUpdate{
        0, 9, 900, std::nullopt
    }));
    assert(tracker.snapshot().durableCheckpointBlocks() == 2);
    assert(tracker.snapshot().checkpointBytes() == 400);
    assert(!tracker.markFailed("too late"));

    ProgressTracker failing(ProgressWorkPlan{100, 8, 0});
    assert(failing.transitionTo(ProgressPhase::validatingCheckpoints));
    assert(failing.markFailed("invalid checkpoint"));
    const auto failed = failing.snapshot();
    assert(failed.phase() == ProgressPhase::failed);
    assert(failed.failureDetail()
        == std::optional<std::string>{"invalid checkpoint"});
    assert(!failing.transitionTo(ProgressPhase::splitting));

    testPhaseWork();
    testPhaseTiming();

    return 0;
}
