#pragma once

#include "checkpoint/CheckpointBlockCodec.hpp"
#include "checkpoint/CheckpointValidation.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>


namespace pi::checkpoint
{

struct InspectedCheckpointFile
{
    std::filesystem::path path;
    std::uint64_t fileSize;
    std::uint64_t payloadSize;
    ChecksumAlgorithm checksumAlgorithm;
    std::uint32_t checksum;
    CheckpointBlock block;
};


struct CheckpointFileInspection
{
    ValidationResult validation;
    std::optional<InspectedCheckpointFile> file;

    [[nodiscard]]
    bool hasAcceptedFile() const noexcept
    {
        return validation.isAccepted() && file.has_value();
    }
};


class CheckpointFileInspector
{
public:
    [[nodiscard]]
    static CheckpointFileInspection inspect(
        const std::filesystem::path& path,
        const ComputationIdentity& expectedIdentity
    );
};

} // namespace pi::checkpoint
