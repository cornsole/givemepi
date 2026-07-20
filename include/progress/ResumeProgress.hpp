#pragma once

#include "checkpoint/CheckpointResumePlanner.hpp"
#include "progress/ProgressTracker.hpp"


namespace pi::progress
{

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
