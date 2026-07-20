#pragma once

#include <cstdint>
#include <limits>
#include <optional>

namespace pi::verification
{

enum class BBPDigitStatus { resolved, inconclusive };

struct BBPHexDigit
{
    std::uint64_t position;
    BBPDigitStatus status;
    std::optional<std::uint8_t> digit;
    long double errorBound;
    std::uint32_t tailTerms;

    [[nodiscard]] bool resolved() const noexcept;
};

class BBPHexDigitCalculator
{
public:
    [[nodiscard]]
    static constexpr std::uint64_t maximumPosition() noexcept
    {
        return (std::numeric_limits<std::uint64_t>::max() - 6) / 8 - 65;
    }

    /// Calculates the zero-based hexadecimal fractional digit of pi without
    /// constructing preceding digits. If the bounded numeric interval crosses
    /// a digit boundary, the result is inconclusive instead of guessed.
    /// Time complexity: O(position log(position)). Memory complexity: O(1).
    [[nodiscard]] static BBPHexDigit calculate(std::uint64_t position);
};

} // namespace pi::verification
