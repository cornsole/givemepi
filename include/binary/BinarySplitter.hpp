#pragma once

#include "binary/BinaryNode.hpp"


namespace pi::scheduler
{
class Scheduler;
}


namespace pi::binary
{

struct ParallelSplitOptions
{
    std::size_t sequentialCutoff;
    std::size_t tasksPerWorker = 4;
};


class BinarySplitter
{
public:

    /**
     * Computes the Binary Splitting node for the half-open range [start, end).
     *
     * @throws std::invalid_argument if start is greater than or equal to end.
     */
    [[nodiscard]]
    static BinaryNode split(
        std::size_t start,
        std::size_t end
    );

    /**
     * Computes [start, end) using recursive sequential Binary Splitting.
     *
     * @throws std::invalid_argument if start is greater than or equal to end.
     */
    [[nodiscard]]
    static BinaryNode splitSequential(
        std::size_t start,
        std::size_t end
    );

    /**
     * Computes [start, end) through the parallel-compatible entry point.
     *
     * @throws std::invalid_argument if start is greater than or equal to end.
     */
    [[nodiscard]]
    static BinaryNode splitParallel(
        std::size_t start,
        std::size_t end
    );

    /**
     * Computes [start, end) through a staged parallel Binary Splitting DAG.
     *
     * Input: a running scheduler and a non-zero minimum sequential block size.
     * Output: the same P/Q/T node as splitSequential().
     * Time complexity: O(n log(n)^2) arithmetic work, distributed by stage.
     * Memory complexity: O(min(n / cutoff, worker count)) result nodes.
     *
     * @throws std::invalid_argument if the range is invalid or cutoff is zero.
     */
    [[nodiscard]]
    static BinaryNode splitParallel(
        std::size_t start,
        std::size_t end,
        scheduler::Scheduler& executor,
        std::size_t sequentialCutoff
    );

    /**
     * Computes [start, end) using an explicit parallel execution policy.
     *
     * @throws std::invalid_argument if either policy value is zero.
     */
    [[nodiscard]]
    static BinaryNode splitParallel(
        std::size_t start,
        std::size_t end,
        scheduler::Scheduler& executor,
        const ParallelSplitOptions& options
    );

    [[nodiscard]]
    static BinaryNode merge(
        const BinaryNode& left,
        const BinaryNode& right
    );

};


} // namespace pi::binary
