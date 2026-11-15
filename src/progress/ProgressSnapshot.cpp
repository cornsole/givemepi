#include "progress/ProgressSnapshot.hpp"

#include <stdexcept>
#include <utility>


namespace pi::progress
{

ProgressSnapshot::ProgressSnapshot(ProgressSnapshotData data)
    : phase_(data.phase),
      targetDigits_(data.targetDigits),
      totalTerms_(data.totalTerms),
      completedTerms_(data.completedTerms),
      totalCheckpointBlocks_(data.totalCheckpointBlocks),
      completedCheckpointBlocks_(data.completedCheckpointBlocks),
      restoredCheckpointBlocks_(data.restoredCheckpointBlocks),
      durableCheckpointBlocks_(data.durableCheckpointBlocks),
      currentMergeLevel_(data.currentMergeLevel),
      activeTasks_(data.activeTasks),
      queuedTasks_(data.queuedTasks),
      elapsed_(data.elapsed),
      restoredTerms_(data.restoredTerms),
      phaseUnits_(data.phaseUnits),
      phaseCosts_(data.phaseCosts),
      phaseElapsed_(data.phaseElapsed),
      phaseBaselineUnits_(data.phaseBaselineUnits),
      sinceLastPhaseUnit_(data.sinceLastPhaseUnit),
      closedPhaseDurations_(data.closedPhaseDurations),
      closedPhaseUnits_(data.closedPhaseUnits),
      memoryBytes_(data.memoryBytes),
      checkpointBytes_(data.checkpointBytes),
      storageResidentBytes_(data.storageResidentBytes),
      storageStoredBytes_(data.storageStoredBytes),
      storageChunkCount_(data.storageChunkCount),
      storageQueuedWrites_(data.storageQueuedWrites),
      storageActiveWrites_(data.storageActiveWrites),
      storageCompletedWrites_(data.storageCompletedWrites),
      storageFailedWrites_(data.storageFailedWrites),
      storageQueuedReads_(data.storageQueuedReads),
      storageActiveReads_(data.storageActiveReads),
      storageCompletedReads_(data.storageCompletedReads),
      storageFailedReads_(data.storageFailedReads),
      lastValidatedCheckpoint_(std::move(data.lastValidatedCheckpoint)),
      lastCommittedCheckpoint_(std::move(data.lastCommittedCheckpoint)),
      failureDetail_(std::move(data.failureDetail))
{
    if (completedTerms_ > totalTerms_)
    {
        throw std::invalid_argument(
            "Completed terms cannot exceed total terms"
        );
    }

    if (completedCheckpointBlocks_ > totalCheckpointBlocks_)
    {
        throw std::invalid_argument(
            "Completed checkpoint blocks cannot exceed total blocks"
        );
    }

    if (elapsed_ < std::chrono::nanoseconds::zero())
    {
        throw std::invalid_argument("Elapsed time cannot be negative");
    }

    const auto emptyRange = [](const auto& checkpoint) {
        return checkpoint.has_value()
            && checkpoint->startTerm >= checkpoint->endTerm;
    };
    if (emptyRange(lastValidatedCheckpoint_)
        || emptyRange(lastCommittedCheckpoint_))
    {
        throw std::invalid_argument(
            "Checkpoint range must be non-empty"
        );
    }

    if (phase_ != ProgressPhase::failed && failureDetail_.has_value())
    {
        throw std::invalid_argument(
            "Failure detail is valid only in the failed phase"
        );
    }

    if (restoredTerms_ > completedTerms_
        || restoredCheckpointBlocks_ > completedCheckpointBlocks_)
    {
        throw std::invalid_argument(
            "Restored work cannot exceed completed work"
        );
    }

    for (std::size_t index = 0; index < PROGRESS_PHASE_COUNT; ++index)
    {
        const auto phase = static_cast<ProgressPhase>(index);
        const PhaseWorkUnits& units = phaseUnits_[index];
        if ((!isWorkPhase(phase) || phase == ProgressPhase::splitting)
            && units != PhaseWorkUnits{})
        {
            throw std::invalid_argument(
                "Phase units are valid only for non-splitting work phases"
            );
        }
        if (units.completed > units.total)
        {
            throw std::invalid_argument(
                "Completed phase units cannot exceed total phase units"
            );
        }
        if (!isWorkPhase(phase) && phaseCosts_[index] != 0)
        {
            throw std::invalid_argument(
                "Only work phases may carry a pipeline cost"
            );
        }
    }

    if (phaseElapsed_ < std::chrono::nanoseconds::zero()
        || phaseElapsed_ > elapsed_)
    {
        throw std::invalid_argument(
            "Phase elapsed time must lie within total elapsed time"
        );
    }

    if (phaseBaselineUnits_ > phaseWork(phase_).completed)
    {
        throw std::invalid_argument(
            "Phase baseline cannot exceed completed phase units"
        );
    }

    if (sinceLastPhaseUnit_ < std::chrono::nanoseconds::zero()
        || sinceLastPhaseUnit_ > phaseElapsed_)
    {
        throw std::invalid_argument(
            "Time since the last phase unit must lie within phase elapsed time"
        );
    }

    std::chrono::nanoseconds closedTotal = std::chrono::nanoseconds::zero();
    for (std::size_t index = 0; index < PROGRESS_PHASE_COUNT; ++index)
    {
        const auto phase = static_cast<ProgressPhase>(index);
        const std::chrono::nanoseconds duration = closedPhaseDurations_[index];
        if (duration < std::chrono::nanoseconds::zero()
            || duration > elapsed_ - phaseElapsed_ - closedTotal)
        {
            throw std::invalid_argument(
                "Closed phase durations must fit within elapsed time"
            );
        }
        closedTotal += duration;

        const PhaseWorkUnits units = phaseWork(phase);
        const std::uint64_t restored =
            phase == ProgressPhase::splitting ? restoredTerms_ : 0;
        if (closedPhaseUnits_[index] > units.completed - restored)
        {
            throw std::invalid_argument(
                "Closed phase units cannot exceed newly completed units"
            );
        }
    }
}


std::uint32_t ProgressSnapshot::schemaVersion() const noexcept
{
    return PROGRESS_SCHEMA_VERSION;
}


ProgressPhase ProgressSnapshot::phase() const noexcept
{
    return phase_;
}


ProgressTerminalState ProgressSnapshot::terminalState() const noexcept
{
    return progress::terminalState(phase_);
}


std::uint64_t ProgressSnapshot::targetDigits() const noexcept
{
    return targetDigits_;
}


std::uint64_t ProgressSnapshot::totalTerms() const noexcept
{
    return totalTerms_;
}


std::uint64_t ProgressSnapshot::completedTerms() const noexcept
{
    return completedTerms_;
}


std::uint64_t ProgressSnapshot::totalCheckpointBlocks() const noexcept
{
    return totalCheckpointBlocks_;
}


std::uint64_t ProgressSnapshot::completedCheckpointBlocks() const noexcept
{
    return completedCheckpointBlocks_;
}


std::uint64_t ProgressSnapshot::restoredCheckpointBlocks() const noexcept
{
    return restoredCheckpointBlocks_;
}


std::uint64_t ProgressSnapshot::durableCheckpointBlocks() const noexcept
{
    return durableCheckpointBlocks_;
}


std::optional<std::uint32_t>
ProgressSnapshot::currentMergeLevel() const noexcept
{
    return currentMergeLevel_;
}


std::uint64_t ProgressSnapshot::activeTasks() const noexcept
{
    return activeTasks_;
}


std::uint64_t ProgressSnapshot::queuedTasks() const noexcept
{
    return queuedTasks_;
}


std::chrono::nanoseconds ProgressSnapshot::elapsed() const noexcept
{
    return elapsed_;
}


std::uint64_t ProgressSnapshot::restoredTerms() const noexcept
{
    return restoredTerms_;
}


PhaseWorkUnits ProgressSnapshot::phaseWork(ProgressPhase phase) const noexcept
{
    if (phase == ProgressPhase::splitting)
    {
        return PhaseWorkUnits{totalTerms_, completedTerms_};
    }
    return phaseUnits_[phaseIndex(phase)];
}


std::uint64_t ProgressSnapshot::phaseCost(ProgressPhase phase) const noexcept
{
    return phaseCosts_[phaseIndex(phase)];
}


std::chrono::nanoseconds ProgressSnapshot::phaseElapsed() const noexcept
{
    return phaseElapsed_;
}


std::uint64_t ProgressSnapshot::phaseBaselineUnits() const noexcept
{
    return phaseBaselineUnits_;
}


std::uint64_t ProgressSnapshot::memoryBytes() const noexcept
{
    return memoryBytes_;
}


std::uint64_t ProgressSnapshot::checkpointBytes() const noexcept
{
    return checkpointBytes_;
}


std::chrono::nanoseconds ProgressSnapshot::sinceLastPhaseUnit() const noexcept
{
    return sinceLastPhaseUnit_;
}


std::chrono::nanoseconds ProgressSnapshot::closedPhaseDuration(
    ProgressPhase phase
) const noexcept
{
    return closedPhaseDurations_[phaseIndex(phase)];
}


std::uint64_t ProgressSnapshot::closedPhaseUnits(
    ProgressPhase phase
) const noexcept
{
    return closedPhaseUnits_[phaseIndex(phase)];
}

std::uint64_t ProgressSnapshot::storageResidentBytes() const noexcept
{
    return storageResidentBytes_;
}

std::uint64_t ProgressSnapshot::storageStoredBytes() const noexcept
{
    return storageStoredBytes_;
}

std::uint64_t ProgressSnapshot::storageChunkCount() const noexcept
{
    return storageChunkCount_;
}

std::uint64_t ProgressSnapshot::storageQueuedWrites() const noexcept
{
    return storageQueuedWrites_;
}

std::uint64_t ProgressSnapshot::storageActiveWrites() const noexcept
{
    return storageActiveWrites_;
}

std::uint64_t ProgressSnapshot::storageCompletedWrites() const noexcept
{
    return storageCompletedWrites_;
}

std::uint64_t ProgressSnapshot::storageFailedWrites() const noexcept
{
    return storageFailedWrites_;
}

std::uint64_t ProgressSnapshot::storageQueuedReads() const noexcept
{
    return storageQueuedReads_;
}

std::uint64_t ProgressSnapshot::storageActiveReads() const noexcept
{
    return storageActiveReads_;
}

std::uint64_t ProgressSnapshot::storageCompletedReads() const noexcept
{
    return storageCompletedReads_;
}

std::uint64_t ProgressSnapshot::storageFailedReads() const noexcept
{
    return storageFailedReads_;
}


const std::optional<CheckpointRangeProgress>&
ProgressSnapshot::lastValidatedCheckpoint() const noexcept
{
    return lastValidatedCheckpoint_;
}


const std::optional<CheckpointRangeProgress>&
ProgressSnapshot::lastCommittedCheckpoint() const noexcept
{
    return lastCommittedCheckpoint_;
}


const std::optional<std::string>&
ProgressSnapshot::failureDetail() const noexcept
{
    return failureDetail_;
}

} // namespace pi::progress
