#include "checkpoint/CheckpointTypes.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>


using pi::checkpoint::AlgorithmId;
using pi::checkpoint::BlockLocation;
using pi::checkpoint::ComputationIdentity;
using pi::checkpoint::ExecutionProvenance;
using pi::chudnovsky::PrecisionPlan;
using pi::chudnovsky::PrecisionPolicy;


namespace
{

template<typename Exception, typename Function>
bool throws(Function&& function)
{
    try
    {
        function();
    }
    catch (const Exception&)
    {
        return true;
    }

    return false;
}

} // namespace


int main()
{
    const PrecisionPlan plan = PrecisionPolicy::create(1000, 32);
    const ComputationIdentity identity =
        ComputationIdentity::fromPrecisionPlan(plan);

    if (identity.algorithm != AlgorithmId::chudnovskyBinarySplitting
        || identity.arithmeticVersion != 1
        || identity.formulaVersion != 1
        || identity.requestedDigits != 1000
        || identity.guardDigits != 32
        || identity.workingDigits != 1032
        || identity.termCount != 74)
    {
        std::cerr << "Computation identity mismatch\n";
        return 1;
    }

    const BlockLocation location = BlockLocation::create(10, 20, 3, identity);

    if (location != BlockLocation{10, 20, 3})
    {
        std::cerr << "Block location mismatch\n";
        return 1;
    }

    if (!throws<std::invalid_argument>([&identity]() {
            static_cast<void>(BlockLocation::create(10, 10, 0, identity));
        })
        || !throws<std::invalid_argument>([&identity]() {
            static_cast<void>(BlockLocation::create(20, 10, 0, identity));
        })
        || !throws<std::out_of_range>([&identity]() {
            static_cast<void>(BlockLocation::create(0, 75, 0, identity));
        }))
    {
        std::cerr << "Invalid block location was accepted\n";
        return 1;
    }

    PrecisionPlan invalidPlan = plan;
    invalidPlan.workingDigits += 1;

    if (!throws<std::invalid_argument>([&invalidPlan]() {
            static_cast<void>(
                ComputationIdentity::fromPrecisionPlan(invalidPlan)
            );
        }))
    {
        std::cerr << "Inconsistent precision plan was accepted\n";
        return 1;
    }

    const ExecutionProvenance firstRun{4, 32, 2, 1024};
    const ExecutionProvenance secondRun{16, 128, 8, 4096};
    const ComputationIdentity sameIdentity =
        ComputationIdentity::fromPrecisionPlan(plan);

    if (firstRun == secondRun || identity != sameIdentity)
    {
        std::cerr << "Execution provenance affected computation identity\n";
        return 1;
    }

    std::cout << "CheckpointTypes OK\n";
    return 0;
}
