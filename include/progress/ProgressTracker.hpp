#pragma once

#include "progress/ProgressSnapshot.hpp"

#include <array>
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
    /// Normalized end-to-end cost of each work phase. Disabled or unplanned
    /// phases keep zero and never contribute to the total estimate.
    ProgressPhaseCosts phaseCosts{};
};


/// One coherent checkpoint publication after a durable manifest replacement.
struct CheckpointProgressUpdate
{
    /// Newly completed checkpoint work; clamped to the remaining fixed total.
    std::uint64_t completedBlocks;
    /// Entry count of the now-authoritative manifest.
    std::uint64_t durableBlocks;
    /// Exact file bytes of blocks referenced by that manifest.
    std::uint64_t checkpointBytes;
    /// Last block newly written by this publication; absent keeps the
    /// previously published value.
    std::optional<CheckpointRangeProgress> lastCommitted;
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
    /// Throws std::invalid_argument for zero digits or terms, or a non-zero
    /// cost on a non-work phase. Time complexity: O(1). Memory: O(1).
    explicit ProgressTracker(ProgressWorkPlan plan);

    ProgressTracker(const ProgressTracker&) = delete;
    ProgressTracker& operator=(const ProgressTracker&) = delete;

    /// Applies a legal, idempotent phase transition.
    ///
    /// Entering a different phase restarts the phase clock and records the
    /// phase's already completed units as its baseline, so phase rates count
    /// only work done inside the phase. Returns false for an illegal
    /// transition. Time and memory: O(1).
    [[nodiscard]] bool transitionTo(ProgressPhase next);

    /// Atomically adds completed terms without exceeding the fixed total.
    ///
    /// Returns false after termination or if the addition exceeds the total.
    /// Lock-free when the platform provides lock-free 64-bit atomics.
    [[nodiscard]] bool addCompletedTerms(std::uint64_t count) noexcept;

    /// Publishes resume-restored terms and checkpoint blocks once.
    ///
    /// Restored work counts as completed but never as newly completed work,
    /// so it cannot inflate observed rates. Valid only before splitting
    /// starts and before any term or checkpoint work was published. The
    /// durable gauge and bytes are replaced. Returns false without mutation
    /// otherwise or when a count exceeds its fixed total. Time/memory: O(1).
    [[nodiscard]]
    bool restoreCompletedWork(
        std::uint64_t terms,
        std::uint64_t checkpointBlocks,
        std::uint64_t checkpointBytes
    );

    /// Declares the raw unit total of a current or future work phase once.
    ///
    /// Splitting is measured by terms and cannot be declared. A zero total
    /// removes the phase from the end-to-end plan by clearing its cost.
    /// Returns false after termination, for splitting or a non-work phase, a
    /// phase already left behind, or a repeated declaration. Time and
    /// memory: O(1).
    [[nodiscard]]
    bool declarePhaseUnits(ProgressPhase phase, std::uint64_t total);

    /// Atomically adds completed units to a declared non-splitting phase.
    ///
    /// Returns false after termination, for an undeclared, splitting, or
    /// non-work phase, or if the addition exceeds the declared total. Never
    /// formats, allocates, or blocks; lock-free with lock-free 64-bit atomics.
    [[nodiscard]]
    bool addPhaseUnits(ProgressPhase phase, std::uint64_t count) noexcept;

    /// Publishes checkpoint work, durable gauge, bytes, and last commit as one
    /// coherent update on the low-frequency coordinating-thread path.
    ///
    /// Completed work only increases and never exceeds the fixed total; the
    /// durable count and bytes are replaced. Returns false without mutation
    /// after termination or for an empty or out-of-range committed range.
    /// Time and memory: O(1).
    [[nodiscard]]
    bool publishCheckpointProgress(const CheckpointProgressUpdate& update);

    /// Publishes current scheduler task counts. Time and memory: O(1).
    void setTaskCounts(
        std::uint64_t activeTasks,
        std::uint64_t queuedTasks
    ) noexcept;

