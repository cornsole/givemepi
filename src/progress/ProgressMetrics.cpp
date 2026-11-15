#include "progress/ProgressMetrics.hpp"

#include <cmath>
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


/// Converts a non-negative nanosecond estimate, saturating on overflow.
std::chrono::nanoseconds saturatedDuration(long double nanoseconds) noexcept
{
    const long double maximum = static_cast<long double>(
        std::chrono::nanoseconds::max().count()
    );
    if (!std::isfinite(nanoseconds) || nanoseconds >= maximum)
    {
        return std::chrono::nanoseconds::max();
    }
    if (nanoseconds <= 0.0L)
    {
        return std::chrono::nanoseconds::zero();
    }
    return std::chrono::nanoseconds(
        static_cast<std::chrono::nanoseconds::rep>(nanoseconds)
    );
}


/// Remaining time of the running phase from its own observed rate.
std::optional<std::chrono::nanoseconds> phaseRemaining(
    const ProgressSnapshot& snapshot,
    const ProgressMetricPolicy& policy
) noexcept
{
    const ProgressPhase phase = snapshot.phase();
    if (!isWorkPhase(phase))
    {
        return std::nullopt;
    }
    const PhaseWorkUnits units = snapshot.phaseWork(phase);
    if (units.total == 0)
    {
        return std::nullopt;
    }
    if (units.completed >= units.total)
    {
        return std::chrono::nanoseconds::zero();
    }

    const std::uint64_t progressed =
        units.completed - snapshot.phaseBaselineUnits();
    if (snapshot.phaseElapsed() < policy.minimumElapsed
        || progressed < policy.minimumCompletedPhaseUnits)
    {
        return std::nullopt;
    }
    return saturatedDuration(
        static_cast<long double>(units.total - units.completed)
        * static_cast<long double>(snapshot.phaseElapsed().count())
        / static_cast<long double>(progressed)
    );
}


/// Normalized cost represented by `count` units of a phase.
long double normalizedWork(
    std::uint64_t cost,
    const PhaseWorkUnits& units,
    std::uint64_t count
) noexcept
{
    return static_cast<long double>(cost)
        * static_cast<long double>(count)
        / static_cast<long double>(units.total);
}


/// End-to-end remaining time combining the phase rate and the observed
/// normalized pipeline rate.
std::optional<std::chrono::nanoseconds> totalRemaining(
    const ProgressSnapshot& snapshot,
    const ProgressMetricPolicy& policy,
    const std::optional<std::chrono::nanoseconds>& phaseEstimate
) noexcept
{
    const std::size_t current = phaseIndex(snapshot.phase());
    long double plannedCost = 0.0L;
    long double newlyCompleted = 0.0L;
    long double remainingCurrent = 0.0L;
    long double remainingLater = 0.0L;

    for (std::size_t index = 0; index < PROGRESS_PHASE_COUNT; ++index)
    {
        const auto phase = static_cast<ProgressPhase>(index);
        const std::uint64_t cost = snapshot.phaseCost(phase);
        if (!isWorkPhase(phase) || cost == 0)
        {
            continue;
        }
        plannedCost += static_cast<long double>(cost);

        const PhaseWorkUnits units = snapshot.phaseWork(phase);
        const std::uint64_t restored = phase == ProgressPhase::splitting
            ? snapshot.restoredTerms()
            : 0;
        if (units.total != 0)
        {
            newlyCompleted += normalizedWork(
                cost,
                units,
                units.completed - restored
            );
        }
        if (index < current)
        {
            continue;
        }

        const long double remaining = units.total == 0
            ? static_cast<long double>(cost)
            : normalizedWork(cost, units, units.total - units.completed);
        (index == current ? remainingCurrent : remainingLater) += remaining;
    }

    if (plannedCost <= 0.0L)
    {
        return std::nullopt;
    }

    const std::chrono::nanoseconds elapsed = snapshot.elapsed();
    const bool rateKnown = elapsed >= policy.minimumElapsed
        && newlyCompleted > 0.0L;
    const auto atObservedRate = [&](long double work) {
        return saturatedDuration(
            work * static_cast<long double>(elapsed.count()) / newlyCompleted
        );
    };

    if (phaseEstimate.has_value())
    {
        if (remainingLater <= 0.0L)
        {
            return phaseEstimate;
        }
        if (!rateKnown)
        {
            return std::nullopt;
        }
        const std::chrono::nanoseconds later = atObservedRate(remainingLater);
        return later > std::chrono::nanoseconds::max() - *phaseEstimate
            ? std::chrono::nanoseconds::max()
            : *phaseEstimate + later;
    }

    const long double remaining = remainingCurrent + remainingLater;
    if (remaining <= 0.0L)
    {
        return std::chrono::nanoseconds::zero();
    }
    if (!rateKnown)
    {
        return std::nullopt;
    }
    return atObservedRate(remaining);
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
    if (policy.minimumCompletedPhaseUnits == 0)
    {
        throw std::invalid_argument(
            "Minimum completed phase units must be greater than zero"
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
    const std::uint64_t newTerms =
        snapshot.completedTerms() - snapshot.restoredTerms();
    if (enoughElapsed && newTerms >= policy.minimumCompletedTerms)
    {
        metrics.termsPerSecond = ratePerSecond(newTerms, snapshot.elapsed());
    }

    const std::uint64_t newBlocks = snapshot.completedCheckpointBlocks()
        - snapshot.restoredCheckpointBlocks();
    if (enoughElapsed
        && newBlocks >= policy.minimumCompletedCheckpointBlocks)
    {
        metrics.checkpointBlocksPerSecond = ratePerSecond(
            newBlocks,
            snapshot.elapsed()
        );
    }

    switch (snapshot.terminalState())
    {
        case ProgressTerminalState::completed:
            metrics.estimatedTotalTimeRemaining =
                std::chrono::nanoseconds::zero();
            metrics.estimatedPhaseTimeRemaining =
                std::chrono::nanoseconds::zero();
            break;
        case ProgressTerminalState::failed:
            break;
        case ProgressTerminalState::running:
            metrics.estimatedPhaseTimeRemaining =
                phaseRemaining(snapshot, policy);
            metrics.estimatedTotalTimeRemaining = totalRemaining(
                snapshot,
                policy,
                metrics.estimatedPhaseTimeRemaining
            );
            break;
    }

    return metrics;
}

} // namespace pi::progress
