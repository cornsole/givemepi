#pragma once

#include "progress/ProgressState.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>


namespace pi::progress
{

/// Range metadata for the most recently validated checkpoint.
struct ValidatedCheckpointProgress
{
    std::uint64_t startTerm;
    std::uint64_t endTerm;
    std::uint32_t treeLevel;

    [[nodiscard]]
    bool operator==(const ValidatedCheckpointProgress&) const noexcept = default;
};


/// Raw authoritative values used to construct one progress snapshot.
struct ProgressSnapshotData
{
    ProgressPhase phase = ProgressPhase::initializing;
    std::uint64_t targetDigits = 0;
    std::uint64_t totalTerms = 0;
    std::uint64_t completedTerms = 0;
    std::uint64_t totalCheckpointBlocks = 0;
    std::uint64_t completedCheckpointBlocks = 0;
    std::optional<std::uint32_t> currentMergeLevel;
    std::uint64_t activeTasks = 0;
    std::uint64_t queuedTasks = 0;
    std::chrono::nanoseconds elapsed = std::chrono::nanoseconds::zero();
    std::uint64_t memoryBytes = 0;
    std::uint64_t checkpointBytes = 0;
    std::uint64_t storageResidentBytes = 0;
    std::uint64_t storageStoredBytes = 0;
    std::uint64_t storageChunkCount = 0;
    std::optional<ValidatedCheckpointProgress> lastValidatedCheckpoint;
    std::optional<std::string> failureDetail;
};


/// Immutable, presentation-independent view of computation progress.
///
/// The snapshot owns all variable-length data and exposes no mutators. Raw
/// counters are authoritative; percentage, throughput, and ETA are deliberately
/// derived outside this type.
///
/// Construction validates range and terminal-state invariants in O(1) time and
/// O(n) memory for copying an optional failure detail of length n. Accessors use
/// O(1) time and memory.
class ProgressSnapshot
{
public:
    /// Constructs a snapshot by taking ownership of raw progress data.
    explicit ProgressSnapshot(ProgressSnapshotData data);

    [[nodiscard]] std::uint32_t schemaVersion() const noexcept;
    [[nodiscard]] ProgressPhase phase() const noexcept;
    [[nodiscard]] ProgressTerminalState terminalState() const noexcept;
    [[nodiscard]] std::uint64_t targetDigits() const noexcept;
    [[nodiscard]] std::uint64_t totalTerms() const noexcept;
    [[nodiscard]] std::uint64_t completedTerms() const noexcept;
    [[nodiscard]] std::uint64_t totalCheckpointBlocks() const noexcept;
    [[nodiscard]] std::uint64_t completedCheckpointBlocks() const noexcept;
    [[nodiscard]] std::optional<std::uint32_t> currentMergeLevel() const noexcept;
    [[nodiscard]] std::uint64_t activeTasks() const noexcept;
    [[nodiscard]] std::uint64_t queuedTasks() const noexcept;
    [[nodiscard]] std::chrono::nanoseconds elapsed() const noexcept;
    [[nodiscard]] std::uint64_t memoryBytes() const noexcept;
    [[nodiscard]] std::uint64_t checkpointBytes() const noexcept;
    [[nodiscard]] std::uint64_t storageResidentBytes() const noexcept;
    [[nodiscard]] std::uint64_t storageStoredBytes() const noexcept;
    [[nodiscard]] std::uint64_t storageChunkCount() const noexcept;

    [[nodiscard]]
    const std::optional<ValidatedCheckpointProgress>&
    lastValidatedCheckpoint() const noexcept;

    [[nodiscard]]
    const std::optional<std::string>& failureDetail() const noexcept;

    [[nodiscard]]
    bool operator==(const ProgressSnapshot&) const noexcept = default;

private:
    ProgressPhase phase_;
    std::uint64_t targetDigits_;
    std::uint64_t totalTerms_;
    std::uint64_t completedTerms_;
    std::uint64_t totalCheckpointBlocks_;
    std::uint64_t completedCheckpointBlocks_;
    std::optional<std::uint32_t> currentMergeLevel_;
    std::uint64_t activeTasks_;
    std::uint64_t queuedTasks_;
    std::chrono::nanoseconds elapsed_;
    std::uint64_t memoryBytes_;
    std::uint64_t checkpointBytes_;
    std::uint64_t storageResidentBytes_;
    std::uint64_t storageStoredBytes_;
    std::uint64_t storageChunkCount_;
    std::optional<ValidatedCheckpointProgress> lastValidatedCheckpoint_;
    std::optional<std::string> failureDetail_;
};

} // namespace pi::progress
