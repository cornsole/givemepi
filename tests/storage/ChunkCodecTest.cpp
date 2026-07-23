#include "chudnovsky/PrecisionPolicy.hpp"
#include "storage/ChunkCodec.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>


using pi::bigint::GMPInteger;
using pi::checkpoint::BlockLocation;
using pi::checkpoint::ComputationIdentity;
using pi::chudnovsky::PrecisionPolicy;
using pi::storage::Chunk;
using pi::storage::ChunkCodec;
using pi::storage::ChunkIdentity;
using pi::storage::ChunkMetadata;


namespace
{

bool same(const Chunk& left, const Chunk& right)
{
    return left.metadata == right.metadata
        && left.p.compare(right.p) == 0
        && left.q.compare(right.q) == 0
        && left.t.compare(right.t) == 0;
}


bool rejects(std::vector<std::uint8_t> bytes)
{
    try
    {
        static_cast<void>(ChunkCodec::decode(bytes));
    }
    catch (const std::invalid_argument&)
    {
        return true;
    }
    catch (const std::length_error&)
    {
        return true;
    }
    return false;
}

} // namespace


int main()
{
    const ComputationIdentity computation =
        ComputationIdentity::fromPrecisionPlan(PrecisionPolicy::create(1000));
    const ChunkIdentity identity = ChunkIdentity::create(
        computation,
        BlockLocation::create(0, 10, 3, computation)
    );

    GMPInteger negative(128);
    negative.negate();
    const ChunkMetadata metadata = ChunkCodec::createMetadata(
        identity,
        GMPInteger(256),
        GMPInteger(1000),
        negative
    );
    const Chunk chunk{
        metadata,
        GMPInteger(256),
        GMPInteger(1000),
        negative
    };
    const std::vector<std::uint8_t> encoded = ChunkCodec::encode(chunk);
    const std::vector<std::uint8_t> encodedAgain = ChunkCodec::encode(chunk);

    if (encoded != encodedAgain
        || encoded.size() != ChunkCodec::version1HeaderSize
            + metadata.storedSize
        || encoded[0] != 'G'
        || encoded[1] != 'M'
        || encoded[8] != 0
        || encoded[9] != ChunkCodec::formatVersion
        || encoded[10] != 0
        || encoded[11] != ChunkCodec::version1HeaderSize
        || encoded[72] != 0
        || encoded[73] != 0
        || encoded[74] != 0
        || encoded[75] != 3
        || encoded[76] != 0
        || encoded[77] != 0
        || encoded[78] != 0
        || encoded[79] != 1)
    {
        std::cerr << "Versioned chunk bytes mismatch\n";
        return 1;
    }

    const Chunk decoded = ChunkCodec::decode(encoded);
    if (!same(chunk, decoded) || ChunkCodec::encode(decoded) != encoded)
    {
        std::cerr << "Chunk round trip mismatch\n";
        return 1;
    }

    std::vector<std::uint8_t> badMagic = encoded;
    badMagic[0] = 'X';
    std::vector<std::uint8_t> badVersion = encoded;
    badVersion[9] = 2;
    std::vector<std::uint8_t> truncated = encoded;
    truncated.pop_back();
    std::vector<std::uint8_t> trailing = encoded;
    trailing.push_back(0);
    std::vector<std::uint8_t> badPayload = encoded;
    badPayload.back() ^= 1;

    if (!rejects(badMagic)
        || !rejects(badVersion)
        || !rejects(truncated)
        || !rejects(trailing)
        || !rejects(badPayload))
    {
        std::cerr << "Malformed chunk was accepted\n";
        return 1;
    }

    const ChunkMetadata zeroMetadata = ChunkCodec::createMetadata(
        identity,
        GMPInteger{},
        GMPInteger{},
        GMPInteger{}
    );
    const Chunk zeroChunk{
        zeroMetadata,
        GMPInteger{},
        GMPInteger{},
        GMPInteger{}
    };
    if (!same(zeroChunk, ChunkCodec::decode(ChunkCodec::encode(zeroChunk))))
    {
        std::cerr << "Zero chunk round trip mismatch\n";
        return 1;
    }

    std::cout << "ChunkCodec OK\n";
    return 0;
}
