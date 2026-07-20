#include "progress/JsonProgressReporter.hpp"

#include <cassert>
#include <chrono>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>


int main()
{
    using namespace std::chrono_literals;
    using pi::progress::JsonProgressReporter;
    using pi::progress::ProgressMetrics;
    using pi::progress::ProgressPhase;
    using pi::progress::ProgressSnapshot;
    using pi::progress::ProgressSnapshotData;
    using pi::progress::ValidatedCheckpointProgress;

    ProgressSnapshotData data;
    data.phase = ProgressPhase::merging;
    data.targetDigits = 1'000;
    data.totalTerms = 100;
    data.completedTerms = 25;
    data.totalCheckpointBlocks = 10;
    data.completedCheckpointBlocks = 2;
    data.currentMergeLevel = 3;
    data.activeTasks = 4;
    data.queuedTasks = 2;
    data.elapsed = 5s;
    data.memoryBytes = 4096;
    data.checkpointBytes = 2048;
    data.lastValidatedCheckpoint =
        ValidatedCheckpointProgress{0, 25, 2};

    ProgressMetrics metrics;
    metrics.completionRatio = 0.25;
    metrics.termsPerSecond = 5.0;
    metrics.checkpointBlocksPerSecond = 0.4;
    metrics.estimatedTimeRemaining = 15s;

    const auto fixedTime = JsonProgressReporter::Clock::time_point{0s};
    std::ostringstream output;
    JsonProgressReporter reporter(
        output,
        [fixedTime]() { return fixedTime; }
    );
    reporter.report(ProgressSnapshot(data), metrics);

    const std::string record = output.str();
    assert(record.starts_with(
        "{\"schema_version\":1,\"sampled_at\":\"1970-01-01T00:00:00Z\""
    ));
    assert(record.find("\"phase\":\"merging\"") != std::string::npos);
    assert(record.find("\"terminal_state\":\"running\"")
        != std::string::npos);
    assert(record.find("\"terms\":{\"completed\":25,\"total\":100}")
        != std::string::npos);
    assert(record.find("\"last_validated_checkpoint\":{\"start_term\":0,")
        != std::string::npos);
    assert(record.find("\"completion_ratio\":0.25")
        != std::string::npos);
    assert(record.find("\"estimated_time_remaining_ns\":15000000000")
        != std::string::npos);
    assert(record.ends_with("}}\n"));
    assert(record.find('\n') == record.size() - 1);

    data.phase = ProgressPhase::failed;
    data.failureDetail = "quote \" slash \\ newline\n tab\t byte\x01";
    metrics.completionRatio = std::numeric_limits<double>::infinity();
    metrics.termsPerSecond.reset();
    metrics.checkpointBlocksPerSecond.reset();
    metrics.estimatedTimeRemaining.reset();
    reporter.report(ProgressSnapshot(data), metrics);

    const std::string allRecords = output.str();
    const std::size_t secondStart = record.size();
    const std::string second = allRecords.substr(secondStart);
    assert(second.find(
        "\"failure_detail\":\"quote \\\" slash \\\\ newline\\n tab\\t byte\\u0001\""
    ) != std::string::npos);
    assert(second.find("\"completion_ratio\":null")
        != std::string::npos);
    assert(second.find("\"terms_per_second\":null")
        != std::string::npos);
    assert(second.find('\n') == second.size() - 1);

    bool rejectedClock = false;
    try
    {
        JsonProgressReporter invalid(
            output,
            JsonProgressReporter::NowFunction{}
        );
    }
    catch (const std::invalid_argument&)
    {
        rejectedClock = true;
    }
    assert(rejectedClock);

    return 0;
}
