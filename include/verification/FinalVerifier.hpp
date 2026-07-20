#pragma once

#include "verification/BBPHexDigit.hpp"
#include "verification/BBPSamplePolicy.hpp"
#include "verification/FinalVerification.hpp"
#include "verification/KnownDigitsVerifier.hpp"
#include "verification/SHA256.hpp"

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace pi::verification
{

struct BBPSampleVerification
{
    std::uint64_t position;
    VerificationStatus status;
    std::optional<std::uint8_t> bbpDigit;
    std::optional<std::uint8_t> outputDigit;
    long double bbpErrorBound;
};

struct FinalVerificationRun
{
    FinalVerificationReport report;
    std::optional<SHA256Digest> sha256;
    std::optional<KnownDigitsVerification> knownDigits;
    std::vector<BBPSampleVerification> bbpSamples;
};


/// Coordinates independent final-output checks without calculator state.
class FinalVerifier
{
public:
    [[nodiscard]] static FinalVerificationRun verifyMemory(
        std::string_view candidate,
        VerificationIdentity identity,
        const BBPSamplePolicy& samplePolicy = BBPSamplePolicy{}
    );

    [[nodiscard]] static FinalVerificationRun verifyFile(
        const std::filesystem::path& path,
        VerificationIdentity identity,
        const BBPSamplePolicy& samplePolicy = BBPSamplePolicy{}
    );

    [[nodiscard]] static FinalVerificationRun verifyFile(
        const std::filesystem::path& path,
        VerificationIdentity identity,
        const SHA256Digest& expectedSha256,
        const BBPSamplePolicy& samplePolicy = BBPSamplePolicy{}
    );
};

} // namespace pi::verification
