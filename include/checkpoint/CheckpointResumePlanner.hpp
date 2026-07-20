#pragma once

#include "checkpoint/CheckpointManifestValidator.hpp"
#include "checkpoint/CheckpointQuarantine.hpp"

#include <filesystem>
#include <optional>
#include <vector>


namespace pi::checkpoint
{

struct AcceptedCheckpoint
{
    std::filesystem::path path;
    std::uint64_t fileSize;
    CheckpointBlock block;
};


struct RejectedCheckpoint
{
    std::filesystem::path path;
    ValidationResult validation;
    std::optional<std::filesystem::path> quarantinedPath;
    std::optional<ValidationResult> quarantineOperation;
};


struct CheckpointResumePlan
{
    std::vector<AcceptedCheckpoint> acceptedBlocks;
    std::vector<MissingRange> rangesToRecalculate;
    std::vector<RejectedCheckpoint> rejectedBlocks;
    std::vector<std::filesystem::path> ignoredTemporaryFiles;
    std::vector<ValidationDiagnostic> diagnostics;
};


class CheckpointResumePlanner
{
public:
    CheckpointResumePlanner(
        std::filesystem::path checkpointDirectory,
        std::filesystem::path quarantineDirectory
    );

    [[nodiscard]]
    CheckpointResumePlan create(
        const ComputationIdentity& expectedIdentity
    ) const;

private:
    std::filesystem::path checkpointDirectory_;
    std::filesystem::path quarantineDirectory_;
};

} // namespace pi::checkpoint
