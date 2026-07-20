#include "progress/ResumeProgress.hpp"

#include "checkpoint/CheckpointTypes.hpp"
#include "chudnovsky/PrecisionPolicy.hpp"

#include <cassert>
#include <filesystem>
#include <utility>


namespace
{

pi::checkpoint::AcceptedCheckpoint accepted(
    const pi::checkpoint::ComputationIdentity& identity,
    std::uint64_t start,
    std::uint64_t end,
    std::uint32_t level,
    std::uint64_t fileSize
)
{
    return pi::checkpoint::AcceptedCheckpoint{
        std::filesystem::path("block.checkpoint"),
        fileSize,
        pi::checkpoint::CheckpointBlock{
            identity,
            pi::checkpoint::BlockLocation::create(
                start, end, level, identity
            ),
            pi::bigint::GMPInteger(1),
            pi::bigint::GMPInteger(1),
            pi::bigint::GMPInteger(1)
        }
    };
}

} // namespace


int main()
{
    using pi::checkpoint::CheckpointResumePlan;
    using pi::checkpoint::ComputationIdentity;
    using pi::chudnovsky::PrecisionPolicy;
    using pi::progress::ProgressPhase;
    using pi::progress::ProgressTracker;
    using pi::progress::ProgressWorkPlan;
    using pi::progress::ResumeProgressRestorer;
    using pi::progress::ValidatedCheckpointProgress;

    const ComputationIdentity identity =
        ComputationIdentity::fromPrecisionPlan(PrecisionPolicy::create(1'000));

    CheckpointResumePlan plan;
    plan.acceptedBlocks.push_back(accepted(identity, 20, 30, 2, 300));
    plan.acceptedBlocks.push_back(accepted(identity, 0, 10, 1, 200));

    ProgressTracker tracker(ProgressWorkPlan{1'000, 74, 2});
    assert(tracker.transitionTo(ProgressPhase::validatingCheckpoints));
    assert(ResumeProgressRestorer::restore(plan, tracker));

    const auto restored = tracker.snapshot();
    assert(restored.completedTerms() == 20);
    assert(restored.completedCheckpointBlocks() == 2);
    assert(restored.checkpointBytes() == 500);
    const ValidatedCheckpointProgress expectedLast{20, 30, 2};
    assert(restored.lastValidatedCheckpoint() == expectedLast);

    CheckpointResumePlan empty;
    ProgressTracker emptyTracker(ProgressWorkPlan{1'000, 74, 2});
    assert(ResumeProgressRestorer::restore(empty, emptyTracker));
    assert(emptyTracker.snapshot().completedTerms() == 0);

    CheckpointResumePlan overlap;
    overlap.acceptedBlocks.push_back(accepted(identity, 0, 20, 1, 100));
    overlap.acceptedBlocks.push_back(accepted(identity, 10, 30, 1, 100));
    ProgressTracker overlapTracker(ProgressWorkPlan{1'000, 74, 2});
    assert(!ResumeProgressRestorer::restore(overlap, overlapTracker));
    assert(overlapTracker.snapshot().completedTerms() == 0);

    CheckpointResumePlan outOfRange;
    outOfRange.acceptedBlocks.push_back(accepted(identity, 70, 74, 1, 100));
    outOfRange.acceptedBlocks.front().block.location.end = 75;
    ProgressTracker rangeTracker(ProgressWorkPlan{1'000, 74, 1});
    assert(!ResumeProgressRestorer::restore(outOfRange, rangeTracker));
    assert(rangeTracker.snapshot().completedTerms() == 0);

    ProgressTracker tooSmall(ProgressWorkPlan{1'000, 74, 1});
    assert(!ResumeProgressRestorer::restore(plan, tooSmall));
    assert(tooSmall.snapshot().completedTerms() == 0);

    const ComputationIdentity otherIdentity =
        ComputationIdentity::fromPrecisionPlan(PrecisionPolicy::create(100));
    CheckpointResumePlan incompatible;
    incompatible.acceptedBlocks.push_back(
        accepted(otherIdentity, 0, 8, 0, 100)
    );
    ProgressTracker identityTracker(ProgressWorkPlan{1'000, 74, 1});
    assert(!ResumeProgressRestorer::restore(incompatible, identityTracker));
    assert(identityTracker.snapshot().completedTerms() == 0);

    ProgressTracker used(ProgressWorkPlan{1'000, 74, 2});
    assert(used.addCompletedTerms(1));
    assert(!ResumeProgressRestorer::restore(plan, used));
    assert(used.snapshot().completedTerms() == 1);

    ProgressTracker terminal(ProgressWorkPlan{1'000, 74, 2});
    assert(terminal.markFailed("stopped"));
    assert(!ResumeProgressRestorer::restore(plan, terminal));

    return 0;
}
