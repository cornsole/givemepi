#include "chudnovsky/ChudnovskyCalculator.hpp"

#include "bigint/GMPInteger.hpp"
#include "binary/BinaryNode.hpp"
#include "scheduler/Scheduler.hpp"
#include "progress/ProgressTracker.hpp"

#include <chrono>
#include <limits>
#include <stdexcept>
#include <utility>


namespace pi::chudnovsky
{

namespace
{

using bigint::GMPInteger;
using binary::BinaryNode;
using Clock = std::chrono::steady_clock;


class TrackerSplitObserver final : public binary::BinarySplitObserver
{
public:
    explicit TrackerSplitObserver(progress::ProgressTracker* tracker) noexcept
        : tracker_(tracker)
    {
    }

    void termsCompleted(std::size_t count) noexcept override
    {
        if (tracker_ != nullptr)
        {
            static_cast<void>(tracker_->addCompletedTerms(count));
        }
    }

    void mergeLevelStarted(std::uint32_t level) noexcept override
    {
        if (tracker_ == nullptr)
        {
            return;
        }
        if (!merging_)
        {
            merging_ = tracker_->transitionTo(
                progress::ProgressPhase::merging
            );
        }
        if (merging_)
        {
            static_cast<void>(tracker_->setCurrentMergeLevel(level));
        }
    }

private:
    progress::ProgressTracker* tracker_;
    bool merging_ = false;
};


GMPInteger finalizeFixedPoint(
    const BinaryNode& node,
    const PrecisionPlan& precision
)
{
    if (precision.workingDigits
        > std::numeric_limits<std::uint64_t>::max() / 2)
    {
        throw std::overflow_error(
            "Fixed-point square-root scale exponent overflow"
        );
    }

    const std::uint64_t squareRootScaleExponent =
        precision.workingDigits * 2;

    GMPInteger squareRootScale;
    squareRootScale.setPowerOfTen(squareRootScaleExponent);
    squareRootScale.mul(GMPInteger(10005));
    squareRootScale.floorSquareRoot();

    GMPInteger scaledPi(squareRootScale);
    scaledPi.mul(GMPInteger(426880));
    scaledPi.mul(node.Q());
    scaledPi.floorDivide(node.T());

    GMPInteger roundingIncrement;
    roundingIncrement.setPowerOfTen(precision.guardDigits - 1);
    roundingIncrement.mul(GMPInteger(5));
    scaledPi.add(roundingIncrement);

    GMPInteger guardScale;
    guardScale.setPowerOfTen(precision.guardDigits);
    scaledPi.floorDivide(guardScale);

    return scaledPi;
}


std::string formatDecimal(
    const GMPInteger& scaledPi,
    const PrecisionPlan& precision
)
{
    std::string digits = scaledPi.toString();

    if (precision.requestedDigits
        > std::numeric_limits<std::size_t>::max() - 1)
    {
        throw std::overflow_error(
            "Requested decimal output does not fit size_t"
        );
    }

    const std::size_t expectedDigits =
        static_cast<std::size_t>(precision.requestedDigits) + 1;

    if (digits.size() != expectedDigits || digits.front() != '3')
    {
        throw std::runtime_error(
            "Final pi fixed-point value has an unexpected width"
        );
    }

    digits.insert(1, 1, '.');
    return digits;
}


PiCalculationResult makeResult(
    PrecisionPlan precision,
    BinaryNode node,
    std::chrono::nanoseconds splitTime,
    progress::ProgressTracker* progress,
    bool deferProgressCompletion
)
{
    if (progress != nullptr
        && !progress->transitionTo(progress::ProgressPhase::finalizing))
    {
        throw std::logic_error("Cannot enter progress finalization phase");
    }
    if (progress != nullptr)
    {
        static_cast<void>(progress->clearCurrentMergeLevel());
    }
    const auto finalizeStart = Clock::now();
    GMPInteger scaledPi = finalizeFixedPoint(node, precision);
    const auto finalizeEnd = Clock::now();

    std::string decimal = formatDecimal(scaledPi, precision);
    const auto formatEnd = Clock::now();

    PiCalculationResult result{
        precision,
        std::move(decimal),
        PiCalculationResult::Timings{
            splitTime,
            finalizeEnd - finalizeStart,
            formatEnd - finalizeEnd
        }
    };
    if (progress != nullptr && !deferProgressCompletion)
    {
        if (!progress->transitionTo(progress::ProgressPhase::writingOutput)
            || !progress->transitionTo(progress::ProgressPhase::completed))
        {
            throw std::logic_error("Cannot complete progress lifecycle");
        }
    }
    return result;
}


void beginProgress(
    progress::ProgressTracker* tracker,
    const PrecisionPlan& precision
)
{
    if (tracker == nullptr)
    {
        return;
    }
    const progress::ProgressSnapshot snapshot = tracker->snapshot();
    if (snapshot.targetDigits() != precision.requestedDigits
        || snapshot.totalTerms() != precision.termCount)
    {
        throw std::invalid_argument(
            "Progress tracker work plan does not match calculation precision"
        );
    }
    if (!tracker->transitionTo(progress::ProgressPhase::splitting))
    {
        throw std::logic_error("Cannot enter progress splitting phase");
    }
}


void completeTerms(
    progress::ProgressTracker* tracker,
    std::uint64_t totalTerms
)
{
    if (tracker == nullptr)
    {
        return;
    }
    const std::uint64_t completed = tracker->snapshot().completedTerms();
    if (completed > totalTerms
        || !tracker->addCompletedTerms(totalTerms - completed))
    {
        throw std::logic_error("Cannot publish completed calculation terms");
    }
    tracker->setTaskCounts(0, 0);
}


void failProgress(progress::ProgressTracker* tracker, const char* detail) noexcept
{
    if (tracker != nullptr)
    {
        try
        {
            static_cast<void>(tracker->markFailed(detail));
        }
        catch (...)
        {
        }
    }
}

} // namespace


PiCalculationResult ChudnovskyCalculator::calculateSequential(
    const PiCalculationRequest& request,
    progress::ProgressTracker* progress
)
{
    try
    {
        PrecisionPlan precision = PrecisionPolicy::create(
            request.digits,
            request.guardDigits
        );
        beginProgress(progress, precision);

        const auto splitStart = Clock::now();
        BinaryNode node = binary::BinarySplitter::splitSequential(
            0,
            precision.termCount
        );
        const auto splitEnd = Clock::now();
        completeTerms(progress, precision.termCount);

        return makeResult(
            precision,
            std::move(node),
            splitEnd - splitStart,
            progress,
            request.deferProgressCompletion
        );
    }
    catch (const std::exception& error)
    {
        failProgress(progress, error.what());
        throw;
    }
    catch (...)
    {
        failProgress(progress, "Unknown calculation failure");
        throw;
    }
}


PiCalculationResult ChudnovskyCalculator::calculateParallel(
    const PiCalculationRequest& request,
    scheduler::Scheduler& executor,
    const binary::ParallelSplitOptions& splitOptions,
    progress::ProgressTracker* progress
)
{
    try
    {
        PrecisionPlan precision = PrecisionPolicy::create(
            request.digits,
            request.guardDigits
        );
        beginProgress(progress, precision);

        const auto splitStart = Clock::now();
        TrackerSplitObserver observer(progress);
        BinaryNode node = binary::BinarySplitter::splitParallel(
            0,
            precision.termCount,
            executor,
            splitOptions,
            &observer
        );
        const auto splitEnd = Clock::now();
        completeTerms(progress, precision.termCount);

        return makeResult(
            precision,
            std::move(node),
            splitEnd - splitStart,
            progress,
            request.deferProgressCompletion
        );
    }
    catch (const std::exception& error)
    {
        failProgress(progress, error.what());
        throw;
    }
    catch (...)
    {
        failProgress(progress, "Unknown calculation failure");
        throw;
    }
}

} // namespace pi::chudnovsky
