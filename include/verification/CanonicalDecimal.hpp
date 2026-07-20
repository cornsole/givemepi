#pragma once

#include "verification/FinalVerification.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>


namespace pi::verification
{

struct CanonicalDecimalInspection;


/// Owned canonical decimal representation `3.<requested digits>`.
///
/// Instances can only be produced by successful canonical inspection. The
/// stored bytes exclude any file record terminator and are the authoritative
/// input for final-output hashing and mathematical verification.
class CanonicalDecimal
{
public:
    [[nodiscard]] std::string_view bytes() const noexcept;
    [[nodiscard]] std::string_view fractionalDigits() const noexcept;
    [[nodiscard]] std::uint64_t requestedDigits() const noexcept;

    [[nodiscard]]
    bool operator==(const CanonicalDecimal&) const noexcept = default;

private:
    CanonicalDecimal(std::string bytes, std::uint64_t requestedDigits);

    std::string bytes_;
    std::uint64_t requestedDigits_;

    friend CanonicalDecimalInspection inspectCanonicalDecimal(
        std::string_view,
        std::uint64_t
    );
};


/// Result of validating a memory-resident canonical decimal.
struct CanonicalDecimalInspection
{
    VerificationCheckResult verification;
    std::optional<CanonicalDecimal> decimal;

    [[nodiscard]] bool accepted() const noexcept;
};


/// Validates and owns one normalized decimal pi value.
///
/// Contract: exactly `3.`, followed by requestedDigits ASCII characters in
/// `0` through `9`, with no sign, whitespace, exponent, newline, or trailing
/// bytes. The canonical hash domain is exactly the returned `bytes()` value.
///
/// Input: candidate bytes and a positive requested digit count.
/// Output: a passed structure check plus canonical value, or one structured
/// failure with the first invalid byte offset when applicable.
/// Time complexity: O(n). Memory complexity: O(n) only on success.
[[nodiscard]]
CanonicalDecimalInspection inspectCanonicalDecimal(
    std::string_view candidate,
    std::uint64_t requestedDigits
);

} // namespace pi::verification
