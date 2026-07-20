#include "chudnovsky/ChudnovskyCalculator.hpp"
#include "verification/BBPSamplePolicy.hpp"
#include "verification/FinalVerifier.hpp"

#include <chrono>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

int main()
{
    using Clock = std::chrono::steady_clock;
    std::cout << "digits,samples,calculate_us,verify_us,verify_ratio\n";
    for (const std::uint64_t digits : {1000ULL, 10000ULL, 100000ULL})
    {
        std::vector<std::int64_t> calculationSamples;
        std::vector<std::int64_t> verificationSamples;
        for (int repetition = 0; repetition < 6; ++repetition)
        {
            const auto calculationStarted = Clock::now();
            const auto result =
                pi::chudnovsky::ChudnovskyCalculator::calculateSequential({digits});
            const auto calculationFinished = Clock::now();
            const pi::verification::VerificationIdentity identity{
                result.precision.requestedDigits,
                result.precision.guardDigits,
                result.precision.workingDigits,
                result.precision.termCount
            };
            const pi::verification::BBPSamplePolicy policy(8);
            const auto verificationStarted = Clock::now();
            const auto verification = pi::verification::FinalVerifier::verifyMemory(
                result.decimal,
                identity,
                policy
            );
            const auto verificationFinished = Clock::now();
            if (verification.report.status()
                != pi::verification::VerificationStatus::passed)
            {
                return 1;
            }
            if (repetition != 0)
            {
                calculationSamples.push_back(std::chrono::duration_cast<
                    std::chrono::microseconds
                >(calculationFinished - calculationStarted).count());
                verificationSamples.push_back(std::chrono::duration_cast<
                    std::chrono::microseconds
                >(verificationFinished - verificationStarted).count());
            }
        }
        std::sort(calculationSamples.begin(), calculationSamples.end());
        std::sort(verificationSamples.begin(), verificationSamples.end());
        const auto calculateUs = calculationSamples[calculationSamples.size() / 2];
        const auto verifyUs = verificationSamples[verificationSamples.size() / 2];
        const double ratio = calculateUs == 0
            ? 0.0
            : static_cast<double>(verifyUs) / static_cast<double>(calculateUs);
        std::cout << digits << ",8," << calculateUs << ',' << verifyUs
                  << ',' << ratio << '\n';
    }
    return 0;
}
