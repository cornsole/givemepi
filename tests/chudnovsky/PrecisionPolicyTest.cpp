#include "chudnovsky/PrecisionPolicy.hpp"

#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>


using pi::chudnovsky::PrecisionPlan;
using pi::chudnovsky::PrecisionPolicy;


int main()
{
    bool rejectedZeroDigits = false;

    try
    {
        static_cast<void>(PrecisionPolicy::create(0));
    }
    catch (const std::invalid_argument&)
    {
        rejectedZeroDigits = true;
    }

    if (!rejectedZeroDigits)
    {
        std::cerr << "Zero requested digits were accepted\n";
        return 1;
    }


    bool rejectedInsufficientGuard = false;

    try
    {
        static_cast<void>(
            PrecisionPolicy::create(
                1,
                PrecisionPolicy::minimumGuardDigits - 1
            )
        );
    }
    catch (const std::invalid_argument&)
    {
        rejectedInsufficientGuard = true;
    }

    if (!rejectedInsufficientGuard)
    {
        std::cerr << "Insufficient guard digits were accepted\n";
        return 1;
    }


    const PrecisionPlan minimumPlan = PrecisionPolicy::create(1, 16);

    if (minimumPlan.requestedDigits != 1
        || minimumPlan.guardDigits != 16
        || minimumPlan.workingDigits != 17
        || minimumPlan.termCount != 3
        || minimumPlan.estimatedBits != 128)
    {
        std::cerr << "Minimum precision plan mismatch\n";
        return 1;
    }


    const PrecisionPlan defaultPlan = PrecisionPolicy::create(1000);
    const PrecisionPlan largerPlan = PrecisionPolicy::create(1001);

    if (defaultPlan.guardDigits != PrecisionPolicy::defaultGuardDigits
        || defaultPlan.workingDigits != 1032
        || defaultPlan.termCount != 74
        || defaultPlan.estimatedBits != 3499
        || largerPlan.workingDigits <= defaultPlan.workingDigits
        || largerPlan.termCount < defaultPlan.termCount
        || largerPlan.estimatedBits < defaultPlan.estimatedBits)
    {
        std::cerr << "Default or monotonic precision plan mismatch\n";
        return 1;
    }


    const PrecisionPlan billionDigitPlan =
        PrecisionPolicy::create(1000000000ULL);

    if (billionDigitPlan.workingDigits != 1000000032ULL
        || billionDigitPlan.termCount != 70513675
        || billionDigitPlan.estimatedBits != 3322000177ULL)
    {
        std::cerr << "Billion-digit precision plan mismatch\n";
        return 1;
    }


    bool rejectedOverflow = false;

    try
    {
        static_cast<void>(
            PrecisionPolicy::create(
                std::numeric_limits<std::uint64_t>::max()
            )
        );
    }
    catch (const std::overflow_error&)
    {
        rejectedOverflow = true;
    }

    if (!rejectedOverflow)
    {
        std::cerr << "Overflowing precision request was accepted\n";
        return 1;
    }


    std::cout << "PrecisionPolicy OK\n";
    return 0;
}
