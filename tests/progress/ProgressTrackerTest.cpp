#include "progress/ProgressTracker.hpp"

#include <atomic>
#include <cassert>
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

} // namespace


int main()
{
    using pi::progress::ProgressPhase;
    using pi::progress::ProgressTracker;
    using pi::progress::ProgressWorkPlan;
    using pi::progress::ValidatedCheckpointProgress;

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
    tracker.setCheckpointBytes(2048);
    tracker.setStorageProgress(8192, 16384, 3);
    assert(tracker.recordValidatedCheckpoint(
        ValidatedCheckpointProgress{0, 10'000, 0}
    ));
    assert(!tracker.recordValidatedCheckpoint(
        ValidatedCheckpointProgress{80'000, 80'001, 0}
    ));

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
    for (std::uint64_t block = 0; block < 8; ++block)
    {
        assert(tracker.addCompletedCheckpointBlocks(1));
    }
    assert(!tracker.addCompletedCheckpointBlocks(1));

    assert(tracker.transitionTo(ProgressPhase::finalizing));
    assert(tracker.clearCurrentMergeLevel());
    assert(tracker.transitionTo(ProgressPhase::completed));
    const auto completed = tracker.snapshot();
    const auto completedAgain = tracker.snapshot();
    assert(completed.completedTerms() == 80'000);
    assert(completed.completedCheckpointBlocks() == 8);
    assert(completed.activeTasks() == 8);
    assert(completed.queuedTasks() == 32);
    assert(completed.memoryBytes() == 4096);
    assert(completed.checkpointBytes() == 2048);
    assert(completed.storageResidentBytes() == 8192);
    assert(completed.storageStoredBytes() == 16384);
    assert(completed.storageChunkCount() == 3);
    assert(completed.lastValidatedCheckpoint().has_value());
    assert(completed.elapsed() == completedAgain.elapsed());
    assert(!tracker.addCompletedTerms(0));
    assert(!tracker.setCurrentMergeLevel(4));
    assert(!tracker.markFailed("too late"));

    ProgressTracker failing(ProgressWorkPlan{100, 8, 0});
    assert(failing.transitionTo(ProgressPhase::validatingCheckpoints));
    assert(failing.markFailed("invalid checkpoint"));
    const auto failed = failing.snapshot();
    assert(failed.phase() == ProgressPhase::failed);
    assert(failed.failureDetail()
        == std::optional<std::string>{"invalid checkpoint"});
    assert(!failing.transitionTo(ProgressPhase::splitting));

    return 0;
}
