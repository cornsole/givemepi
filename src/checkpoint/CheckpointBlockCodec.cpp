#include "checkpoint/CheckpointBlockCodec.hpp"

#include "checkpoint/CRC32C.hpp"
#include "checkpoint/GMPSerialization.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>


namespace pi::checkpoint
{

namespace
{

constexpr std::array<std::uint8_t, 8> blockMagic{
    'G', 'M', 'P', 'I', 'P', 'Q', 'T', 0
};

constexpr std::size_t checksumOffset = 80;
constexpr std::size_t checksumSize = 4;


std::uint32_t blockChecksum(
    std::span<const std::uint8_t> bytes
)
{
    if (bytes.size() < CheckpointBlockCodec::version1HeaderSize)
    {
        throw std::invalid_argument("Truncated checkpoint checksum input");
    }

    std::uint32_t checksum = CRC32C::calculate(
        std::span<const std::uint8_t>(bytes.data(), checksumOffset)
    );
    constexpr std::array<std::uint8_t, checksumSize> zeroChecksum{};
    checksum = CRC32C::calculate(zeroChecksum, checksum);
    checksum = CRC32C::calculate(
        std::span<const std::uint8_t>(
            bytes.data() + checksumOffset + checksumSize,
            bytes.size() - checksumOffset - checksumSize
        ),
        checksum
    );
    return checksum;
}


void storeU32(
    std::vector<std::uint8_t>& output,
    std::size_t offset,
    std::uint32_t value
)
{
    for (int shift = 24; shift >= 0; shift -= 8)
    {
        output[offset++] = static_cast<std::uint8_t>(value >> shift);
    }
}


void appendU8(std::vector<std::uint8_t>& output, std::uint8_t value)
{
    output.push_back(value);
}


void appendU16(std::vector<std::uint8_t>& output, std::uint16_t value)
{
    output.push_back(static_cast<std::uint8_t>(value >> 8));
    output.push_back(static_cast<std::uint8_t>(value));
}


void appendU32(std::vector<std::uint8_t>& output, std::uint32_t value)
{
    for (int shift = 24; shift >= 0; shift -= 8)
    {
        output.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}


void appendU64(std::vector<std::uint8_t>& output, std::uint64_t value)
{
    for (int shift = 56; shift >= 0; shift -= 8)
    {
        output.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}


std::size_t checkedPayloadSize(
    std::size_t pSize,
    std::size_t qSize,
    std::size_t tSize
)
{
    std::size_t total = 0;

    for (const std::size_t size : {pSize, qSize, tSize})
    {
        if (size > std::numeric_limits<std::size_t>::max()
                - CheckpointBlockCodec::version1HeaderSize - total)
        {
            throw std::overflow_error("Checkpoint block size overflow");
        }

        total += size;
    }

    return total;
}


class Reader
{
public:

    explicit Reader(std::span<const std::uint8_t> bytes)
        : bytes_(bytes)
    {
    }

    std::uint8_t readU8()
    {
        return take(1).front();
    }

    std::uint16_t readU16()
    {
        const auto data = take(2);
        return static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(data[0]) << 8) | data[1]
        );
    }

    std::uint32_t readU32()
    {
        const auto data = take(4);
        std::uint32_t value = 0;

        for (const std::uint8_t byte : data)
        {
            value = (value << 8) | byte;
        }

        return value;
    }

    std::uint64_t readU64()
    {
        const auto data = take(8);
        std::uint64_t value = 0;

        for (const std::uint8_t byte : data)
        {
            value = (value << 8) | byte;
        }

        return value;
    }

    std::span<const std::uint8_t> take(std::size_t count)
    {
        if (count > bytes_.size() - position_)
        {
            throw std::invalid_argument("Truncated checkpoint block");
        }

        const std::span<const std::uint8_t> result(
            bytes_.data() + position_, count
        );
        position_ += count;
        return result;
    }

    [[nodiscard]]
    std::size_t remaining() const noexcept
    {
        return bytes_.size() - position_;
    }

private:

    std::span<const std::uint8_t> bytes_;
    std::size_t position_ = 0;
};


std::size_t checkedSize(std::uint64_t value)
{
    if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t))
    {
        if (value > std::numeric_limits<std::size_t>::max())
        {
            throw std::invalid_argument(
                "Checkpoint payload length is unsupported"
            );
        }
    }

    return static_cast<std::size_t>(value);
}

} // namespace


std::vector<std::uint8_t> CheckpointBlockCodec::encode(
    const CheckpointBlock& block
)
{
    return encode(
        block.identity, block.location, block.p, block.q, block.t
    ).bytes;
}


