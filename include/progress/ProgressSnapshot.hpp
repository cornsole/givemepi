#pragma once

#include "progress/ProgressState.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>


namespace pi::progress
{

/// Term range and tree level of one durable checkpoint block.
struct CheckpointRangeProgress
{
    std::uint64_t startTerm;
    std::uint64_t endTerm;
    std::uint32_t treeLevel;

    [[nodiscard]]
    bool operator==(const CheckpointRangeProgress&) const noexcept = default;
};


/// Raw bounded work units of one pipeline phase.
///
/// Units are phase-specific (terms, weighted merge units, finalization steps,
/// bytes, verification units) and are only compared within their phase.
struct PhaseWorkUnits
{
    std::uint64_t total = 0;
    std::uint64_t completed = 0;

    [[nodiscard]]
    bool operator==(const PhaseWorkUnits&) const noexcept = default;
};


/// Normalized end-to-end cost of each phase, indexed by phaseIndex().
///
/// Costs share one dimensionless unit across phases so remaining work can be
/// summed. A zero cost removes the phase from the end-to-end estimate; only
/// work phases may carry a non-zero cost.
using ProgressPhaseCosts = std::array<std::uint64_t, PROGRESS_PHASE_COUNT>;


/// Raw authoritative values used to construct one progress snapshot.
struct ProgressSnapshotData
{
    ProgressPhase phase = ProgressPhase::initializing;
    std::uint64_t targetDigits = 0;
    std::uint64_t totalTerms = 0;
    std::uint64_t completedTerms = 0;
    std::uint64_t totalCheckpointBlocks = 0;
    /// Monotonic checkpoint work: accepted plus newly calculated blocks.
    std::uint64_t completedCheckpointBlocks = 0;
    /// Accepted blocks restored by resume; included in completed blocks but
    /// never newly completed work.
    std::uint64_t restoredCheckpointBlocks = 0;
    /// Blocks referenced by the current durable manifest; may decrease.
    std::uint64_t durableCheckpointBlocks = 0;
    std::optional<std::uint32_t> currentMergeLevel;
    std::uint64_t activeTasks = 0;
    std::uint64_t queuedTasks = 0;
    std::chrono::nanoseconds elapsed = std::chrono::nanoseconds::zero();
    /// Terms restored by resume; included in completedTerms but never newly
    /// completed work.
    std::uint64_t restoredTerms = 0;
    /// Units of every phase other than splitting, indexed by phaseIndex().
    /// Splitting work is the authoritative term counters, so its entry and
    /// those of non-work phases must stay zero.
    std::array<PhaseWorkUnits, PROGRESS_PHASE_COUNT> phaseUnits{};
    ProgressPhaseCosts phaseCosts{};
    /// Time spent in the current phase; zero for terminal phases.
    std::chrono::nanoseconds phaseElapsed = std::chrono::nanoseconds::zero();
    /// Completed units of the current phase when it was entered.
    std::uint64_t phaseBaselineUnits = 0;
    std::uint64_t memoryBytes = 0;
    /// Exact file bytes of blocks referenced by the current manifest.
    std::uint64_t checkpointBytes = 0;
    std::uint64_t storageResidentBytes = 0;
    std::uint64_t storageStoredBytes = 0;
    std::uint64_t storageChunkCount = 0;
    std::uint64_t storageQueuedWrites = 0;
    std::uint64_t storageActiveWrites = 0;
    std::uint64_t storageCompletedWrites = 0;
    std::uint64_t storageFailedWrites = 0;
    std::uint64_t storageQueuedReads = 0;
    std::uint64_t storageActiveReads = 0;
    std::uint64_t storageCompletedReads = 0;
    std::uint64_t storageFailedReads = 0;
    /// Last checkpoint accepted by resume validation.
    std::optional<CheckpointRangeProgress> lastValidatedCheckpoint;
    /// Last block newly written by a successfully published manifest.
    std::optional<CheckpointRangeProgress> lastCommittedCheckpoint;
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
    [[nodiscard]] std::uint64_t restoredCheckpointBlocks() const noexcept;
    [[nodiscard]] std::uint64_t durableCheckpointBlocks() const noexcept;
    [[nodiscard]] std::optional<std::uint32_t> currentMergeLevel() const noexcept;
    [[nodiscard]] std::uint64_t activeTasks() const noexcept;
    [[nodiscard]] std::uint64_t queuedTasks() const noexcept;
    [[nodiscard]] std::chrono::nanoseconds elapsed() const noexcept;
    [[nodiscard]] std::uint64_t restoredTerms() const noexcept;

