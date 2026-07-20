#include "checkpoint/ModularPQTVerifier.hpp"

#include "binary/BinarySplitter.hpp"
#include "chudnovsky/PrecisionPolicy.hpp"

#include <array>
#include <cstdint>
#include <iostream>


using pi::bigint::GMPInteger;
using pi::binary::BinaryNode;
using pi::binary::BinarySplitter;
using pi::checkpoint::BlockLocation;
using pi::checkpoint::CheckpointBlock;
using pi::checkpoint::ComputationIdentity;
using pi::checkpoint::ModularPQTVerifier;
using pi::checkpoint::RejectionReason;
using pi::checkpoint::ValidationResult;
using pi::chudnovsky::PrecisionPolicy;


namespace
{

CheckpointBlock blockFor(
    const ComputationIdentity& identity,
    std::uint64_t start,
    std::uint64_t end,
    std::uint32_t treeLevel
)
{
    const BinaryNode node = BinarySplitter::splitSequential(
        static_cast<std::size_t>(start),
        static_cast<std::size_t>(end)
    );
    return CheckpointBlock{
        identity,
        BlockLocation::create(start, end, treeLevel, identity),
        node.P(),
        node.Q(),
        node.T()
    };
}


bool onlyModularMismatches(const ValidationResult& result)
{
    if (!result.isRejected() || result.diagnostics().empty()) return false;
    for (const auto& diagnostic : result.diagnostics())
    {
        if (diagnostic.reason != RejectionReason::modularMismatch)
            return false;
    }
    return true;
}

} // namespace


int main()
{
    const ComputationIdentity identity =
        ComputationIdentity::fromPrecisionPlan(PrecisionPolicy::create(1000));

    const std::array<CheckpointBlock, 4> validBlocks{
        blockFor(identity, 0, 1, 0),
        blockFor(identity, 1, 2, 0),
        blockFor(identity, 0, 10, 4),
        blockFor(identity, 10, 74, 6)
    };

    for (const auto& block : validBlocks)
    {
        if (!ModularPQTVerifier::validate(block).isAccepted())
        {
            std::cerr << "Correct P/Q/T block failed modular verification\n";
            return 1;
        }
    }

    CheckpointBlock wrongP = validBlocks[2];
    wrongP.p.add(GMPInteger(1));

    CheckpointBlock wrongQ = validBlocks[2];
    wrongQ.q.add(GMPInteger(1));

    CheckpointBlock wrongT = validBlocks[2];
    wrongT.t.add(GMPInteger(1));

    if (!onlyModularMismatches(ModularPQTVerifier::validate(wrongP))
        || !onlyModularMismatches(ModularPQTVerifier::validate(wrongQ))
        || !onlyModularMismatches(ModularPQTVerifier::validate(wrongT)))
    {
        std::cerr << "Incorrect P/Q/T value passed modular verification\n";
        return 1;
    }

    std::cout << "ModularPQTVerifier OK\n";
    return 0;
}
