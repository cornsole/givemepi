#include "progress/JsonProgressReporter.hpp"

#include <cmath>
#include <ctime>
#include <iomanip>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>


namespace pi::progress
{
namespace
{

std::string utcTimestamp(JsonProgressReporter::Clock::time_point time)
{
    const std::time_t raw = JsonProgressReporter::Clock::to_time_t(time);
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


void writeString(std::ostream& output, std::string_view value)
{
    constexpr char hex[] = "0123456789abcdef";
    output << '"';
    for (const unsigned char character : value)
    {
        switch (character)
        {
            case '"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\b': output << "\\b"; break;
            case '\f': output << "\\f"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (character < 0x20)
                {
                    output << "\\u00"
                           << hex[(character >> 4) & 0x0f]
                           << hex[character & 0x0f];
                }
                else
                {
                    output << static_cast<char>(character);
                }
        }
    }
    output << '"';
}


void writeOptionalDouble(
    std::ostream& output,
    const std::optional<double>& value
)
{
    if (!value.has_value() || !std::isfinite(*value))
    {
        output << "null";
        return;
    }
    output << std::setprecision(std::numeric_limits<double>::max_digits10)
           << *value;
}


void writeOptionalDuration(
    std::ostream& output,
    const std::optional<std::chrono::nanoseconds>& value
)
{
    if (value.has_value())
    {
        output << value->count();
    }
    else
    {
        output << "null";
    }
}

} // namespace


JsonProgressReporter::JsonProgressReporter(
    std::ostream& output,
    NowFunction now
)
    : output_(output),
      now_(std::move(now))
{
    if (!now_)
    {
        throw std::invalid_argument("JSON reporter clock must be callable");
    }
}


void JsonProgressReporter::report(
    const ProgressSnapshot& snapshot,
    const ProgressMetrics& metrics
)
{
    std::ostringstream record;
    record.imbue(std::locale::classic());
    record << '{';
    record << "\"schema_version\":" << snapshot.schemaVersion();
    record << ",\"sampled_at\":";
    writeString(record, utcTimestamp(now_()));
    record << ",\"phase\":";
    writeString(record, toString(snapshot.phase()));
    record << ",\"terminal_state\":";
    writeString(record, toString(snapshot.terminalState()));
    record << ",\"target_digits\":" << snapshot.targetDigits();
    record << ",\"terms\":{\"completed\":" << snapshot.completedTerms()
           << ",\"total\":" << snapshot.totalTerms() << '}';
    record << ",\"checkpoint_blocks\":{\"completed\":"
           << snapshot.completedCheckpointBlocks()
           << ",\"total\":" << snapshot.totalCheckpointBlocks() << '}';
    record << ",\"current_merge_level\":";
    if (snapshot.currentMergeLevel().has_value())
    {
        record << *snapshot.currentMergeLevel();
    }
    else
    {
        record << "null";
    }
    record << ",\"tasks\":{\"active\":" << snapshot.activeTasks()
           << ",\"queued\":" << snapshot.queuedTasks() << '}';
    record << ",\"elapsed_ns\":" << snapshot.elapsed().count();
    record << ",\"memory_bytes\":" << snapshot.memoryBytes();
    record << ",\"checkpoint_bytes\":" << snapshot.checkpointBytes();
    record << ",\"last_validated_checkpoint\":";
    if (snapshot.lastValidatedCheckpoint().has_value())
    {
        const auto& checkpoint = *snapshot.lastValidatedCheckpoint();
        record << "{\"start_term\":" << checkpoint.startTerm
               << ",\"end_term\":" << checkpoint.endTerm
               << ",\"tree_level\":" << checkpoint.treeLevel << '}';
    }
    else
    {
        record << "null";
    }
    record << ",\"failure_detail\":";
    if (snapshot.failureDetail().has_value())
    {
        writeString(record, *snapshot.failureDetail());
    }
    else
    {
        record << "null";
    }
    record << ",\"metrics\":{\"completion_ratio\":";
    writeOptionalDouble(record, metrics.completionRatio);
    record << ",\"terms_per_second\":";
    writeOptionalDouble(record, metrics.termsPerSecond);
    record << ",\"checkpoint_blocks_per_second\":";
    writeOptionalDouble(record, metrics.checkpointBlocksPerSecond);
    record << ",\"estimated_time_remaining_ns\":";
    writeOptionalDuration(record, metrics.estimatedTimeRemaining);
    record << "}}\n";

    output_ << record.str();
    output_.flush();
}

} // namespace pi::progress
