#pragma once

#include "bigint/GMPInteger.hpp"
#include "checkpoint/CheckpointTypes.hpp"

#include <cstdint>
#include <span>
#include <vector>


namespace pi::checkpoint
{

enum class ChecksumAlgorithm : std::uint16_t
{
    none = 0,
    crc32c = 1
};


struct CheckpointBlock
{
    ComputationIdentity identity;
    BlockLocation location;
    bigint::GMPInteger p;
    bigint::GMPInteger q;
    bigint::GMPInteger t;
};


class CheckpointBlockCodec
{
public:

    static constexpr std::uint16_t formatVersion = 1;
    static constexpr std::uint16_t version1HeaderSize = 112;

    [[nodiscard]]
    static std::vector<std::uint8_t> encode(
        const CheckpointBlock& block
    );

    [[nodiscard]]
    static CheckpointBlock decode(
        std::span<const std::uint8_t> bytes
    );
};

} // namespace pi::checkpoint
