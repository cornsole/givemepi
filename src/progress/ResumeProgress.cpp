#include "progress/ResumeProgress.hpp"

#include <algorithm>
#include <limits>
#include <vector>


namespace pi::progress
{
namespace
{

struct AcceptedMetadata
{
    checkpoint::BlockLocation location;
    std::uint64_t fileSize;
};

} // namespace


bool ResumeProgressRestorer::restore(
    const checkpoint::CheckpointResumePlan& plan,
    ProgressTracker& tracker
)
{
    const ProgressSnapshot initial = tracker.snapshot();
    if (initial.completedTerms() != 0
        || initial.completedCheckpointBlocks() != 0
        || initial.checkpointBytes() != 0
        || initial.lastValidatedCheckpoint().has_value()
        || initial.terminalState() != ProgressTerminalState::running
        || plan.acceptedBlocks.size()
            > initial.totalCheckpointBlocks())
    {
        return false;
    }

    std::vector<AcceptedMetadata> accepted;
    accepted.reserve(plan.acceptedBlocks.size());
    for (const auto& checkpoint : plan.acceptedBlocks)
    {
        if (checkpoint.block.identity.requestedDigits
                != initial.targetDigits()
            || checkpoint.block.identity.termCount != initial.totalTerms())
        {
            return false;
        }
        accepted.push_back(AcceptedMetadata{
            checkpoint.block.location,
            checkpoint.fileSize
        });
    }
    std::sort(accepted.begin(), accepted.end(), [](const auto& left, const auto& right) {
        if (left.location.start != right.location.start)
        {
            return left.location.start < right.location.start;
        }
        return left.location.end < right.location.end;
    });

    std::uint64_t completedTerms = 0;
    std::uint64_t checkpointBytes = 0;
    std::uint64_t previousEnd = 0;
    bool hasPrevious = false;
    for (const AcceptedMetadata& metadata : accepted)
    {
        const auto& location = metadata.location;
        if (location.start >= location.end
            || location.end > initial.totalTerms()
            || (hasPrevious && location.start < previousEnd))
        {
            return false;
        }

        const std::uint64_t length = location.end - location.start;
        if (completedTerms > initial.totalTerms() - length
            || checkpointBytes
                > std::numeric_limits<std::uint64_t>::max()
                    - metadata.fileSize)
        {
            return false;
        }
        completedTerms += length;
        checkpointBytes += metadata.fileSize;
        previousEnd = location.end;
        hasPrevious = true;
    }

    if (!tracker.addCompletedTerms(completedTerms)
        || !tracker.addCompletedCheckpointBlocks(accepted.size()))
    {
        return false;
    }
    tracker.setCheckpointBytes(checkpointBytes);

    if (!accepted.empty())
    {
        const auto& last = accepted.back().location;
        if (!tracker.recordValidatedCheckpoint(
                ValidatedCheckpointProgress{
                    last.start,
                    last.end,
                    last.treeLevel
                }))
        {
            return false;
        }
    }
    return true;
}

} // namespace pi::progress
