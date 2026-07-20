#pragma once

#include "progress/ProgressReporter.hpp"

#include <chrono>
#include <functional>
#include <iosfwd>


namespace pi::progress
{

/// Rendering mode selected from whether the destination is an interactive TTY.
enum class TextOutputMode
{
    terminal,
    log
};


/// Human-readable progress reporter for interactive terminals and text logs.
class TextProgressReporter final : public IProgressReporter
{
public:
    using Clock = std::chrono::system_clock;
    using NowFunction = std::function<Clock::time_point()>;

    /// Creates a reporter for an injected stream and output mode.
    ///
    /// The optional clock makes timestamped log output deterministic in tests.
    /// Time and memory complexity: O(1).
    TextProgressReporter(
        std::ostream& output,
        TextOutputMode mode,
        NowFunction now = []() { return Clock::now(); }
    );

    /// Formats and emits one human-readable progress record.
    ///
    /// Terminal mode replaces the current line and closes it on terminal
    /// states. Log mode emits one UTC-timestamped newline-delimited record.
    /// Time and memory complexity: O(n) in the rendered record length.
    void report(
        const ProgressSnapshot& snapshot,
        const ProgressMetrics& metrics
    ) override;

private:
    std::ostream& output_;
    TextOutputMode mode_;
    NowFunction now_;
};

} // namespace pi::progress
