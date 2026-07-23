#include "progress/ProgressSnapshot.hpp"

#include <cassert>
#include <chrono>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>


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
    using namespace std::chrono_literals;
    using pi::progress::ProgressPhase;
    using pi::progress::ProgressSnapshot;
    using pi::progress::ProgressSnapshotData;
    using pi::progress::ProgressTerminalState;
    using pi::progress::ValidatedCheckpointProgress;

    static_assert(std::is_same_v<
        decltype(std::declval<const ProgressSnapshot&>().failureDetail()),
        const std::optional<std::string>&
    >);

    ProgressSnapshotData data;
    data.phase = ProgressPhase::merging;
    data.targetDigits = 1'000;
    data.totalTerms = 74;
    data.completedTerms = 48;
    data.totalCheckpointBlocks = 10;
    data.completedCheckpointBlocks = 6;
    data.currentMergeLevel = 3;
    data.activeTasks = 4;
    data.queuedTasks = 2;
    data.elapsed = 1500ms;
    data.memoryBytes = 4096;
    data.checkpointBytes = 2048;
    data.lastValidatedCheckpoint =
        ValidatedCheckpointProgress{40, 48, 2};

    const ProgressSnapshot snapshot(data);
    assert(snapshot.schemaVersion() == 3);
    assert(snapshot.phase() == ProgressPhase::merging);
    assert(snapshot.terminalState() == ProgressTerminalState::running);
    assert(snapshot.targetDigits() == 1'000);
    assert(snapshot.totalTerms() == 74);
    assert(snapshot.completedTerms() == 48);
    assert(snapshot.totalCheckpointBlocks() == 10);
    assert(snapshot.completedCheckpointBlocks() == 6);
    assert(snapshot.currentMergeLevel() == 3);
    assert(snapshot.activeTasks() == 4);
    assert(snapshot.queuedTasks() == 2);
    assert(snapshot.elapsed() == 1500ms);
    assert(snapshot.memoryBytes() == 4096);
    assert(snapshot.checkpointBytes() == 2048);
    const ValidatedCheckpointProgress expectedCheckpoint{40, 48, 2};
    assert(snapshot.lastValidatedCheckpoint() == expectedCheckpoint);
    assert(!snapshot.failureDetail().has_value());

    data.completedTerms = 49;
    data.lastValidatedCheckpoint =
        ValidatedCheckpointProgress{48, 49, 0};
    assert(snapshot.completedTerms() == 48);
    assert(snapshot.lastValidatedCheckpoint() == expectedCheckpoint);

    ProgressSnapshotData failed;
    failed.phase = ProgressPhase::failed;
    failed.failureDetail = "checkpoint validation failed";
    const ProgressSnapshot failedSnapshot(std::move(failed));
    assert(failedSnapshot.terminalState() == ProgressTerminalState::failed);
    assert(failedSnapshot.failureDetail()
        == std::optional<std::string>{"checkpoint validation failed"});

    ProgressSnapshotData invalidTerms;
    invalidTerms.totalTerms = 4;
    invalidTerms.completedTerms = 5;
    assert(throws<std::invalid_argument>([&invalidTerms]() {
        static_cast<void>(ProgressSnapshot(invalidTerms));
    }));

    ProgressSnapshotData invalidBlocks;
    invalidBlocks.totalCheckpointBlocks = 2;
    invalidBlocks.completedCheckpointBlocks = 3;
    assert(throws<std::invalid_argument>([&invalidBlocks]() {
        static_cast<void>(ProgressSnapshot(invalidBlocks));
    }));

    ProgressSnapshotData invalidElapsed;
    invalidElapsed.elapsed = -1ns;
    assert(throws<std::invalid_argument>([&invalidElapsed]() {
        static_cast<void>(ProgressSnapshot(invalidElapsed));
    }));

    ProgressSnapshotData invalidRange;
    invalidRange.lastValidatedCheckpoint =
        ValidatedCheckpointProgress{10, 10, 0};
    assert(throws<std::invalid_argument>([&invalidRange]() {
        static_cast<void>(ProgressSnapshot(invalidRange));
    }));

    ProgressSnapshotData invalidFailure;
    invalidFailure.phase = ProgressPhase::completed;
    invalidFailure.failureDetail = "not a failure";
    assert(throws<std::invalid_argument>([&invalidFailure]() {
        static_cast<void>(ProgressSnapshot(invalidFailure));
    }));

    return 0;
}
