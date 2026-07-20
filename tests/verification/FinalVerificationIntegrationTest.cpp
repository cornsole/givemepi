#include "chudnovsky/ChudnovskyCalculator.hpp"
#include "verification/FinalVerifier.hpp"
#include "verification/VerificationManifest.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

int main()
{
    using namespace pi::verification;
    for (const std::uint64_t digits : {1ULL, 10ULL, 50ULL, 100ULL})
    {
        const auto small =
            pi::chudnovsky::ChudnovskyCalculator::calculateSequential({digits});
        const auto verified = FinalVerifier::verifyMemory(
            small.decimal,
            {
                small.precision.requestedDigits,
                small.precision.guardDigits,
                small.precision.workingDigits,
                small.precision.termCount
            }
        );
        assert(verified.report.status() == VerificationStatus::passed);
    }
    const auto calculated =
        pi::chudnovsky::ChudnovskyCalculator::calculateSequential({1000});
    const VerificationIdentity identity{
        calculated.precision.requestedDigits,
        calculated.precision.guardDigits,
        calculated.precision.workingDigits,
        calculated.precision.termCount
    };

    const auto memory = FinalVerifier::verifyMemory(
        calculated.decimal,
        identity
    );
    assert(memory.report.status() == VerificationStatus::passed);
    assert(memory.bbpSamples.size() == DEFAULT_BBP_SAMPLE_COUNT);

    const auto directory = std::filesystem::temp_directory_path()
        / ("pi-final-integration-" + std::to_string(::getpid()));
    std::filesystem::create_directory(directory);
    const auto outputPath = directory / "pi-1000.txt";
    const auto manifestPath = directory / "pi-1000.txt.verify";
    {
        std::ofstream output(outputPath, std::ios::binary);
        output << calculated.decimal << '\n';
    }

    const auto file = FinalVerifier::verifyFile(outputPath, identity);
    assert(file.report.status() == VerificationStatus::passed);
    assert(file.sha256 == memory.sha256);
    assert(file.bbpSamples.size() == memory.bbpSamples.size());
    for (std::size_t index = 0; index < file.bbpSamples.size(); ++index)
    {
        assert(file.bbpSamples[index].position
            == memory.bbpSamples[index].position);
        assert(file.bbpSamples[index].bbpDigit
            == memory.bbpSamples[index].bbpDigit);
        assert(file.bbpSamples[index].outputDigit
            == memory.bbpSamples[index].outputDigit);
    }

    const auto manifest = VerificationManifest::fromRun(
        file,
        outputPath.filename().string(),
        calculated.decimal.size(),
        1'753'000'001
    );
    VerificationManifestStore::save(manifestPath, manifest);
    assert(VerificationManifestStore::load(manifestPath) == manifest);

    for (const std::size_t offset : {
        calculated.decimal.size() / 2,
        calculated.decimal.size() - 1
    })
    {
        std::string mutated = calculated.decimal;
        mutated[offset] = mutated[offset] == '0' ? '1' : '0';
        {
            std::ofstream output(
                outputPath,
                std::ios::binary | std::ios::trunc
            );
            output << mutated << '\n';
        }
        const auto rejectedFile = FinalVerifier::verifyFile(
            outputPath,
            identity,
            manifest.sha256
        );
        assert(rejectedFile.report.status() == VerificationStatus::failed);
        assert(rejectedFile.report.find(VerificationStage::hash)->status()
            == VerificationStatus::failed);
    }

    // A structurally valid mutation must not retain a passed math report.
    std::string corrupted = calculated.decimal;
    corrupted[20] = corrupted[20] == '0' ? '1' : '0';
    const auto rejected = FinalVerifier::verifyMemory(corrupted, identity);
    assert(rejected.report.status() == VerificationStatus::failed);
    assert(rejected.report.find(VerificationStage::knownDigits)->status()
        == VerificationStatus::failed);

    std::filesystem::remove_all(directory);
    return 0;
}
