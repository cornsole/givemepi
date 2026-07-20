#include "verification/BBPHexDigit.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace pi::verification
{
namespace
{

struct SeriesResult
{
    long double fractional;
    long double tailBound;
    std::uint32_t tailTerms;
};

std::uint64_t addModulo(
    std::uint64_t left,
    std::uint64_t right,
    std::uint64_t modulus
) noexcept
{
    return left >= modulus - right
        ? left - (modulus - right)
        : left + right;
}

std::uint64_t multiplyModulo(
    std::uint64_t left,
    std::uint64_t right,
    std::uint64_t modulus
) noexcept
{
    if (left == 0 || right <= std::numeric_limits<std::uint64_t>::max() / left)
    {
        return (left * right) % modulus;
    }
    std::uint64_t result = 0;
    left %= modulus;
    while (right != 0)
    {
        if ((right & 1U) != 0)
        {
            result = addModulo(result, left, modulus);
        }
        right >>= 1U;
        if (right != 0)
        {
            left = addModulo(left, left, modulus);
        }
    }
    return result;
}

std::uint64_t power16Modulo(
    std::uint64_t exponent,
    std::uint64_t modulus
) noexcept
{
    std::uint64_t result = 1 % modulus;
    std::uint64_t base = 16 % modulus;
    while (exponent != 0)
    {
        if ((exponent & 1U) != 0)
        {
            result = multiplyModulo(result, base, modulus);
        }
        exponent >>= 1U;
        if (exponent != 0)
        {
            base = multiplyModulo(base, base, modulus);
        }
    }
    return result;
}

SeriesResult series(std::uint64_t position, std::uint64_t offset)
{
    long double sum = 0;
    for (std::uint64_t k = 0;; ++k)
    {
        const std::uint64_t denominator = 8 * k + offset;
        sum += static_cast<long double>(
            power16Modulo(position - k, denominator)
        ) / static_cast<long double>(denominator);
        sum -= std::floor(sum);
        if (k == position)
        {
            break;
        }
    }

    long double power = 1.0L / 16.0L;
    std::uint64_t k = position + 1;
    std::uint32_t tailTerms = 0;
    while (tailTerms < 64)
    {
        const auto denominator = 8 * k + offset;
        sum += power / static_cast<long double>(denominator);
        ++tailTerms;
        ++k;
        power /= 16.0L;
        if (power <= std::numeric_limits<long double>::epsilon())
        {
            break;
        }
    }
    sum -= std::floor(sum);
    const long double tailBound = power
        / static_cast<long double>(8 * k + offset) * (16.0L / 15.0L);
    return {sum, tailBound, tailTerms};
}

} // namespace

bool BBPHexDigit::resolved() const noexcept
{
    return status == BBPDigitStatus::resolved && digit.has_value();
}

BBPHexDigit BBPHexDigitCalculator::calculate(std::uint64_t position)
{
    // Reserve all 64 tail terms plus the first omitted term so every
    // denominator remains representable.
    if (position > maximumPosition())
    {
        throw std::out_of_range("BBP hexadecimal position is too large");
    }

    const SeriesResult s1 = series(position, 1);
    const SeriesResult s4 = series(position, 4);
    const SeriesResult s5 = series(position, 5);
    const SeriesResult s6 = series(position, 6);
    long double fraction = 4 * s1.fractional - 2 * s4.fractional
        - s5.fractional - s6.fractional;
    fraction -= std::floor(fraction);

    const long double tailBound = 4 * s1.tailBound + 2 * s4.tailBound
        + s5.tailBound + s6.tailBound;
    const long double operationCount = static_cast<long double>(position + 1)
        * 4.0L + s1.tailTerms + s4.tailTerms + s5.tailTerms + s6.tailTerms;
    const long double roundingBound =
        64.0L * std::numeric_limits<long double>::epsilon()
        * std::max(1.0L, operationCount);
    const long double errorBound = tailBound + roundingBound;

    const long double scaled = fraction * 16.0L;
    const long double lowerBoundary = std::floor(scaled);
    const long double distance = std::min(
        scaled - lowerBoundary,
        lowerBoundary + 1.0L - scaled
    );
    const auto tailTerms = std::max({
        s1.tailTerms, s4.tailTerms, s5.tailTerms, s6.tailTerms
    });
    if (errorBound * 16.0L >= distance)
    {
        return {position, BBPDigitStatus::inconclusive, std::nullopt,
            errorBound, tailTerms};
    }
    return {position, BBPDigitStatus::resolved,
        static_cast<std::uint8_t>(lowerBoundary), errorBound, tailTerms};
}

} // namespace pi::verification
