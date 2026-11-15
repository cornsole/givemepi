#include "progress/TextProgressReporter.hpp"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>


namespace pi::progress
{
namespace
{

std::string utcTimestamp(TextProgressReporter::Clock::time_point time)
{
    const std::time_t raw = TextProgressReporter::Clock::to_time_t(time);
    std::tm utc{};
#if defined(_WIN32)
    if (::gmtime_s(&utc, &raw) != 0)
#else
    if (::gmtime_r(&raw, &utc) == nullptr)
#endif
    {
        return "unknown-time";
    }

    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return output.str();
}


std::string byteCount(std::uint64_t bytes)
{
    constexpr std::uint64_t kibibyte = 1024;
    constexpr std::uint64_t mebibyte = kibibyte * 1024;
    constexpr std::uint64_t gibibyte = mebibyte * 1024;

    const char* unit = "B";
    long double value = static_cast<long double>(bytes);
    if (bytes >= gibibyte)
    {
        value /= gibibyte;
        unit = "GiB";
    }
    else if (bytes >= mebibyte)
    {
        value /= mebibyte;
        unit = "MiB";
    }
    else if (bytes >= kibibyte)
    {
        value /= kibibyte;
        unit = "KiB";
    }

    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(unit[0] == 'B' ? 0 : 2)
           << value << ' ' << unit;
    return output.str();
}


std::string seconds(std::chrono::nanoseconds duration)
{
    const long double value =
        static_cast<long double>(duration.count()) / 1'000'000'000.0L;
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(1) << value << 's';
    return output.str();
}


std::string singleLine(std::string value)
{
    std::replace_if(value.begin(), value.end(), [](unsigned char character) {
        return character < 0x20 || character == 0x7f;
    }, ' ');
    return value;
}


void writeRange(
    std::ostream& output,
    const char* label,
    const std::optional<CheckpointRangeProgress>& range
)
{
    if (range.has_value())
    {
        output << ' ' << label << "=[" << range->startTerm << ','
               << range->endTerm << ")@L" << range->treeLevel;
    }
}


void writeTerms(std::ostream& output, const ProgressSnapshot& snapshot)
{
    output << "phase=" << toString(snapshot.phase())
           << " terms=" << snapshot.completedTerms()
           << '/' << snapshot.totalTerms();
}


void writeProgress(
    std::ostream& output,
    const ProgressSnapshot& snapshot,
    const ProgressMetrics& metrics
)
{
    writeTerms(output, snapshot);
    if (metrics.completionRatio.has_value())
    {
        output << " progress=" << std::fixed << std::setprecision(2)
               << (*metrics.completionRatio * 100.0) << '%';
    }
}


void writeDuration(
    std::ostream& output,
    const char* label,
    const std::optional<std::chrono::nanoseconds>& value
)
{
    output << ' ' << label << '=';
    if (value.has_value())
    {
        output << seconds(*value);
    }
    else
    {
        output << "unknown";
    }
}


void writeEta(
    std::ostream& output,
    const ProgressMetrics& metrics,
    EtaSelection selection
)
{
    if (selection.total)
    {
        writeDuration(
            output,
            "eta_total",
            metrics.estimatedTotalTimeRemaining
        );
    }
    if (selection.phase)
    {
        writeDuration(
            output,
            "eta_phase",
            metrics.estimatedPhaseTimeRemaining
        );
    }
}


void writeCheckpointCounts(
    std::ostream& output,
    const ProgressSnapshot& snapshot
)
{
    output << " checkpoints=" << snapshot.completedCheckpointBlocks()
           << '/' << snapshot.totalCheckpointBlocks()
           << " durable=" << snapshot.durableCheckpointBlocks();
}


void writeFailure(std::ostream& output, const ProgressSnapshot& snapshot)
{
    if (snapshot.failureDetail().has_value())
    {
        output << " failure=\""
               << singleLine(*snapshot.failureDetail()) << '"';
    }
}


/// Short single-row record for in-place terminal redraws. Excludes speed,
/// task, memory, storage, I/O, merge, and range detail, and the progress
/// percentage already implied by the term fraction, so routine updates stay
/// inside common terminal widths and `\r` redraws do not wrap. When both
/// ETAs are selected the checkpoint counts are also omitted for width.
std::string renderCompact(
    const ProgressSnapshot& snapshot,
    const ProgressMetrics& metrics,
    EtaSelection selection
)
{
    std::ostringstream output;
    output.imbue(std::locale::classic());
    writeTerms(output, snapshot);
    writeEta(output, metrics, selection);
    if (!(selection.total && selection.phase))
    {
        writeCheckpointCounts(output, snapshot);
    }
    writeFailure(output, snapshot);
    return output.str();
}


std::string renderFull(
    const ProgressSnapshot& snapshot,
    const ProgressMetrics& metrics,
    EtaSelection selection
)
{
    std::ostringstream output;
    output.imbue(std::locale::classic());
    writeProgress(output, snapshot, metrics);
    if (metrics.termsPerSecond.has_value())
    {
        output << " speed=" << std::fixed << std::setprecision(2)
               << *metrics.termsPerSecond << " terms/s";
    }
    writeEta(output, metrics, selection);

    output << " tasks=" << snapshot.activeTasks() << " active/"
           << snapshot.queuedTasks() << " queued"
           << " memory=" << byteCount(snapshot.memoryBytes());
    writeCheckpointCounts(output, snapshot);
    output << " checkpoint_bytes=" << byteCount(snapshot.checkpointBytes());
    output << " storage=" << byteCount(snapshot.storageResidentBytes())
           << " resident/" << byteCount(snapshot.storageStoredBytes())
           << " stored/" << snapshot.storageChunkCount() << " chunks";
    output << " io=write[" << snapshot.storageActiveWrites() << " active/"
           << snapshot.storageQueuedWrites() << " queued/"
           << snapshot.storageCompletedWrites() << " done/"
           << snapshot.storageFailedWrites() << " failed]"
           << " read[" << snapshot.storageActiveReads() << " active/"
           << snapshot.storageQueuedReads() << " queued/"
           << snapshot.storageCompletedReads() << " done/"
           << snapshot.storageFailedReads() << " failed]";

    if (snapshot.currentMergeLevel().has_value())
    {
        output << " merge_level=" << *snapshot.currentMergeLevel();
    }
    writeRange(output, "last_validated", snapshot.lastValidatedCheckpoint());
    writeRange(output, "last_committed", snapshot.lastCommittedCheckpoint());
    writeFailure(output, snapshot);
    return output.str();
}

} // namespace


TextProgressReporter::TextProgressReporter(
    std::ostream& output,
    TextOutputMode mode,
    EtaSelection selection,
    NowFunction now
)
    : output_(output),
      mode_(mode),
      selection_(selection),
      now_(std::move(now))
{
    if (!now_)
    {
        throw std::invalid_argument("Text reporter clock must be callable");
    }
}


void TextProgressReporter::report(
    const ProgressSnapshot& snapshot,
    const ProgressMetrics& metrics
)
{
    if (mode_ == TextOutputMode::terminal)
    {
        output_ << '\r' << renderCompact(snapshot, metrics, selection_)
                << "\x1b[K";
        if (snapshot.terminalState() != ProgressTerminalState::running)
        {
            output_ << '\n';
        }
    }
    else
    {
        output_ << '[' << utcTimestamp(now_()) << "] "
                << renderFull(snapshot, metrics, selection_) << '\n';
    }
    output_.flush();
    if (!output_)
    {
        throw std::runtime_error("Failed to write text progress output");
    }
}

} // namespace pi::progress
