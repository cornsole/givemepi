#pragma once

#include "binary/BinarySplitter.hpp"
#include "checkpoint/CheckpointResumePlanner.hpp"
#include "progress/ProgressTracker.hpp"

#include <cstddef>
#include <cstdint>


namespace pi::progress
{

/// Plans the checkpoint-block total reported by a CLI run.
///
/// Verified accepted resume blocks always count, because they are restored
/// as completed durable work. Leaf blocks for recalculated ranges count only
/// when checkpoint writing is enabled; otherwise no checkpoint can ever be
/// produced for them. Recalculated ranges contribute the parallel splitter's
/// leaf count, capped at workerCount * tasksPerWorker per range.
///
/// Input: a resume plan, split options with non-zero cutoff and tasks per
/// worker, the worker count, and whether checkpoint writing is enabled.
/// Output: the planned checkpoint-block total, saturating at UINT64_MAX.
/// Throws std::invalid_argument for zero cutoff or tasks per worker when
/// recalculated work must be planned.
/// Time complexity: O(r) for r recalculated ranges. Memory complexity: O(1).
[[nodiscard]]
std::uint64_t planCheckpointBlocks(
    const checkpoint::CheckpointResumePlan& plan,
    const binary::ParallelSplitOptions& options,
    std::size_t workerCount,
    bool checkpointWritingEnabled
);


/// Restores raw progress exclusively from PR-0022 accepted checkpoints.
class ResumeProgressRestorer
{
public:
    /// Validates and applies accepted checkpoint metadata to a fresh tracker.
    ///
    /// Rejected blocks, temporary files, diagnostics, and persisted display
    /// percentages are intentionally ignored. Accepted ranges must be
    /// non-overlapping, within the tracker's term count, and fit its configured
    /// checkpoint-block total. The tracker must not already contain completed
    /// work.
    ///
    /// Input: a verified resume plan and the tracker to initialize.
    /// Output: true after complete restoration; false without tracker mutation
    /// when any precondition or accepted-range invariant fails.
    /// Time complexity: O(n log n) for n accepted blocks.
    /// Memory complexity: O(n).
    [[nodiscard]]
    static bool restore(
        const checkpoint::CheckpointResumePlan& plan,
        ProgressTracker& tracker
    );
};

} // namespace pi::progress
