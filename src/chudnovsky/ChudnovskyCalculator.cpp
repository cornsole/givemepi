#include "chudnovsky/ChudnovskyCalculator.hpp"

#include "bigint/GMPInteger.hpp"
#include "binary/BinaryNode.hpp"
#include "scheduler/Scheduler.hpp"

#include <limits>
#include <stdexcept>
#include <utility>


namespace pi::chudnovsky
{

namespace
{

using bigint::GMPInteger;
using binary::BinaryNode;


std::string finalizeDecimal(
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
    BinaryNode node
)
{
    std::string decimal = finalizeDecimal(node, precision);

    return PiCalculationResult{
        precision,
        std::move(decimal)
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

    BinaryNode node = binary::BinarySplitter::splitSequential(
        0,
        precision.termCount
    );

    return makeResult(
        precision,
        std::move(node)
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

    BinaryNode node = binary::BinarySplitter::splitParallel(
        0,
        precision.termCount,
        executor,
        splitOptions
    );

    return makeResult(
        precision,
        std::move(node)
    );
}

} // namespace pi::chudnovsky
