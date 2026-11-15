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
    using pi::progress::CheckpointRangeProgress;

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
    data.durableCheckpointBlocks = 3;
    data.currentMergeLevel = 3;
    data.activeTasks = 4;
    data.queuedTasks = 2;
    data.elapsed = 1500ms;
    data.memoryBytes = 4096;
    data.checkpointBytes = 2048;
    data.lastValidatedCheckpoint =
        CheckpointRangeProgress{40, 48, 2};
    data.lastCommittedCheckpoint =
        CheckpointRangeProgress{48, 74, 1};

    const ProgressSnapshot snapshot(data);
    assert(snapshot.schemaVersion() == 5);
    assert(snapshot.phase() == ProgressPhase::merging);
    assert(snapshot.terminalState() == ProgressTerminalState::running);
    assert(snapshot.targetDigits() == 1'000);
    assert(snapshot.totalTerms() == 74);
    assert(snapshot.completedTerms() == 48);
    assert(snapshot.totalCheckpointBlocks() == 10);
    assert(snapshot.completedCheckpointBlocks() == 6);
    assert(snapshot.durableCheckpointBlocks() == 3);
    assert(snapshot.currentMergeLevel() == 3);
    assert(snapshot.activeTasks() == 4);
    assert(snapshot.queuedTasks() == 2);
    assert(snapshot.elapsed() == 1500ms);
    assert(snapshot.memoryBytes() == 4096);
    assert(snapshot.checkpointBytes() == 2048);
    const CheckpointRangeProgress expectedCheckpoint{40, 48, 2};
    assert(snapshot.lastValidatedCheckpoint() == expectedCheckpoint);
    const CheckpointRangeProgress expectedCommit{48, 74, 1};
    assert(snapshot.lastCommittedCheckpoint() == expectedCommit);
    assert(!snapshot.failureDetail().has_value());

    data.completedTerms = 49;
    data.lastValidatedCheckpoint =
        CheckpointRangeProgress{48, 49, 0};
    data.lastCommittedCheckpoint.reset();
    assert(snapshot.completedTerms() == 48);
    assert(snapshot.lastValidatedCheckpoint() == expectedCheckpoint);
    assert(snapshot.lastCommittedCheckpoint() == expectedCommit);

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
        CheckpointRangeProgress{10, 10, 0};
    assert(throws<std::invalid_argument>([&invalidRange]() {
        static_cast<void>(ProgressSnapshot(invalidRange));
    }));

    ProgressSnapshotData invalidCommit;
    invalidCommit.lastCommittedCheckpoint =
        CheckpointRangeProgress{12, 11, 0};
    assert(throws<std::invalid_argument>([&invalidCommit]() {
        static_cast<void>(ProgressSnapshot(invalidCommit));
    }));

    ProgressSnapshotData invalidFailure;
    invalidFailure.phase = ProgressPhase::completed;
    invalidFailure.failureDetail = "not a failure";
    assert(throws<std::invalid_argument>([&invalidFailure]() {
        static_cast<void>(ProgressSnapshot(invalidFailure));
    }));

    using pi::progress::PhaseWorkUnits;
    using pi::progress::phaseIndex;

    ProgressSnapshotData work;
    work.phase = ProgressPhase::merging;
    work.totalTerms = 10;
    work.completedTerms = 10;
    work.restoredTerms = 4;
    work.totalCheckpointBlocks = 3;
    work.completedCheckpointBlocks = 2;
    work.restoredCheckpointBlocks = 1;
    work.phaseUnits[phaseIndex(ProgressPhase::merging)] =
        PhaseWorkUnits{100, 30};
    work.phaseCosts[phaseIndex(ProgressPhase::splitting)] = 7;
    work.phaseCosts[phaseIndex(ProgressPhase::merging)] = 5;
    work.elapsed = 3s;
    work.phaseElapsed = 1s;
    work.phaseBaselineUnits = 10;
    const ProgressSnapshot workSnapshot(work);
    assert(workSnapshot.restoredTerms() == 4);
    assert(workSnapshot.restoredCheckpointBlocks() == 1);
    assert((workSnapshot.phaseWork(ProgressPhase::splitting)
        == PhaseWorkUnits{10, 10}));
    assert((workSnapshot.phaseWork(ProgressPhase::merging)
        == PhaseWorkUnits{100, 30}));
    assert((workSnapshot.phaseWork(ProgressPhase::completed)
        == PhaseWorkUnits{}));
    assert(workSnapshot.phaseCost(ProgressPhase::splitting) == 7);
    assert(workSnapshot.phaseElapsed() == 1s);
    assert(workSnapshot.phaseBaselineUnits() == 10);

    const auto rejects = [](ProgressSnapshotData invalid) {
        return throws<std::invalid_argument>([&invalid]() {
            static_cast<void>(ProgressSnapshot(invalid));
        });
    };
    ProgressSnapshotData invalidWork = work;
    invalidWork.restoredTerms = 11;
    assert(rejects(invalidWork));
    invalidWork = work;
    invalidWork.restoredCheckpointBlocks = 3;
    assert(rejects(invalidWork));
    invalidWork = work;
    invalidWork.phaseUnits[phaseIndex(ProgressPhase::merging)] =
        PhaseWorkUnits{10, 11};
    assert(rejects(invalidWork));
    invalidWork = work;
    invalidWork.phaseUnits[phaseIndex(ProgressPhase::splitting)] =
        PhaseWorkUnits{10, 1};
    assert(rejects(invalidWork));
    invalidWork = work;
    invalidWork.phaseUnits[phaseIndex(ProgressPhase::completed)] =
        PhaseWorkUnits{1, 0};
    assert(rejects(invalidWork));
    invalidWork = work;
    invalidWork.phaseCosts[phaseIndex(ProgressPhase::initializing)] = 1;
    assert(rejects(invalidWork));
    invalidWork = work;
    invalidWork.phaseElapsed = 4s;
    assert(rejects(invalidWork));
    invalidWork = work;
    invalidWork.phaseElapsed = -1ns;
    assert(rejects(invalidWork));
    invalidWork = work;
    invalidWork.phaseBaselineUnits = 31;
    assert(rejects(invalidWork));

    return 0;
}
