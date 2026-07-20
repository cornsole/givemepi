#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>


namespace pi::checkpoint
{

enum class ValidationStatus
{
    accepted,
    rejected,
    ignored
};


enum class ValidationStage
{
    discovery,
    structure,
    compatibility,
    checksum,
    manifest,
    mathematics,
    quarantine
};


enum class RejectionReason
{
    temporaryFile,
    fileReadFailed,
    invalidMagic,
    unsupportedVersion,
    invalidIdentity,
    invalidRange,
    invalidTreeLevel,
    invalidPayloadLength,
    malformedBlock,
    nonCanonicalInteger,
    trailingBytes,
    checksumMetadataMismatch,
    checksumMismatch,
    invalidManifest,
    manifestIdentityMismatch,
    locationMismatch,
    missingManifestEntry,
    unexpectedBlockFile,
    filenameMismatch,
    sizeMismatch,
    entryChecksumMismatch,
    incompleteEntry,
    nonCanonicalOrder,
    duplicateRange,
    overlappingRange,
    missingRange,
    modularMismatch,
    quarantineFailed
};


struct ValidationDiagnostic
{
    ValidationStage stage;
    RejectionReason reason;
    std::string detail;

    [[nodiscard]]
    bool operator==(const ValidationDiagnostic&) const noexcept = default;
};


class ValidationResult
{
public:
    [[nodiscard]]
    static ValidationResult accepted();

    [[nodiscard]]
    static ValidationResult rejected(ValidationDiagnostic diagnostic);

    [[nodiscard]]
    static ValidationResult rejected(
        std::vector<ValidationDiagnostic> diagnostics
    );

    [[nodiscard]]
    static ValidationResult ignored(
        RejectionReason reason,
        std::string detail = {}
    );

    [[nodiscard]] ValidationStatus status() const noexcept;
    [[nodiscard]] bool isAccepted() const noexcept;
    [[nodiscard]] bool isRejected() const noexcept;
    [[nodiscard]] bool isIgnored() const noexcept;

    [[nodiscard]]
    std::span<const ValidationDiagnostic> diagnostics() const noexcept;

private:
    ValidationResult(
        ValidationStatus status,
        std::vector<ValidationDiagnostic> diagnostics
    );

    ValidationStatus status_;
    std::vector<ValidationDiagnostic> diagnostics_;
};


[[nodiscard]]
std::string_view toString(ValidationStatus status) noexcept;

[[nodiscard]]
std::string_view toString(ValidationStage stage) noexcept;

[[nodiscard]]
std::string_view toString(RejectionReason reason) noexcept;

} // namespace pi::checkpoint
