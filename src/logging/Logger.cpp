#include "logging/Logger.hpp"

#include <cstddef>


namespace pi::logging
{
namespace
{

/// Writes `text` with every CR and LF replaced by a single space.
void writeSingleLine(std::ostream& sink, std::string_view text)
{
    std::size_t start = 0;
    while (start < text.size())
    {
        const std::size_t breakAt = text.find_first_of("\r\n", start);
        const std::size_t end =
            breakAt == std::string_view::npos ? text.size() : breakAt;
        sink.write(
            text.data() + start,
            static_cast<std::streamsize>(end - start)
        );
        if (breakAt == std::string_view::npos)
        {
            return;
        }
        sink.put(' ');
        start = breakAt + 1;
    }
}

} // namespace


std::string_view toString(LogLevel level) noexcept
{
    switch (level)
    {
    case LogLevel::debug:
        return "debug";
    case LogLevel::info:
        return "info";
    case LogLevel::warning:
        return "warning";
    case LogLevel::error:
        return "error";
    }
    return "unknown";
}


Logger::Logger(std::ostream& sink, LogLevel threshold) noexcept
    : sink_(sink),
      threshold_(threshold)
{
}


bool Logger::enabled(LogLevel level) const noexcept
{
    return static_cast<std::uint8_t>(level)
        >= static_cast<std::uint8_t>(threshold_);
}


void Logger::write(
    LogLevel level,
    std::string_view component,
    std::string_view message
) noexcept
{
    if (!enabled(level))
    {
        return;
    }

    try
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        const std::string_view label = toString(level);
        sink_.write("pi-engine: ", 11);
        sink_.write(label.data(), static_cast<std::streamsize>(label.size()));
        sink_.write(": ", 2);
        writeSingleLine(sink_, component);
        sink_.write(": ", 2);
        writeSingleLine(sink_, message);
        sink_.put('\n');
        sink_.flush();
    }
    catch (...)
    {
        // Logging is diagnostic only; sink and lock failures are dropped.
    }
}

} // namespace pi::logging
