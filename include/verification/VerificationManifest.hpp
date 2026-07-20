#pragma once

#include "checkpoint/AtomicFileCommit.hpp"
#include "verification/FinalVerifier.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace pi::verification
{

struct VerificationManifestSample
{
    std::uint64_t position;
    VerificationStatus status;
    std::uint8_t bbpDigit;
    std::uint8_t outputDigit;

    [[nodiscard]] bool operator==(const VerificationManifestSample&) const noexcept = default;
};

struct VerificationManifest
{
    VerificationIdentity identity;
    std::string outputFilename;
    std::uint64_t canonicalByteCount;
    SHA256Digest sha256;
    std::uint32_t knownDigitsReferenceVersion;
    std::uint64_t verifiedAtUnixSeconds;
    VerificationStatus overallStatus;
    std::vector<VerificationManifestSample> bbpSamples;

    [[nodiscard]] bool operator==(const VerificationManifest&) const noexcept = default;

    /// Builds a publishable manifest. Only fully passed runs are accepted.
    [[nodiscard]] static VerificationManifest fromRun(
        const FinalVerificationRun& run,
        std::string outputFilename,
        std::uint64_t canonicalByteCount,
        std::uint64_t verifiedAtUnixSeconds
    );
};

class VerificationManifestCodec
{
public:
    static constexpr std::uint16_t formatVersion = 1;
    static constexpr std::uint16_t version1HeaderSize = 120;

    [[nodiscard]] static std::vector<std::uint8_t> encode(
        const VerificationManifest& manifest
    );
    [[nodiscard]] static VerificationManifest decode(
        std::span<const std::uint8_t> bytes
    );
};

class VerificationManifestStore
{
public:
    static void save(
        const std::filesystem::path& destination,
        const VerificationManifest& manifest,
        pi::checkpoint::AtomicCommitObserver observer = nullptr
    );
    [[nodiscard]] static VerificationManifest load(
        const std::filesystem::path& path
    );
};

} // namespace pi::verification
