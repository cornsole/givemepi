#include "verification/FinalVerifier.hpp"

#include "verification/DecimalHexDigit.hpp"
#include "verification/FinalOutputInspector.hpp"

#include <chrono>
#include <exception>
#include <memory>
#include <string>
#include <utility>

namespace pi::verification
{
namespace
{

constexpr std::uint64_t HASH_UNITS_PER_BYTE = 4;
constexpr std::uint64_t IMPORT_UNITS_PER_DIGIT = 8;
/// Divisor of the digits^2 term of the quadratic decimal import cost.
constexpr long double IMPORT_QUADRATIC_DIVISOR = 256.0L;
constexpr std::uint64_t EXTRACTION_UNITS_PER_DIGIT = 8;
constexpr std::uint64_t KNOWN_DIGITS_UNITS = 1;

std::uint64_t saturatingAdd(std::uint64_t left, std::uint64_t right) noexcept
{
    return right > ~std::uint64_t{0} - left ? ~std::uint64_t{0} : left + right;
}

std::uint64_t saturatingMultiply(
    std::uint64_t left,
    std::uint64_t right
) noexcept
{
    return left != 0 && right > ~std::uint64_t{0} / left
        ? ~std::uint64_t{0}
        : left * right;
}

/// Work of one BBP sample: series terms plus interval extraction.
std::uint64_t sampleUnits(
    std::uint64_t position,
    std::uint64_t requestedDigits
) noexcept
{
    return saturatingAdd(
        saturatingAdd(position, 1),
        saturatingMultiply(requestedDigits, EXTRACTION_UNITS_PER_DIGIT)
    );
}

/// Converts raw imported digit counts into quadratic import units.
///
/// Publishes importWorkUnits(after) - importWorkUnits(before) per batch, so
/// the published sum telescopes to importWorkUnits(total digits).
class QuadraticImportWork final : public VerificationWorkObserver
{
public:
    explicit QuadraticImportWork(VerificationWorkObserver* target) noexcept
        : target_(target)
    {
    }

