#include "checkpoint/CheckpointBlockCodec.hpp"

#include "chudnovsky/PrecisionPolicy.hpp"
#include "checkpoint/CRC32C.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>


using pi::bigint::GMPInteger;
using pi::checkpoint::BlockLocation;
using pi::checkpoint::CheckpointBlock;
using pi::checkpoint::CheckpointBlockCodec;
using pi::checkpoint::ComputationIdentity;
using pi::checkpoint::CRC32C;
using pi::chudnovsky::PrecisionPolicy;


namespace
{

bool sameBlock(const CheckpointBlock& left, const CheckpointBlock& right)
{
    return left.identity == right.identity
        && left.location == right.location
        && left.p.compare(right.p) == 0
        && left.q.compare(right.q) == 0
        && left.t.compare(right.t) == 0;
}


bool rejects(const std::vector<std::uint8_t>& bytes)
{
    try
    {
        static_cast<void>(CheckpointBlockCodec::decode(bytes));
    }
    catch (const std::invalid_argument&)
    {
        return true;
    }

    return false;
}


void refreshChecksum(std::vector<std::uint8_t>& bytes)
{
    for (std::size_t offset = 80; offset < 84; ++offset)
    {
        bytes[offset] = 0;
    }

    const std::uint32_t checksum = CRC32C::calculate(bytes);

    for (int shift = 24, offset = 80; shift >= 0; shift -= 8, ++offset)
    {
        bytes[offset] = static_cast<std::uint8_t>(checksum >> shift);
    }
}


bool roundTripsDeterministically(const CheckpointBlock& block)
{
    const std::vector<std::uint8_t> first =
        CheckpointBlockCodec::encode(block);
    const std::vector<std::uint8_t> second =
        CheckpointBlockCodec::encode(block);
    const CheckpointBlock decoded = CheckpointBlockCodec::decode(first);

    return first == second
        && sameBlock(block, decoded)
        && CheckpointBlockCodec::encode(decoded) == first;
}

} // namespace


int main()
{
    const ComputationIdentity identity =
        ComputationIdentity::fromPrecisionPlan(PrecisionPolicy::create(1000));

    GMPInteger negative(128);
    negative.negate();

    const CheckpointBlock block{
        identity,
        BlockLocation::create(0, 10, 3, identity),
        GMPInteger(256),
        GMPInteger(1000),
        negative
    };

    const std::vector<std::uint8_t> encoded =
        CheckpointBlockCodec::encode(block);

    if (encoded.size() != CheckpointBlockCodec::version1HeaderSize + 5
        || encoded[0] != 'G'
        || encoded[7] != 0
        || encoded[8] != 0
        || encoded[9] != CheckpointBlockCodec::formatVersion
        || encoded[10] != 0
        || encoded[11] != CheckpointBlockCodec::version1HeaderSize
        || encoded[84] != 1
        || encoded[85] != 1
        || encoded[86] != 2
        || encoded[112] != 0x01
        || encoded[113] != 0x00)
    {
        std::cerr << "Version 1 checkpoint bytes mismatch\n";
        return 1;
    }

    const CheckpointBlock decoded = CheckpointBlockCodec::decode(encoded);

    if (!sameBlock(block, decoded)
        || CheckpointBlockCodec::encode(decoded) != encoded)
    {
        std::cerr << "Checkpoint block round trip mismatch\n";
        return 1;
    }

    GMPInteger largeP;
    largeP.setPowerOfTen(10000);
    GMPInteger largeQ(123456789);
    largeQ.mul(largeP);
    GMPInteger largeT(987654321);
    largeT.mul(largeP);
    largeT.negate();

    const CheckpointBlock zeroBlock{
        identity,
        BlockLocation::create(20, 21, 0, identity),
        GMPInteger{},
        GMPInteger{},
        GMPInteger{}
    };
    const CheckpointBlock largeBlock{
        identity,
        BlockLocation::create(10, 20, 4, identity),
        largeP,
        largeQ,
        largeT
    };

    if (!roundTripsDeterministically(zeroBlock)
        || !roundTripsDeterministically(largeBlock))
    {
        std::cerr << "In-memory deterministic round trip failed\n";
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

    std::vector<std::uint8_t> badReserved = encoded;
    badReserved[87] = 1;

    std::vector<std::uint8_t> nonCanonicalMagnitude = encoded;
    nonCanonicalMagnitude[112] = 0;
    refreshChecksum(nonCanonicalMagnitude);

    std::vector<std::uint8_t> unknownSign = encoded;
    unknownSign[84] = 255;
    refreshChecksum(unknownSign);

    std::vector<std::uint8_t> inconsistentLength = encoded;
    inconsistentLength[95] += 1;
    refreshChecksum(inconsistentLength);

    std::vector<std::uint8_t> badIdentity = encoded;
    badIdentity[47] ^= 1;

    std::vector<std::uint8_t> badHeaderChecksum = encoded;
    badHeaderChecksum[56] ^= 1;

    std::vector<std::uint8_t> badPayloadChecksum = encoded;
    badPayloadChecksum.back() ^= 1;

    if (!rejects(badMagic)
        || !rejects(badVersion)
        || !rejects(truncated)
        || !rejects(trailing)
        || !rejects(badReserved)
        || !rejects(nonCanonicalMagnitude)
        || !rejects(unknownSign)
        || !rejects(inconsistentLength)
        || !rejects(badIdentity)
        || !rejects(badHeaderChecksum)
        || !rejects(badPayloadChecksum))
    {
        std::cerr << "Malformed checkpoint block was accepted\n";
        return 1;
    }

    std::cout << "CheckpointBlockCodec OK\n";
    return 0;
}
