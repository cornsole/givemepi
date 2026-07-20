#include "chudnovsky/PrecisionPolicy.hpp"

#include <limits>
#include <stdexcept>


namespace pi::chudnovsky
{

namespace
{

std::uint64_t checkedAdd(
    std::uint64_t left,
    std::uint64_t right
)
{
    if (right > std::numeric_limits<std::uint64_t>::max() - left)
    {
        throw std::overflow_error(
            "Precision plan addition overflow"
        );
    }

    return left + right;
}


std::uint64_t multiplyDivideCeil(
    std::uint64_t value,
    std::uint64_t multiplier,
    std::uint64_t divisor
)
{
    const std::uint64_t quotient = value / divisor;
    const std::uint64_t remainder = value % divisor;

    if (quotient > std::numeric_limits<std::uint64_t>::max() / multiplier)
    {
        throw std::overflow_error(
            "Precision plan multiplication overflow"
        );
    }

    const std::uint64_t whole = quotient * multiplier;
    const std::uint64_t remainderProduct = remainder * multiplier;
    const std::uint64_t fraction =
        remainderProduct / divisor
        + (remainderProduct % divisor != 0 ? 1 : 0);

    return checkedAdd(whole, fraction);
}

} // namespace


PrecisionPlan PrecisionPolicy::create(
    std::uint64_t requestedDigits,
    std::uint32_t guardDigits
)
{
    if (requestedDigits == 0)
    {
        throw std::invalid_argument(
            "Requested pi digits must be greater than zero"
        );
    }

    if (guardDigits < minimumGuardDigits)
    {
        throw std::invalid_argument(
            "Guard digits are below the supported minimum"
        );
    }


    const std::uint64_t workingDigits = checkedAdd(
        requestedDigits,
        guardDigits
    );

    // 14.181647 is a conservative fixed-point lower bound for the decimal
    // digits contributed by one Chudnovsky term. One additional term protects
    // finite-range and finalization boundaries without floating-point policy.
    constexpr std::uint64_t digitsPerTermScale = 1000000;
    constexpr std::uint64_t digitsPerTermLower = 14181647;
    constexpr std::uint64_t safetyTerms = 1;

    const std::uint64_t termCount64 = checkedAdd(
        multiplyDivideCeil(
            workingDigits,
            digitsPerTermScale,
            digitsPerTermLower
        ),
        safetyTerms
    );

    if (termCount64 > std::numeric_limits<std::size_t>::max())
    {
        throw std::overflow_error(
            "Chudnovsky term count does not fit size_t"
        );
    }

    // log2(10) < 3.322. Two decimal scale digits and 64 bits of headroom cover
    // the integer part and fixed-point finalization intermediates.
    constexpr std::uint64_t binaryScaleNumerator = 3322;
    constexpr std::uint64_t binaryScaleDenominator = 1000;
    constexpr std::uint64_t binaryHeadroom = 64;

    const std::uint64_t estimatedBits = checkedAdd(
        multiplyDivideCeil(
            checkedAdd(workingDigits, 2),
            binaryScaleNumerator,
            binaryScaleDenominator
        ),
        binaryHeadroom
    );

    return PrecisionPlan{
        requestedDigits,
        guardDigits,
        workingDigits,
        static_cast<std::size_t>(termCount64),
        estimatedBits
    };
}

} // namespace pi::chudnovsky
