#pragma once

#include "bigint/GMPInteger.hpp"
#include "verification/BBPHexDigit.hpp"
#include "verification/CanonicalDecimal.hpp"
#include "verification/FinalOutputInspector.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <istream>
#include <optional>

namespace pi::verification
{

struct DecimalHexDigit
{
    std::uint64_t position;
    BBPDigitStatus status;
    std::optional<std::uint8_t> digit;

    [[nodiscard]] bool resolved() const noexcept;
};

/// Imports exactly `digitCount` decimal digits from the current stream
/// position into one non-negative GMP integer.
///
/// Fixed-size leaves are parsed independently and combined through a balanced
/// binary tree. The optional observer receives leaf-digit units plus the
/// combined digit span after every merge.
/// Time complexity: O(M(n) log n) for n digits.
/// Memory complexity: O(n).
/// Throws when `leafDigits` is zero, input ends early, or a byte is not a
/// decimal digit.
[[nodiscard]] pi::bigint::GMPInteger importDecimalDigits(
    std::istream& input,
    std::uint64_t digitCount,
    std::size_t leafDigits,
    VerificationWorkObserver* observer = nullptr
);



/// Reusable exact GMP representation of a finite canonical decimal fraction.
class DecimalHexDigitExtractor
{

public:
    /// Default decimal leaf size selected by the PR-0048 Release benchmark.
    static constexpr std::size_t DEFAULT_IMPORT_LEAF_DIGITS = 1024 * 1024;
    /// Parses the fractional digits once. Locale and floating point are unused.
    explicit DecimalHexDigitExtractor(const CanonicalDecimal& decimal);

    /// Streams an already inspected file into the same reusable GMP form.
    /// An optional observer receives balanced-import work units: one per
    /// parsed digit and one per digit in each completed merge.
    /// Throws when the file changed, cannot be read, or metadata is invalid.
    [[nodiscard]] static DecimalHexDigitExtractor fromFile(
        const InspectedOutputFile& file,
        VerificationWorkObserver* observer = nullptr
    );

    /// Extracts a zero-based hexadecimal fractional digit. The digit resolves
    /// only when the complete +/- 0.5 decimal ULP interval maps to one digit.
    [[nodiscard]] DecimalHexDigit extract(std::uint64_t position) const;

    [[nodiscard]] std::uint64_t requestedDigits() const noexcept;

private:
    DecimalHexDigitExtractor(
        pi::bigint::GMPInteger numerator,
        std::uint64_t requestedDigits
    );

    pi::bigint::GMPInteger numerator_;
    pi::bigint::GMPInteger denominator_;
    std::uint64_t requestedDigits_;
};

} // namespace pi::verification
