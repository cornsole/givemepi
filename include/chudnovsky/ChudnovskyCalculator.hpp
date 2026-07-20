#pragma once

#include "binary/BinarySplitter.hpp"
#include "chudnovsky/PrecisionPolicy.hpp"

#include <chrono>
#include <cstdint>
#include <string>


namespace pi::scheduler
{
class Scheduler;
}

namespace pi::progress
{
class ProgressTracker;
}


namespace pi::chudnovsky
{

struct PiCalculationRequest
{
    std::uint64_t digits;
    std::uint32_t guardDigits = PrecisionPolicy::defaultGuardDigits;
    /// Leaves an attached tracker in finalizing for a caller-owned output and
    /// verification pipeline. The default preserves standalone API behavior.
    bool deferProgressCompletion = false;
};


struct PiCalculationResult
{
    PrecisionPlan precision;
    std::string decimal;

    struct Timings
    {
        std::chrono::nanoseconds split;
        std::chrono::nanoseconds finalize;
        std::chrono::nanoseconds format;

        [[nodiscard]]
        std::chrono::nanoseconds total() const noexcept
        {
            return split + finalize + format;
        }
    } timings;
};


class ChudnovskyCalculator
{
public:

    /**
     * Calculates pi through sequential Binary Splitting.
     *
     * Input: requested decimal digits and an explicit guard policy.
     * Output: the precision identity and normalized rounded decimal string.
     * Time complexity: O(M(n) log(n)^2).
     * Memory complexity: O(n log(n)).
     */
    [[nodiscard]]
    static PiCalculationResult calculateSequential(
        const PiCalculationRequest& request,
        progress::ProgressTracker* progress = nullptr
    );

    /**
     * Calculates pi through scheduler-backed staged Binary Splitting.
     *
     * The scheduler and split policy remain caller-owned. Scheduler fallback
     * rules are inherited from BinarySplitter::splitParallel().
     */
    [[nodiscard]]
    static PiCalculationResult calculateParallel(
        const PiCalculationRequest& request,
        scheduler::Scheduler& executor,
        const binary::ParallelSplitOptions& splitOptions,
        progress::ProgressTracker* progress = nullptr
    );
};

} // namespace pi::chudnovsky
