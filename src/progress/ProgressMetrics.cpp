#include "progress/ProgressMetrics.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>


namespace pi::progress
{
namespace
{

double ratePerSecond(
    std::uint64_t completed,
    std::chrono::nanoseconds elapsed
) noexcept
{
    constexpr long double nanosecondsPerSecond = 1'000'000'000.0L;
    return static_cast<double>(
        static_cast<long double>(completed) * nanosecondsPerSecond
        / static_cast<long double>(elapsed.count())
    );
}


std::chrono::nanoseconds estimateRemaining(
    std::uint64_t remaining,
    double rate
) noexcept
{
    constexpr long double nanosecondsPerSecond = 1'000'000'000.0L;
    const long double estimate =
        static_cast<long double>(remaining) * nanosecondsPerSecond
        / static_cast<long double>(rate);
    const long double maximum = static_cast<long double>(
        std::chrono::nanoseconds::max().count()
    );

    if (!std::isfinite(estimate) || estimate >= maximum)
    {
        return std::chrono::nanoseconds::max();
    }
    return std::chrono::nanoseconds(
        static_cast<std::chrono::nanoseconds::rep>(estimate)
    );
}

} // namespace


void ProgressMetricsCalculator::validatePolicy(
    const ProgressMetricPolicy& policy
)
{
    if (policy.minimumElapsed <= std::chrono::nanoseconds::zero())
    {
        throw std::invalid_argument(
            "Minimum metric sample time must be positive"
        );
    }
    if (policy.minimumCompletedTerms == 0)
    {
        throw std::invalid_argument(
            "Minimum completed terms must be greater than zero"
        );
    }
    if (policy.minimumCompletedCheckpointBlocks == 0)
    {
        throw std::invalid_argument(
            "Minimum completed checkpoint blocks must be greater than zero"
        );
    }
}


ProgressMetrics ProgressMetricsCalculator::calculate(
    const ProgressSnapshot& snapshot,
    const ProgressMetricPolicy& policy
)
{
    validatePolicy(policy);
    ProgressMetrics metrics;

    if (snapshot.totalTerms() > 0)
    {
        metrics.completionRatio = static_cast<double>(
            static_cast<long double>(snapshot.completedTerms())
            / static_cast<long double>(snapshot.totalTerms())
        );
    }

    const bool enoughElapsed = snapshot.elapsed() >= policy.minimumElapsed;
    if (enoughElapsed
        && snapshot.completedTerms() >= policy.minimumCompletedTerms)
    {
        metrics.termsPerSecond = ratePerSecond(
            snapshot.completedTerms(),
            snapshot.elapsed()
        );
    }

    if (enoughElapsed
        && snapshot.completedCheckpointBlocks()
            >= policy.minimumCompletedCheckpointBlocks)
    {
        metrics.checkpointBlocksPerSecond = ratePerSecond(
            snapshot.completedCheckpointBlocks(),
            snapshot.elapsed()
        );
    }

    if (snapshot.phase() == ProgressPhase::completed)
    {
        metrics.estimatedTimeRemaining = std::chrono::nanoseconds::zero();
    }
    else if (snapshot.terminalState() == ProgressTerminalState::running
        && snapshot.completedTerms() < snapshot.totalTerms()
        && metrics.termsPerSecond.has_value()
        && *metrics.termsPerSecond > 0.0)
    {
        metrics.estimatedTimeRemaining = estimateRemaining(
            snapshot.totalTerms() - snapshot.completedTerms(),
            *metrics.termsPerSecond
        );
    }

    return metrics;
}

} // namespace pi::progress
