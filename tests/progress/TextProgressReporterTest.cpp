#include "progress/TextProgressReporter.hpp"

#include <cassert>
#include <chrono>
#include <sstream>
#include <stdexcept>
#include <string>


int main()
{
    using namespace std::chrono_literals;
    using pi::progress::EtaFormat;
    using pi::progress::EtaSelection;
    using pi::progress::EtaUnit;
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
    metrics.estimatedTotalTimeRemaining = 15s;
    metrics.estimatedPhaseTimeRemaining = 4s;

    std::ostringstream terminalOutput;
    TextProgressReporter terminal(
        terminalOutput,
        TextOutputMode::terminal
    );
    terminal.report(ProgressSnapshot(data), metrics);
    const std::string terminalRecord = terminalOutput.str();
    assert(terminalRecord ==
        "\rphase=merging terms=25/100 eta_total=15.00sec"
        " checkpoints=2/10 durable=1\x1b[K");
    assert(terminalRecord.find("speed=") == std::string::npos);
    assert(terminalRecord.find("tasks=") == std::string::npos);
    assert(terminalRecord.find("eta_phase=") == std::string::npos);
    assert(terminalRecord.find("progress=") == std::string::npos);
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

    std::ostringstream bothTerminalOutput;
    TextProgressReporter bothTerminal(
        bothTerminalOutput,
        TextOutputMode::terminal,
        EtaSelection{true, true}
    );
    bothTerminal.report(ProgressSnapshot(data), metrics);
    const std::string bothRecord = bothTerminalOutput.str();
    assert(bothRecord ==
        "\rphase=merging terms=25/100 eta_total=15.00sec"
        " eta_phase=4.00sec\x1b[K");
    assert(bothRecord.find("checkpoints=") == std::string::npos);
    assert(bothRecord.size() - controlBytes < 80);

    // Overdue estimates render as `overdue`, never as 0.0s, and the compact
    // line of a 5,000,000-digit run stays within 80 columns.
    ProgressSnapshotData overdueData = data;
    overdueData.phase = ProgressPhase::verifyingOutput;
    overdueData.totalTerms = 352'620;
    overdueData.completedTerms = 352'620;
    ProgressMetrics overdueMetrics = metrics;
    overdueMetrics.estimatedTotalTimeRemaining.reset();
    overdueMetrics.estimatedPhaseTimeRemaining.reset();
    overdueMetrics.totalBasis = pi::progress::EtaBasis::overdue;
    overdueMetrics.phaseOverdue = true;
    std::ostringstream overdueOutput;
    TextProgressReporter overdueTerminal(
        overdueOutput,
        TextOutputMode::terminal,
        EtaSelection{true, true}
    );
    overdueTerminal.report(ProgressSnapshot(overdueData), overdueMetrics);
    const std::string overdueRecord = overdueOutput.str();
    assert(overdueRecord ==
        "\rphase=verifying_output terms=352620/352620"
        " eta_total=overdue eta_phase=overdue\x1b[K");
    assert(overdueRecord.size() - controlBytes <= 80);

    // Only the overdue ETA says so; an unavailable one stays `unknown`.
    overdueMetrics.totalBasis = pi::progress::EtaBasis::unknown;
    std::ostringstream mixedOutput;
    TextProgressReporter mixed(
        mixedOutput,
        TextOutputMode::terminal,
        EtaSelection{true, true}
    );
    mixed.report(ProgressSnapshot(overdueData), overdueMetrics);
    assert(mixedOutput.str().find("eta_total=unknown eta_phase=overdue")
        != std::string::npos);

    data.phase = ProgressPhase::verifyingOutput;
    terminal.report(ProgressSnapshot(data), metrics);
    assert(terminalOutput.str().find("phase=verifying_output")
        != std::string::npos);

    data.phase = ProgressPhase::completed;
    data.completedTerms = 100;
    data.completedCheckpointBlocks = 10;
    data.durableCheckpointBlocks = 10;
    metrics.completionRatio = 1.0;
    metrics.estimatedTotalTimeRemaining = 0ns;
    metrics.estimatedPhaseTimeRemaining = 0ns;
    terminal.report(ProgressSnapshot(data), metrics);
    const std::string terminalFinal = terminalOutput.str();
    assert(terminalFinal.ends_with(
        "\rphase=completed terms=100/100 eta_total=0.00sec"
        " checkpoints=10/10 durable=10\x1b[K\n"
    ));

    std::ostringstream phaseOnlyOutput;
    TextProgressReporter phaseOnly(
        phaseOnlyOutput,
        TextOutputMode::terminal,
        EtaSelection{false, true}
    );
    phaseOnly.report(ProgressSnapshot(data), metrics);
    assert(phaseOnlyOutput.str()
        == "\rphase=completed terms=100/100 eta_phase=0.00sec"
           " checkpoints=10/10 durable=10\x1b[K\n");

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
        EtaSelection{true, true},
        EtaFormat{},
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
    metrics.estimatedTotalTimeRemaining = 15s;
    metrics.estimatedPhaseTimeRemaining = 4s;
    fullLog.report(ProgressSnapshot(data), metrics);
    const std::string fullLogRecord = fullLogOutput.str();
    assert(fullLogRecord ==
        "[1970-01-01T00:00:00Z] phase=merging terms=25/100"
        " progress=25.00% speed=5.00 terms/s"
        " eta_total=15.00sec eta_phase=4.00sec"
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
        EtaSelection{false, true},
        EtaFormat{},
        [fixedTime]() { return fixedTime; }
    );
    data.phase = ProgressPhase::failed;
    data.completedTerms = 50;
    data.failureDetail = "line one\nline two";
    data.lastValidatedCheckpoint.reset();
    data.lastCommittedCheckpoint.reset();
    metrics.completionRatio = 0.5;
    metrics.estimatedTotalTimeRemaining.reset();
    metrics.estimatedPhaseTimeRemaining.reset();
    log.report(ProgressSnapshot(data), metrics);

    const std::string logRecord = logOutput.str();
    assert(logRecord.starts_with("[1970-01-01T00:00:00Z] phase=failed"));
    assert(logRecord.find("eta_phase=unknown") != std::string::npos);
    assert(logRecord.find("eta_total=") == std::string::npos);
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
        EtaSelection{},
        EtaFormat{},
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
            EtaSelection{},
            EtaFormat{},
            TextProgressReporter::NowFunction{}
        );
    }
    catch (const std::invalid_argument&)
    {
        rejectedClock = true;
    }
    assert(rejectedClock);

    bool rejectedPrecision = false;
    try
    {
        TextProgressReporter invalid(
            logOutput,
            TextOutputMode::log,
            EtaSelection{},
            EtaFormat{EtaUnit::automatic, EtaFormat::maxPrecision + 1}
        );
    }
    catch (const std::invalid_argument&)
    {
        rejectedPrecision = true;
    }
    assert(rejectedPrecision);

    // Renders one total ETA in terminal mode and returns its value text.
    const auto renderEta = [&data](
        std::chrono::nanoseconds eta,
        EtaFormat format
    ) {
        ProgressMetrics etaMetrics;
        etaMetrics.estimatedTotalTimeRemaining = eta;
        std::ostringstream output;
        TextProgressReporter reporter(
            output,
            TextOutputMode::terminal,
            EtaSelection{},
            format
        );
        reporter.report(ProgressSnapshot(data), etaMetrics);
        const std::string record = output.str();
        const std::string key = "eta_total=";
        const std::size_t begin = record.find(key) + key.size();
        return record.substr(begin, record.find(' ', begin) - begin);
    };
    const EtaFormat automatic{};
    const EtaFormat automatic9{EtaUnit::automatic, 9};

    // Automatic unit boundaries compare the exact duration, not the rounded
    // value: one nanosecond below a minute or hour keeps the smaller unit.
    assert(renderEta(0ns, automatic) == "0.00sec");
    assert(renderEta(59'999'999'999ns, automatic) == "60.00sec");
    assert(renderEta(59'999'999'999ns, automatic9) == "59.999999999sec");
    assert(renderEta(60s, automatic) == "1.00min");
    assert(renderEta(90s, automatic) == "1.50min");
    assert(renderEta(1h - 1ns, automatic) == "60.00min");
    assert(renderEta(59min + 30s, automatic) == "59.50min");
    assert(renderEta(1h, automatic) == "1.00hour");
    assert(renderEta(36h + 45min, automatic) == "36.75hour");

    // Fixed units never switch.
    const EtaFormat minutes{EtaUnit::minutes, 2};
    assert(renderEta(0ns, minutes) == "0.00min");
    assert(renderEta(30s, minutes) == "0.50min");
    assert(renderEta(2h, minutes) == "120.00min");
    const EtaFormat hours{EtaUnit::hours, 2};
    assert(renderEta(30s, hours) == "0.01hour");
    assert(renderEta(90min, hours) == "1.50hour");
    assert(renderEta(100h, hours) == "100.00hour");

    // Precision 0 has no decimal point; precision 9 reaches nanoseconds.
    assert(renderEta(15s, EtaFormat{EtaUnit::automatic, 0}) == "15sec");
    assert(renderEta(100s, EtaFormat{EtaUnit::automatic, 0}) == "2min");
    assert(renderEta(3h, EtaFormat{EtaUnit::hours, 0}) == "3hour");
    assert(renderEta(1ns, automatic9) == "0.000000001sec");
    assert(renderEta(4s, automatic9) == "4.000000000sec");
    assert(renderEta(90s, EtaFormat{EtaUnit::minutes, 9}) == "1.500000000min");

    // High precision drops lower-priority compact fields rather than wrapping
    // an in-place terminal update.
    ProgressMetrics preciseMetrics;
    preciseMetrics.estimatedTotalTimeRemaining = 15s;
    preciseMetrics.estimatedPhaseTimeRemaining = 4s;
    std::ostringstream preciseOutput;
    TextProgressReporter preciseTerminal(
        preciseOutput,
        TextOutputMode::terminal,
        EtaSelection{true, true},
        EtaFormat{EtaUnit::hours, 9}
    );
    preciseTerminal.report(ProgressSnapshot(overdueData), preciseMetrics);
    const std::string preciseRecord = preciseOutput.str();
    assert(preciseRecord ==
        "\rphase=verifying_output eta_total=0.004166667hour"
        " eta_phase=0.001111111hour\x1b[K");
    assert(preciseRecord.find("terms=") == std::string::npos);
    assert(preciseRecord.size() - controlBytes < 80);

    // Both ETAs share the format in log mode.
    std::ostringstream minuteLogOutput;
    TextProgressReporter minuteLog(
        minuteLogOutput,
        TextOutputMode::log,
        EtaSelection{true, true},
        minutes,
        [fixedTime]() { return fixedTime; }
    );
    ProgressMetrics minuteMetrics;
    minuteMetrics.estimatedTotalTimeRemaining = 15s;
    minuteMetrics.estimatedPhaseTimeRemaining = 4s;
    minuteLog.report(ProgressSnapshot(data), minuteMetrics);
    assert(minuteLogOutput.str().find(" eta_total=0.25min eta_phase=0.07min ")
        != std::string::npos);

    // `unknown` and `overdue` are unchanged by the format.
    overdueMetrics.totalBasis = pi::progress::EtaBasis::overdue;
    std::ostringstream fixedOverdueOutput;
    TextProgressReporter fixedOverdue(
        fixedOverdueOutput,
        TextOutputMode::terminal,
        EtaSelection{true, true},
        EtaFormat{EtaUnit::hours, 9}
    );
    fixedOverdue.report(ProgressSnapshot(overdueData), overdueMetrics);
    assert(fixedOverdueOutput.str() == overdueRecord);
    overdueMetrics.totalBasis = pi::progress::EtaBasis::unknown;
    std::ostringstream fixedMixedOutput;
    TextProgressReporter fixedMixed(
        fixedMixedOutput,
        TextOutputMode::terminal,
        EtaSelection{true, true},
        EtaFormat{EtaUnit::minutes, 0}
    );
    fixedMixed.report(ProgressSnapshot(overdueData), overdueMetrics);
    assert(fixedMixedOutput.str() == mixedOutput.str());

    return 0;
}