EncodedCheckpointBlock CheckpointBlockCodec::encode(
    const ComputationIdentity& identity,
    const BlockLocation& location,
    const bigint::GMPInteger& p,
    const bigint::GMPInteger& q,
    const bigint::GMPInteger& t
)
{
    identity.validate();
    location.validate(identity);

    const std::size_t pSize = GMPSerialization::magnitudeSize(p);
    const std::size_t qSize = GMPSerialization::magnitudeSize(q);
    const std::size_t tSize = GMPSerialization::magnitudeSize(t);
    const std::size_t payloadSize = checkedPayloadSize(pSize, qSize, tSize);

    std::vector<std::uint8_t> output;
    output.reserve(version1HeaderSize + payloadSize);
    output.insert(output.end(), blockMagic.begin(), blockMagic.end());
    appendU16(output, formatVersion);
    appendU16(output, version1HeaderSize);
    appendU32(output, 0); // flags
    appendU16(output, static_cast<std::uint16_t>(identity.algorithm));
    appendU16(output, identity.arithmeticVersion);
    appendU16(output, identity.formulaVersion);
    appendU16(output, 0); // reserved
    appendU64(output, identity.requestedDigits);
    appendU32(output, identity.guardDigits);
    appendU32(output, 0); // reserved
    appendU64(output, identity.workingDigits);
    appendU64(output, identity.termCount);
    appendU64(output, location.start);
    appendU64(output, location.end);
    appendU32(output, location.treeLevel);
    appendU16(output, static_cast<std::uint16_t>(ChecksumAlgorithm::crc32c));
    appendU16(output, checksumSize);
    appendU32(output, 0); // checksum value slot
    appendU8(output, static_cast<std::uint8_t>(GMPSerialization::sign(p)));
    appendU8(output, static_cast<std::uint8_t>(GMPSerialization::sign(q)));
    appendU8(output, static_cast<std::uint8_t>(GMPSerialization::sign(t)));
    appendU8(output, 0); // reserved
    appendU64(output, pSize);
    appendU64(output, qSize);
    appendU64(output, tSize);

    if (output.size() != version1HeaderSize)
    {
        throw std::logic_error("Checkpoint header size invariant failed");
    }

    output.resize(version1HeaderSize + payloadSize);
    std::uint8_t* const payload = output.data() + version1HeaderSize;
    GMPSerialization::exportMagnitude(p, std::span(payload, pSize));
    GMPSerialization::exportMagnitude(q, std::span(payload + pSize, qSize));
    GMPSerialization::exportMagnitude(
        t, std::span(payload + pSize + qSize, tSize)
    );

    const std::uint32_t checksum = blockChecksum(output);
    storeU32(output, checksumOffset, checksum);
    return EncodedCheckpointBlock{
        std::move(output),
        static_cast<std::uint64_t>(payloadSize),
        ChecksumAlgorithm::crc32c,
        checksum
    };
}


CheckpointBlock CheckpointBlockCodec::decode(
    std::span<const std::uint8_t> bytes
)
{
    Reader reader(bytes);
    const auto magic = reader.take(blockMagic.size());

    if (!std::equal(magic.begin(), magic.end(), blockMagic.begin()))
    {
        throw std::invalid_argument("Invalid checkpoint block magic");
    }

    if (reader.readU16() != formatVersion
        || reader.readU16() != version1HeaderSize)
    {
        throw std::invalid_argument("Unsupported checkpoint block version");
    }

    if (reader.readU32() != 0)
    {
        throw std::invalid_argument("Unsupported checkpoint block flags");
    }

    const AlgorithmId algorithm = static_cast<AlgorithmId>(reader.readU16());
    const std::uint16_t arithmeticVersion = reader.readU16();
    const std::uint16_t formulaVersion = reader.readU16();

    if (reader.readU16() != 0)
    {
        throw std::invalid_argument("Non-zero checkpoint reserved field");
    }

    ComputationIdentity identity{
        algorithm,
        arithmeticVersion,
        formulaVersion,
        reader.readU64(),
        reader.readU32(),
        0,
        0
    };

    if (reader.readU32() != 0)
    {
        throw std::invalid_argument("Non-zero checkpoint reserved field");
    }

    identity.workingDigits = reader.readU64();
    identity.termCount = reader.readU64();
    identity.validate();

    BlockLocation location{
        reader.readU64(), reader.readU64(), reader.readU32()
    };
    location.validate(identity);

    const auto checksumAlgorithm =
        static_cast<ChecksumAlgorithm>(reader.readU16());
    const std::uint16_t checksumLength = reader.readU16();
    const std::uint32_t checksumValue = reader.readU32();

    if (checksumAlgorithm != ChecksumAlgorithm::crc32c
        || checksumLength != checksumSize)
    {
        throw std::invalid_argument("Unsupported checkpoint checksum metadata");
    }

    if (checksumValue != blockChecksum(bytes))
    {
        throw std::invalid_argument("Checkpoint CRC32C mismatch");
    }

    const IntegerSign pSign = static_cast<IntegerSign>(reader.readU8());
    const IntegerSign qSign = static_cast<IntegerSign>(reader.readU8());
    const IntegerSign tSign = static_cast<IntegerSign>(reader.readU8());

    if (reader.readU8() != 0)
    {
        throw std::invalid_argument("Non-zero checkpoint reserved field");
    }

    const std::size_t pLength = checkedSize(reader.readU64());
    const std::size_t qLength = checkedSize(reader.readU64());
    const std::size_t tLength = checkedSize(reader.readU64());

    std::size_t payloadLength = pLength;

    for (const std::size_t length : {qLength, tLength})
    {
        if (length > std::numeric_limits<std::size_t>::max() - payloadLength)
        {
            throw std::invalid_argument("Checkpoint payload length overflow");
        }
        payloadLength += length;
    }

    if (payloadLength != reader.remaining())
    {
        throw std::invalid_argument(
            "Checkpoint payload lengths do not match the file size"
        );
    }

    bigint::GMPInteger p = GMPSerialization::decode(pSign, reader.take(pLength));
    bigint::GMPInteger q = GMPSerialization::decode(qSign, reader.take(qLength));
    bigint::GMPInteger t = GMPSerialization::decode(tSign, reader.take(tLength));

    return CheckpointBlock{
        identity, location, std::move(p), std::move(q), std::move(t)
    };
}

} // namespace pi::checkpoint