    /// Returns raw units of a phase: term counters for splitting, declared
    /// phase units for other work phases, and zero for non-work phases.
    [[nodiscard]] PhaseWorkUnits phaseWork(ProgressPhase phase) const noexcept;

    /// Returns the normalized end-to-end cost planned for a phase.
    [[nodiscard]] std::uint64_t phaseCost(ProgressPhase phase) const noexcept;

    [[nodiscard]] std::chrono::nanoseconds phaseElapsed() const noexcept;
    [[nodiscard]] std::uint64_t phaseBaselineUnits() const noexcept;
    [[nodiscard]] std::uint64_t memoryBytes() const noexcept;
    [[nodiscard]] std::uint64_t checkpointBytes() const noexcept;
    [[nodiscard]] std::uint64_t storageResidentBytes() const noexcept;
    [[nodiscard]] std::uint64_t storageStoredBytes() const noexcept;
    [[nodiscard]] std::uint64_t storageChunkCount() const noexcept;
    [[nodiscard]] std::uint64_t storageQueuedWrites() const noexcept;
    [[nodiscard]] std::uint64_t storageActiveWrites() const noexcept;
    [[nodiscard]] std::uint64_t storageCompletedWrites() const noexcept;
    [[nodiscard]] std::uint64_t storageFailedWrites() const noexcept;
    [[nodiscard]] std::uint64_t storageQueuedReads() const noexcept;
    [[nodiscard]] std::uint64_t storageActiveReads() const noexcept;
    [[nodiscard]] std::uint64_t storageCompletedReads() const noexcept;
    [[nodiscard]] std::uint64_t storageFailedReads() const noexcept;

    [[nodiscard]]
    const std::optional<CheckpointRangeProgress>&
    lastValidatedCheckpoint() const noexcept;

    [[nodiscard]]
    const std::optional<CheckpointRangeProgress>&
    lastCommittedCheckpoint() const noexcept;

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
    std::uint64_t restoredCheckpointBlocks_;
    std::uint64_t durableCheckpointBlocks_;
    std::optional<std::uint32_t> currentMergeLevel_;
    std::uint64_t activeTasks_;
    std::uint64_t queuedTasks_;
    std::chrono::nanoseconds elapsed_;
    std::uint64_t restoredTerms_;
    std::array<PhaseWorkUnits, PROGRESS_PHASE_COUNT> phaseUnits_;
    ProgressPhaseCosts phaseCosts_;
    std::chrono::nanoseconds phaseElapsed_;
    std::uint64_t phaseBaselineUnits_;
    std::uint64_t memoryBytes_;
    std::uint64_t checkpointBytes_;
    std::uint64_t storageResidentBytes_;
    std::uint64_t storageStoredBytes_;
    std::uint64_t storageChunkCount_;
    std::uint64_t storageQueuedWrites_;
    std::uint64_t storageActiveWrites_;
    std::uint64_t storageCompletedWrites_;
    std::uint64_t storageFailedWrites_;
    std::uint64_t storageQueuedReads_;
    std::uint64_t storageActiveReads_;
    std::uint64_t storageCompletedReads_;
    std::uint64_t storageFailedReads_;
    std::optional<CheckpointRangeProgress> lastValidatedCheckpoint_;
    std::optional<CheckpointRangeProgress> lastCommittedCheckpoint_;
    std::optional<std::string> failureDetail_;
};

} // namespace pi::progress
