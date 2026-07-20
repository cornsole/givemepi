#include "chudnovsky/ChudnovskyCalculator.hpp"
#include "scheduler/Scheduler.hpp"
#include "progress/ProgressTracker.hpp"

#include <iostream>
#include <stdexcept>


using pi::binary::ParallelSplitOptions;
using pi::chudnovsky::ChudnovskyCalculator;
using pi::chudnovsky::PiCalculationRequest;
using pi::chudnovsky::PiCalculationResult;
using pi::scheduler::Scheduler;


namespace
{

bool matches(
    std::uint64_t digits,
    const char* expected
)
{
    const PiCalculationResult result =
        ChudnovskyCalculator::calculateSequential(
            PiCalculationRequest{digits}
        );

    return result.precision.requestedDigits == digits
        && result.decimal == expected;
}

} // namespace


int main()
{
    if (!matches(1, "3.1")
        || !matches(2, "3.14")
        || !matches(10, "3.1415926536")
        || !matches(20, "3.14159265358979323846")
        || !matches(
            50,
            "3.14159265358979323846264338327950288419716939937511"
        ))
    {
        std::cerr << "Sequential known-digits calculation failed\n";
        return 1;
    }


    bool rejectedZeroDigits = false;

    try
    {
        static_cast<void>(
            ChudnovskyCalculator::calculateSequential(
                PiCalculationRequest{0}
            )
        );
    }
    catch (const std::invalid_argument&)
    {
        rejectedZeroDigits = true;
    }

    if (!rejectedZeroDigits)
    {
        std::cerr << "Invalid calculation request was accepted\n";
        return 1;
    }


    const PiCalculationRequest parallelRequest{100};
    const PiCalculationResult sequential =
        ChudnovskyCalculator::calculateSequential(parallelRequest);

    Scheduler stoppedScheduler(4, 128);
    const PiCalculationResult stoppedFallback =
        ChudnovskyCalculator::calculateParallel(
            parallelRequest,
            stoppedScheduler,
            ParallelSplitOptions{1, 4}
        );

    Scheduler scheduler(4, 128);
    scheduler.start();
    const PiCalculationResult parallel =
        ChudnovskyCalculator::calculateParallel(
            parallelRequest,
            scheduler,
            ParallelSplitOptions{1, 4}
        );
    scheduler.stop();

    const std::string knownRounded100 =
        "3.14159265358979323846264338327950288419716939937510"
        "58209749445923078164062862089986280348253421170680";

    if (sequential.decimal != stoppedFallback.decimal
        || sequential.decimal != parallel.decimal
        || sequential.decimal != knownRounded100
        || sequential.decimal.size() != 102
        || sequential.precision.termCount != parallel.precision.termCount)
    {
        std::cerr << "Parallel end-to-end calculation failed\n";
        return 1;
    }


    const PiCalculationResult guard16 =
        ChudnovskyCalculator::calculateSequential(
            PiCalculationRequest{1000, 16}
        );

    const PiCalculationResult guard32 =
        ChudnovskyCalculator::calculateSequential(
            PiCalculationRequest{1000, 32}
        );

    const PiCalculationResult guard64 =
        ChudnovskyCalculator::calculateSequential(
            PiCalculationRequest{1000, 64}
        );

    if (guard16.decimal != guard32.decimal
        || guard32.decimal != guard64.decimal
        || guard16.decimal.size() != 1002
        || guard16.precision.termCount >= guard64.precision.termCount
        || guard16.timings.total().count() < 0
        || guard32.timings.total().count() < 0
        || guard64.timings.total().count() < 0)
    {
        std::cerr << "Guard stability or timing contract failed\n";
        return 1;
    }

    const auto progressPlan = pi::chudnovsky::PrecisionPolicy::create(100);
    pi::progress::ProgressTracker sequentialProgress({
        progressPlan.requestedDigits,
        progressPlan.termCount,
        0
    });
    const PiCalculationResult progressResult =
        ChudnovskyCalculator::calculateSequential(
            PiCalculationRequest{100},
            &sequentialProgress
        );
    const auto progressSnapshot = sequentialProgress.snapshot();
    if (progressResult.decimal != knownRounded100
        || progressSnapshot.phase()
            != pi::progress::ProgressPhase::completed
        || progressSnapshot.completedTerms() != progressPlan.termCount
        || progressSnapshot.activeTasks() != 0
        || progressSnapshot.queuedTasks() != 0)
    {
        std::cerr << "End-to-end sequential progress mismatch\n";
        return 1;
    }

    pi::progress::ProgressTracker parallelProgress({
        progressPlan.requestedDigits,
        progressPlan.termCount,
        0
    });
    Scheduler progressScheduler(2, 64);
    progressScheduler.start();
    static_cast<void>(ChudnovskyCalculator::calculateParallel(
        PiCalculationRequest{100},
        progressScheduler,
        ParallelSplitOptions{1, 2},
        &parallelProgress
    ));
    progressScheduler.stop();
    if (parallelProgress.snapshot().phase()
            != pi::progress::ProgressPhase::completed
        || parallelProgress.snapshot().completedTerms()
            != progressPlan.termCount)
    {
        std::cerr << "End-to-end parallel progress mismatch\n";
        return 1;
    }


    std::cout << "ChudnovskyCalculator OK\n";
    return 0;
}
