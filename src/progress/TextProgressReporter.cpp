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


std::string render(
    const ProgressSnapshot& snapshot,
    const ProgressMetrics& metrics
)
{
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "phase=" << toString(snapshot.phase())
           << " terms=" << snapshot.completedTerms()
           << '/' << snapshot.totalTerms();

    if (metrics.completionRatio.has_value())
    {
        output << " progress=" << std::fixed << std::setprecision(2)
               << (*metrics.completionRatio * 100.0) << '%';
    }
    if (metrics.termsPerSecond.has_value())
    {
        output << " speed=" << std::fixed << std::setprecision(2)
               << *metrics.termsPerSecond << " terms/s";
    }
    output << " eta=";
    if (metrics.estimatedTimeRemaining.has_value())
    {
        output << seconds(*metrics.estimatedTimeRemaining);
    }
    else
    {
        output << "unknown";
    }

    output << " tasks=" << snapshot.activeTasks() << " active/"
           << snapshot.queuedTasks() << " queued"
           << " memory=" << byteCount(snapshot.memoryBytes())
           << " checkpoints=" << snapshot.completedCheckpointBlocks()
           << '/' << snapshot.totalCheckpointBlocks()
           << " checkpoint_bytes=" << byteCount(snapshot.checkpointBytes());
    output << " storage=" << byteCount(snapshot.storageResidentBytes())
           << " resident/" << byteCount(snapshot.storageStoredBytes())
           << " stored/" << snapshot.storageChunkCount() << " chunks";

    if (snapshot.currentMergeLevel().has_value())
    {
        output << " merge_level=" << *snapshot.currentMergeLevel();
    }
    if (snapshot.lastValidatedCheckpoint().has_value())
    {
        const auto& checkpoint = *snapshot.lastValidatedCheckpoint();
        output << " last_checkpoint=[" << checkpoint.startTerm << ','
               << checkpoint.endTerm << ")@L" << checkpoint.treeLevel;
    }
    if (snapshot.failureDetail().has_value())
    {
        output << " failure=\""
               << singleLine(*snapshot.failureDetail()) << '"';
    }
    return output.str();
}

} // namespace


TextProgressReporter::TextProgressReporter(
    std::ostream& output,
    TextOutputMode mode,
    NowFunction now
)
    : output_(output),
      mode_(mode),
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
    const std::string record = render(snapshot, metrics);
    if (mode_ == TextOutputMode::terminal)
    {
        output_ << '\r' << record << "\x1b[K";
        if (snapshot.terminalState() != ProgressTerminalState::running)
        {
            output_ << '\n';
        }
    }
    else
    {
        output_ << '[' << utcTimestamp(now_()) << "] " << record << '\n';
    }
    output_.flush();
}

} // namespace pi::progress
