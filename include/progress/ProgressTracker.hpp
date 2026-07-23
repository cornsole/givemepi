#pragma once

#include "progress/ProgressSnapshot.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>


namespace pi::progress
{

/// Fixed work totals for one tracker lifecycle.
struct ProgressWorkPlan
{
    std::uint64_t targetDigits;
    std::uint64_t totalTerms;
    std::uint64_t totalCheckpointBlocks;
};


/// Thread-safe owner of raw computation progress.
///
/// Worker-facing numeric updates use atomics and never format, allocate, or
/// perform I/O. Low-frequency compound metadata uses a short mutex-protected
/// path. Snapshot creation copies current state into an immutable value.
class ProgressTracker
{
public:
    /// Creates a tracker for a fixed work plan.
    ///
    /// Time complexity: O(1). Memory complexity: O(1).
    explicit ProgressTracker(ProgressWorkPlan plan);

    ProgressTracker(const ProgressTracker&) = delete;
    ProgressTracker& operator=(const ProgressTracker&) = delete;

    /// Applies a legal, idempotent phase transition.
    ///
    /// Returns false for an illegal transition. Time and memory: O(1).
    [[nodiscard]] bool transitionTo(ProgressPhase next);

    /// Atomically adds completed terms without exceeding the fixed total.
    ///
    /// Returns false after termination or if the addition exceeds the total.
    /// Lock-free when the platform provides lock-free 64-bit atomics.
    [[nodiscard]] bool addCompletedTerms(std::uint64_t count) noexcept;

    /// Atomically adds completed checkpoint blocks within the fixed total.
    ///
    /// Returns false after termination or if the addition exceeds the total.
    /// Lock-free when the platform provides lock-free 64-bit atomics.
    [[nodiscard]]
    bool addCompletedCheckpointBlocks(std::uint64_t count) noexcept;

    /// Publishes current scheduler task counts. Time and memory: O(1).
    void setTaskCounts(
        std::uint64_t activeTasks,
        std::uint64_t queuedTasks
    ) noexcept;

    /// Publishes current process memory consumption. Time and memory: O(1).
    void setMemoryBytes(std::uint64_t bytes) noexcept;

    /// Publishes total durable checkpoint bytes. Time and memory: O(1).
    void setCheckpointBytes(std::uint64_t bytes) noexcept;

    /// Publishes storage resident bytes, durable bytes, and indexed chunk count.
    void setStorageProgress(
        std::uint64_t residentBytes,
        std::uint64_t storedBytes,
        std::uint64_t chunkCount
    ) noexcept;

    /// Publishes the current merge level on the low-frequency metadata path.
    ///
    /// Returns false after termination. Time and memory: O(1).
    [[nodiscard]] bool setCurrentMergeLevel(std::uint32_t level);

    /// Clears the merge level when the computation leaves the merge phase.
    ///
    /// Returns false after termination. Time and memory: O(1).
    [[nodiscard]] bool clearCurrentMergeLevel();

    /// Records the most recently validated checkpoint range.
    ///
    /// Returns false for an invalid range or after termination. Time and
    /// memory: O(1).
    [[nodiscard]]
    bool recordValidatedCheckpoint(ValidatedCheckpointProgress checkpoint);

    /// Atomically enters the failed terminal phase with owned diagnostic text.
    ///
    /// Returns false if already terminal. Time and memory are O(n) for a
    /// failure detail of length n.
    [[nodiscard]] bool markFailed(std::string detail);

    /// Copies a stable immutable view of the current tracker state.
    ///
    /// Time and memory are O(n) for an optional failure detail of length n.
    [[nodiscard]] ProgressSnapshot snapshot() const;

private:
    [[nodiscard]] bool isRunning() const noexcept;

    const ProgressWorkPlan plan_;
    const std::chrono::steady_clock::time_point startedAt_;

    std::atomic<ProgressPhase> phase_{ProgressPhase::initializing};
    std::atomic<std::uint64_t> completedTerms_{0};
    std::atomic<std::uint64_t> completedCheckpointBlocks_{0};
    std::atomic<std::uint64_t> activeTasks_{0};
    std::atomic<std::uint64_t> queuedTasks_{0};
    std::atomic<std::uint64_t> memoryBytes_{0};
    std::atomic<std::uint64_t> checkpointBytes_{0};
    std::atomic<std::uint64_t> storageResidentBytes_{0};
    std::atomic<std::uint64_t> storageStoredBytes_{0};
    std::atomic<std::uint64_t> storageChunkCount_{0};

    mutable std::mutex metadataMutex_;
    std::optional<std::uint32_t> currentMergeLevel_;
    std::optional<ValidatedCheckpointProgress> lastValidatedCheckpoint_;
    std::optional<std::string> failureDetail_;
    std::optional<std::chrono::steady_clock::time_point> finishedAt_;
};

} // namespace pi::progress
