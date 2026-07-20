#include "progress/ProgressReporter.hpp"

#include <cassert>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>


namespace
{

struct RecordedSample
{
    pi::progress::ProgressSnapshot snapshot;
    pi::progress::ProgressMetrics metrics;
};


class RecordingReporter final : public pi::progress::IProgressReporter
{
public:
    void report(
        const pi::progress::ProgressSnapshot& snapshot,
        const pi::progress::ProgressMetrics& metrics
    ) override
    {
        samples.push_back(RecordedSample{snapshot, metrics});
    }

    std::vector<RecordedSample> samples;
};

} // namespace


int main()
{
    using pi::progress::IProgressReporter;
    using pi::progress::ProgressMetrics;
    using pi::progress::ProgressPhase;
    using pi::progress::ProgressSnapshot;
    using pi::progress::ProgressSnapshotData;

    static_assert(std::is_abstract_v<IProgressReporter>);
    static_assert(std::has_virtual_destructor_v<IProgressReporter>);
    static_assert(!std::is_copy_constructible_v<IProgressReporter>);
    static_assert(!std::is_move_constructible_v<IProgressReporter>);

    ProgressSnapshotData data;
    data.phase = ProgressPhase::splitting;
    data.targetDigits = 1'000;
    data.totalTerms = 100;
    data.completedTerms = 25;

    ProgressMetrics metrics;
    metrics.completionRatio = 0.25;
    metrics.termsPerSecond = 5.0;

    auto concrete = std::make_unique<RecordingReporter>();
    RecordingReporter* recording = concrete.get();
    std::unique_ptr<IProgressReporter> reporter = std::move(concrete);
    reporter->report(ProgressSnapshot(data), metrics);

    data.completedTerms = 50;
    metrics.completionRatio = 0.5;

    assert(recording->samples.size() == 1);
    assert(recording->samples.front().snapshot.completedTerms() == 25);
    assert(recording->samples.front().metrics.completionRatio
        == std::optional<double>{0.25});

    return 0;
}
