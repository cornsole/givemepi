#include "scheduler/Scheduler.hpp"

#include "verification/DecimalHexDigit.hpp"
#include "verification/FinalVerifier.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <unistd.h>

namespace
{

/// Records published verification units and call count.
class RecordingWork final : public pi::verification::VerificationWorkObserver
{
public:
    void workCompleted(std::uint64_t units) noexcept override
    {
        assert(units != 0);
        total += units;
        ++calls;
    }

    std::uint64_t total = 0;
    std::uint64_t calls = 0;
};

} // namespace


int main()
{
    using namespace pi::verification;
    constexpr std::string_view digits =
        "14159265358979323846264338327950288419716939937510"
        "58209749445923078164062862089986280348253421170680";
    const std::string canonical = "3." + std::string(digits);
    const VerificationIdentity identity{100, 32, 132, 9};

    const auto memory = FinalVerifier::verifyMemory(canonical, identity);
    assert(memory.report.status() == VerificationStatus::passed);
    assert(memory.sha256.has_value());
    assert(memory.knownDigits->matched());
    assert(memory.bbpSamples.size() == DEFAULT_BBP_SAMPLE_COUNT);
    for (const auto& sample : memory.bbpSamples)
    {
        assert(sample.status == VerificationStatus::passed);
        assert(sample.bbpDigit == sample.outputDigit);
    }

    const auto malformed = FinalVerifier::verifyMemory("3.14\n", {2, 1, 3, 1});
    assert(malformed.report.status() == VerificationStatus::failed);
    assert(!malformed.sha256.has_value());
    assert(malformed.report.find(VerificationStage::knownDigits)->status()
        == VerificationStatus::skipped);
    assert(malformed.report.find(VerificationStage::hash)->status()
        == VerificationStatus::skipped);

    std::string wrong = canonical;
    wrong[10] = wrong[10] == '0' ? '1' : '0';
    const auto corrupted = FinalVerifier::verifyMemory(wrong, identity);
    assert(corrupted.report.status() == VerificationStatus::failed);
    assert(corrupted.report.find(VerificationStage::knownDigits)->status()
        == VerificationStatus::failed);
    assert(corrupted.report.find(VerificationStage::hash)->status()
        == VerificationStatus::passed);
    assert(corrupted.sha256.has_value());

    const auto path = std::filesystem::temp_directory_path()
        / ("pi-final-verifier-" + std::to_string(::getpid()) + ".txt");
    {
        std::ofstream output(path, std::ios::binary);
        output << canonical << '\n';
    }
    const auto file = FinalVerifier::verifyFile(path, identity);
    assert(file.report.status() == VerificationStatus::passed);
    assert(file.sha256 == memory.sha256);
    assert(file.bbpSamples.size() == memory.bbpSamples.size());
    pi::scheduler::Scheduler verificationScheduler(4, 32);
    verificationScheduler.start();
    RecordingWork parallelWork;
    const BBPSamplePolicy parallelPolicy;
    const auto parallelFile = FinalVerifier::verifyFile(
        path,
        identity,
        parallelPolicy,
        &parallelWork,
        &verificationScheduler
    );
    verificationScheduler.stop();
    assert(parallelFile.report.status() == VerificationStatus::passed);
    assert(parallelFile.sha256 == file.sha256);
    assert(parallelFile.bbpSamples.size() == file.bbpSamples.size());
    for (std::size_t index = 0; index < file.bbpSamples.size(); ++index)
    {
        assert(parallelFile.bbpSamples[index].position
            == file.bbpSamples[index].position);
        assert(parallelFile.bbpSamples[index].status
            == file.bbpSamples[index].status);
        assert(parallelFile.bbpSamples[index].bbpDigit
            == file.bbpSamples[index].bbpDigit);
        assert(parallelFile.bbpSamples[index].outputDigit
            == file.bbpSamples[index].outputDigit);
    }
    assert(parallelWork.total
        == FinalVerifier::plannedFileWork(identity, parallelPolicy, 4));
    assert(parallelWork.total
        < FinalVerifier::plannedFileWork(identity, parallelPolicy));


    // Progress units of a complete run sum exactly to the planned work:
    // 103 inspected bytes, 1 known-digit unit, 4 * 102 hashed bytes, the
    // balanced import of 100 digits, and per sample
    // (position + 1) + 8 * 100.
    constexpr std::uint64_t leaf =
        DecimalHexDigitExtractor::DEFAULT_IMPORT_LEAF_DIGITS;
    assert(FinalVerifier::importWorkUnits(0) == 0);
    assert(FinalVerifier::importWorkUnits(100) == 100);
    assert(FinalVerifier::importWorkUnits(leaf - 1) == leaf - 1);
    assert(FinalVerifier::importWorkUnits(leaf) == leaf);
    assert(FinalVerifier::importWorkUnits(leaf + 1) == 2 * leaf + 2);
    assert(FinalVerifier::importWorkUnits(2 * leaf) == 4 * leaf);
    assert(FinalVerifier::importWorkUnits(1'000'000) == 1'000'000);
    assert(FinalVerifier::importWorkUnits(10'000'000) == 46'777'216);
    assert(FinalVerifier::importWorkUnits(~std::uint64_t{0})
        == ~std::uint64_t{0});
    RecordingWork work;
    const BBPSamplePolicy policy;
    const auto observed = FinalVerifier::verifyFile(
        path,
        identity,
        policy,
        &work
    );
    assert(observed.report.status() == VerificationStatus::passed);
    std::uint64_t expected = 103 + 1 + 4 * 102 + 100;
    for (const std::uint64_t position : policy.select(identity))
    {
        expected += position + 1 + 8 * 100;
    }
    assert(FinalVerifier::plannedFileWork(identity, policy) == expected);
    assert(work.total == expected);
    assert(work.calls >= 4 + policy.select(identity).size());

    std::string middleMutation = canonical;
    middleMutation[50] = middleMutation[50] == '0' ? '1' : '0';
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << middleMutation << '\n';
    }
    const auto hashRejected = FinalVerifier::verifyFile(
        path,
        identity,
        *memory.sha256
    );
    assert(hashRejected.report.status() == VerificationStatus::failed);
    assert(hashRejected.report.find(VerificationStage::hash)
        ->diagnostics()[0].reason == VerificationReason::hashMismatch);

    const auto inconclusive = FinalVerifier::verifyMemory(
        "3.1250",
        {4, 1, 5, 1}
    );
    assert(inconclusive.report.find(VerificationStage::bbp)->status()
        == VerificationStatus::inconclusive);
    std::filesystem::remove(path);
    return 0;
}
