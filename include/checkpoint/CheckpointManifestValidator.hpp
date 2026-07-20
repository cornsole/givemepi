#pragma once

#include "checkpoint/CheckpointManifest.hpp"
#include "checkpoint/CheckpointValidation.hpp"

#include <cstdint>
#include <vector>


namespace pi::checkpoint
{

struct MissingRange
{
    std::uint64_t start;
    std::uint64_t end;

    [[nodiscard]]
    bool operator==(const MissingRange&) const noexcept = default;
};


struct CheckpointManifestValidation
{
    ValidationResult validation;
    std::vector<ManifestEntry> acceptedEntries;
    std::vector<MissingRange> missingRanges;
};


class CheckpointManifestValidator
{
public:
    [[nodiscard]]
    static CheckpointManifestValidation validate(
        const CheckpointManifest& manifest,
        const ComputationIdentity& expectedIdentity
    );
};

} // namespace pi::checkpoint
