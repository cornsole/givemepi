#include "checkpoint/CheckpointTypes.hpp"

#include <limits>
#include <stdexcept>


namespace pi::checkpoint
{

ComputationIdentity ComputationIdentity::fromPrecisionPlan(
    const chudnovsky::PrecisionPlan& plan
)
{
    if (plan.requestedDigits == 0 || plan.guardDigits == 0)
    {
        throw std::invalid_argument(
            "Checkpoint precision digits must be non-zero"
        );
    }

    if (plan.guardDigits
        > std::numeric_limits<std::uint64_t>::max() - plan.requestedDigits)
    {
        throw std::overflow_error(
            "Checkpoint working digit calculation overflow"
        );
    }

    if (plan.workingDigits != plan.requestedDigits + plan.guardDigits)
    {
        throw std::invalid_argument(
            "Checkpoint working digits do not match the precision plan"
        );
    }

    if (plan.termCount == 0)
    {
        throw std::invalid_argument(
            "Checkpoint term count must be non-zero"
        );
    }

    if constexpr (sizeof(std::size_t) > sizeof(std::uint64_t))
    {
        if (plan.termCount > std::numeric_limits<std::uint64_t>::max())
        {
            throw std::overflow_error(
                "Checkpoint term count does not fit the durable identity"
            );
        }
    }

    ComputationIdentity identity{
        AlgorithmId::chudnovskyBinarySplitting,
        arithmeticFormatVersion,
        binarySplittingFormulaVersion,
        plan.requestedDigits,
        plan.guardDigits,
        plan.workingDigits,
        static_cast<std::uint64_t>(plan.termCount)
    };

    identity.validate();
    return identity;
}


void ComputationIdentity::validate() const
{
    if (algorithm != AlgorithmId::chudnovskyBinarySplitting
        || arithmeticVersion != arithmeticFormatVersion
        || formulaVersion != binarySplittingFormulaVersion)
    {
        throw std::invalid_argument(
            "Unsupported checkpoint computation identity version"
        );
    }

    const chudnovsky::PrecisionPlan expected =
        chudnovsky::PrecisionPolicy::create(requestedDigits, guardDigits);

    if (workingDigits != expected.workingDigits
        || termCount != expected.termCount)
    {
        throw std::invalid_argument(
            "Checkpoint identity does not match its precision policy"
        );
    }
}


BlockLocation BlockLocation::create(
    std::uint64_t start,
    std::uint64_t end,
    std::uint32_t treeLevel,
    const ComputationIdentity& identity
)
{
    BlockLocation location{start, end, treeLevel};
    location.validate(identity);
    return location;
}


void BlockLocation::validate(const ComputationIdentity& identity) const
{
    if (start >= end)
    {
        throw std::invalid_argument(
            "Checkpoint block range must be non-empty"
        );
    }

    if (end > identity.termCount)
    {
        throw std::out_of_range(
            "Checkpoint block range exceeds the computation term count"
        );
    }

}

} // namespace pi::checkpoint
