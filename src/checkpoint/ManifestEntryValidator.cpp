#include "checkpoint/ManifestEntryValidator.hpp"

#include "checkpoint/CheckpointStore.hpp"

#include <exception>
#include <string>
#include <utility>
#include <vector>


namespace pi::checkpoint
{

namespace
{

void add(
    std::vector<ValidationDiagnostic>& diagnostics,
    RejectionReason reason,
    std::string detail
)
{
    diagnostics.push_back(ValidationDiagnostic{
        ValidationStage::manifest,
        reason,
        std::move(detail)
    });
}

} // namespace


ValidationResult ManifestEntryValidator::validate(
    const ManifestEntry& entry,
    const InspectedCheckpointFile& file,
    const ComputationIdentity& manifestIdentity
)
{
    std::vector<ValidationDiagnostic> diagnostics;

    try
    {
        manifestIdentity.validate();
        entry.location.validate(manifestIdentity);
    }
    catch (const std::exception& error)
    {
        add(diagnostics, RejectionReason::invalidRange, error.what());
    }

    if (file.block.identity != manifestIdentity)
    {
        add(
            diagnostics,
            RejectionReason::manifestIdentityMismatch,
            "Block identity differs from manifest identity"
        );
    }

    if (file.block.location != entry.location)
    {
        add(
            diagnostics,
            RejectionReason::locationMismatch,
            "Block range or tree level differs from manifest entry"
        );
    }

    const std::string actualFilename = file.path.filename().string();
    if (entry.filename != actualFilename)
    {
        add(
            diagnostics,
            RejectionReason::filenameMismatch,
            "Manifest filename differs from the inspected file"
        );
    }

    try
    {
        const CheckpointStore store(file.path.parent_path());
        const std::string deterministicFilename = store.blockPath(
            manifestIdentity, entry.location
        ).filename().string();
        if (entry.filename != deterministicFilename)
        {
            add(
                diagnostics,
                RejectionReason::filenameMismatch,
                "Manifest filename is not deterministic for its identity and location"
            );
        }
    }
    catch (const std::exception& error)
    {
        add(diagnostics, RejectionReason::filenameMismatch, error.what());
    }

    if (entry.payloadSize != file.payloadSize)
    {
        add(
            diagnostics,
            RejectionReason::sizeMismatch,
            "Manifest payload size differs from the encoded P/Q/T payload"
        );
    }

    if (entry.checksumAlgorithm != file.checksumAlgorithm
        || entry.checksum != file.checksum)
    {
        add(
            diagnostics,
            RejectionReason::entryChecksumMismatch,
            "Manifest checksum metadata differs from the block"
        );
    }

    if (entry.completion != CompletionState::complete)
    {
        add(
            diagnostics,
            RejectionReason::incompleteEntry,
            "Manifest entry is not complete"
        );
    }

    if (diagnostics.empty())
    {
        return ValidationResult::accepted();
    }

    return ValidationResult::rejected(std::move(diagnostics));
}

} // namespace pi::checkpoint
