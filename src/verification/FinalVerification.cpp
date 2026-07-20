#include "verification/FinalVerification.hpp"

#include <stdexcept>
#include <utility>


namespace pi::verification
{
namespace
{

int severity(VerificationStatus status) noexcept
{
    switch (status)
    {
        case VerificationStatus::skipped: return 0;
        case VerificationStatus::passed: return 1;
        case VerificationStatus::inconclusive: return 2;
        case VerificationStatus::failed: return 3;
    }
    return 3;
}

} // namespace


VerificationCheckResult::VerificationCheckResult(
    VerificationStage stage,
    VerificationStatus status,
    std::chrono::nanoseconds duration,
    std::vector<VerificationDiagnostic> diagnostics
)
    : stage_(stage),
      status_(status),
      duration_(duration),
      diagnostics_(std::move(diagnostics))
{
    if (duration_ < std::chrono::nanoseconds::zero())
    {
        throw std::invalid_argument(
            "Verification duration cannot be negative"
        );
    }
    if (status_ == VerificationStatus::passed && !diagnostics_.empty())
    {
        throw std::invalid_argument(
            "Passed verification result cannot contain diagnostics"
        );
    }
    if ((status_ == VerificationStatus::failed
            || status_ == VerificationStatus::inconclusive)
        && diagnostics_.empty())
    {
        throw std::invalid_argument(
            "Failed or inconclusive verification requires diagnostics"
        );
    }
    for (const VerificationDiagnostic& diagnostic : diagnostics_)
    {
        if (diagnostic.stage != stage_)
        {
            throw std::invalid_argument(
                "Verification diagnostic stage does not match its result"
            );
        }
    }
}


VerificationStage VerificationCheckResult::stage() const noexcept
{
    return stage_;
}


VerificationStatus VerificationCheckResult::status() const noexcept
{
    return status_;
}


std::chrono::nanoseconds VerificationCheckResult::duration() const noexcept
{
    return duration_;
}


std::span<const VerificationDiagnostic>
VerificationCheckResult::diagnostics() const noexcept
{
    return diagnostics_;
}


FinalVerificationReport::FinalVerificationReport(
    VerificationIdentity identity,
    std::vector<VerificationCheckResult> checks
)
    : identity_(identity),
      status_(VerificationStatus::skipped),
      checks_(std::move(checks))
{
    if (identity_.requestedDigits == 0
        || identity_.workingDigits < identity_.requestedDigits
        || identity_.termCount == 0)
    {
        throw std::invalid_argument("Invalid final verification identity");
    }
    if (checks_.empty())
    {
        throw std::invalid_argument(
            "Final verification report requires at least one check"
        );
    }

    for (std::size_t index = 0; index < checks_.size(); ++index)
    {
        for (std::size_t prior = 0; prior < index; ++prior)
        {
            if (checks_[prior].stage() == checks_[index].stage())
            {
                throw std::invalid_argument(
                    "Final verification report contains a duplicate stage"
                );
            }
        }
        if (severity(checks_[index].status()) > severity(status_))
        {
            status_ = checks_[index].status();
        }
    }
}


std::uint32_t FinalVerificationReport::schemaVersion() const noexcept
{
    return FINAL_VERIFICATION_SCHEMA_VERSION;
}


const VerificationIdentity& FinalVerificationReport::identity() const noexcept
{
    return identity_;
}


VerificationStatus FinalVerificationReport::status() const noexcept
{
    return status_;
}


std::span<const VerificationCheckResult>
FinalVerificationReport::checks() const noexcept
{
    return checks_;
}


const VerificationCheckResult* FinalVerificationReport::find(
    VerificationStage stage
) const noexcept
{
    for (const VerificationCheckResult& check : checks_)
    {
        if (check.stage() == stage)
        {
            return &check;
        }
    }
    return nullptr;
}


std::string_view toString(VerificationStatus status) noexcept
{
    switch (status)
    {
        case VerificationStatus::passed: return "passed";
        case VerificationStatus::failed: return "failed";
        case VerificationStatus::skipped: return "skipped";
        case VerificationStatus::inconclusive: return "inconclusive";
    }
    return "unknown";
}


std::string_view toString(VerificationStage stage) noexcept
{
    switch (stage)
    {
        case VerificationStage::outputStructure: return "output_structure";
        case VerificationStage::knownDigits: return "known_digits";
        case VerificationStage::hash: return "hash";
        case VerificationStage::bbp: return "bbp";
        case VerificationStage::manifest: return "manifest";
    }
    return "unknown";
}


std::string_view toString(VerificationReason reason) noexcept
{
    switch (reason)
    {
        case VerificationReason::fileReadFailed: return "file_read_failed";
        case VerificationReason::invalidOutputPrefix:
            return "invalid_output_prefix";
        case VerificationReason::invalidOutputLength:
            return "invalid_output_length";
        case VerificationReason::invalidOutputCharacter:
            return "invalid_output_character";
        case VerificationReason::trailingData: return "trailing_data";
        case VerificationReason::knownDigitsMismatch:
            return "known_digits_mismatch";
        case VerificationReason::hashMismatch: return "hash_mismatch";
        case VerificationReason::bbpMismatch: return "bbp_mismatch";
        case VerificationReason::insufficientPrecision:
            return "insufficient_precision";
        case VerificationReason::unsupportedVersion:
            return "unsupported_version";
        case VerificationReason::invalidManifest: return "invalid_manifest";
        case VerificationReason::operationFailed: return "operation_failed";
    }
    return "unknown";
}

} // namespace pi::verification
