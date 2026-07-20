#include "verification/BBPHexDigit.hpp"

#include <cassert>
#include <limits>
#include <stdexcept>
#include <string_view>

int main()
{
    using pi::verification::BBPHexDigitCalculator;
    constexpr std::string_view knownHex =
        "243F6A8885A308D313198A2E03707344"
        "A4093822299F31D0082EFA98EC4E6C89";
    for (std::uint64_t position = 0; position < knownHex.size(); ++position)
    {
        const auto result = BBPHexDigitCalculator::calculate(position);
        assert(result.resolved());
        const char expected = knownHex[static_cast<std::size_t>(position)];
        const auto expectedValue = static_cast<std::uint8_t>(
            expected <= '9' ? expected - '0' : expected - 'A' + 10
        );
        assert(*result.digit == expectedValue);
        assert(result.position == position);
        assert(result.errorBound >= 0);
        assert(result.tailTerms != 0);
    }

    const auto distant = BBPHexDigitCalculator::calculate(1000);
    assert(distant.resolved());
    assert(*distant.digit == 4);

    bool rejected = false;
    try
    {
        (void)BBPHexDigitCalculator::calculate(
            std::numeric_limits<std::uint64_t>::max()
        );
    }
    catch (const std::out_of_range&)
    {
        rejected = true;
    }
    assert(rejected);
    return 0;
}
