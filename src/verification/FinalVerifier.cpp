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
    const BBPSamplePolicy& policy
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
        samplePolicy
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
    const BBPSamplePolicy& samplePolicy
)
{
    auto inspection = FinalOutputInspector::inspectFile(
        path,
        identity.requestedDigits
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
    std::optional<SHA256Digest> digest;
    const auto hashStarted = std::chrono::steady_clock::now();
    VerificationCheckResult hashCheck = skipped(VerificationStage::hash);
    try
    {
        digest = SHA256::hashFilePrefix(file.path, file.canonicalByteCount);
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
        auto extractor = DecimalHexDigitExtractor::fromFile(file);
        bbp = verifySamples(extractor, identity, samplePolicy);
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
