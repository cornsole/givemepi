#include "checkpoint/CheckpointValidation.hpp"

#include <stdexcept>
#include <utility>


namespace pi::checkpoint
{

ValidationResult::ValidationResult(
    ValidationStatus status,
    std::vector<ValidationDiagnostic> diagnostics
)
    : status_(status),
      diagnostics_(std::move(diagnostics))
{
    if (status_ == ValidationStatus::accepted && !diagnostics_.empty())
    {
        throw std::invalid_argument(
            "Accepted validation result cannot contain diagnostics"
        );
    }

    if (status_ != ValidationStatus::accepted && diagnostics_.empty())
    {
        throw std::invalid_argument(
            "Non-accepted validation result requires a diagnostic"
        );
    }
}


ValidationResult ValidationResult::accepted()
{
    return ValidationResult(ValidationStatus::accepted, {});
}


ValidationResult ValidationResult::rejected(
    ValidationDiagnostic diagnostic
)
{
    return rejected(std::vector<ValidationDiagnostic>{
        std::move(diagnostic)
    });
}


ValidationResult ValidationResult::rejected(
    std::vector<ValidationDiagnostic> diagnostics
)
{
    return ValidationResult(
        ValidationStatus::rejected,
        std::move(diagnostics)
    );
}


ValidationResult ValidationResult::ignored(
    RejectionReason reason,
    std::string detail
)
{
    return ValidationResult(
        ValidationStatus::ignored,
        {ValidationDiagnostic{
            ValidationStage::discovery,
            reason,
            std::move(detail)
        }}
    );
}


ValidationStatus ValidationResult::status() const noexcept
{
    return status_;
}


bool ValidationResult::isAccepted() const noexcept
{
    return status_ == ValidationStatus::accepted;
}


bool ValidationResult::isRejected() const noexcept
{
    return status_ == ValidationStatus::rejected;
}


bool ValidationResult::isIgnored() const noexcept
{
    return status_ == ValidationStatus::ignored;
}


std::span<const ValidationDiagnostic> ValidationResult::diagnostics() const noexcept
{
    return diagnostics_;
}


std::string_view toString(ValidationStatus status) noexcept
{
    switch (status)
    {
        case ValidationStatus::accepted: return "accepted";
        case ValidationStatus::rejected: return "rejected";
        case ValidationStatus::ignored: return "ignored";
    }
    return "unknown";
}


std::string_view toString(ValidationStage stage) noexcept
{
    switch (stage)
    {
        case ValidationStage::discovery: return "discovery";
        case ValidationStage::structure: return "structure";
        case ValidationStage::compatibility: return "compatibility";
        case ValidationStage::checksum: return "checksum";
        case ValidationStage::manifest: return "manifest";
        case ValidationStage::mathematics: return "mathematics";
        case ValidationStage::quarantine: return "quarantine";
    }
    return "unknown";
}


std::string_view toString(RejectionReason reason) noexcept
{
    switch (reason)
    {
        case RejectionReason::temporaryFile: return "temporary_file";
        case RejectionReason::fileReadFailed: return "file_read_failed";
        case RejectionReason::invalidMagic: return "invalid_magic";
        case RejectionReason::unsupportedVersion: return "unsupported_version";
        case RejectionReason::invalidIdentity: return "invalid_identity";
        case RejectionReason::invalidRange: return "invalid_range";
        case RejectionReason::invalidTreeLevel: return "invalid_tree_level";
        case RejectionReason::invalidPayloadLength: return "invalid_payload_length";
        case RejectionReason::malformedBlock: return "malformed_block";
        case RejectionReason::nonCanonicalInteger: return "non_canonical_integer";
        case RejectionReason::trailingBytes: return "trailing_bytes";
        case RejectionReason::checksumMetadataMismatch: return "checksum_metadata_mismatch";
        case RejectionReason::checksumMismatch: return "checksum_mismatch";
        case RejectionReason::invalidManifest: return "invalid_manifest";
        case RejectionReason::manifestIdentityMismatch: return "manifest_identity_mismatch";
        case RejectionReason::locationMismatch: return "location_mismatch";
        case RejectionReason::missingManifestEntry: return "missing_manifest_entry";
        case RejectionReason::unexpectedBlockFile: return "unexpected_block_file";
        case RejectionReason::filenameMismatch: return "filename_mismatch";
        case RejectionReason::sizeMismatch: return "size_mismatch";
        case RejectionReason::entryChecksumMismatch: return "entry_checksum_mismatch";
        case RejectionReason::incompleteEntry: return "incomplete_entry";
        case RejectionReason::nonCanonicalOrder: return "non_canonical_order";
        case RejectionReason::duplicateRange: return "duplicate_range";
        case RejectionReason::overlappingRange: return "overlapping_range";
        case RejectionReason::missingRange: return "missing_range";
        case RejectionReason::modularMismatch: return "modular_mismatch";
        case RejectionReason::quarantineFailed: return "quarantine_failed";
    }
    return "unknown";
}

} // namespace pi::checkpoint
