#include "verification/DecimalHexDigit.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace pi::verification
{
namespace
{

pi::bigint::GMPInteger parseFraction(const CanonicalDecimal& decimal)
{
    pi::bigint::GMPInteger value;
    // CanonicalDecimal owns a null-terminated std::string, so its fractional
    // suffix can be parsed directly without another output-sized copy.
    if (mpz_set_str(*value.raw(), decimal.bytes().data() + 2, 10) != 0)
    {
        throw std::invalid_argument("Invalid canonical decimal digits");
    }
    return value;
}

void appendDecimalChunk(
    pi::bigint::GMPInteger& value,
    std::string_view digits
)
{
    unsigned long chunk = 0;
    for (const char digit : digits)
    {
        if (digit < '0' || digit > '9')
        {
            throw std::runtime_error("Output changed after inspection");
        }
        chunk = chunk * 10UL + static_cast<unsigned long>(digit - '0');
    }
    unsigned long scale = 1;
    for (std::size_t index = 0; index < digits.size(); ++index)
    {
        scale *= 10UL;
    }
    mpz_mul_ui(*value.raw(), *value.raw(), scale);
    mpz_add_ui(*value.raw(), *value.raw(), chunk);
}

} // namespace

bool DecimalHexDigit::resolved() const noexcept
{
    return status == BBPDigitStatus::resolved && digit.has_value();
}

DecimalHexDigitExtractor::DecimalHexDigitExtractor(
    const CanonicalDecimal& decimal
)
    : DecimalHexDigitExtractor(
        parseFraction(decimal),
        decimal.requestedDigits()
    )
{
}

DecimalHexDigitExtractor::DecimalHexDigitExtractor(
    pi::bigint::GMPInteger numerator,
    std::uint64_t requestedDigits
)
    : numerator_(std::move(numerator)),
      denominator_(),
      requestedDigits_(requestedDigits)
{
    if (requestedDigits_ == 0)
    {
        throw std::invalid_argument("Decimal hex extraction requires digits");
    }
    denominator_.setPowerOfTen(requestedDigits_);
}

DecimalHexDigitExtractor DecimalHexDigitExtractor::fromFile(
    const InspectedOutputFile& file
)
{
    if (file.requestedDigits == 0
        || file.requestedDigits > std::numeric_limits<std::uint64_t>::max() - 3
        || file.canonicalByteCount != file.requestedDigits + 2
        || file.fileSize != file.canonicalByteCount + 1)
    {
        throw std::invalid_argument("Invalid inspected output metadata");
    }

    std::ifstream input(file.path, std::ios::binary);
    char prefix[2]{};
    input.read(prefix, 2);
    if (!input || prefix[0] != '3' || prefix[1] != '.')
    {
        throw std::runtime_error("Output changed after inspection");
    }

    pi::bigint::GMPInteger numerator;
    std::uint64_t remaining = file.requestedDigits;
    std::string chunk(9, '\0');
    while (remaining != 0)
    {
        const auto count = static_cast<std::size_t>(
            std::min<std::uint64_t>(remaining, chunk.size())
        );
        input.read(chunk.data(), static_cast<std::streamsize>(count));
        if (input.gcount() != static_cast<std::streamsize>(count))
        {
            throw std::runtime_error("Output ended during decimal conversion");
        }
        appendDecimalChunk(numerator, std::string_view(chunk.data(), count));
        remaining -= count;
    }
    char newline = '\0';
    input.get(newline);
    if (newline != '\n' || input.peek() != std::char_traits<char>::eof())
    {
        throw std::runtime_error("Output framing changed after inspection");
    }
    return DecimalHexDigitExtractor(std::move(numerator), file.requestedDigits);
}

DecimalHexDigit DecimalHexDigitExtractor::extract(
    std::uint64_t position
) const
{
    if (position == std::numeric_limits<std::uint64_t>::max()
        || position + 1 > std::numeric_limits<mp_bitcnt_t>::max() / 4)
    {
        throw std::out_of_range("Hexadecimal position is too large");
    }
    const mp_bitcnt_t shift = static_cast<mp_bitcnt_t>(4 * (position + 1));

    // center = D / 10^n, true value is conservatively in center +/- 0.5/10^n.
    // Multiplying all interval terms by two keeps the calculation integral.
    pi::bigint::GMPInteger center(numerator_);
    mpz_mul_2exp(*center.raw(), *center.raw(), shift + 1);
    pi::bigint::GMPInteger radius(1);
    mpz_mul_2exp(*radius.raw(), *radius.raw(), shift);
    pi::bigint::GMPInteger doubledDenominator(denominator_);
    mpz_mul_2exp(
        *doubledDenominator.raw(),
        *doubledDenominator.raw(),
        1
    );

    pi::bigint::GMPInteger lower(center);
    lower.sub(radius);
    lower.floorDivide(doubledDenominator);
    pi::bigint::GMPInteger upper(center);
    upper.add(radius);
    upper.floorDivide(doubledDenominator);
    if (lower.compare(upper) != 0)
    {
        return {position, BBPDigitStatus::inconclusive, std::nullopt};
    }
    const auto digit = static_cast<std::uint8_t>(
        mpz_fdiv_ui(*lower.raw(), 16)
    );
    return {position, BBPDigitStatus::resolved, digit};
}

std::uint64_t DecimalHexDigitExtractor::requestedDigits() const noexcept
{
    return requestedDigits_;
}

} // namespace pi::verification