    void workCompleted(std::uint64_t digits) noexcept override
    {
        if (target_ == nullptr || digits == 0)
        {
            return;
        }
        const std::uint64_t before = FinalVerifier::importWorkUnits(imported_);
        imported_ = saturatingAdd(imported_, digits);
        const std::uint64_t after = FinalVerifier::importWorkUnits(imported_);
        if (after > before)
        {
            target_->workCompleted(after - before);
        }
    }

private:
    VerificationWorkObserver* target_;
    std::uint64_t imported_ = 0;
};

VerificationCheckResult skipped(VerificationStage stage)
{
    return VerificationCheckResult(
        stage,
        VerificationStatus::skipped,
        std::chrono::nanoseconds::zero()
    );
}

VerificationCheckResult failed(
    VerificationStage stage,
    VerificationReason reason,
    std::string detail,
    std::chrono::steady_clock::time_point started
)
{
    return VerificationCheckResult(
        stage,
        VerificationStatus::failed,
        std::chrono::steady_clock::now() - started,
        {{stage, reason, std::move(detail), std::nullopt}}
    );
}

struct BBPCheck
{
    VerificationCheckResult check;
    std::vector<BBPSampleVerification> samples;
};

BBPCheck verifySamples(
    const DecimalHexDigitExtractor& extractor,
    const VerificationIdentity& identity,
    const BBPSamplePolicy& policy,
    VerificationWorkObserver* observer
)
{
    const auto started = std::chrono::steady_clock::now();
    const auto positions = policy.select(identity);
    if (positions.empty())
    {
        return {skipped(VerificationStage::bbp), {}};
    }

    std::vector<BBPSampleVerification> samples;
    std::vector<VerificationDiagnostic> diagnostics;
    samples.reserve(positions.size());
    VerificationStatus aggregate = VerificationStatus::passed;
    for (const std::uint64_t position : positions)
    {
        const auto expected = BBPHexDigitCalculator::calculate(position);
        const auto actual = extractor.extract(position);
        VerificationStatus status = VerificationStatus::passed;
        if (!expected.resolved() || !actual.resolved())
        {
            status = VerificationStatus::inconclusive;
            if (aggregate != VerificationStatus::failed)
            {
                aggregate = VerificationStatus::inconclusive;
            }
            diagnostics.push_back({
                VerificationStage::bbp,
                VerificationReason::insufficientPrecision,
                "BBP sample interval does not resolve to one digit",
                position
            });
        }
        else if (expected.digit != actual.digit)
        {
            status = VerificationStatus::failed;
            aggregate = VerificationStatus::failed;
            diagnostics.push_back({
                VerificationStage::bbp,
                VerificationReason::bbpMismatch,
                "BBP digit differs from canonical decimal output",
                position
            });
        }
        samples.push_back({
            position,
            status,
            expected.digit,
            actual.digit,
            expected.errorBound
        });
        if (observer != nullptr)
        {
            observer->workCompleted(
                sampleUnits(position, identity.requestedDigits)
            );
        }
    }
    return {
        VerificationCheckResult(
            VerificationStage::bbp,
            aggregate,
            std::chrono::steady_clock::now() - started,
            std::move(diagnostics)
        ),
        std::move(samples)
    };
}

FinalVerificationRun stoppedAfterStructure(
    VerificationIdentity identity,
    VerificationCheckResult structure
)
{
    std::vector<VerificationCheckResult> checks;
    checks.push_back(std::move(structure));
    checks.push_back(skipped(VerificationStage::knownDigits));
    checks.push_back(skipped(VerificationStage::hash));
    checks.push_back(skipped(VerificationStage::bbp));
    return {
        FinalVerificationReport(identity, std::move(checks)),
        std::nullopt,
        std::nullopt,
        {}
    };
}

} // namespace


std::uint64_t FinalVerifier::plannedFileWork(
    VerificationIdentity identity,
    const BBPSamplePolicy& samplePolicy
)
{
    const std::uint64_t digits = identity.requestedDigits;
    std::uint64_t units = saturatingAdd(digits, 3);
    units = saturatingAdd(units, KNOWN_DIGITS_UNITS);
    units = saturatingAdd(
        units,
        saturatingMultiply(saturatingAdd(digits, 2), HASH_UNITS_PER_BYTE)
    );
    units = saturatingAdd(units, importWorkUnits(digits));
    for (const std::uint64_t position : samplePolicy.select(identity))
    {
        units = saturatingAdd(units, sampleUnits(position, digits));
    }
    return units;
}

std::uint64_t FinalVerifier::importWorkUnits(std::uint64_t digits) noexcept
{
    const auto value = static_cast<long double>(digits);
    const long double quadratic = value * value / IMPORT_QUADRATIC_DIVISOR;
    constexpr long double maximum = 18446744073709551615.0L;
    const std::uint64_t square = quadratic >= maximum
        ? ~std::uint64_t{0}
        : static_cast<std::uint64_t>(quadratic);
    return saturatingAdd(
        saturatingMultiply(digits, IMPORT_UNITS_PER_DIGIT),
        square
    );
}

FinalVerificationRun FinalVerifier::verifyMemory(
    std::string_view candidate,
    VerificationIdentity identity,
    const BBPSamplePolicy& samplePolicy
)
{
    auto inspection = FinalOutputInspector::inspectMemory(
        candidate,
        identity.requestedDigits
    );
    if (!inspection.accepted())
    {
        return stoppedAfterStructure(
            identity,
            std::move(inspection.verification)
        );
    }

    const CanonicalDecimal& decimal = *inspection.decimal;
    auto known = KnownDigitsVerifier::verify(decimal);
    const auto hashStarted = std::chrono::steady_clock::now();
    const auto digest = SHA256::hash(decimal.bytes());
    VerificationCheckResult hashCheck(
        VerificationStage::hash,
        VerificationStatus::passed,
        std::chrono::steady_clock::now() - hashStarted
    );

    BBPCheck bbp = verifySamples(
        DecimalHexDigitExtractor(decimal),
        identity,
        samplePolicy,
        nullptr
    );
    std::vector<VerificationCheckResult> checks;
    checks.push_back(std::move(inspection.verification));
    checks.push_back(known.verification);
    checks.push_back(std::move(hashCheck));
    checks.push_back(std::move(bbp.check));
    return {
        FinalVerificationReport(identity, std::move(checks)),
        digest,
        std::move(known),
        std::move(bbp.samples)
    };
}

FinalVerificationRun FinalVerifier::verifyFile(
    const std::filesystem::path& path,
    VerificationIdentity identity,
    const BBPSamplePolicy& samplePolicy,
    VerificationWorkObserver* observer
)
{
    auto inspection = FinalOutputInspector::inspectFile(
        path,
        identity.requestedDigits,
        observer
    );
    if (!inspection.accepted())
    {
        return stoppedAfterStructure(
            identity,
            std::move(inspection.verification)
        );
    }

    const InspectedOutputFile& file = *inspection.file;
    auto known = KnownDigitsVerifier::verify(file);
    if (observer != nullptr)
    {
        observer->workCompleted(KNOWN_DIGITS_UNITS);
    }
    std::optional<SHA256Digest> digest;
    const auto hashStarted = std::chrono::steady_clock::now();
    VerificationCheckResult hashCheck = skipped(VerificationStage::hash);
    try
    {
        ScaledVerificationWork hashWork(observer, HASH_UNITS_PER_BYTE);
        digest = SHA256::hashFilePrefix(
            file.path,
            file.canonicalByteCount,
            &hashWork
        );
        hashCheck = VerificationCheckResult(
            VerificationStage::hash,
            VerificationStatus::passed,
            std::chrono::steady_clock::now() - hashStarted
        );
    }
    catch (const std::exception& error)
    {
        hashCheck = failed(
            VerificationStage::hash,
            VerificationReason::fileReadFailed,
            error.what(),
            hashStarted
        );
    }

    BBPCheck bbp{skipped(VerificationStage::bbp), {}};
    try
    {
        QuadraticImportWork importWork(observer);
        auto extractor = DecimalHexDigitExtractor::fromFile(file, &importWork);
        bbp = verifySamples(extractor, identity, samplePolicy, observer);
    }
    catch (const std::exception& error)
    {
        const auto started = std::chrono::steady_clock::now();
        bbp.check = failed(
            VerificationStage::bbp,
            VerificationReason::operationFailed,
            error.what(),
            started
        );
    }

    std::vector<VerificationCheckResult> checks;
    checks.push_back(std::move(inspection.verification));
    checks.push_back(known.verification);
    checks.push_back(std::move(hashCheck));
    checks.push_back(std::move(bbp.check));
    return {
        FinalVerificationReport(identity, std::move(checks)),
        std::move(digest),
        std::move(known),
        std::move(bbp.samples)
    };
}

FinalVerificationRun FinalVerifier::verifyFile(
    const std::filesystem::path& path,
    VerificationIdentity identity,
    const SHA256Digest& expectedSha256,
    const BBPSamplePolicy& samplePolicy
)
{
    FinalVerificationRun run = verifyFile(path, identity, samplePolicy);
    if (!run.sha256 || *run.sha256 == expectedSha256)
    {
        return run;
    }

    std::vector<VerificationCheckResult> checks;
    checks.reserve(run.report.checks().size());
    for (const auto& check : run.report.checks())
    {
        if (check.stage() == VerificationStage::hash)
        {
            checks.emplace_back(
                VerificationStage::hash,
                VerificationStatus::failed,
                check.duration(),
                std::vector<VerificationDiagnostic>{{
                    VerificationStage::hash,
                    VerificationReason::hashMismatch,
                    "Canonical SHA-256 differs from verification manifest",
                    std::nullopt
                }}
            );
        }
        else
        {
            checks.push_back(check);
        }
    }
    run.report = FinalVerificationReport(identity, std::move(checks));
    return run;
}

} // namespace pi::verification
