#pragma once

#include "verification/CanonicalDecimal.hpp"
#include "verification/FinalOutputInspector.hpp"

#include <cstdint>
#include <string_view>


namespace pi::verification
{

inline constexpr std::uint32_t KNOWN_DIGITS_REFERENCE_VERSION = 1;


/// Result metadata for a known-prefix comparison.
struct KnownDigitsVerification
{
    VerificationCheckResult verification;
    std::uint32_t referenceVersion;
    std::uint64_t referenceDigits;
    std::uint64_t comparedDigits;

    [[nodiscard]] bool matched() const noexcept;
};


/// Fast, deterministic comparison against the project's versioned pi prefix.
class KnownDigitsVerifier
{
public:
    [[nodiscard]] static std::string_view referenceFractionalDigits() noexcept;

    /// Compares a structurally accepted memory value against the known prefix.
    /// Time complexity: O(min(requested digits, reference digits)).
    /// Memory complexity: O(1).
    [[nodiscard]]
    static KnownDigitsVerification verify(const CanonicalDecimal& decimal);

    /// Compares only the bounded canonical prefix of an inspected output file.
    /// The complete output is never loaded into memory. The file must still be
    /// unchanged since structural inspection; a changed prefix is reported as
    /// a mismatch and an unreadable/short file as a read failure.
    /// Time complexity and memory are bounded by the reference size.
    [[nodiscard]]
    static KnownDigitsVerification verify(const InspectedOutputFile& file);
};

} // namespace pi::verification
