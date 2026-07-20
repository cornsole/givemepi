#pragma once

#include "verification/FinalVerification.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pi::verification
{

inline constexpr std::size_t DEFAULT_BBP_SAMPLE_COUNT = 8;
inline constexpr std::size_t MAXIMUM_BBP_SAMPLE_COUNT = 32;


/// Deterministic, bounded policy for selecting independent BBP checks.
class BBPSamplePolicy
{
public:
    explicit BBPSamplePolicy(
        std::size_t maximumSamples = DEFAULT_BBP_SAMPLE_COUNT
    );

    [[nodiscard]] std::size_t maximumSamples() const noexcept;

    /// Returns sorted, unique zero-based hexadecimal fractional positions.
    /// The usable range conservatively excludes two decimal guard positions;
    /// exact DecimalHexDigitExtractor intervals remain authoritative.
    /// Time and memory complexity: O(maximumSamples), schema-bounded to 32.
    [[nodiscard]]
    std::vector<std::uint64_t> select(
        const VerificationIdentity& identity
    ) const;

    /// Exclusive conservative hex-position bound derived without floating
    /// point and without overflowing for uint64 decimal digit counts.
    [[nodiscard]]
    static std::uint64_t safePositionCount(
        std::uint64_t requestedDigits
    ) noexcept;

private:
    std::size_t maximumSamples_;
};

} // namespace pi::verification
