#pragma once

#include "checkpoint/CheckpointBlockCodec.hpp"
#include "checkpoint/CheckpointValidation.hpp"

#include <cstdint>
#include <optional>
#include <span>


namespace pi::checkpoint
{

struct ValidatedCheckpointBlock
{
    std::uint64_t encodedSize;
    std::uint64_t payloadSize;
    ChecksumAlgorithm checksumAlgorithm;
    std::uint32_t checksum;
    CheckpointBlock block;
};


struct CheckpointBlockValidation
{
    ValidationResult validation;
    std::optional<ValidatedCheckpointBlock> block;

    [[nodiscard]]
    bool hasAcceptedBlock() const noexcept
    {
        return validation.isAccepted() && block.has_value();
    }
};


class CheckpointBlockValidator
{
public:
    [[nodiscard]]
    static CheckpointBlockValidation validate(
        std::span<const std::uint8_t> bytes,
        const ComputationIdentity& expectedIdentity
    );
};

} // namespace pi::checkpoint
