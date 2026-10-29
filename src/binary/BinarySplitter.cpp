#include "binary/BinarySplitter.hpp"
#include "scheduler/Scheduler.hpp"
#include "scheduler/Task.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>


using pi::bigint::GMPInteger;

namespace pi::binary
{

namespace
{

static_assert(
    std::numeric_limits<std::size_t>::digits
    <= std::numeric_limits<std::uint64_t>::digits,
    "Binary Splitting term indices must fit in GMPInteger's input type"
);


BinaryNode makeChudnovskyLeaf(
    std::size_t term,
    std::size_t end
)
{
    BinaryNode node(
        term,
        end
    );


    if (term == 0)
    {
        node.P() = GMPInteger(1);
        node.Q() = GMPInteger(1);
        node.T() = GMPInteger(13591409);

        return node;
    }


    const GMPInteger k(
        static_cast<std::uint64_t>(term)
    );


    GMPInteger firstFactor(k);
    firstFactor.mul(GMPInteger(6));
    firstFactor.sub(GMPInteger(5));

    GMPInteger secondFactor(k);
    secondFactor.mul(GMPInteger(2));
    secondFactor.sub(GMPInteger(1));

    GMPInteger thirdFactor(k);
    thirdFactor.mul(GMPInteger(6));
    thirdFactor.sub(GMPInteger(1));


    node.P().set(firstFactor);
    node.P().mul(secondFactor);
    node.P().mul(thirdFactor);


    node.Q().set(k);
    node.Q().mul(k);
    node.Q().mul(k);
    node.Q().mul(GMPInteger(10939058860032000ULL));


    node.T().set(k);
    node.T().mul(GMPInteger(545140134));
    node.T().add(GMPInteger(13591409));
    node.T().mul(node.P());

    if (term % 2 != 0)
    {
        node.T().negate();
    }


    return node;
}

void validateRange(
    std::size_t start,
    std::size_t end
)
{
    if (start >= end)
    {
        throw std::invalid_argument(
            "Binary Splitting requires start < end"
        );
    }
}


BinaryNode splitSequentialValidated(
    std::size_t start,
    std::size_t end
)
{
    if (end - start == 1)
    {
        return makeChudnovskyLeaf(
            start,
            end
        );
    }


    const std::size_t mid =
        start + (end - start) / 2;


    BinaryNode left =
        splitSequentialValidated(
            start,
            mid
        );

    BinaryNode right =
        splitSequentialValidated(
            mid,
            end
        );


    return BinarySplitter::merge(
        left,
        right
    );
}


void waitForStage(
    std::vector<scheduler::TaskHandle>& handles
)
{
    for (const scheduler::TaskHandle& handle : handles)
    {
        handle.wait();
    }


    for (const scheduler::TaskHandle& handle : handles)
    {
        handle.rethrowIfFailed();
    }
}


std::size_t boundedBlockCount(
    std::size_t termCount,
    std::size_t sequentialCutoff,
    std::size_t workerCount,
    std::size_t tasksPerWorker
) noexcept
{
    const std::size_t requestedBlocks =
        1 + (termCount - 1) / sequentialCutoff;

    const std::size_t maximumBlocks =
        workerCount > std::numeric_limits<std::size_t>::max() / tasksPerWorker
        ? std::numeric_limits<std::size_t>::max()
        : workerCount * tasksPerWorker;

    return std::min(
        requestedBlocks,
        maximumBlocks
    );
}

std::uint32_t maximumTreeLevel(std::size_t termCount) noexcept
{
    std::uint32_t level = 0;
    std::size_t capacity = 1;
    while (capacity < termCount)
    {
        if (capacity > std::numeric_limits<std::size_t>::max() / 2)
        {
            return level + 1;
        }
        capacity *= 2;
        ++level;
    }
    return level;
}


bool acceptsStagedWork(
    scheduler::Scheduler& executor
)
{
    return executor.running()
        && executor.workerCount() != 0
        && !executor.ownsCurrentWorker();
}


void sortAndValidateFrontier(
    std::vector<BinaryFrontierNode>& frontier,
    std::size_t start,
    std::size_t end
)
{
    std::sort(
        frontier.begin(),
        frontier.end(),
        [](const BinaryFrontierNode& left, const BinaryFrontierNode& right)
        {
            return left.node.start() < right.node.start();
        }
    );


    std::size_t coveredEnd = start;

    for (const BinaryFrontierNode& seed : frontier)
    {
        if (seed.node.start() >= seed.node.end())
        {
            throw std::invalid_argument(
                "Binary Splitting frontier node requires start < end"
            );
        }

        if (seed.node.start() < start || seed.node.end() > end)
        {
            throw std::invalid_argument(
                "Binary Splitting frontier node is outside the split range"
            );
        }

        if (seed.node.start() < coveredEnd)
        {
            throw std::invalid_argument(
                "Binary Splitting frontier nodes overlap"
            );
        }

        if (!seed.node.hasValues())
        {
            throw std::invalid_argument(
                "Binary Splitting frontier node is not resident"
            );
        }

        coveredEnd = seed.node.end();
    }
}

} // namespace

BinaryNode BinarySplitter::merge(
    const BinaryNode& left,
    const BinaryNode& right
)
{
    if (left.end() != right.start())
    {
        throw std::invalid_argument(
            "Binary Splitting merge requires adjacent nodes"
        );
    }


    BinaryNode result(
        left.start(),
        right.end()
    );


    result.P().set(
        left.P()
    );

    result.P().mul(
        right.P()
    );


    result.Q().set(
        left.Q()
    );

    result.Q().mul(
        right.Q()
    );


    GMPInteger temp(
        left.T()
    );


    temp.mul(
        right.Q()
    );


    result.T().set(
        left.P()
    );

    result.T().mul(
        right.T()
    );


    temp.add(
        result.T()
    );


    result.T().swap(
        temp
    );


    return result;
}

BinaryNode BinarySplitter::split(
    std::size_t start,
    std::size_t end
)
{
    return splitSequential(
        start,
        end
    );
}

BinaryNode BinarySplitter::splitSequential(
    std::size_t start,
    std::size_t end
)
{
    validateRange(
        start,
        end
    );


    return splitSequentialValidated(
        start,
        end
    );
}

BinaryNode BinarySplitter::splitParallel(
    std::size_t start,
    std::size_t end,
    scheduler::Scheduler& executor,
    std::size_t sequentialCutoff
)
{
    return splitParallel(
        start,
        end,
        executor,
        ParallelSplitOptions{
            sequentialCutoff,
            4
        }
    );
}


BinaryNode BinarySplitter::splitParallel(
    std::size_t start,
    std::size_t end,
    scheduler::Scheduler& executor,
    const ParallelSplitOptions& options,
    BinarySplitObserver* observer,
    BinaryMergeCoordinator* mergeCoordinator,
    std::vector<BinaryFrontierNode> initialFrontier,
    BinaryCheckpointCoordinator* checkpointCoordinator
)
{
    validateRange(start, end);

    if (options.sequentialCutoff == 0)
    {
        throw std::invalid_argument(
            "Binary Splitting sequential cutoff must be greater than zero"
        );
    }

    if (options.tasksPerWorker == 0)
    {
        throw std::invalid_argument(
            "Binary Splitting tasks per worker must be greater than zero"
        );
    }

    sortAndValidateFrontier(initialFrontier, start, end);


    const std::size_t termCount = end - start;
    const bool parallelExecution = acceptsStagedWork(executor);

    if (initialFrontier.empty()
        && (termCount <= options.sequentialCutoff
            || !parallelExecution
            || boundedBlockCount(
                termCount,
                options.sequentialCutoff,
                executor.workerCount(),
                options.tasksPerWorker
            ) < 2))
    {
        BinaryNode result = splitSequentialValidated(start, end);
        if (observer != nullptr)
        {
            observer->termsCompleted(termCount);
        }
        if (checkpointCoordinator != nullptr)
        {
            constexpr std::uint32_t treeLevel = 0;
            checkpointCoordinator->commitFrontier(
                std::span<const BinaryNode>(&result, 1),
                std::span<const std::uint32_t>(&treeLevel, 1)
            );
        }
        return result;
    }


    // Lay out the contiguous frontier: seeds keep their slot and level,
    // every uncovered gap is partitioned into level-0 leaf blocks.
    struct PendingBlock
    {
        std::size_t index;
        std::size_t start;
        std::size_t end;
    };

    std::vector<std::optional<BinaryNode>> slots;
    std::vector<std::uint32_t> treeLevels;
    std::vector<PendingBlock> pendingBlocks;

    const auto appendGap = [&](std::size_t gapStart, std::size_t gapEnd)
    {
        const std::size_t gapSize = gapEnd - gapStart;
        const std::size_t blockCount = parallelExecution
            ? boundedBlockCount(
                gapSize,
                options.sequentialCutoff,
                executor.workerCount(),
                options.tasksPerWorker
            )
            : 1;
        const std::size_t baseBlockSize = gapSize / blockCount;
        const std::size_t largerBlockCount = gapSize % blockCount;
        std::size_t blockStart = gapStart;

        for (std::size_t block = 0; block < blockCount; ++block)
        {
            const std::size_t blockEnd = blockStart
                + baseBlockSize + (block < largerBlockCount ? 1 : 0);
            pendingBlocks.push_back(
                PendingBlock{slots.size(), blockStart, blockEnd}
            );
            slots.emplace_back();
            treeLevels.push_back(0);
            blockStart = blockEnd;
        }
    };

    std::size_t coveredEnd = start;

    for (BinaryFrontierNode& seed : initialFrontier)
    {
        if (coveredEnd < seed.node.start())
        {
            appendGap(coveredEnd, seed.node.start());
        }

        coveredEnd = seed.node.end();
        slots.emplace_back(std::move(seed.node));
        treeLevels.push_back(seed.treeLevel);
    }

    if (coveredEnd < end)
    {
        appendGap(coveredEnd, end);
    }

    initialFrontier.clear();


    std::vector<scheduler::TaskHandle> handles;
    handles.reserve(pendingBlocks.size());

    for (const PendingBlock& block : pendingBlocks)
    {
        const auto computeBlock = [&slots, block, observer]()
        {
            slots[block.index].emplace(
                splitSequentialValidated(block.start, block.end)
            );
            if (observer != nullptr)
            {
                observer->termsCompleted(block.end - block.start);
            }
        };

        scheduler::TaskHandle handle;
        if (parallelExecution)
        {
            handle = executor.submit(scheduler::Task(computeBlock));
        }

        if (handle.valid())
        {
            handles.push_back(std::move(handle));
        }
        else
        {
            computeBlock();
        }
    }

    waitForStage(handles);


    std::vector<BinaryNode> nodes;
    nodes.reserve(slots.size());

    for (std::optional<BinaryNode>& slot : slots)
    {
        nodes.push_back(std::move(slot.value()));
    }

    slots.clear();


    // Each stage: storage accounting, durable checkpoint, then spill.
    const auto completeStage =
        [&](std::uint32_t mergeLevel, bool computedNodes)
    {
        if (mergeCoordinator != nullptr)
        {
            mergeCoordinator->observeResidentNodes(nodes, mergeLevel);
        }
        if (checkpointCoordinator != nullptr && computedNodes)
        {
            checkpointCoordinator->commitFrontier(nodes, treeLevels);
        }
        if (mergeCoordinator != nullptr && nodes.size() > 1)
        {
            mergeCoordinator->spillAfterMergeLevel(nodes, mergeLevel);
        }
    };

    std::uint32_t mergeLevel = 0;
    completeStage(mergeLevel, !pendingBlocks.empty());


    while (nodes.size() > 1)
    {
        if (mergeCoordinator != nullptr)
        {
            mergeCoordinator->prepareMergeNodes(nodes, mergeLevel);
        }
        if (observer != nullptr)
        {
            observer->mergeLevelStarted(mergeLevel);
        }
        ++mergeLevel;
        const std::size_t pairCount = nodes.size() / 2;

        std::vector<std::optional<BinaryNode>> mergeResults(
            pairCount
        );
        handles.clear();
        handles.reserve(pairCount);

        for (std::size_t pair = 0; pair < pairCount; ++pair)
        {
            const std::size_t leftIndex = pair * 2;
            const auto mergePair = [&nodes, &mergeResults, pair, leftIndex]()
            {
                mergeResults[pair].emplace(
                    BinarySplitter::merge(
                        nodes[leftIndex],
                        nodes[leftIndex + 1]
                    )
                );
            };

            scheduler::TaskHandle handle;
            if (parallelExecution && pairCount > 1)
            {
                handle = executor.submit(scheduler::Task(mergePair));
            }

            if (handle.valid())
            {
                handles.push_back(std::move(handle));
            }
            else
            {
                mergePair();
            }
        }

        waitForStage(handles);


        std::vector<BinaryNode> nextLevel;
        std::vector<std::uint32_t> nextTreeLevels;
        nextLevel.reserve(pairCount + nodes.size() % 2);
        nextTreeLevels.reserve(pairCount + nodes.size() % 2);

        for (std::size_t pair = 0; pair < pairCount; ++pair)
        {
            nextLevel.push_back(std::move(mergeResults[pair].value()));
            const std::uint32_t childLevel =
                std::max(treeLevels[pair * 2], treeLevels[pair * 2 + 1]);
            nextTreeLevels.push_back(
                std::min(
                    childLevel == std::numeric_limits<std::uint32_t>::max()
                        ? childLevel
                        : childLevel + 1,
                    maximumTreeLevel(termCount)
                )
            );
        }

        if (nodes.size() % 2 != 0)
        {
            nextLevel.push_back(std::move(nodes.back()));
            nextTreeLevels.push_back(treeLevels.back());
        }

        nodes = std::move(nextLevel);
        treeLevels = std::move(nextTreeLevels);
        completeStage(mergeLevel, true);
    }


    return std::move(nodes.front());
}

} // namespace pi::binary
