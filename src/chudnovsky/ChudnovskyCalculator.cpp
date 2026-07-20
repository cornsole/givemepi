#include "chudnovsky/ChudnovskyCalculator.hpp"

#include "bigint/GMPInteger.hpp"
#include "binary/BinaryNode.hpp"
#include "scheduler/Scheduler.hpp"

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
    std::chrono::nanoseconds splitTime
)
{
    const auto finalizeStart = Clock::now();
    GMPInteger scaledPi = finalizeFixedPoint(node, precision);
    const auto finalizeEnd = Clock::now();

    std::string decimal = formatDecimal(scaledPi, precision);
    const auto formatEnd = Clock::now();

    return PiCalculationResult{
        precision,
        std::move(decimal),
        PiCalculationResult::Timings{
            splitTime,
            finalizeEnd - finalizeStart,
            formatEnd - finalizeEnd
        }
    };
}

} // namespace


PiCalculationResult ChudnovskyCalculator::calculateSequential(
    const PiCalculationRequest& request
)
{
    PrecisionPlan precision = PrecisionPolicy::create(
        request.digits,
        request.guardDigits
    );

    const auto splitStart = Clock::now();
    BinaryNode node = binary::BinarySplitter::splitSequential(
        0,
        precision.termCount
    );
    const auto splitEnd = Clock::now();

    return makeResult(
        precision,
        std::move(node),
        splitEnd - splitStart
    );
}


PiCalculationResult ChudnovskyCalculator::calculateParallel(
    const PiCalculationRequest& request,
    scheduler::Scheduler& executor,
    const binary::ParallelSplitOptions& splitOptions
)
{
    PrecisionPlan precision = PrecisionPolicy::create(
        request.digits,
        request.guardDigits
    );

    const auto splitStart = Clock::now();
    BinaryNode node = binary::BinarySplitter::splitParallel(
        0,
        precision.termCount,
        executor,
        splitOptions
    );
    const auto splitEnd = Clock::now();

    return makeResult(
        precision,
        std::move(node),
        splitEnd - splitStart
    );
}

} // namespace pi::chudnovsky
