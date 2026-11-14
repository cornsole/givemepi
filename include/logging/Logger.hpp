#pragma once

#include <cstdint>
#include <mutex>
#include <ostream>
#include <string_view>


namespace pi::logging
{

/// Diagnostic severity, ordered from least to most severe.
///
/// A record is enabled when its level is at least the logger threshold.
enum class LogLevel : std::uint8_t
{
    debug,
    info,
    warning,
    error
};


/// Returns the lowercase record label for a level.
///
/// Values outside the enumerators yield "unknown". Time complexity: O(1).
std::string_view toString(LogLevel level) noexcept;


/// Minimal thread-safe line logger for one injected text sink.
///
/// Each enabled record is emitted as exactly one line:
/// `pi-engine: <level>: <component>: <message>\n`. CR and LF characters in
/// the component and message are replaced with spaces so a record cannot
/// span or forge additional lines. Complete records are serialized by an
/// instance mutex; the sink must outlive the logger and must not be written
/// concurrently through other paths. Sink failures and exceptions are
/// contained: logging never throws and never alters the caller's control flow.
class Logger
{
public:
    /// Binds the logger to a sink and a minimum enabled level.
    ///
    /// Time and memory complexity: O(1).
    Logger(std::ostream& sink, LogLevel threshold) noexcept;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    /// Reports whether records at `level` would be written.
    ///
    /// Time complexity: O(1).
    [[nodiscard]] bool enabled(LogLevel level) const noexcept;

    /// Writes one normalized record when `level` is enabled.
    ///
    /// The record is written directly to the sink without intermediate heap
    /// allocation and then flushed. Exceptions thrown by the sink are caught
    /// and discarded; a failed sink silently drops records. Time complexity:
    /// O(n) in the component and message length.
    void write(
        LogLevel level,
        std::string_view component,
        std::string_view message
    ) noexcept;

private:
    std::ostream& sink_;
    LogLevel threshold_;
    std::mutex mutex_;
};

} // namespace pi::logging
