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


/// Encoded version-1 block bytes plus the manifest metadata they imply.
struct EncodedCheckpointBlock
{
    std::vector<std::uint8_t> bytes;
    std::uint64_t payloadSize;
    ChecksumAlgorithm checksumAlgorithm;
    std::uint32_t checksum;
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

    /**
     * Encodes version-1 bytes directly from borrowed P/Q/T values without
     * copying GMP payloads, returning manifest metadata alongside the bytes.
     */
    [[nodiscard]]
    static EncodedCheckpointBlock encode(
        const ComputationIdentity& identity,
        const BlockLocation& location,
        const bigint::GMPInteger& p,
        const bigint::GMPInteger& q,
        const bigint::GMPInteger& t
    );

    [[nodiscard]]
    static CheckpointBlock decode(
        std::span<const std::uint8_t> bytes
    );
};

} // namespace pi::checkpoint
