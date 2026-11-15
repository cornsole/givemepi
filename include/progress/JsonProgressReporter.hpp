#pragma once

#include "progress/ProgressReporter.hpp"

#include <chrono>
#include <functional>
#include <iosfwd>


namespace pi::progress
{

/// Versioned machine-readable JSON Lines progress reporter.
class JsonProgressReporter final : public IProgressReporter
{
public:
    using Clock = std::chrono::system_clock;
    using NowFunction = std::function<Clock::time_point()>;

    /// Creates a JSON reporter for an injected stream, ETA selection, and
    /// optional clock.
    ///
    /// Time and memory complexity: O(1).
    explicit JsonProgressReporter(
        std::ostream& output,
        EtaSelection selection = {},
        NowFunction now = []() { return Clock::now(); }
    );

    /// Emits one complete versioned JSON object followed by a newline.
    ///
    /// Unavailable optional values are encoded as JSON null. Non-finite derived
    /// floating-point values are also encoded as null so output remains valid
    /// JSON. Both `estimated_total_time_remaining_ns` and
    /// `estimated_phase_time_remaining_ns` are always present; each is null
    /// when unselected or not yet estimable. Time and memory complexity: O(n)
    /// in the record length.
    void report(
        const ProgressSnapshot& snapshot,
        const ProgressMetrics& metrics
    ) override;

private:
    std::ostream& output_;
    EtaSelection selection_;
    NowFunction now_;
};

} // namespace pi::progress
