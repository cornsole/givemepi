#pragma once

#include "bigint/GMPInteger.hpp"
#include "verification/BBPHexDigit.hpp"
#include "verification/CanonicalDecimal.hpp"
#include "verification/FinalOutputInspector.hpp"

#include <cstdint>
#include <filesystem>
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


/// Reusable exact GMP representation of a finite canonical decimal fraction.
class DecimalHexDigitExtractor
{
public:
    /// Parses the fractional digits once. Locale and floating point are unused.
    explicit DecimalHexDigitExtractor(const CanonicalDecimal& decimal);

    /// Streams an already inspected file into the same reusable GMP form.
    /// Throws when the file changed, cannot be read, or metadata is invalid.
    [[nodiscard]] static DecimalHexDigitExtractor fromFile(
        const InspectedOutputFile& file
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
