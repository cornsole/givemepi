#include "progress/ProgressTracker.hpp"

#include <limits>
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
      startedAt_(std::chrono::steady_clock::now())
{
    if (plan_.targetDigits == 0)
    {
        throw std::invalid_argument("Target digits must be greater than zero");
    }
    if (plan_.totalTerms == 0)
    {
        throw std::invalid_argument("Total terms must be greater than zero");
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

    phase_.store(next, std::memory_order_release);
    if (isTerminal(next) && !finishedAt_.has_value())
    {
        finishedAt_ = std::chrono::steady_clock::now();
    }
    return true;
}


bool ProgressTracker::addCompletedTerms(std::uint64_t count) noexcept
{
    return isRunning()
        && addWithinLimit(completedTerms_, count, plan_.totalTerms);
}


bool ProgressTracker::addCompletedCheckpointBlocks(
    std::uint64_t count
) noexcept
{
    return isRunning()
        && addWithinLimit(
            completedCheckpointBlocks_,
            count,
            plan_.totalCheckpointBlocks
        );
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


void ProgressTracker::setCheckpointBytes(std::uint64_t bytes) noexcept
{
    if (isRunning())
    {
        checkpointBytes_.store(bytes, std::memory_order_relaxed);
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
    ValidatedCheckpointProgress checkpoint
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
        data.lastValidatedCheckpoint = lastValidatedCheckpoint_;
        data.failureDetail = failureDetail_;

        const auto endpoint = finishedAt_.value_or(
            std::chrono::steady_clock::now()
        );
        data.elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            endpoint - startedAt_
        );
    }

    data.completedTerms = completedTerms_.load(std::memory_order_relaxed);
    data.completedCheckpointBlocks =
        completedCheckpointBlocks_.load(std::memory_order_relaxed);
    data.activeTasks = activeTasks_.load(std::memory_order_relaxed);
    data.queuedTasks = queuedTasks_.load(std::memory_order_relaxed);
    data.memoryBytes = memoryBytes_.load(std::memory_order_relaxed);
    data.checkpointBytes = checkpointBytes_.load(std::memory_order_relaxed);
    data.storageResidentBytes = storageResidentBytes_.load(std::memory_order_relaxed);
    data.storageStoredBytes = storageStoredBytes_.load(std::memory_order_relaxed);
    data.storageChunkCount = storageChunkCount_.load(std::memory_order_relaxed);

    return ProgressSnapshot(std::move(data));
}


bool ProgressTracker::isRunning() const noexcept
{
    return !isTerminal(phase_.load(std::memory_order_acquire));
}

} // namespace pi::progress
