#include "scheduler/Scheduler.hpp"

#include "chudnovsky/ChudnovskyCalculator.hpp"
#include "verification/BBPSamplePolicy.hpp"
#include "verification/DecimalHexDigit.hpp"
#include "verification/FinalVerifier.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

namespace
{

int benchmarkImport(
    const std::filesystem::path& path,
    std::uint64_t digits,
    std::size_t leafDigits
)
{
    std::ifstream input(path, std::ios::binary);
    char prefix[2]{};
    input.read(prefix, 2);
    if (!input || prefix[0] != '3' || prefix[1] != '.')
    {
        throw std::runtime_error("benchmark input is not canonical decimal");
    }

    const auto started = std::chrono::steady_clock::now();
    const auto value = pi::verification::importDecimalDigits(
        input,
        digits,
        leafDigits
    );
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - started
    ).count();
    std::cout << "digits,leaf_digits,import_us,bits,remainder_1000000007\n"
              << digits << ',' << leafDigits << ',' << elapsed << ','
              << mpz_sizeinbase(*value.raw(), 2) << ','
              << mpz_fdiv_ui(*value.raw(), 1'000'000'007UL) << '\n';
    return 0;
}

int benchmarkFileVerification(
    const std::filesystem::path& path,
    pi::verification::VerificationIdentity identity
)
{
    pi::scheduler::Scheduler scheduler(13, 128);
    scheduler.start();
    const auto started = std::chrono::steady_clock::now();
    const auto verification = pi::verification::FinalVerifier::verifyFile(
        path,
        identity,
        pi::verification::BBPSamplePolicy(8),
        nullptr,
        &scheduler
    );
    scheduler.stop();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - started
    ).count();
    if (verification.report.status()
            != pi::verification::VerificationStatus::passed
        || !verification.sha256.has_value())
    {
        return 1;
    }
    std::cout << "digits,samples,file_verify_us,sha256\n"
              << identity.requestedDigits << ",8," << elapsed << ','
              << pi::verification::toHex(*verification.sha256) << '\n';
    return 0;
}

} // namespace


int main(
    int argc,
    char** argv
)
{
    if (argc == 5 && std::string(argv[1]) == "--import-file")
    {
        try
        {
            return benchmarkImport(
                argv[2],
                std::stoull(argv[3]),
                std::stoull(argv[4])
            );
        }
        catch (const std::exception& error)
        {
            std::cerr << "final-verification-benchmark: " << error.what()
                      << '\n';
            return 1;
        }
    }

    if (argc == 7 && std::string(argv[1]) == "--verify-file")
    {
        try
        {
            return benchmarkFileVerification(
                argv[2],
                {
                    std::stoull(argv[3]),
                    static_cast<std::uint32_t>(std::stoul(argv[4])),
                    std::stoull(argv[5]),
                    std::stoull(argv[6])
                }
            );
        }
        catch (const std::exception& error)
        {
            std::cerr << "final-verification-benchmark: " << error.what()
                      << '\n';
            return 1;
        }
    }

    std::vector<std::uint64_t> digitsMatrix{1000ULL, 10000ULL, 100000ULL};
    int repetitions = 6;
    if (argc == 2 && std::string(argv[1]) == "--quick")
    {
        // CI-safe quick profile: one warmup plus one measured sample per row.
        digitsMatrix = {1000ULL, 10000ULL};
        repetitions = 2;
    }
    else if (argc != 1)
    {
        std::cerr
            << "final-verification-benchmark: unsupported arguments; usage: "
               "final-verification-benchmark [--quick] | "
               "--import-file <path> <digits> <leaf-digits> | "
               "--verify-file <path> <digits> <guard> <working> <terms>\n";
        return 1;
    }

    using Clock = std::chrono::steady_clock;
    std::cout
        << "digits,samples,calculate_us,memory_verify_us,file_verify_us,"
           "memory_verify_ratio,file_verify_ratio\n";
    const auto benchmarkPath = std::filesystem::temp_directory_path()
        / ("pi-final-verification-benchmark-"
           + std::to_string(::getpid()) + ".txt");
    for (const std::uint64_t digits : digitsMatrix)
    {
        std::vector<std::int64_t> calculationSamples;
        std::vector<std::int64_t> memoryVerificationSamples;
        std::vector<std::int64_t> fileVerificationSamples;
        for (int repetition = 0; repetition < repetitions; ++repetition)
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
            {
                std::ofstream output(benchmarkPath, std::ios::binary);
                output << result.decimal << '\n';
            }
            const auto fileVerificationStarted = Clock::now();
            const auto fileVerification =
                pi::verification::FinalVerifier::verifyFile(
                    benchmarkPath,
                    identity,
                    policy
                );
            const auto fileVerificationFinished = Clock::now();
            if (verification.report.status()
                    != pi::verification::VerificationStatus::passed
                || fileVerification.report.status()
                    != pi::verification::VerificationStatus::passed
                || fileVerification.sha256 != verification.sha256)
            {
                std::filesystem::remove(benchmarkPath);
                return 1;
            }
            if (repetition != 0)
            {
                calculationSamples.push_back(std::chrono::duration_cast<
                    std::chrono::microseconds
                >(calculationFinished - calculationStarted).count());
                memoryVerificationSamples.push_back(std::chrono::duration_cast<
                    std::chrono::microseconds
                >(verificationFinished - verificationStarted).count());
                fileVerificationSamples.push_back(std::chrono::duration_cast<
                    std::chrono::microseconds
                >(fileVerificationFinished - fileVerificationStarted).count());
            }
        }
        std::sort(calculationSamples.begin(), calculationSamples.end());
        std::sort(
            memoryVerificationSamples.begin(),
            memoryVerificationSamples.end()
        );
        std::sort(
            fileVerificationSamples.begin(),
            fileVerificationSamples.end()
        );
        const auto calculateUs = calculationSamples[calculationSamples.size() / 2];
        const auto memoryVerifyUs =
            memoryVerificationSamples[memoryVerificationSamples.size() / 2];
        const auto fileVerifyUs =
            fileVerificationSamples[fileVerificationSamples.size() / 2];
        const double memoryRatio = calculateUs == 0
            ? 0.0
            : static_cast<double>(memoryVerifyUs) / static_cast<double>(calculateUs);
        const double fileRatio = calculateUs == 0
            ? 0.0
            : static_cast<double>(fileVerifyUs) / static_cast<double>(calculateUs);
        std::cout << digits << ",8," << calculateUs << ',' << memoryVerifyUs
                  << ',' << fileVerifyUs << ',' << memoryRatio << ','
                  << fileRatio << '\n';
    }
    std::filesystem::remove(benchmarkPath);
    return 0;
}
