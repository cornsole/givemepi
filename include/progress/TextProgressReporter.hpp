#pragma once

#include "progress/EtaFormat.hpp"
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

    /// Creates a reporter for an injected stream, output mode, ETA
    /// selection, and ETA presentation format.
    ///
    /// The optional clock makes timestamped log output deterministic in tests.
    /// Throws `std::invalid_argument` when the clock is not callable or the
    /// ETA precision exceeds `EtaFormat::maxPrecision`.
    /// Time and memory complexity: O(1).
    TextProgressReporter(
        std::ostream& output,
        TextOutputMode mode,
        EtaSelection selection = {},
        EtaFormat etaFormat = {},
        NowFunction now = []() { return Clock::now(); }
    );

    /// Formats and emits one human-readable progress record.
    ///
    /// Terminal mode redraws one compact line with phase, selected ETAs, and
    /// lower-priority term/checkpoint fields while they fit without wrapping;
    /// high ETA precision drops checkpoints first and then the term fraction.
    /// Terminal states close the line. Log mode emits one UTC-timestamped
    /// newline-delimited record
    /// with the full field set, including progress percentage, speed,
    /// tasks, memory, checkpoint bytes, storage, I/O, merge,
    /// and ranges. Selected ETAs render as `eta_total=` and `eta_phase=`
    /// followed by the value in fixed-point notation with the configured
    /// fraction digits and a `sec`, `min`, or `hour` suffix (`overdue` when
    /// the estimate has passed, `unknown` when otherwise unavailable);
    /// unselected ETAs are omitted.
    /// Time and memory complexity: O(n) in the rendered record length.
    void report(
        const ProgressSnapshot& snapshot,
        const ProgressMetrics& metrics
    ) override;

private:
    std::ostream& output_;
    TextOutputMode mode_;
    EtaSelection selection_;
    EtaFormat etaFormat_;
    NowFunction now_;
};

} // namespace pi::progress