    /// Publishes current process memory consumption. Time and memory: O(1).
    void setMemoryBytes(std::uint64_t bytes) noexcept;

    /// Publishes storage resident bytes, durable bytes, and indexed chunk count.
    void setStorageProgress(
        std::uint64_t residentBytes,
        std::uint64_t storedBytes,
        std::uint64_t chunkCount
    ) noexcept;

    void setStorageIoProgress(
        std::uint64_t queuedWrites,
        std::uint64_t activeWrites,
        std::uint64_t completedWrites,
        std::uint64_t failedWrites,
        std::uint64_t queuedReads,
        std::uint64_t activeReads,
        std::uint64_t completedReads,
        std::uint64_t failedReads
    ) noexcept;

    /// Publishes the current merge level on the low-frequency metadata path.
    ///
    /// Returns false after termination. Time and memory: O(1).
    [[nodiscard]] bool setCurrentMergeLevel(std::uint32_t level);

    /// Clears the merge level when the computation leaves the merge phase.
    ///
    /// Returns false after termination. Time and memory: O(1).
    [[nodiscard]] bool clearCurrentMergeLevel();

    /// Records the most recently resume-validated checkpoint range.
    ///
    /// Returns false for an invalid range or after termination. Time and
    /// memory: O(1).
    [[nodiscard]]
    bool recordValidatedCheckpoint(CheckpointRangeProgress checkpoint);

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
    [[nodiscard]]
    std::uint64_t completedUnitsOf(ProgressPhase phase) const noexcept;

    const ProgressWorkPlan plan_;
    const std::chrono::steady_clock::time_point startedAt_;

    std::atomic<ProgressPhase> phase_{ProgressPhase::initializing};
    std::atomic<std::uint64_t> completedTerms_{0};
    std::atomic<std::uint64_t> activeTasks_{0};
    std::atomic<std::uint64_t> queuedTasks_{0};
    std::atomic<std::uint64_t> memoryBytes_{0};
    std::atomic<std::uint64_t> storageResidentBytes_{0};
    std::atomic<std::uint64_t> storageStoredBytes_{0};
    std::atomic<std::uint64_t> storageChunkCount_{0};
    std::atomic<std::uint64_t> storageQueuedWrites_{0};
    std::atomic<std::uint64_t> storageActiveWrites_{0};
    std::atomic<std::uint64_t> storageCompletedWrites_{0};
    std::atomic<std::uint64_t> storageFailedWrites_{0};
    std::atomic<std::uint64_t> storageQueuedReads_{0};
    std::atomic<std::uint64_t> storageActiveReads_{0};
    std::atomic<std::uint64_t> storageCompletedReads_{0};
    std::atomic<std::uint64_t> storageFailedReads_{0};
    std::array<std::atomic<std::uint64_t>, PROGRESS_PHASE_COUNT>
        phaseTotals_{};
    std::array<std::atomic<std::uint64_t>, PROGRESS_PHASE_COUNT>
        phaseCompleted_{};

    mutable std::mutex metadataMutex_;
    std::optional<std::uint32_t> currentMergeLevel_;
    std::uint64_t completedCheckpointBlocks_ = 0;
    std::uint64_t restoredCheckpointBlocks_ = 0;
    std::uint64_t restoredTerms_ = 0;
    std::array<bool, PROGRESS_PHASE_COUNT> phaseDeclared_{};
    ProgressPhaseCosts phaseCosts_{};
    std::chrono::steady_clock::time_point phaseStartedAt_;
    std::uint64_t phaseBaselineUnits_ = 0;
    std::uint64_t durableCheckpointBlocks_ = 0;
    std::uint64_t checkpointBytes_ = 0;
    std::optional<CheckpointRangeProgress> lastValidatedCheckpoint_;
    std::optional<CheckpointRangeProgress> lastCommittedCheckpoint_;
    std::optional<std::string> failureDetail_;
    std::optional<std::chrono::steady_clock::time_point> finishedAt_;
};

} // namespace pi::progress
