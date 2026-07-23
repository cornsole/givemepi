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
      currentMergeLevel_(data.currentMergeLevel),
      activeTasks_(data.activeTasks),
      queuedTasks_(data.queuedTasks),
      elapsed_(data.elapsed),
      memoryBytes_(data.memoryBytes),
      checkpointBytes_(data.checkpointBytes),
      storageResidentBytes_(data.storageResidentBytes),
      storageStoredBytes_(data.storageStoredBytes),
      storageChunkCount_(data.storageChunkCount),
      lastValidatedCheckpoint_(std::move(data.lastValidatedCheckpoint)),
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

    if (lastValidatedCheckpoint_.has_value()
        && lastValidatedCheckpoint_->startTerm
            >= lastValidatedCheckpoint_->endTerm)
    {
        throw std::invalid_argument(
            "Validated checkpoint range must be non-empty"
        );
    }

    if (phase_ != ProgressPhase::failed && failureDetail_.has_value())
    {
        throw std::invalid_argument(
            "Failure detail is valid only in the failed phase"
        );
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


std::uint64_t ProgressSnapshot::memoryBytes() const noexcept
{
    return memoryBytes_;
}


std::uint64_t ProgressSnapshot::checkpointBytes() const noexcept
{
    return checkpointBytes_;
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


const std::optional<ValidatedCheckpointProgress>&
ProgressSnapshot::lastValidatedCheckpoint() const noexcept
{
    return lastValidatedCheckpoint_;
}


const std::optional<std::string>&
ProgressSnapshot::failureDetail() const noexcept
{
    return failureDetail_;
}

} // namespace pi::progress
