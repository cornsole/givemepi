#pragma once

#include "verification/BBPHexDigit.hpp"
#include "verification/BBPSamplePolicy.hpp"
#include "verification/FinalVerification.hpp"
#include "verification/KnownDigitsVerifier.hpp"
#include "verification/SHA256.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace pi::scheduler
{
class Scheduler;
}


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
    /// importWorkUnits(requestedDigits) for the BBP decimal import, and each
    /// selected BBP sample's work divided by the effective sample parallelism.
    /// A zero parallelism is treated as one; parallelism is capped at the
    /// selected sample count. Saturates at UINT64_MAX.
    /// Time complexity: O(s + log D) for s samples and D digits. Memory: O(s).
    [[nodiscard]] static std::uint64_t plannedFileWork(
        VerificationIdentity identity,
        const BBPSamplePolicy& samplePolicy = BBPSamplePolicy{},
        std::size_t sampleParallelism = 1
    );

    /// Returns the work units of the balanced decimal import.
    ///
    /// Units are one per parsed digit plus one per digit in each completed
    /// merge. The calculation replays the default leaf tree without allocating
    /// it, exactly matching successful fromFile() publications. Monotonic;
    /// saturates at UINT64_MAX.
    /// Time complexity: O(log D). Memory complexity: O(1).
    [[nodiscard]] static std::uint64_t importWorkUnits(std::uint64_t digits)
        noexcept;

    /// Verifies a stored output file.
    ///
    /// An optional observer receives completed units as structure, known
    /// digits, hash, BBP import, and each BBP sample finish; their sum
    /// equals plannedFileWork() when every stage runs to completion. When a
    /// running scheduler is supplied, independent BBP samples run concurrently;
    /// observer calls and result aggregation remain on the calling thread.
    [[nodiscard]] static FinalVerificationRun verifyFile(
        const std::filesystem::path& path,
        VerificationIdentity identity,
        const BBPSamplePolicy& samplePolicy = BBPSamplePolicy{},
        VerificationWorkObserver* observer = nullptr,
        pi::scheduler::Scheduler* executor = nullptr
    );

    [[nodiscard]] static FinalVerificationRun verifyFile(
        const std::filesystem::path& path,
        VerificationIdentity identity,
        const SHA256Digest& expectedSha256,
        const BBPSamplePolicy& samplePolicy = BBPSamplePolicy{},
        pi::scheduler::Scheduler* executor = nullptr
    );
};

} // namespace pi::verification
