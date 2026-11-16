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

    /// Receives the total weighted merge work once, on the coordinating
    /// thread, after the leaf stage and before the first merge level. It is
    /// the sum of BinarySplitter::mergeLevelWorkUnits() over every planned
    /// pair. Zero means the frontier needs no merge. O(1) expected.
    virtual void mergeWorkPlanned(std::uint64_t units) noexcept = 0;

    /// Receives the level-scaled units (mergeLevelWorkUnits()) of one
    /// finished pair merge from a worker or the caller. Must not throw or
    /// block. O(1) expected.
    virtual void mergeWorkCompleted(std::uint64_t units) noexcept = 0;
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

    /**
     * Returns the weighted progress units of merging a node pair spanning
     * termSpan terms: termSpan * bit_width(termSpan), an n log n proxy for
     * the multiplication cost, saturating at UINT64_MAX.
     *
     * Time complexity: O(1). Memory complexity: O(1).
     */
    [[nodiscard]]
    static std::uint64_t mergeWorkUnits(std::size_t termSpan) noexcept;

    /**
     * Returns the progress units published for one pair merge spanning
     * termSpan terms at a merge level of `pairs` pairs run on `workers`
     * threads: mergeWorkUnits(termSpan) divided by min(workers, pairs),
     * rounded up. Pairs of one level run concurrently, so a level's units
     * are proportional to its wall time and the merge rate stays constant
     * from wide to serial levels. Zero workers or pairs count as one.
     *
     * Time complexity: O(1). Memory complexity: O(1).
     */
    [[nodiscard]]
    static std::uint64_t mergeLevelWorkUnits(
        std::size_t termSpan,
        std::size_t pairs,
        std::size_t workers
    ) noexcept;

    /// One output position of a merge level.
    struct MergeSlot
    {
        /// Index of the first input node of this slot.
        std::size_t left;
        /// True: merge inputs left and left + 1. False: carry input left.
        bool merge;
    };

    /**
     * Plans one merge level over adjacent node term spans.
     *
     * Purpose: keep the merge tree balanced when node sizes differ, for
     * example a large promoted or resumed seed next to small leaf blocks.
     * A node whose span exceeds twice the level's median span is carried to
     * the next level unchanged; the other nodes pair left to right with an
     * adjacent non-carried neighbour. When no pair forms, the adjacent pair
     * with the smallest combined span merges, so every level of two or more
     * nodes makes progress. A level without oversized nodes pairs exactly as
     * (0,1), (2,3), ... with an odd tail carried, as before PR-0041.
     *
     * Input: spans of the current level in term order; zero or one span
     * yields an empty plan. Output: slots in term order covering every
     * input exactly once. The result depends only on the spans.
     *
     * Time complexity: O(n). Memory complexity: O(n).
     */
    [[nodiscard]]
    static std::vector<MergeSlot> planMergeLevel(
        std::span<const std::size_t> spans
    );

};


} // namespace pi::binary
