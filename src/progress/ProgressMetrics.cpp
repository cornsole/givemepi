#include "progress/ProgressMetrics.hpp"

#include <algorithm>
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


long double nanoseconds(std::chrono::nanoseconds duration) noexcept
{
    return static_cast<long double>(duration.count());
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


/// Measured rate sample of the current phase: new units over the span from
/// phase entry to the latest unit.
struct PhaseSample
{
    std::uint64_t progressed;
    long double span;
};


std::optional<PhaseSample> currentPhaseSample(
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
    const std::uint64_t progressed =
        units.completed - snapshot.phaseBaselineUnits();
    const std::chrono::nanoseconds span =
        snapshot.phaseElapsed() - snapshot.sinceLastPhaseUnit();
    if (units.total == 0
        || progressed < policy.minimumCompletedPhaseUnits
        || span < policy.minimumElapsed)
    {
        return std::nullopt;
    }
    return PhaseSample{progressed, nanoseconds(span)};
}


/// Signed remaining time of the current phase in nanoseconds; negative when
/// the phase outlived its measured-rate prediction.
std::optional<long double> phaseRemaining(
    const ProgressSnapshot& snapshot,
    const std::optional<PhaseSample>& sample
) noexcept
{
    const ProgressPhase phase = snapshot.phase();
    if (!isWorkPhase(phase))
    {
        return std::nullopt;
    }
    const PhaseWorkUnits units = snapshot.phaseWork(phase);
    if (units.total != 0 && units.completed >= units.total)
    {
        // Unit publication can precede the phase transition. The current
        // phase has no work left during that gap, regardless of its baseline.
        return 0.0L;
    }
    if (!sample.has_value())
    {
        return std::nullopt;
    }
    return static_cast<long double>(units.total - units.completed)
        * sample->span / static_cast<long double>(sample->progressed)
        - nanoseconds(snapshot.sinceLastPhaseUnit());
}


/// Nanoseconds per normalized cost unit, from closed phases or, before any
/// planned phase closed, from the current phase sample.
std::optional<long double> calibration(
    const ProgressSnapshot& snapshot,
    const std::optional<PhaseSample>& sample
) noexcept
{
    long double duration = 0.0L;
    long double cost = 0.0L;
    for (std::size_t index = 0; index < PROGRESS_PHASE_COUNT; ++index)
    {
        const auto phase = static_cast<ProgressPhase>(index);
        const std::uint64_t planned = snapshot.phaseCost(phase);
        const PhaseWorkUnits units = snapshot.phaseWork(phase);
        const std::uint64_t closedUnits = snapshot.closedPhaseUnits(phase);
        if (!isWorkPhase(phase) || planned == 0 || units.total == 0
            || closedUnits == 0)
        {
            continue;
        }
        duration += nanoseconds(snapshot.closedPhaseDuration(phase));
        cost += normalizedWork(planned, units, closedUnits);
    }
    if (duration > 0.0L && cost > 0.0L)
    {
        return duration / cost;
    }

    const ProgressPhase current = snapshot.phase();
    const std::uint64_t planned = snapshot.phaseCost(current);
    if (!sample.has_value() || planned == 0)
    {
        return std::nullopt;
    }
    return sample->span / normalizedWork(
        planned,
        snapshot.phaseWork(current),
        sample->progressed
    );
}


/// Remaining planned cost of a phase from `from` completed units; the whole
/// cost while its units are undeclared.
long double remainingCost(
    const ProgressSnapshot& snapshot,
    ProgressPhase phase,
    std::uint64_t from
) noexcept
{
    const std::uint64_t planned = snapshot.phaseCost(phase);
    const PhaseWorkUnits units = snapshot.phaseWork(phase);
    if (planned == 0)
    {
        return 0.0L;
    }
    if (units.total == 0)
    {
        return static_cast<long double>(planned);
    }
    return normalizedWork(planned, units, units.total - from);
}


struct TotalEstimate
{
    std::optional<std::chrono::nanoseconds> value;
    EtaBasis basis;
};


/// End-to-end remaining time from the current phase remainder and the
/// calibrated cost of later phases.
TotalEstimate totalRemaining(
    const ProgressSnapshot& snapshot,
    const std::optional<PhaseSample>& sample,
    const std::optional<long double>& phase
) noexcept
{
    const std::size_t current = phaseIndex(snapshot.phase());
    bool planned = false;
    long double later = 0.0L;
    for (std::size_t index = 0; index < PROGRESS_PHASE_COUNT; ++index)
    {
        const auto candidate = static_cast<ProgressPhase>(index);
        if (!isWorkPhase(candidate) || snapshot.phaseCost(candidate) == 0)
        {
            continue;
        }
        planned = true;
        if (index > current)
        {
            later += remainingCost(
                snapshot,
                candidate,
                snapshot.phaseWork(candidate).completed
            );
        }
    }
    if (!planned)
    {
        return {std::nullopt, EtaBasis::unknown};
    }

    const std::optional<long double> rate = calibration(snapshot, sample);
    const auto withLater = [&](long double currentRemaining, EtaBasis basis) {
        if (later <= 0.0L)
        {
            return currentRemaining < 0.0L
                ? TotalEstimate{std::nullopt, EtaBasis::overdue}
                : TotalEstimate{saturatedDuration(currentRemaining), basis};
        }
        if (!rate.has_value())
        {
            return TotalEstimate{std::nullopt, EtaBasis::unknown};
        }
        return TotalEstimate{
            saturatedDuration(
                std::max(currentRemaining, 0.0L) + *rate * later
            ),
            basis
        };
    };

    if (phase.has_value())
    {
        return withLater(*phase, EtaBasis::measured);
    }

    const ProgressPhase currentPhase = snapshot.phase();
    const long double entryCost = isWorkPhase(currentPhase)
        ? remainingCost(
            snapshot,
            currentPhase,
            snapshot.phaseBaselineUnits()
        )
        : 0.0L;
    if (entryCost <= 0.0L && later <= 0.0L)
    {
        return {std::nullopt, EtaBasis::unknown};
    }
    if (!rate.has_value())
    {
        return {std::nullopt, EtaBasis::unknown};
    }
    const long double currentRemaining = entryCost > 0.0L
        ? *rate * entryCost - nanoseconds(snapshot.phaseElapsed())
        : 0.0L;
    return withLater(currentRemaining, EtaBasis::calibratedPlan);
}

} // namespace


std::string_view toString(EtaBasis basis) noexcept
{
    switch (basis)
    {
        case EtaBasis::measured:
            return "measured";
        case EtaBasis::calibratedPlan:
            return "calibrated_plan";
        case EtaBasis::overdue:
            return "overdue";
        case EtaBasis::unknown:
            return "unknown";
    }
    return "unknown";
}


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
            metrics.totalBasis = EtaBasis::measured;
            break;
        case ProgressTerminalState::failed:
            break;
        case ProgressTerminalState::running:
        {
            const std::optional<PhaseSample> sample =
                currentPhaseSample(snapshot, policy);
            const std::optional<long double> phase =
                phaseRemaining(snapshot, sample);
            if (phase.has_value())
            {
                metrics.phaseOverdue = *phase < 0.0L;
                if (!metrics.phaseOverdue)
                {
                    metrics.estimatedPhaseTimeRemaining =
                        saturatedDuration(*phase);
                }
            }
            const TotalEstimate total =
                totalRemaining(snapshot, sample, phase);
            metrics.estimatedTotalTimeRemaining = total.value;
            metrics.totalBasis = total.basis;
            break;
        }
    }

    return metrics;
}

} // namespace pi::progress
