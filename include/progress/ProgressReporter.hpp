#pragma once

#include "progress/ProgressMetrics.hpp"
#include "progress/ProgressSnapshot.hpp"


namespace pi::progress
{

/// Extension boundary for presenting immutable computation progress.
///
/// Reporters consume already captured raw state and derived metrics. They do
/// not own computation, scheduler, checkpoint, or tracker lifecycles. A
/// reporting runner must serialize calls, choose sampling policy, and isolate
/// slow or throwing implementations from computation workers.
class IProgressReporter
{
public:
    virtual ~IProgressReporter();

    IProgressReporter(const IProgressReporter&) = delete;
    IProgressReporter& operator=(const IProgressReporter&) = delete;
    IProgressReporter(IProgressReporter&&) = delete;
    IProgressReporter& operator=(IProgressReporter&&) = delete;

    /// Presents one immutable progress sample.
    ///
    /// Input: snapshot raw values and metrics derived from that same snapshot.
    /// Output: implementation-defined presentation side effect. Exceptions may
    /// escape and must be contained by the reporting runner. Complexity and
    /// memory use are implementation-defined; computation workers never call
    /// this function directly.
    virtual void report(
        const ProgressSnapshot& snapshot,
        const ProgressMetrics& metrics
    ) = 0;

protected:
    IProgressReporter() = default;
};

} // namespace pi::progress
