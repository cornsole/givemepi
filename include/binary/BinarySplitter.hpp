#pragma once

#include "binary/BinaryNode.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>


namespace pi::scheduler
{
class Scheduler;
}


namespace pi::binary
{

class BinaryMergeCoordinator
{
public:
    virtual ~BinaryMergeCoordinator() = default;

    /// Observes the currently resident merge-level nodes on the coordinator.
    /// Implementations may update synchronous storage accounting, but must
    /// not mutate the supplied nodes during this observation.
    virtual void observeResidentNodes(
        std::span<const BinaryNode> nodes,
        std::uint32_t level
    ) = 0;

    /// Reloads any spilled nodes before the next merge level.
    virtual void prepareMergeNodes(
        std::span<BinaryNode> nodes,
        std::uint32_t level
    ) = 0;

    /// Spills selected nodes after a level has produced its resident set.
    virtual void spillAfterMergeLevel(
        std::span<BinaryNode> nodes,
        std::uint32_t level
    ) = 0;
};

class BinarySplitObserver
{
public:
    virtual ~BinarySplitObserver() = default;

    /// Receives one completed leaf-block term count from a worker or caller.
    /// Implementations must not throw or perform blocking work. O(1) expected.
    virtual void termsCompleted(std::size_t count) noexcept = 0;

    /// Receives each merge level on the coordinating thread. O(1) expected.
    virtual void mergeLevelStarted(std::uint32_t level) noexcept = 0;
};


/// One resident node from a verified checkpoint frontier and its tree level.
struct BinaryFrontierNode
{
    BinaryNode node;
    std::uint32_t treeLevel = 0;
};


class BinaryCheckpointCoordinator
{
public:
    virtual ~BinaryCheckpointCoordinator() = default;

    /// Receives the complete contiguous frontier after each completed stage,
    /// on the coordinating thread and before any storage spill. treeLevels is
    /// index-aligned with nodes. Implementations must not mutate the nodes.
    virtual void commitFrontier(
        std::span<const BinaryNode> nodes,
        std::span<const std::uint32_t> treeLevels
    ) = 0;
};


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
     * initialFrontier supplies resident, non-overlapping nodes inside the
     * range; only the uncovered gaps are computed and reported to observer.
     * Gap leaf blocks have tree level 0 and merged levels increase toward the
     * computation's format-valid maximum. checkpointCoordinator receives the
     * frontier after every stage that computed new nodes, before spill.
     *
     * @throws std::invalid_argument if either policy value is zero or the
     *         initial frontier is out of range, overlapping, empty-ranged, or
     *         not resident.
     */
    [[nodiscard]]
    static BinaryNode splitParallel(
        std::size_t start,
        std::size_t end,
        scheduler::Scheduler& executor,
        const ParallelSplitOptions& options,
        BinarySplitObserver* observer = nullptr,
        BinaryMergeCoordinator* mergeCoordinator = nullptr,
        std::vector<BinaryFrontierNode> initialFrontier = {},
        BinaryCheckpointCoordinator* checkpointCoordinator = nullptr
    );

    /**
     * Merges adjacent nodes left = [a, b) and right = [b, c).
     *
     * @throws std::invalid_argument if left.end() != right.start().
     */
    [[nodiscard]]
    static BinaryNode merge(
        const BinaryNode& left,
        const BinaryNode& right
    );

};


} // namespace pi::binary
