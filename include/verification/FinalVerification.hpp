#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>


namespace pi::verification
{

inline constexpr std::uint32_t FINAL_VERIFICATION_SCHEMA_VERSION = 1;


enum class VerificationStatus
{
    passed,
    failed,
    skipped,
    inconclusive
};


enum class VerificationStage
{
    outputStructure,
    knownDigits,
    hash,
    bbp,
    manifest
};


enum class VerificationReason
{
    fileReadFailed,
    invalidOutputPrefix,
    invalidOutputLength,
    invalidOutputCharacter,
    trailingData,
    knownDigitsMismatch,
    hashMismatch,
    bbpMismatch,
    insufficientPrecision,
    unsupportedVersion,
    invalidManifest,
    operationFailed
};


/// Mathematical identity copied into a final verification report.
struct VerificationIdentity
{
    std::uint64_t requestedDigits;
    std::uint32_t guardDigits;
    std::uint64_t workingDigits;
    std::uint64_t termCount;

    [[nodiscard]]
    bool operator==(const VerificationIdentity&) const noexcept = default;
};


/// One structured verification failure or uncertainty.
struct VerificationDiagnostic
{
    VerificationStage stage;
    VerificationReason reason;
    std::string detail;
    std::optional<std::uint64_t> offset;

    [[nodiscard]]
    bool operator==(const VerificationDiagnostic&) const noexcept = default;
};


/// Immutable result of one final-output verification stage.
class VerificationCheckResult
{
public:
    /// Creates a stage result and validates status/diagnostic invariants.
    ///
    /// Passed results cannot contain diagnostics. Failed and inconclusive
    /// results require at least one diagnostic belonging to the same stage.
    /// Duration must be non-negative. Time and memory complexity: O(n) for n
    /// diagnostics.
    VerificationCheckResult(
        VerificationStage stage,
        VerificationStatus status,
        std::chrono::nanoseconds duration,
        std::vector<VerificationDiagnostic> diagnostics = {}
    );

    [[nodiscard]] VerificationStage stage() const noexcept;
    [[nodiscard]] VerificationStatus status() const noexcept;
    [[nodiscard]] std::chrono::nanoseconds duration() const noexcept;

    [[nodiscard]]
    std::span<const VerificationDiagnostic> diagnostics() const noexcept;

private:
    VerificationStage stage_;
    VerificationStatus status_;
    std::chrono::nanoseconds duration_;
    std::vector<VerificationDiagnostic> diagnostics_;
};


/// Immutable aggregate report for one canonical final output.
class FinalVerificationReport
{
public:
    /// Creates a versioned report and derives its overall status.
    ///
    /// Stages must be unique and at least one result must be present. Failed
    /// dominates inconclusive. Skipped is neutral when another stage passes,
    /// and remains overall only when every stage is skipped.
    /// Time complexity: O(n^2) for n stage results; n is schema-bounded.
    /// Memory complexity: O(n).
    FinalVerificationReport(
        VerificationIdentity identity,
        std::vector<VerificationCheckResult> checks
    );

    [[nodiscard]] std::uint32_t schemaVersion() const noexcept;
    [[nodiscard]] const VerificationIdentity& identity() const noexcept;
    [[nodiscard]] VerificationStatus status() const noexcept;

    [[nodiscard]]
    std::span<const VerificationCheckResult> checks() const noexcept;

    [[nodiscard]]
    const VerificationCheckResult* find(VerificationStage stage) const noexcept;

private:
    VerificationIdentity identity_;
    VerificationStatus status_;
    std::vector<VerificationCheckResult> checks_;
};


[[nodiscard]]
std::string_view toString(VerificationStatus status) noexcept;

[[nodiscard]]
std::string_view toString(VerificationStage stage) noexcept;

[[nodiscard]]
std::string_view toString(VerificationReason reason) noexcept;

} // namespace pi::verification
