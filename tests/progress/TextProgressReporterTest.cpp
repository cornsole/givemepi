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
    data.memoryBytes = 4096;
    data.checkpointBytes = 2048;
    data.lastValidatedCheckpoint =
        ValidatedCheckpointProgress{0, 25, 2};

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
    assert(terminalRecord.starts_with("\rphase=merging"));
    assert(terminalRecord.find("terms=25/100 progress=25.00%")
        != std::string::npos);
    assert(terminalRecord.find("speed=5.00 terms/s eta=15.0s")
        != std::string::npos);
    assert(terminalRecord.find("memory=4.00 KiB") != std::string::npos);
    assert(terminalRecord.find("last_checkpoint=[0,25)@L2")
        != std::string::npos);
    assert(terminalRecord.ends_with("\x1b[K"));

    data.phase = ProgressPhase::verifyingOutput;
    terminal.report(ProgressSnapshot(data), metrics);
    assert(terminalOutput.str().find("phase=verifying_output")
        != std::string::npos);

    data.phase = ProgressPhase::completed;
    data.completedTerms = 100;
    metrics.completionRatio = 1.0;
    metrics.estimatedTimeRemaining = 0ns;
    terminal.report(ProgressSnapshot(data), metrics);
    assert(terminalOutput.str().ends_with("\x1b[K\n"));

    const auto fixedTime = TextProgressReporter::Clock::time_point{0s};
    std::ostringstream logOutput;
    TextProgressReporter log(
        logOutput,
        TextOutputMode::log,
        [fixedTime]() { return fixedTime; }
    );
    data.phase = ProgressPhase::failed;
    data.completedTerms = 50;
    data.failureDetail = "line one\nline two";
    metrics.completionRatio = 0.5;
    metrics.estimatedTimeRemaining.reset();
    log.report(ProgressSnapshot(data), metrics);

    const std::string logRecord = logOutput.str();
    assert(logRecord.starts_with("[1970-01-01T00:00:00Z] phase=failed"));
    assert(logRecord.find("eta=unknown") != std::string::npos);
    assert(logRecord.find("failure=\"line one line two\"")
        != std::string::npos);
    assert(logRecord.find('\r') == std::string::npos);
    assert(logRecord.find("\x1b[K") == std::string::npos);
    assert(logRecord.ends_with('\n'));
    assert(logRecord.find('\n') == logRecord.size() - 1);

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
