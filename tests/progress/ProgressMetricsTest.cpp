#include "progress/ProgressMetrics.hpp"

#include <cassert>
#include <chrono>
#include <cmath>
#include <stdexcept>


namespace
{

template<typename Exception, typename Function>
bool throws(Function&& function)
{
    try
    {
        function();
    }
    catch (const Exception&)
    {
        return true;
    }
    return false;
}


bool near(double value, double expected) noexcept
{
    return std::abs(value - expected) < 1e-12;
}

} // namespace


int main()
{
    using namespace std::chrono_literals;
    using pi::progress::ProgressMetricPolicy;
    using pi::progress::ProgressMetricsCalculator;
    using pi::progress::ProgressPhase;
    using pi::progress::ProgressSnapshot;
    using pi::progress::ProgressSnapshotData;

    ProgressSnapshotData data;
    data.phase = ProgressPhase::splitting;
    data.targetDigits = 1'000;
    data.totalTerms = 100;
    data.completedTerms = 25;
    data.totalCheckpointBlocks = 10;
    data.completedCheckpointBlocks = 2;
    data.elapsed = 5s;

    const auto metrics = ProgressMetricsCalculator::calculate(
        ProgressSnapshot(data)
    );
    assert(metrics.completionRatio.has_value());
    assert(near(*metrics.completionRatio, 0.25));
    assert(metrics.termsPerSecond.has_value());
    assert(near(*metrics.termsPerSecond, 5.0));
    assert(metrics.checkpointBlocksPerSecond.has_value());
    assert(near(*metrics.checkpointBlocksPerSecond, 0.4));
    assert(metrics.estimatedTimeRemaining == 15s);

    data.elapsed = 500ms;
    const auto early = ProgressMetricsCalculator::calculate(
        ProgressSnapshot(data)
    );
    assert(early.completionRatio.has_value());
    assert(!early.termsPerSecond.has_value());
    assert(!early.checkpointBlocksPerSecond.has_value());
    assert(!early.estimatedTimeRemaining.has_value());

    data.elapsed = 5s;
    data.completedTerms = data.totalTerms;
    data.phase = ProgressPhase::finalizing;
    const auto finalizing = ProgressMetricsCalculator::calculate(
        ProgressSnapshot(data)
    );
    assert(finalizing.completionRatio == 1.0);
    assert(!finalizing.estimatedTimeRemaining.has_value());

    data.phase = ProgressPhase::completed;
    const auto completed = ProgressMetricsCalculator::calculate(
        ProgressSnapshot(data)
    );
    assert(completed.estimatedTimeRemaining == 0ns);

    data.phase = ProgressPhase::failed;
    data.completedTerms = 50;
    const auto failed = ProgressMetricsCalculator::calculate(
        ProgressSnapshot(data)
    );
    assert(failed.termsPerSecond.has_value());
    assert(!failed.estimatedTimeRemaining.has_value());

    ProgressSnapshotData unknownTotal;
    unknownTotal.targetDigits = 1;
    const auto unknown = ProgressMetricsCalculator::calculate(
        ProgressSnapshot(unknownTotal)
    );
    assert(!unknown.completionRatio.has_value());
    assert(!unknown.termsPerSecond.has_value());
    assert(!unknown.estimatedTimeRemaining.has_value());

    ProgressMetricPolicy strict;
    strict.minimumElapsed = 1s;
    strict.minimumCompletedTerms = 60;
    strict.minimumCompletedCheckpointBlocks = 3;
    const auto insufficient = ProgressMetricsCalculator::calculate(
        ProgressSnapshot(data),
        strict
    );
    assert(!insufficient.termsPerSecond.has_value());
    assert(!insufficient.checkpointBlocksPerSecond.has_value());

    ProgressMetricPolicy invalid;
    invalid.minimumElapsed = 0ns;
    assert(throws<std::invalid_argument>([&invalid, &data]() {
        static_cast<void>(ProgressMetricsCalculator::calculate(
            ProgressSnapshot(data),
            invalid
        ));
    }));
    invalid.minimumElapsed = 1s;
    invalid.minimumCompletedTerms = 0;
    assert(throws<std::invalid_argument>([&invalid]() {
        ProgressMetricsCalculator::validatePolicy(invalid);
    }));
    invalid.minimumCompletedTerms = 1;
    invalid.minimumCompletedCheckpointBlocks = 0;
    assert(throws<std::invalid_argument>([&invalid]() {
        ProgressMetricsCalculator::validatePolicy(invalid);
    }));

    return 0;
}
