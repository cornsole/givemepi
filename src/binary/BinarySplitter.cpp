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

} // namespace

BinaryNode BinarySplitter::merge(
    const BinaryNode& left,
    const BinaryNode& right
)
{
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
    std::size_t end
)
{
    // Placeholder.
    // Scheduler-based parallel execution will be introduced
    // after task synchronization support is available.

    return splitSequential(
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
    const ParallelSplitOptions& options
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


    const std::size_t termCount = end - start;

    if (termCount <= options.sequentialCutoff
        || !executor.running()
        || executor.workerCount() == 0
        || executor.ownsCurrentWorker())
    {
        return splitSequentialValidated(start, end);
    }


    const std::size_t blockCount = boundedBlockCount(
        termCount,
        options.sequentialCutoff,
        executor.workerCount(),
        options.tasksPerWorker
    );

    if (blockCount < 2)
    {
        return splitSequentialValidated(start, end);
    }


    std::vector<std::optional<BinaryNode>> leafResults(blockCount);
    std::vector<scheduler::TaskHandle> handles;
    handles.reserve(blockCount);

    const std::size_t baseBlockSize = termCount / blockCount;
    const std::size_t largerBlockCount = termCount % blockCount;
    std::size_t blockStart = start;

    for (std::size_t index = 0; index < blockCount; ++index)
    {
        const std::size_t blockSize =
            baseBlockSize + (index < largerBlockCount ? 1 : 0);
        const std::size_t blockEnd = blockStart + blockSize;

        scheduler::TaskHandle handle = executor.submit(
            scheduler::Task(
                [&leafResults, index, blockStart, blockEnd]()
                {
                    leafResults[index].emplace(
                        splitSequentialValidated(blockStart, blockEnd)
                    );
                }
            )
        );

        if (handle.valid())
        {
            handles.push_back(std::move(handle));
        }
        else
        {
            leafResults[index].emplace(
                splitSequentialValidated(blockStart, blockEnd)
            );
        }

        blockStart = blockEnd;
    }

    waitForStage(handles);


    std::vector<BinaryNode> nodes;
    nodes.reserve(blockCount);

    for (std::optional<BinaryNode>& result : leafResults)
    {
        nodes.push_back(std::move(result.value()));
    }


    while (nodes.size() > 1)
    {
        const std::size_t pairCount = nodes.size() / 2;

        if (pairCount == 1)
        {
            std::vector<BinaryNode> finalLevel;
            finalLevel.reserve(1 + nodes.size() % 2);
            finalLevel.push_back(
                merge(nodes[0], nodes[1])
            );

            if (nodes.size() % 2 != 0)
            {
                finalLevel.push_back(std::move(nodes.back()));
            }

            nodes = std::move(finalLevel);
            continue;
        }

        std::vector<std::optional<BinaryNode>> mergeResults(
            pairCount
        );
        handles.clear();
        handles.reserve(pairCount);

        for (std::size_t pair = 0; pair < pairCount; ++pair)
        {
            const std::size_t leftIndex = pair * 2;

            scheduler::TaskHandle handle = executor.submit(
                scheduler::Task(
                    [&nodes, &mergeResults, pair, leftIndex]()
                    {
                        mergeResults[pair].emplace(
                            BinarySplitter::merge(
                                nodes[leftIndex],
                                nodes[leftIndex + 1]
                            )
                        );
                    }
                )
            );

            if (handle.valid())
            {
                handles.push_back(std::move(handle));
            }
            else
            {
                mergeResults[pair].emplace(
                    merge(
                        nodes[leftIndex],
                        nodes[leftIndex + 1]
                    )
                );
            }
        }

        waitForStage(handles);


        std::vector<BinaryNode> nextLevel;
        nextLevel.reserve(pairCount + nodes.size() % 2);

        for (std::optional<BinaryNode>& result : mergeResults)
        {
            nextLevel.push_back(std::move(result.value()));
        }

        if (nodes.size() % 2 != 0)
        {
            nextLevel.push_back(std::move(nodes.back()));
        }

        nodes = std::move(nextLevel);
    }


    return std::move(nodes.front());
}

} // namespace pi::binary
