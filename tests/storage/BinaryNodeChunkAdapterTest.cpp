#include "storage/BinaryNodeChunkAdapter.hpp"

#include "chudnovsky/PrecisionPolicy.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>

int main()
{
    using namespace pi::binary;
    using namespace pi::checkpoint;
    using namespace pi::chudnovsky;
    using namespace pi::storage;

    const auto computation = ComputationIdentity::fromPrecisionPlan(
        PrecisionPolicy::create(1000));
    BinaryNode original(8, 16);
    original.P() = pi::bigint::GMPInteger(123456);
    original.Q() = pi::bigint::GMPInteger(789);
    original.T() = pi::bigint::GMPInteger(42);
    original.T().negate();

    const Chunk chunk = BinaryNodeChunkAdapter::toChunk(
        original, computation, 3);
    assert(chunk.metadata.identity.computation == computation);
    const BlockLocation expectedLocation{8, 16, 3};
    assert(chunk.metadata.identity.location == expectedLocation);
    assert(chunk.p.compare(original.P()) == 0);
    assert(chunk.q.compare(original.Q()) == 0);
    assert(chunk.t.compare(original.T()) == 0);

    const BinaryNode restored = BinaryNodeChunkAdapter::toBinaryNode(chunk);
    assert(restored.start() == original.start());
    assert(restored.end() == original.end());
    assert(restored.P().compare(original.P()) == 0);
    assert(restored.Q().compare(original.Q()) == 0);
    assert(restored.T().compare(original.T()) == 0);

    bool rejected = false;
    try
    {
        static_cast<void>(BinaryNodeChunkAdapter::toChunk(
            BinaryNode(2, 2), computation, 0));
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    assert(rejected);

    std::cout << "BinaryNodeChunkAdapter OK\n";
    return 0;
}
