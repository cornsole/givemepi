#include "checkpoint/CheckpointBlockValidator.hpp"

#include "checkpoint/CRC32C.hpp"

#include <algorithm>
#include <array>
#include <exception>
#include <limits>
#include <string>
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


CheckpointBlockValidation reject(
    ValidationStage stage,
    RejectionReason reason,
    std::string detail
)
{
    return CheckpointBlockValidation{
        ValidationResult::rejected(
            ValidationDiagnostic{stage, reason, std::move(detail)}
        ),
        std::nullopt
    };
}


std::uint16_t readU16(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(bytes[offset]) << 8) | bytes[offset + 1]
    );
}


std::uint32_t readU32(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < 4; ++index)
        value = (value << 8) | bytes[offset + index];
    return value;
}


std::uint64_t readU64(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < 8; ++index)
        value = (value << 8) | bytes[offset + index];
    return value;
}


std::uint32_t calculateChecksum(std::span<const std::uint8_t> bytes)
{
    std::uint32_t checksum = CRC32C::calculate(bytes.first(checksumOffset));
    constexpr std::array<std::uint8_t, checksumSize> zeros{};
    checksum = CRC32C::calculate(zeros, checksum);
    return CRC32C::calculate(
        bytes.subspan(checksumOffset + checksumSize), checksum
    );
}


std::uint32_t maximumTreeLevel(std::uint64_t termCount) noexcept
{
    std::uint32_t level = 0;
    std::uint64_t capacity = 1;
    while (capacity < termCount)
    {
        if (capacity > std::numeric_limits<std::uint64_t>::max() / 2)
        {
            return level + 1;
        }
        capacity <<= 1;
        ++level;
    }
    return level;
}

} // namespace


CheckpointBlockValidation CheckpointBlockValidator::validate(
    std::span<const std::uint8_t> bytes,
    const ComputationIdentity& expectedIdentity
)
{
    try
    {
        expectedIdentity.validate();
    }
    catch (const std::exception& error)
    {
        return reject(
            ValidationStage::compatibility,
            RejectionReason::invalidIdentity,
            error.what()
        );
    }

    if (bytes.size() < CheckpointBlockCodec::version1HeaderSize)
        return reject(ValidationStage::structure,
            RejectionReason::invalidPayloadLength, "Truncated block header");
    if (!std::equal(blockMagic.begin(), blockMagic.end(), bytes.begin()))
        return reject(ValidationStage::structure,
            RejectionReason::invalidMagic, "Block magic does not match");
    if (readU16(bytes, 8) != CheckpointBlockCodec::formatVersion
        || readU16(bytes, 10) != CheckpointBlockCodec::version1HeaderSize)
        return reject(ValidationStage::structure,
            RejectionReason::unsupportedVersion, "Unsupported block version");
    if (readU32(bytes, 12) != 0 || readU16(bytes, 22) != 0
        || readU32(bytes, 36) != 0 || bytes[87] != 0)
        return reject(ValidationStage::structure,
            RejectionReason::malformedBlock, "Non-zero reserved block field");

    const std::uint64_t termCount = readU64(bytes, 48);
    const std::uint64_t start = readU64(bytes, 56);
    const std::uint64_t end = readU64(bytes, 64);
    const std::uint32_t treeLevel = readU32(bytes, 72);
    if (start >= end || end > termCount)
        return reject(ValidationStage::structure,
            RejectionReason::invalidRange, "Invalid block range");
    if (termCount == 0 || treeLevel > maximumTreeLevel(termCount))
        return reject(ValidationStage::structure,
            RejectionReason::invalidTreeLevel, "Invalid block tree level");

    const std::uint64_t lengths64[3]{
        readU64(bytes, 88), readU64(bytes, 96), readU64(bytes, 104)
    };
    std::size_t lengths[3]{};
    std::size_t payloadSize = 0;
    for (std::size_t index = 0; index < 3; ++index)
    {
        if (lengths64[index] > std::numeric_limits<std::size_t>::max())
            return reject(ValidationStage::structure,
                RejectionReason::invalidPayloadLength, "Payload length is unsupported");
        lengths[index] = static_cast<std::size_t>(lengths64[index]);
        if (lengths[index] > std::numeric_limits<std::size_t>::max() - payloadSize)
            return reject(ValidationStage::structure,
                RejectionReason::invalidPayloadLength, "Payload length overflow");
        payloadSize += lengths[index];
    }

    const std::size_t remaining = bytes.size()
        - CheckpointBlockCodec::version1HeaderSize;
    if (payloadSize > remaining)
        return reject(ValidationStage::structure,
            RejectionReason::invalidPayloadLength, "Payload exceeds file boundary");
    if (payloadSize < remaining)
        return reject(ValidationStage::structure,
            RejectionReason::trailingBytes, "Trailing bytes follow block payload");

    std::size_t payloadOffset = CheckpointBlockCodec::version1HeaderSize;
    for (std::size_t index = 0; index < 3; ++index)
    {
        const std::uint8_t sign = bytes[84 + index];
        const bool zero = sign == 0;
        const bool nonZero = sign == 1 || sign == 2;
        if ((!zero && !nonZero) || (zero && lengths[index] != 0)
            || (nonZero && lengths[index] == 0)
            || (lengths[index] != 0 && bytes[payloadOffset] == 0))
            return reject(ValidationStage::structure,
                RejectionReason::nonCanonicalInteger,
                "Non-canonical P/Q/T signed magnitude");
        payloadOffset += lengths[index];
    }

    const auto checksumAlgorithm = static_cast<ChecksumAlgorithm>(readU16(bytes, 76));
    if (checksumAlgorithm != ChecksumAlgorithm::crc32c || readU16(bytes, 78) != 4)
        return reject(ValidationStage::checksum,
            RejectionReason::checksumMetadataMismatch, "Unsupported checksum metadata");
    const std::uint32_t storedChecksum = readU32(bytes, checksumOffset);
    if (storedChecksum != calculateChecksum(bytes))
        return reject(ValidationStage::checksum,
            RejectionReason::checksumMismatch, "Block CRC32C mismatch");

    try
    {
        CheckpointBlock block = CheckpointBlockCodec::decode(bytes);
        if (block.identity != expectedIdentity)
            return reject(ValidationStage::compatibility,
                RejectionReason::invalidIdentity, "Block identity differs from target");
        return CheckpointBlockValidation{
            ValidationResult::accepted(),
            ValidatedCheckpointBlock{
                static_cast<std::uint64_t>(bytes.size()),
                static_cast<std::uint64_t>(payloadSize),
                checksumAlgorithm,
                storedChecksum,
                std::move(block)
            }
        };
    }
    catch (const std::exception& error)
    {
        return reject(ValidationStage::structure,
            RejectionReason::malformedBlock, error.what());
    }
}

} // namespace pi::checkpoint
