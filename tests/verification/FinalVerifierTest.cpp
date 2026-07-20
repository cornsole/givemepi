#include "verification/FinalVerifier.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <unistd.h>

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
