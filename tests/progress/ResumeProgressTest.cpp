#include "progress/ResumeProgress.hpp"

#include "checkpoint/CheckpointTypes.hpp"
#include "chudnovsky/PrecisionPolicy.hpp"

#include <cassert>
#include <filesystem>
#include <stdexcept>
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
    const pi::checkpoint::BlockLocation location =
        pi::checkpoint::BlockLocation::create(start, end, level, identity);
    return pi::checkpoint::AcceptedCheckpoint{
        std::filesystem::path("block.checkpoint"),
        fileSize,
        pi::checkpoint::CheckpointBlock{
            identity,
            location,
            pi::bigint::GMPInteger(1),
            pi::bigint::GMPInteger(1),
            pi::bigint::GMPInteger(1)
        },
        pi::checkpoint::ManifestEntry{
            location,
            "block.checkpoint",
            0,
            pi::checkpoint::ChecksumAlgorithm::crc32c,
            0,
            pi::checkpoint::CompletionState::complete
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
    using pi::progress::CheckpointProgressUpdate;
    using pi::progress::CheckpointRangeProgress;

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
    assert(restored.durableCheckpointBlocks() == 2);
    assert(restored.checkpointBytes() == 500);
    const CheckpointRangeProgress expectedLast{20, 30, 2};
    assert(restored.lastValidatedCheckpoint() == expectedLast);
    assert(!restored.lastCommittedCheckpoint().has_value());

    CheckpointResumePlan empty;
    ProgressTracker emptyTracker(ProgressWorkPlan{1'000, 74, 2});
    assert(ResumeProgressRestorer::restore(empty, emptyTracker));
    assert(emptyTracker.snapshot().completedTerms() == 0);
    assert(emptyTracker.snapshot().durableCheckpointBlocks() == 0);
    assert(emptyTracker.snapshot().checkpointBytes() == 0);

    CheckpointResumePlan overlap;
    overlap.acceptedBlocks.push_back(accepted(identity, 0, 20, 1, 100));
    overlap.acceptedBlocks.push_back(accepted(identity, 10, 30, 1, 100));
    ProgressTracker overlapTracker(ProgressWorkPlan{1'000, 74, 2});
    assert(!ResumeProgressRestorer::restore(overlap, overlapTracker));
    assert(overlapTracker.snapshot().completedTerms() == 0);
    assert(overlapTracker.snapshot().durableCheckpointBlocks() == 0);
    assert(overlapTracker.snapshot().checkpointBytes() == 0);

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

    ProgressTracker published(ProgressWorkPlan{1'000, 74, 2});
    assert(published.publishCheckpointProgress(CheckpointProgressUpdate{
        0, 1, 100, std::nullopt
    }));
    assert(!ResumeProgressRestorer::restore(plan, published));
    assert(published.snapshot().completedTerms() == 0);
    assert(published.snapshot().durableCheckpointBlocks() == 1);

    ProgressTracker terminal(ProgressWorkPlan{1'000, 74, 2});
    assert(terminal.markFailed("stopped"));
    assert(!ResumeProgressRestorer::restore(plan, terminal));

    using pi::binary::ParallelSplitOptions;
    using pi::progress::planCheckpointBlocks;

    CheckpointResumePlan resumeOnlyEmpty;
    resumeOnlyEmpty.rangesToRecalculate.push_back({0, 74});
    const std::uint64_t emptyTotal = planCheckpointBlocks(
        resumeOnlyEmpty,
        ParallelSplitOptions{32, 4},
        8,
        false
    );
    assert(emptyTotal == 0);
    ProgressTracker resumeOnlyEmptyTracker(
        ProgressWorkPlan{1'000, 74, emptyTotal}
    );
    assert(resumeOnlyEmptyTracker.transitionTo(
        ProgressPhase::validatingCheckpoints));
    assert(ResumeProgressRestorer::restore(
        resumeOnlyEmpty,
        resumeOnlyEmptyTracker
    ));
    assert(resumeOnlyEmptyTracker.snapshot().totalCheckpointBlocks() == 0);
    assert(resumeOnlyEmptyTracker.snapshot().completedCheckpointBlocks() == 0);

    CheckpointResumePlan resumeOnlyAccepted;
    resumeOnlyAccepted.acceptedBlocks.push_back(
        accepted(identity, 0, 10, 1, 200)
    );
    resumeOnlyAccepted.acceptedBlocks.push_back(
        accepted(identity, 20, 30, 2, 300)
    );
    resumeOnlyAccepted.rangesToRecalculate.push_back({10, 20});
    resumeOnlyAccepted.rangesToRecalculate.push_back({30, 74});
    const std::uint64_t acceptedTotal = planCheckpointBlocks(
        resumeOnlyAccepted,
        ParallelSplitOptions{4, 2},
        3,
        false
    );
    assert(acceptedTotal == 2);
    ProgressTracker resumeOnlyAcceptedTracker(
        ProgressWorkPlan{1'000, 74, acceptedTotal}
    );
    assert(resumeOnlyAcceptedTracker.transitionTo(
        ProgressPhase::validatingCheckpoints));
    assert(ResumeProgressRestorer::restore(
        resumeOnlyAccepted,
        resumeOnlyAcceptedTracker
    ));
    assert(resumeOnlyAcceptedTracker.snapshot().completedCheckpointBlocks()
        == 2);
    assert(resumeOnlyAcceptedTracker.snapshot().durableCheckpointBlocks()
        == 2);

    // [10,20) needs 3 leaves at cutoff 4; [30,74) needs 11, capped at 3 * 2.
    assert(planCheckpointBlocks(
        resumeOnlyAccepted,
        ParallelSplitOptions{4, 2},
        3,
        true
    ) == 2 + 3 + 6);

    CheckpointResumePlan fresh;
    fresh.rangesToRecalculate.push_back({0, 74});
    assert(planCheckpointBlocks(fresh, ParallelSplitOptions{32, 4}, 8, true)
        == 3);
    assert(planCheckpointBlocks(fresh, ParallelSplitOptions{32, 4}, 8, false)
        == 0);

    assert(planCheckpointBlocks(fresh, ParallelSplitOptions{0, 4}, 8, false)
        == 0);
    bool rejectedOptions = false;
    try
    {
        static_cast<void>(planCheckpointBlocks(
            fresh,
            ParallelSplitOptions{0, 4},
            8,
            true
        ));
    }
    catch (const std::invalid_argument&)
    {
        rejectedOptions = true;
    }
    assert(rejectedOptions);

    return 0;
}
