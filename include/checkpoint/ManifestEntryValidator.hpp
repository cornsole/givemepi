#pragma once

#include "checkpoint/CheckpointFileInspector.hpp"
#include "checkpoint/CheckpointManifest.hpp"


namespace pi::checkpoint
{

class ManifestEntryValidator
{
public:
    [[nodiscard]]
    static ValidationResult validate(
        const ManifestEntry& entry,
        const InspectedCheckpointFile& file,
        const ComputationIdentity& manifestIdentity
    );
};

} // namespace pi::checkpoint
