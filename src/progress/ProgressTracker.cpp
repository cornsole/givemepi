#include "progress/ProgressTracker.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>


namespace pi::progress
{
namespace
{

bool addWithinLimit(
    std::atomic<std::uint64_t>& value,
    std::uint64_t count,
    std::uint64_t limit
) noexcept
{
    std::uint64_t current = value.load(std::memory_order_relaxed);
    while (count <= limit - current)
    {
        if (value.compare_exchange_weak(
                current,
                current + count,
                std::memory_order_relaxed,
                std::memory_order_relaxed))
        {
            return true;
        }
    }
    return false;
}

} // namespace


ProgressTracker::ProgressTracker(ProgressWorkPlan plan)
    : plan_(plan),
      startedAt_(std::chrono::steady_clock::now()),
      phaseCosts_(plan.phaseCosts),
      phaseStartedAt_(startedAt_)
{
    if (plan_.targetDigits == 0)
    {
        throw std::invalid_argument("Target digits must be greater than zero");
    }
    if (plan_.totalTerms == 0)
    {
        throw std::invalid_argument("Total terms must be greater than zero");
    }
    for (std::size_t index = 0; index < PROGRESS_PHASE_COUNT; ++index)
    {
        if (!isWorkPhase(static_cast<ProgressPhase>(index))
            && phaseCosts_[index] != 0)
        {
            throw std::invalid_argument(
                "Only work phases may carry a pipeline cost"
            );
        }
    }
}


bool ProgressTracker::transitionTo(ProgressPhase next)
{
    std::lock_guard lock(metadataMutex_);
    const ProgressPhase current = phase_.load(std::memory_order_relaxed);
    if (!isValidTransition(current, next))
    {
        return false;
    }
    if (current == next)
    {
        return true;
    }

    const auto now = std::chrono::steady_clock::now();
    phase_.store(next, std::memory_order_release);
    if (isTerminal(next) && !finishedAt_.has_value())
    {
        finishedAt_ = now;
    }
    phaseStartedAt_ = now;
    phaseBaselineUnits_ = completedUnitsOf(next);
    return true;
}


bool ProgressTracker::addCompletedTerms(std::uint64_t count) noexcept
{
    return isRunning()
        && addWithinLimit(completedTerms_, count, plan_.totalTerms);
}


bool ProgressTracker::restoreCompletedWork(
    std::uint64_t terms,
    std::uint64_t checkpointBlocks,
    std::uint64_t checkpointBytes
)
{
    std::lock_guard lock(metadataMutex_);
    const ProgressPhase current = phase_.load(std::memory_order_relaxed);
    if ((current != ProgressPhase::initializing
            && current != ProgressPhase::validatingCheckpoints)
        || completedCheckpointBlocks_ != 0
        || terms > plan_.totalTerms
        || checkpointBlocks > plan_.totalCheckpointBlocks)
    {
        return false;
    }

    std::uint64_t expected = 0;
    if (!completedTerms_.compare_exchange_strong(
            expected,
            terms,
            std::memory_order_relaxed,
            std::memory_order_relaxed))
    {
        return false;
    }
    restoredTerms_ = terms;
    completedCheckpointBlocks_ = checkpointBlocks;
    restoredCheckpointBlocks_ = checkpointBlocks;
    durableCheckpointBlocks_ = checkpointBlocks;
    checkpointBytes_ = checkpointBytes;
    return true;
}


bool ProgressTracker::declarePhaseUnits(
    ProgressPhase phase,
    std::uint64_t total
)
{
    if (!isWorkPhase(phase) || phase == ProgressPhase::splitting)
    {
        return false;
    }

    std::lock_guard lock(metadataMutex_);
    const ProgressPhase current = phase_.load(std::memory_order_relaxed);
    const std::size_t index = phaseIndex(phase);
    if (isTerminal(current)
        || phaseIndex(current) > index
        || phaseDeclared_[index])
    {
        return false;
    }

    phaseDeclared_[index] = true;
    phaseTotals_[index].store(total, std::memory_order_relaxed);
    if (total == 0)
    {
        phaseCosts_[index] = 0;
    }
    return true;
}


bool ProgressTracker::addPhaseUnits(
    ProgressPhase phase,
    std::uint64_t count
) noexcept
{
    if (!isWorkPhase(phase) || phase == ProgressPhase::splitting
        || !isRunning())
    {
        return false;
    }
    const std::size_t index = phaseIndex(phase);
    return addWithinLimit(
        phaseCompleted_[index],
        count,
        phaseTotals_[index].load(std::memory_order_relaxed)
    );
}


bool ProgressTracker::publishCheckpointProgress(
    const CheckpointProgressUpdate& update
)
{
    if (update.lastCommitted.has_value()
        && (update.lastCommitted->startTerm >= update.lastCommitted->endTerm
            || update.lastCommitted->endTerm > plan_.totalTerms))
    {
        return false;
    }

    std::lock_guard lock(metadataMutex_);
    if (isTerminal(phase_.load(std::memory_order_relaxed)))
    {
        return false;
    }

    const std::uint64_t remaining =
        plan_.totalCheckpointBlocks - completedCheckpointBlocks_;
    completedCheckpointBlocks_ += std::min(update.completedBlocks, remaining);
    durableCheckpointBlocks_ = update.durableBlocks;
    checkpointBytes_ = update.checkpointBytes;
    if (update.lastCommitted.has_value())
    {
        lastCommittedCheckpoint_ = update.lastCommitted;
    }
    return true;
}


void ProgressTracker::setTaskCounts(
    std::uint64_t activeTasks,
    std::uint64_t queuedTasks
) noexcept
{
    if (!isRunning())
    {
        return;
    }
    activeTasks_.store(activeTasks, std::memory_order_relaxed);
    queuedTasks_.store(queuedTasks, std::memory_order_relaxed);
}


void ProgressTracker::setMemoryBytes(std::uint64_t bytes) noexcept
{
    if (isRunning())
    {
        memoryBytes_.store(bytes, std::memory_order_relaxed);
    }
}


void ProgressTracker::setStorageProgress(
    std::uint64_t residentBytes,
    std::uint64_t storedBytes,
    std::uint64_t chunkCount
) noexcept
{
    if (!isRunning()) return;
    storageResidentBytes_.store(residentBytes, std::memory_order_relaxed);
    storageStoredBytes_.store(storedBytes, std::memory_order_relaxed);
    storageChunkCount_.store(chunkCount, std::memory_order_relaxed);
}

void ProgressTracker::setStorageIoProgress(
    std::uint64_t queuedWrites,
    std::uint64_t activeWrites,
    std::uint64_t completedWrites,
    std::uint64_t failedWrites,
    std::uint64_t queuedReads,
    std::uint64_t activeReads,
    std::uint64_t completedReads,
    std::uint64_t failedReads
) noexcept
{
    if (!isRunning()) return;
    storageQueuedWrites_.store(queuedWrites, std::memory_order_relaxed);
    storageActiveWrites_.store(activeWrites, std::memory_order_relaxed);
    storageCompletedWrites_.store(completedWrites, std::memory_order_relaxed);
    storageFailedWrites_.store(failedWrites, std::memory_order_relaxed);
    storageQueuedReads_.store(queuedReads, std::memory_order_relaxed);
    storageActiveReads_.store(activeReads, std::memory_order_relaxed);
    storageCompletedReads_.store(completedReads, std::memory_order_relaxed);
    storageFailedReads_.store(failedReads, std::memory_order_relaxed);
}


bool ProgressTracker::setCurrentMergeLevel(std::uint32_t level)
{
    std::lock_guard lock(metadataMutex_);
    if (isTerminal(phase_.load(std::memory_order_relaxed)))
    {
        return false;
    }
    currentMergeLevel_ = level;
    return true;
}


bool ProgressTracker::clearCurrentMergeLevel()
{
    std::lock_guard lock(metadataMutex_);
    if (isTerminal(phase_.load(std::memory_order_relaxed)))
    {
        return false;
    }
    currentMergeLevel_.reset();
    return true;
}


bool ProgressTracker::recordValidatedCheckpoint(
    CheckpointRangeProgress checkpoint
)
{
    if (checkpoint.startTerm >= checkpoint.endTerm
        || checkpoint.endTerm > plan_.totalTerms)
    {
        return false;
    }

    std::lock_guard lock(metadataMutex_);
    if (isTerminal(phase_.load(std::memory_order_relaxed)))
    {
        return false;
    }
    lastValidatedCheckpoint_ = checkpoint;
    return true;
}


bool ProgressTracker::markFailed(std::string detail)
{
    std::lock_guard lock(metadataMutex_);
    const ProgressPhase current = phase_.load(std::memory_order_relaxed);
    if (isTerminal(current))
    {
        return false;
    }

    failureDetail_ = std::move(detail);
    phase_.store(ProgressPhase::failed, std::memory_order_release);
    finishedAt_ = std::chrono::steady_clock::now();
    phaseStartedAt_ = *finishedAt_;
    phaseBaselineUnits_ = 0;
    return true;
}


ProgressSnapshot ProgressTracker::snapshot() const
{
    ProgressSnapshotData data;
    data.targetDigits = plan_.targetDigits;
    data.totalTerms = plan_.totalTerms;
    data.totalCheckpointBlocks = plan_.totalCheckpointBlocks;

    {
        std::lock_guard lock(metadataMutex_);
        data.phase = phase_.load(std::memory_order_acquire);
        data.currentMergeLevel = currentMergeLevel_;
        data.completedCheckpointBlocks = completedCheckpointBlocks_;
        data.restoredCheckpointBlocks = restoredCheckpointBlocks_;
        data.restoredTerms = restoredTerms_;
        data.durableCheckpointBlocks = durableCheckpointBlocks_;
        data.checkpointBytes = checkpointBytes_;
        data.lastValidatedCheckpoint = lastValidatedCheckpoint_;
        data.lastCommittedCheckpoint = lastCommittedCheckpoint_;
        data.failureDetail = failureDetail_;
        data.phaseCosts = phaseCosts_;
        data.phaseBaselineUnits = phaseBaselineUnits_;
        // Declarations take this mutex, so each total read here bounds the
        // completed count read after it.
        for (std::size_t index = 0; index < PROGRESS_PHASE_COUNT; ++index)
        {
            if (!phaseDeclared_[index])
            {
                continue;
            }
            data.phaseUnits[index].total =
                phaseTotals_[index].load(std::memory_order_relaxed);
            data.phaseUnits[index].completed =
                phaseCompleted_[index].load(std::memory_order_relaxed);
        }

        const auto endpoint = finishedAt_.value_or(
            std::chrono::steady_clock::now()
        );
        data.elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            endpoint - startedAt_
        );
        data.phaseElapsed =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                endpoint - phaseStartedAt_
            );
    }

    data.completedTerms = completedTerms_.load(std::memory_order_relaxed);
    data.activeTasks = activeTasks_.load(std::memory_order_relaxed);
    data.queuedTasks = queuedTasks_.load(std::memory_order_relaxed);
    data.memoryBytes = memoryBytes_.load(std::memory_order_relaxed);
    data.storageResidentBytes = storageResidentBytes_.load(std::memory_order_relaxed);
    data.storageStoredBytes = storageStoredBytes_.load(std::memory_order_relaxed);
    data.storageChunkCount = storageChunkCount_.load(std::memory_order_relaxed);
    data.storageQueuedWrites = storageQueuedWrites_.load(std::memory_order_relaxed);
    data.storageActiveWrites = storageActiveWrites_.load(std::memory_order_relaxed);
    data.storageCompletedWrites = storageCompletedWrites_.load(std::memory_order_relaxed);
    data.storageFailedWrites = storageFailedWrites_.load(std::memory_order_relaxed);
    data.storageQueuedReads = storageQueuedReads_.load(std::memory_order_relaxed);
    data.storageActiveReads = storageActiveReads_.load(std::memory_order_relaxed);
    data.storageCompletedReads = storageCompletedReads_.load(std::memory_order_relaxed);
    data.storageFailedReads = storageFailedReads_.load(std::memory_order_relaxed);

    return ProgressSnapshot(std::move(data));
}


bool ProgressTracker::isRunning() const noexcept
{
    return !isTerminal(phase_.load(std::memory_order_acquire));
}


std::uint64_t ProgressTracker::completedUnitsOf(
    ProgressPhase phase
) const noexcept
{
    if (phase == ProgressPhase::splitting)
    {
        return completedTerms_.load(std::memory_order_relaxed);
    }
    if (!isWorkPhase(phase))
    {
        return 0;
    }
    return phaseCompleted_[phaseIndex(phase)].load(std::memory_order_relaxed);
}

} // namespace pi::progress
