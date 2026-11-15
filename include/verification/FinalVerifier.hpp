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

    /// Returns the work units verifyFile publishes for an accepted output.
    ///
    /// Units approximate sequential byte passes: one per inspected file byte,
    /// one for the known-digit comparison, four per hashed canonical byte,
    /// importWorkUnits(requestedDigits) for the BBP decimal import, and for
    /// each selected BBP position (position + 1) series units plus eight per
    /// fractional digit for interval extraction. Saturates at UINT64_MAX.
    /// Time complexity: O(s) for s selected samples. Memory: O(s).
    [[nodiscard]] static std::uint64_t plannedFileWork(
        VerificationIdentity identity,
        const BBPSamplePolicy& samplePolicy = BBPSamplePolicy{}
    );

    /// Returns the cumulative work units of importing the first `digits`
    /// fractional digits into the BBP extractor.
    ///
    /// The import multiplies a growing integer by each nine-digit chunk, so
    /// its cost is quadratic: eight units per digit plus digits^2 / 256.
    /// verifyFile publishes the difference of this function across each
    /// import batch, so published import units sum to importWorkUnits(D) and
    /// grow in proportion to wall time. Monotonic; saturates at UINT64_MAX.
    /// Time and memory complexity: O(1).
    [[nodiscard]] static std::uint64_t importWorkUnits(std::uint64_t digits)
        noexcept;

    /// Verifies a stored output file.
    ///
    /// An optional observer receives completed units as structure, known
    /// digits, hash, BBP import, and each BBP sample finish; their sum
    /// equals plannedFileWork() when every stage runs to completion. Units
    /// are published on the calling thread only.
    [[nodiscard]] static FinalVerificationRun verifyFile(
        const std::filesystem::path& path,
        VerificationIdentity identity,
        const BBPSamplePolicy& samplePolicy = BBPSamplePolicy{},
        VerificationWorkObserver* observer = nullptr
    );

    [[nodiscard]] static FinalVerificationRun verifyFile(
        const std::filesystem::path& path,
        VerificationIdentity identity,
        const SHA256Digest& expectedSha256,
        const BBPSamplePolicy& samplePolicy = BBPSamplePolicy{}
    );
};

} // namespace pi::verification
