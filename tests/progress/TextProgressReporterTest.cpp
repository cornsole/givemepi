#include "progress/TextProgressReporter.hpp"

#include <cassert>
#include <chrono>
#include <sstream>
#include <stdexcept>
#include <string>


int main()
{
    using namespace std::chrono_literals;
    using pi::progress::ProgressMetrics;
    using pi::progress::ProgressPhase;
    using pi::progress::ProgressSnapshot;
    using pi::progress::ProgressSnapshotData;
    using pi::progress::TextOutputMode;
    using pi::progress::TextProgressReporter;
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
    data.memoryBytes = 4096;
    data.checkpointBytes = 2048;
    data.storageResidentBytes = 8192;
    data.storageStoredBytes = 16384;
    data.storageChunkCount = 3;
    data.lastValidatedCheckpoint =
        CheckpointRangeProgress{0, 25, 2};
    data.lastCommittedCheckpoint =
        CheckpointRangeProgress{25, 100, 3};

    ProgressMetrics metrics;
    metrics.completionRatio = 0.25;
    metrics.termsPerSecond = 5.0;
    metrics.estimatedTimeRemaining = 15s;

    std::ostringstream terminalOutput;
    TextProgressReporter terminal(
        terminalOutput,
        TextOutputMode::terminal
    );
    terminal.report(ProgressSnapshot(data), metrics);
    const std::string terminalRecord = terminalOutput.str();
    assert(terminalRecord ==
        "\rphase=merging terms=25/100 progress=25.00% eta=15.0s"
        " checkpoints=2/10 durable=1\x1b[K");
    assert(terminalRecord.find("speed=") == std::string::npos);
    assert(terminalRecord.find("tasks=") == std::string::npos);
    assert(terminalRecord.find("memory=") == std::string::npos);
    assert(terminalRecord.find("checkpoint_bytes=") == std::string::npos);
    assert(terminalRecord.find("storage=") == std::string::npos);
    assert(terminalRecord.find("io=") == std::string::npos);
    assert(terminalRecord.find("merge_level=") == std::string::npos);
    assert(terminalRecord.find("last_validated=") == std::string::npos);
    assert(terminalRecord.find("last_committed=") == std::string::npos);
    assert(terminalRecord.find('\n') == std::string::npos);
    const std::size_t controlBytes = std::string("\r\x1b[K").size();
    assert(terminalRecord.size() - controlBytes < 80);

    data.phase = ProgressPhase::verifyingOutput;
    terminal.report(ProgressSnapshot(data), metrics);
    assert(terminalOutput.str().find("phase=verifying_output")
        != std::string::npos);

    data.phase = ProgressPhase::completed;
    data.completedTerms = 100;
    data.completedCheckpointBlocks = 10;
    data.durableCheckpointBlocks = 10;
    metrics.completionRatio = 1.0;
    metrics.estimatedTimeRemaining = 0ns;
    terminal.report(ProgressSnapshot(data), metrics);
    const std::string terminalFinal = terminalOutput.str();
    assert(terminalFinal.ends_with(
        "\rphase=completed terms=100/100 progress=100.00% eta=0.0s"
        " checkpoints=10/10 durable=10\x1b[K\n"
    ));

    std::ostringstream failedTerminalOutput;
    TextProgressReporter failedTerminal(
        failedTerminalOutput,
        TextOutputMode::terminal
    );
    ProgressSnapshotData failedData = data;
    failedData.phase = ProgressPhase::failed;
    failedData.failureDetail = "disk\nfull";
    failedTerminal.report(ProgressSnapshot(failedData), metrics);
    assert(failedTerminalOutput.str().find("failure=\"disk full\"")
        != std::string::npos);
    assert(failedTerminalOutput.str().ends_with("\x1b[K\n"));

    const auto fixedTime = TextProgressReporter::Clock::time_point{0s};
    std::ostringstream fullLogOutput;
    TextProgressReporter fullLog(
        fullLogOutput,
        TextOutputMode::log,
        [fixedTime]() { return fixedTime; }
    );
    data.phase = ProgressPhase::merging;
    data.completedTerms = 25;
    data.completedCheckpointBlocks = 2;
    data.durableCheckpointBlocks = 1;
    data.storageActiveWrites = 1;
    data.storageQueuedWrites = 2;
    data.storageCompletedWrites = 3;
    data.storageFailedWrites = 0;
    data.storageActiveReads = 0;
    data.storageQueuedReads = 1;
    data.storageCompletedReads = 4;
    data.storageFailedReads = 0;
    metrics.completionRatio = 0.25;
    metrics.estimatedTimeRemaining = 15s;
    fullLog.report(ProgressSnapshot(data), metrics);
    const std::string fullLogRecord = fullLogOutput.str();
    assert(fullLogRecord ==
        "[1970-01-01T00:00:00Z] phase=merging terms=25/100"
        " progress=25.00% speed=5.00 terms/s eta=15.0s"
        " tasks=4 active/2 queued memory=4.00 KiB"
        " checkpoints=2/10 durable=1 checkpoint_bytes=2.00 KiB"
        " storage=8.00 KiB resident/16.00 KiB stored/3 chunks"
        " io=write[1 active/2 queued/3 done/0 failed]"
        " read[0 active/1 queued/4 done/0 failed]"
        " merge_level=3 last_validated=[0,25)@L2"
        " last_committed=[25,100)@L3\n");

    std::ostringstream logOutput;
    TextProgressReporter log(
        logOutput,
        TextOutputMode::log,
        [fixedTime]() { return fixedTime; }
    );
    data.phase = ProgressPhase::failed;
    data.completedTerms = 50;
    data.failureDetail = "line one\nline two";
    data.lastValidatedCheckpoint.reset();
    data.lastCommittedCheckpoint.reset();
    metrics.completionRatio = 0.5;
    metrics.estimatedTimeRemaining.reset();
    log.report(ProgressSnapshot(data), metrics);

    const std::string logRecord = logOutput.str();
    assert(logRecord.starts_with("[1970-01-01T00:00:00Z] phase=failed"));
    assert(logRecord.find("eta=unknown") != std::string::npos);
    assert(logRecord.find("failure=\"line one line two\"")
        != std::string::npos);
    assert(logRecord.find('\r') == std::string::npos);
    assert(logRecord.find("last_validated=") == std::string::npos);
    assert(logRecord.find("last_committed=") == std::string::npos);
    assert(logRecord.find("\x1b[K") == std::string::npos);
    assert(logRecord.ends_with('\n'));
    assert(logRecord.find('\n') == logRecord.size() - 1);
    std::ostringstream failedOutput;
    failedOutput.setstate(std::ios::badbit);
    TextProgressReporter failedReporter(
        failedOutput,
        TextOutputMode::log,
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
            == "Failed to write text progress output";
    }
    assert(writeFailureReported);


    bool rejectedClock = false;
    try
    {
        TextProgressReporter invalid(
            logOutput,
            TextOutputMode::log,
            TextProgressReporter::NowFunction{}
        );
    }
    catch (const std::invalid_argument&)
    {
        rejectedClock = true;
    }
    assert(rejectedClock);

    return 0;
}
