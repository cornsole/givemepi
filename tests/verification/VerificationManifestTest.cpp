#include "verification/VerificationManifest.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace
{
void failBeforeRename(pi::checkpoint::AtomicCommitStage stage)
{
    if (stage == pi::checkpoint::AtomicCommitStage::beforeRename)
        throw std::runtime_error("injected manifest commit failure");
}
}

int main()
{
    using namespace pi::verification;
    constexpr std::string_view digits =
        "14159265358979323846264338327950288419716939937510"
        "58209749445923078164062862089986280348253421170680";
    const auto run = FinalVerifier::verifyMemory(
        "3." + std::string(digits), {100, 32, 132, 9}
    );
    const auto manifest = VerificationManifest::fromRun(
        run, "pi-100.txt", 102, 1'753'000'000
    );
    const auto encoded = VerificationManifestCodec::encode(manifest);
    assert(encoded.size() == VerificationManifestCodec::version1HeaderSize
        + manifest.outputFilename.size() + manifest.bbpSamples.size() * 16);
    assert(VerificationManifestCodec::decode(encoded) == manifest);
    assert(VerificationManifestCodec::encode(
        VerificationManifestCodec::decode(encoded)) == encoded);

    auto corrupt = encoded;
    corrupt.back() ^= 1;
    bool corruptionRejected = false;
    try { (void)VerificationManifestCodec::decode(corrupt); }
    catch (const std::invalid_argument&) { corruptionRejected = true; }
    assert(corruptionRejected);

    auto failedRun = FinalVerifier::verifyMemory("3.0", {1, 1, 2, 1});
    bool failedRejected = false;
    try
    {
        (void)VerificationManifest::fromRun(failedRun, "bad.txt", 3, 1);
    }
    catch (const std::invalid_argument&) { failedRejected = true; }
    assert(failedRejected);

    const auto directory = std::filesystem::temp_directory_path()
        / ("pi-verification-manifest-" + std::to_string(::getpid()));
    std::filesystem::create_directory(directory);
    const auto path = directory / "pi-100.verify";
    VerificationManifestStore::save(path, manifest);
    assert(VerificationManifestStore::load(path) == manifest);

    bool commitFailed = false;
    try
    {
        VerificationManifestStore::save(path, manifest, failBeforeRename);
    }
    catch (const std::runtime_error&) { commitFailed = true; }
    assert(commitFailed);
    assert(VerificationManifestStore::load(path) == manifest);
    std::filesystem::remove_all(directory);
    return 0;
}
