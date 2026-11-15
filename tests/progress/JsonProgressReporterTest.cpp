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
    using pi::progress::EtaSelection;
    using pi::progress::JsonProgressReporter;
    using pi::progress::ProgressMetrics;
    using pi::progress::ProgressPhase;
    using pi::progress::ProgressSnapshot;
    using pi::progress::ProgressSnapshotData;
    using pi::progress::CheckpointRangeProgress;

    ProgressSnapshotData data;
    data.phase = ProgressPhase::merging;
    data.targetDigits = 1'000;
    data.totalTerms = 100;
    data.completedTerms = 25;
    data.totalCheckpointBlocks = 10;
    data.completedCheckpointBlocks = 2;
    data.durableCheckpointBlocks = 1;
    data.currentMergeLevel = 3;
    data.activeTasks = 4;
    data.queuedTasks = 2;
    data.elapsed = 5s;
    data.memoryBytes = 4096;
    data.checkpointBytes = 2048;
    data.storageResidentBytes = 8192;
    data.storageStoredBytes = 16384;
    data.storageChunkCount = 3;
    data.lastValidatedCheckpoint =
        CheckpointRangeProgress{0, 25, 2};
    data.lastCommittedCheckpoint =
        CheckpointRangeProgress{25, 100, 3};
    data.restoredTerms = 5;
    data.restoredCheckpointBlocks = 1;
    data.phaseUnits[pi::progress::phaseIndex(ProgressPhase::merging)] =
        pi::progress::PhaseWorkUnits{200, 50};
    data.phaseBaselineUnits = 10;
    data.phaseElapsed = 2s;

    ProgressMetrics metrics;
    metrics.completionRatio = 0.25;
    metrics.termsPerSecond = 5.0;
    metrics.checkpointBlocksPerSecond = 0.4;
    metrics.estimatedTotalTimeRemaining = 15s;
    metrics.estimatedPhaseTimeRemaining = 4s;
    metrics.totalBasis = pi::progress::EtaBasis::measured;

    const auto fixedTime = JsonProgressReporter::Clock::time_point{0s};
    std::ostringstream output;
    JsonProgressReporter reporter(
        output,
        EtaSelection{},
        [fixedTime]() { return fixedTime; }
    );
    reporter.report(ProgressSnapshot(data), metrics);

    const std::string record = output.str();
    assert(record.starts_with("{\"storage_io\":{\"writes\":{"));
    assert(record.find(
        "\"storage_io\":{\"writes\":{\"queued\":0,\"active\":0"
    ) != std::string::npos);
    assert(record.find(
        "\"reads\":{\"queued\":0,\"active\":0,\"completed\":0,\"failed\":0}},"
        "\"schema_version\":6"
    ) != std::string::npos);
    assert(record.find(
        "\"schema_version\":6,\"sampled_at\":\"1970-01-01T00:00:00Z\""
    ) != std::string::npos);
    assert(record.find("\"phase\":\"merging\"") != std::string::npos);
    assert(record.find("\"terminal_state\":\"running\"")
        != std::string::npos);
    assert(record.find("\"terms\":{\"completed\":25,\"total\":100}")
        != std::string::npos);
    assert(record.find(
        "\"checkpoint_blocks\":{\"completed\":2,\"total\":10,\"durable\":1}"
    ) != std::string::npos);
    assert(record.find("\"checkpoint_bytes\":2048") != std::string::npos);
    assert(record.find(
        "\"last_validated_checkpoint\":{\"start_term\":0,\"end_term\":25,"
        "\"tree_level\":2}"
    ) != std::string::npos);
    assert(record.find(
        "\"last_committed_checkpoint\":{\"start_term\":25,\"end_term\":100,"
        "\"tree_level\":3}"
    ) != std::string::npos);
    assert(record.find("\"storage\":{\"resident_bytes\":8192,\"stored_bytes\":16384,\"chunk_count\":3}")
        != std::string::npos);
    assert(record.find("\"completion_ratio\":0.25")
        != std::string::npos);
    assert(record.find("\"restored_terms\":5,\"restored_checkpoint_blocks\":1")
        != std::string::npos);
    assert(record.find(
        "\"phase_work\":{\"completed_units\":50,\"total_units\":200,"
        "\"baseline_units\":10,\"elapsed_ns\":2000000000}"
    ) != std::string::npos);
    // Default selection: total only; phase keys stay present as null.
    assert(record.find(
        "\"estimated_total_time_remaining_ns\":15000000000,"
        "\"estimated_phase_time_remaining_ns\":null,"
        "\"eta_total_basis\":\"measured\",\"eta_phase_overdue\":null}"
    ) != std::string::npos);
    assert(record.find("\"estimated_time_remaining_ns\"")
        == std::string::npos);
    assert(record.ends_with("}}\n"));
    assert(record.find('\n') == record.size() - 1);

    std::ostringstream bothOutput;
    JsonProgressReporter both(
        bothOutput,
        EtaSelection{true, true},
        [fixedTime]() { return fixedTime; }
    );
    both.report(ProgressSnapshot(data), metrics);
    assert(bothOutput.str().find(
        "\"estimated_total_time_remaining_ns\":15000000000,"
        "\"estimated_phase_time_remaining_ns\":4000000000,"
        "\"eta_total_basis\":\"measured\",\"eta_phase_overdue\":false}"
    ) != std::string::npos);

    // Overdue estimates stay null and are flagged rather than reported as 0.
    ProgressMetrics overdue = metrics;
    overdue.estimatedTotalTimeRemaining.reset();
    overdue.estimatedPhaseTimeRemaining.reset();
    overdue.totalBasis = pi::progress::EtaBasis::overdue;
    overdue.phaseOverdue = true;
    both.report(ProgressSnapshot(data), overdue);
    assert(bothOutput.str().find(
        "\"estimated_total_time_remaining_ns\":null,"
        "\"estimated_phase_time_remaining_ns\":null,"
        "\"eta_total_basis\":\"overdue\",\"eta_phase_overdue\":true}"
    ) != std::string::npos);
    ProgressMetrics calibrated = metrics;
    calibrated.totalBasis = pi::progress::EtaBasis::calibratedPlan;
    both.report(ProgressSnapshot(data), calibrated);
    assert(bothOutput.str().find("\"eta_total_basis\":\"calibrated_plan\"")
        != std::string::npos);
    calibrated.totalBasis = pi::progress::EtaBasis::unknown;
    calibrated.estimatedTotalTimeRemaining.reset();
    both.report(ProgressSnapshot(data), calibrated);
    assert(bothOutput.str().find("\"eta_total_basis\":\"unknown\"")
        != std::string::npos);

    std::ostringstream phaseOutput;
    JsonProgressReporter phaseOnly(
        phaseOutput,
        EtaSelection{false, true},
        [fixedTime]() { return fixedTime; }
    );
    phaseOnly.report(ProgressSnapshot(data), metrics);
    assert(phaseOutput.str().find(
        "\"estimated_total_time_remaining_ns\":null,"
        "\"estimated_phase_time_remaining_ns\":4000000000,"
        "\"eta_total_basis\":null,\"eta_phase_overdue\":false}"
    ) != std::string::npos);

    data.phaseBaselineUnits = 0;
    data.phase = ProgressPhase::verifyingOutput;
    reporter.report(ProgressSnapshot(data), metrics);
    assert(output.str().find("\"phase\":\"verifying_output\"")
        != std::string::npos);

    data.phase = ProgressPhase::failed;
    data.failureDetail = "quote \" slash \\ newline\n tab\t byte\x01";
    data.lastValidatedCheckpoint.reset();
    data.lastCommittedCheckpoint.reset();
    metrics.completionRatio = std::numeric_limits<double>::infinity();
    metrics.termsPerSecond.reset();
    metrics.checkpointBlocksPerSecond.reset();
    metrics.estimatedTotalTimeRemaining.reset();
    metrics.estimatedPhaseTimeRemaining.reset();
    reporter.report(ProgressSnapshot(data), metrics);

    const std::string allRecords = output.str();
    const std::size_t failedStart = allRecords.rfind(
        "\"schema_version\""
    );
    const std::string second = allRecords.substr(failedStart);
    assert(second.find(
        "\"failure_detail\":\"quote \\\" slash \\\\ newline\\n tab\\t byte\\u0001\""
    ) != std::string::npos);
    assert(second.find("\"completion_ratio\":null")
        != std::string::npos);
    assert(second.find("\"last_validated_checkpoint\":null")
        != std::string::npos);
    assert(second.find("\"last_committed_checkpoint\":null")
        != std::string::npos);
    assert(second.find("\"terms_per_second\":null")
        != std::string::npos);
    assert(second.find('\n') == second.size() - 1);
    std::ostringstream failedOutput;
    failedOutput.setstate(std::ios::badbit);
    JsonProgressReporter failedReporter(
        failedOutput,
        EtaSelection{},
        [fixedTime]() { return fixedTime; }
    );
    bool writeFailureReported = false;
    try
    {
        failedReporter.report(ProgressSnapshot(data), metrics);
    }
    catch (const std::runtime_error& error)
    {
        writeFailureReported = std::string(error.what())
            == "Failed to write JSON progress output";
    }
    assert(writeFailureReported);


    bool rejectedClock = false;
    try
    {
        JsonProgressReporter invalid(
            output,
            EtaSelection{},
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
