#include "chudnovsky/ChudnovskyCalculator.hpp"
#include "scheduler/Scheduler.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>


using pi::binary::ParallelSplitOptions;
using pi::chudnovsky::ChudnovskyCalculator;
using pi::chudnovsky::PiCalculationRequest;
using pi::chudnovsky::PiCalculationResult;
using pi::scheduler::Scheduler;


namespace
{

struct Sample
{
    std::int64_t split;
    std::int64_t finalize;
    std::int64_t format;
    std::int64_t total;
};


Sample toMicros(
    const PiCalculationResult::Timings& timings
)
{
    const auto micros = [](std::chrono::nanoseconds value)
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            value
        ).count();
    };

    return Sample{
        micros(timings.split),
        micros(timings.finalize),
        micros(timings.format),
        micros(timings.total())
    };
}


Sample median(
    std::vector<Sample> samples
)
{
    const auto select = [&samples](auto member)
    {
        std::vector<std::int64_t> values;
        values.reserve(samples.size());

        for (const Sample& sample : samples)
        {
            values.push_back(sample.*member);
        }

        std::sort(values.begin(), values.end());
        return values[values.size() / 2];
    };

    return Sample{
        select(&Sample::split),
        select(&Sample::finalize),
        select(&Sample::format),
        select(&Sample::total)
    };
}


template <typename Calculate>
Sample measure(
    Calculate&& calculate,
    std::string& expected
)
{
    std::vector<Sample> samples;

    for (int repetition = 0; repetition < 6; ++repetition)
    {
        PiCalculationResult result = calculate();

        if (expected.empty())
        {
            expected = result.decimal;
        }
        else if (result.decimal != expected)
        {
            std::abort();
        }

        if (repetition != 0)
        {
            samples.push_back(toMicros(result.timings));
        }
    }

    return median(std::move(samples));
}


void print(
    std::size_t digits,
    const char* mode,
    std::size_t workers,
    const Sample& sample
)
{
    std::cout << digits << ',' << mode << ',' << workers << ','
              << sample.split << ',' << sample.finalize << ','
              << sample.format << ',' << sample.total << '\n';
}

} // namespace


int main()
{
    std::cout
        << "digits,mode,workers,split_us,finalize_us,format_us,total_us\n";

    for (const std::size_t digits : {1000U, 10000U, 100000U, 1000000U})
    {
        std::string expected;
        const Sample sequential = measure(
            [digits]()
            {
                return ChudnovskyCalculator::calculateSequential(
                    PiCalculationRequest{digits}
                );
            },
            expected
        );
        print(digits, "sequential", 1, sequential);

        for (const std::size_t workers : {4U, 8U})
        {
            Scheduler scheduler(workers, 512);
            scheduler.start();
            const Sample parallel = measure(
                [&]()
                {
                    return ChudnovskyCalculator::calculateParallel(
                        PiCalculationRequest{digits},
                        scheduler,
                        ParallelSplitOptions{128, 4}
                    );
                },
                expected
            );
            scheduler.stop();
            print(digits, "parallel", workers, parallel);
        }
    }
}
