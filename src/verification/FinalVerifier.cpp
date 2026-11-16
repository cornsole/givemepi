#include "verification/FinalVerifier.hpp"

#include "scheduler/Scheduler.hpp"
#include "scheduler/Task.hpp"

#include "verification/DecimalHexDigit.hpp"
#include "verification/FinalOutputInspector.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <string>
#include <utility>

namespace pi::verification
{
namespace
{

constexpr std::uint64_t HASH_UNITS_PER_BYTE = 4;
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

/// Work of one BBP sample: series terms plus interval extraction, divided by
/// the effective parallel sample count.
std::uint64_t sampleUnits(
    std::uint64_t position,
    std::uint64_t requestedDigits,
    std::size_t parallelism
) noexcept
{
    const std::uint64_t sequential = saturatingAdd(
        saturatingAdd(position, 1),
        saturatingMultiply(requestedDigits, EXTRACTION_UNITS_PER_DIGIT)
    );
    const std::uint64_t divisor = std::max<std::size_t>(1, parallelism);
    return sequential / divisor + (sequential % divisor != 0 ? 1 : 0);
}


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
struct ComputedSample
{
    BBPHexDigit expected;
    DecimalHexDigit actual;
};


BBPCheck verifySamples(
    const DecimalHexDigitExtractor& extractor,
    const VerificationIdentity& identity,
    const BBPSamplePolicy& policy,
    VerificationWorkObserver* observer,
    pi::scheduler::Scheduler* executor
)
{
    const auto started = std::chrono::steady_clock::now();
    const auto positions = policy.select(identity);
    if (positions.empty())
    {
        return {skipped(VerificationStage::bbp), {}};
    }
    const std::size_t parallelism =
        executor != nullptr && executor->running()
        ? std::max<std::size_t>(
            1,
            std::min(executor->workerCount(), positions.size())
        )
        : 1;

    std::vector<std::optional<ComputedSample>> computed(positions.size());
    std::vector<pi::scheduler::TaskHandle> handles;
    handles.reserve(positions.size());
    for (std::size_t index = 0; index < positions.size(); ++index)
    {
        const auto calculate = [&computed, &extractor, &positions, index]()
        {
            const std::uint64_t position = positions[index];
            computed[index] = ComputedSample{
                BBPHexDigitCalculator::calculate(position),
                extractor.extract(position)
            };
        };

        pi::scheduler::TaskHandle handle;
        if (executor != nullptr)
        {
            handle = executor->submit(pi::scheduler::Task(calculate));
        }
        if (handle.valid())
        {
            handles.push_back(std::move(handle));
        }
        else
        {
            calculate();
        }
    }
    for (const pi::scheduler::TaskHandle& handle : handles)
    {
        handle.wait();
    }
    for (const pi::scheduler::TaskHandle& handle : handles)
    {
        handle.rethrowIfFailed();
    }

    std::vector<BBPSampleVerification> samples;
    std::vector<VerificationDiagnostic> diagnostics;
    samples.reserve(positions.size());
    VerificationStatus aggregate = VerificationStatus::passed;
    for (std::size_t index = 0; index < positions.size(); ++index)
    {
        const std::uint64_t position = positions[index];
        const auto& expected = computed[index]->expected;
        const auto& actual = computed[index]->actual;
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
                sampleUnits(
                    position,
                    identity.requestedDigits,
                    parallelism
                )
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
    const BBPSamplePolicy& samplePolicy,
    std::size_t sampleParallelism
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
    const auto positions = samplePolicy.select(identity);
    const std::size_t parallelism = positions.empty()
        ? 1
        : std::max<std::size_t>(
            1,
            std::min(sampleParallelism, positions.size())
        );
    for (const std::uint64_t position : positions)
    {
        units = saturatingAdd(
            units,
            sampleUnits(position, digits, parallelism)
        );
    }
    return units;
}

std::uint64_t FinalVerifier::importWorkUnits(std::uint64_t digits) noexcept
{
    constexpr std::uint64_t leaf =
        DecimalHexDigitExtractor::DEFAULT_IMPORT_LEAF_DIGITS;
    std::uint64_t units = digits;
    const std::uint64_t fullLeaves = digits / leaf;

    std::uint64_t groups = fullLeaves;
    std::uint64_t mergedDigits = leaf;
    while (groups >= 2)
    {
        groups /= 2;
        mergedDigits = saturatingMultiply(mergedDigits, 2);
        units = saturatingAdd(
            units,
            saturatingMultiply(groups, mergedDigits)
        );
    }

    std::uint64_t accumulated = digits % leaf;
    bool hasAccumulated = accumulated != 0;
    std::uint64_t leafGroupDigits = leaf;
    for (std::uint64_t remaining = fullLeaves;
         remaining != 0;
         remaining >>= 1)
    {
        if ((remaining & 1U) != 0)
        {
            if (!hasAccumulated)
            {
                accumulated = leafGroupDigits;
                hasAccumulated = true;
            }
            else
            {
                accumulated = saturatingAdd(accumulated, leafGroupDigits);
                units = saturatingAdd(units, accumulated);
            }
        }
        leafGroupDigits = saturatingMultiply(leafGroupDigits, 2);
    }
    return units;
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
        nullptr,
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
    VerificationWorkObserver* observer,
    pi::scheduler::Scheduler* executor
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
        auto extractor = DecimalHexDigitExtractor::fromFile(file, observer);
        bbp = verifySamples(
            extractor,
            identity,
            samplePolicy,
            observer,
            executor
        );
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
    const BBPSamplePolicy& samplePolicy,
    pi::scheduler::Scheduler* executor
)
{
    FinalVerificationRun run = verifyFile(
        path,
        identity,
        samplePolicy,
        nullptr,
        executor
    );
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
