#include "binary/BinaryNode.hpp"
#include "binary/BinarySplitter.hpp"
#include "scheduler/IQueue.hpp"
#include "scheduler/Scheduler.hpp"
#include "scheduler/Task.hpp"

#include <gmp.h>

#include <cstddef>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <set>
#include <stdexcept>
#include <thread>


using pi::binary::BinaryNode;
using pi::binary::BinarySplitter;
using pi::binary::ParallelSplitOptions;
using pi::scheduler::IQueue;
using pi::scheduler::Scheduler;
using pi::scheduler::Task;


namespace
{

class RejectingQueue final : public IQueue
{
public:

    bool push(Task) override
    {
        return false;
    }


    std::optional<Task> pop() override
    {
        return std::nullopt;
    }


    [[nodiscard]]
    bool empty() const noexcept override
    {
        return true;
    }


    [[nodiscard]]
    std::size_t capacity() const noexcept override
    {
        return 0;
    }
};


class DistinctWorkerQueue final : public IQueue
{
public:

    explicit DistinctWorkerQueue(
        std::size_t capacity
    )
        : capacity_(capacity)
    {
    }


    bool push(Task task) override
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (tasks_.size() >= capacity_)
        {
            return false;
        }

        tasks_.push(std::move(task));
        return true;
    }


    std::optional<Task> pop() override
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (tasks_.empty())
        {
            return std::nullopt;
        }

        const std::thread::id current = std::this_thread::get_id();

        if (workerThreads_.size() < 2
            && workerThreads_.contains(current))
        {
            return std::nullopt;
        }

        workerThreads_.insert(current);
        Task task = std::move(tasks_.front());
        tasks_.pop();
        return task;
    }


    [[nodiscard]]
    bool empty() const noexcept override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return tasks_.empty();
    }


    [[nodiscard]]
    std::size_t capacity() const noexcept override
    {
        return capacity_;
    }


    [[nodiscard]]
    std::size_t workerThreadCount() const noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return workerThreads_.size();
    }


private:

    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::queue<Task> tasks_;
    std::set<std::thread::id> workerThreads_;
};

using SplitFunction = BinaryNode (*)(std::size_t, std::size_t);


bool rejectsRange(
    SplitFunction splitFunction,
    std::size_t start,
    std::size_t end
)
{
    try
    {
        static_cast<void>(
            splitFunction(
                start,
                end
            )
        );
    }
    catch (const std::invalid_argument&)
    {
        return true;
    }


    return false;
}


bool hasValues(
    const BinaryNode& node,
    const char* expectedP,
    const char* expectedQ,
    const char* expectedT
)
{
    return node.P().toString() == expectedP
        && node.Q().toString() == expectedQ
        && node.T().toString() == expectedT;
}


bool hasRangeAndValues(
    const BinaryNode& node,
    std::size_t expectedStart,
    std::size_t expectedEnd,
    const char* expectedP,
    const char* expectedQ,
    const char* expectedT
)
{
    return node.start() == expectedStart
        && node.end() == expectedEnd
        && hasValues(
            node,
            expectedP,
            expectedQ,
            expectedT
        );
}


bool matchesKnownPiDigits(
    const BinaryNode& node
)
{
    constexpr mp_bitcnt_t precisionBits = 256;

    mpf_t q;
    mpf_t t;
    mpf_t squareRoot;
    mpf_t pi;

    mpf_init2(q, precisionBits);
    mpf_init2(t, precisionBits);
    mpf_init2(squareRoot, precisionBits);
    mpf_init2(pi, precisionBits);

    mpf_set_z(q, *node.Q().raw());
    mpf_set_z(t, *node.T().raw());
    mpf_sqrt_ui(squareRoot, 10005);

    mpf_mul_ui(pi, squareRoot, 426880);
    mpf_mul(pi, pi, q);
    mpf_div(pi, pi, t);

    char decimal[64] = {};
    const int written = gmp_snprintf(
        decimal,
        sizeof(decimal),
        "%.20Ff",
        pi
    );

    mpf_clear(pi);
    mpf_clear(squareRoot);
    mpf_clear(t);
    mpf_clear(q);

    return written > 0
        && std::strcmp(
            decimal,
            "3.14159265358979323846"
        ) == 0;
}


bool nodesEqual(
    const BinaryNode& left,
    const BinaryNode& right
)
{
    return left.start() == right.start()
        && left.end() == right.end()
        && left.P().compare(right.P()) == 0
        && left.Q().compare(right.Q()) == 0
        && left.T().compare(right.T()) == 0;
}

} // namespace


int main()
{
    const SplitFunction splitFunctions[] = {
        &BinarySplitter::split,
        &BinarySplitter::splitSequential,
        &BinarySplitter::splitParallel
    };


    for (const SplitFunction splitFunction : splitFunctions)
    {
        constexpr std::size_t maximumIndex =
            std::numeric_limits<std::size_t>::max();

        if (!rejectsRange(splitFunction, 0, 0)
            || !rejectsRange(splitFunction, 1, 1)
            || !rejectsRange(splitFunction, 2, 1)
            || !rejectsRange(splitFunction, maximumIndex, maximumIndex)
            || !rejectsRange(splitFunction, maximumIndex, 0))
        {
            std::cerr
                << "Invalid range was accepted\n";

            return 1;
        }


        const BinaryNode zeroLeaf =
            splitFunction(
                0,
                1
            );

        const BinaryNode oneLeaf =
            splitFunction(
                1,
                2
            );


        if (zeroLeaf.start() != 0 || zeroLeaf.end() != 1
            || oneLeaf.start() != 1 || oneLeaf.end() != 2)
        {
            std::cerr
                << "Minimum valid range failed\n";

            return 1;
        }
    }


    constexpr std::size_t maximumIndex =
        std::numeric_limits<std::size_t>::max();

    const BinaryNode upperBoundaryLeaf =
        BinarySplitter::splitSequential(
            maximumIndex - 1,
            maximumIndex
        );


    if (upperBoundaryLeaf.start() != maximumIndex - 1
        || upperBoundaryLeaf.end() != maximumIndex
        || upperBoundaryLeaf.P().isZero()
        || upperBoundaryLeaf.Q().isZero()
        || upperBoundaryLeaf.T().isZero())
    {
        std::cerr
            << "Upper-boundary leaf failed\n";

        return 1;
    }


    const BinaryNode leafZero =
        BinarySplitter::splitSequential(
            0,
            1
        );

    const BinaryNode leafOne =
        BinarySplitter::splitSequential(
            1,
            2
        );

    const BinaryNode leafTwo =
        BinarySplitter::splitSequential(
            2,
            3
        );


    if (!hasValues(leafZero, "1", "1", "13591409"))
    {
        std::cerr
            << "Chudnovsky leaf k=0 failed\n";

        return 1;
    }


    if (!hasValues(
            leafOne,
            "5",
            "10939058860032000",
            "-2793657715"
        ))
    {
        std::cerr
            << "Chudnovsky leaf k=1 failed\n";

        return 1;
    }


    if (!hasValues(
            leafTwo,
            "231",
            "87512470880256000",
            "254994357387"
        ))
    {
        std::cerr
            << "Chudnovsky leaf k=2 failed\n";

        return 1;
    }


    const BinaryNode rangeZeroToTwo =
        BinarySplitter::splitSequential(
            0,
            2
        );

    const BinaryNode rangeOneToThree =
        BinarySplitter::splitSequential(
            1,
            3
        );

    const BinaryNode rangeZeroToThree =
        BinarySplitter::splitSequential(
            0,
            3
        );


    if (!hasRangeAndValues(
            rangeZeroToTwo,
            0,
            2,
            "5",
            "10939058860032000",
            "148677223041765871430285"
        ))
    {
        std::cerr
            << "Chudnovsky range [0, 2) failed\n";

        return 1;
    }


    if (!hasRangeAndValues(
            rangeOneToThree,
            1,
            3,
            "1155",
            "957304069945956794936328192000000",
            "-244479889433338740603253065"
        ))
    {
        std::cerr
            << "Chudnovsky range [1, 3) failed\n";

        return 1;
    }


    if (!hasRangeAndValues(
            rangeZeroToThree,
            0,
            3,
            "1155",
            "957304069945956794936328192000000",
            "13011111151999862216419332076961924746935"
        ))
    {
        std::cerr
            << "Chudnovsky range [0, 3) failed\n";

        return 1;
    }


    if (!matchesKnownPiDigits(rangeZeroToThree))
    {
        std::cerr
            << "Known pi digits integration failed\n";

        return 1;
    }


    const BinaryNode recursiveRange =
        BinarySplitter::splitSequential(
            0,
            16
        );

    const BinaryNode defaultRecursiveRange =
        BinarySplitter::split(
            0,
            16
        );

    const BinaryNode parallelRecursiveRange =
        BinarySplitter::splitParallel(
            0,
            16
        );


    if (!nodesEqual(recursiveRange, defaultRecursiveRange)
        || !nodesEqual(recursiveRange, parallelRecursiveRange))
    {
        std::cerr
            << "Multi-level recursive range mismatch\n";

        return 1;
    }


    Scheduler stoppedScheduler(2, 64);

    if (!nodesEqual(
            recursiveRange,
            BinarySplitter::splitParallel(
                0,
                16,
                stoppedScheduler,
                2
            )
        ))
    {
        std::cerr
            << "Stopped scheduler fallback failed\n";

        return 1;
    }


    bool rejectedZeroCutoff = false;

    try
    {
        static_cast<void>(
            BinarySplitter::splitParallel(
                0,
                16,
                stoppedScheduler,
                0
            )
        );
    }
    catch (const std::invalid_argument&)
    {
        rejectedZeroCutoff = true;
    }

    if (!rejectedZeroCutoff)
    {
        std::cerr
            << "Zero parallel cutoff was accepted\n";

        return 1;
    }


    bool rejectedZeroTasksPerWorker = false;

    try
    {
        static_cast<void>(
            BinarySplitter::splitParallel(
                0,
                16,
                stoppedScheduler,
                ParallelSplitOptions{
                    1,
                    0
                }
            )
        );
    }
    catch (const std::invalid_argument&)
    {
        rejectedZeroTasksPerWorker = true;
    }

    if (!rejectedZeroTasksPerWorker)
    {
        std::cerr
            << "Zero tasks-per-worker policy was accepted\n";

        return 1;
    }


    Scheduler zeroWorkerScheduler(0, 2);
    zeroWorkerScheduler.start();

    const BinaryNode zeroWorkerResult =
        BinarySplitter::splitParallel(
            0,
            16,
            zeroWorkerScheduler,
            1
        );

    zeroWorkerScheduler.stop();

    if (!nodesEqual(recursiveRange, zeroWorkerResult))
    {
        std::cerr
            << "Zero-worker scheduler fallback failed\n";

        return 1;
    }


    Scheduler parallelScheduler(4, 128);
    parallelScheduler.start();

    const BinaryNode parallelDagResult =
        BinarySplitter::splitParallel(
            0,
            16,
            parallelScheduler,
            1
        );

    const BinaryNode unevenSequentialResult =
        BinarySplitter::splitSequential(
            0,
            17
        );

    const BinaryNode unevenParallelResult =
        BinarySplitter::splitParallel(
            0,
            17,
            parallelScheduler,
            ParallelSplitOptions{
                4,
                2
            }
        );

    const BinaryNode cutoffBoundarySequential =
        BinarySplitter::splitSequential(
            0,
            4
        );

    const BinaryNode cutoffBoundaryParallel =
        BinarySplitter::splitParallel(
            0,
            4,
            parallelScheduler,
            4
        );

    parallelScheduler.stop();

    if (!nodesEqual(recursiveRange, parallelDagResult)
        || !nodesEqual(unevenSequentialResult, unevenParallelResult)
        || !nodesEqual(
            cutoffBoundarySequential,
            cutoffBoundaryParallel
        ))
    {
        std::cerr
            << "Parallel DAG result mismatch\n";

        return 1;
    }


    auto rejectingQueue = std::make_unique<RejectingQueue>();
    Scheduler rejectingScheduler(std::move(rejectingQueue), 2);
    rejectingScheduler.start();

    const BinaryNode rejectedSubmissionResult =
        BinarySplitter::splitParallel(
            0,
            16,
            rejectingScheduler,
            1
        );

    rejectingScheduler.stop();

    if (!nodesEqual(recursiveRange, rejectedSubmissionResult))
    {
        std::cerr
            << "Rejected submission fallback failed\n";

        return 1;
    }


    Scheduler workerFallbackScheduler(2, 32);
    workerFallbackScheduler.start();
    std::optional<BinaryNode> workerFallbackResult;
    bool observedOwnedWorker = false;

    pi::scheduler::TaskHandle workerFallbackHandle =
        workerFallbackScheduler.submit(
            Task(
                [&]()
                {
                    observedOwnedWorker =
                        workerFallbackScheduler.ownsCurrentWorker();
                    workerFallbackResult.emplace(
                        BinarySplitter::splitParallel(
                            0,
                            16,
                            workerFallbackScheduler,
                            1
                        )
                    );
                }
            )
        );

    workerFallbackHandle.wait();
    workerFallbackHandle.rethrowIfFailed();
    workerFallbackScheduler.stop();

    if (!observedOwnedWorker
        || !workerFallbackResult.has_value()
        || !nodesEqual(recursiveRange, *workerFallbackResult))
    {
        std::cerr
            << "Worker-context sequential fallback failed\n";

        return 1;
    }


    auto distinctQueue = std::make_unique<DistinctWorkerQueue>(128);
    DistinctWorkerQueue* distinctQueueObserver = distinctQueue.get();
    Scheduler observedParallelScheduler(std::move(distinctQueue), 4);
    observedParallelScheduler.start();

    const BinaryNode observedParallelResult =
        BinarySplitter::splitParallel(
            0,
            16,
            observedParallelScheduler,
            1
        );

    observedParallelScheduler.stop();

    if (!nodesEqual(recursiveRange, observedParallelResult)
        || distinctQueueObserver->workerThreadCount() < 2)
    {
        std::cerr
            << "Observable parallel execution failed\n";

        return 1;
    }


    const BinaryNode defaultRange =
        BinarySplitter::split(
            0,
            3
        );

    const BinaryNode parallelCompatibleRange =
        BinarySplitter::splitParallel(
            0,
            3
        );


    if (!hasRangeAndValues(
            defaultRange,
            0,
            3,
            "1155",
            "957304069945956794936328192000000",
            "13011111151999862216419332076961924746935"
        )
        || !hasRangeAndValues(
            parallelCompatibleRange,
            0,
            3,
            "1155",
            "957304069945956794936328192000000",
            "13011111151999862216419332076961924746935"
        ))
    {
        std::cerr
            << "Binary Splitting entry-point result mismatch\n";

        return 1;
    }


    BinaryNode left(
        0,
        5
    );


    BinaryNode right(
        5,
        10
    );


    left.P() = pi::bigint::GMPInteger(
        2
    );

    left.Q() = pi::bigint::GMPInteger(
        3
    );

    left.T() = pi::bigint::GMPInteger(
        4
    );


    right.P() = pi::bigint::GMPInteger(
        5
    );

    right.Q() = pi::bigint::GMPInteger(
        7
    );

    right.T() = pi::bigint::GMPInteger(
        11
    );


    BinaryNode result =
        BinarySplitter::merge(
            left,
            right
        );


    // P = 2 * 5
    if (result.P().toString() != "10")
    {
        std::cerr
            << "P merge failed\n";

        return 1;
    }


    // Q = 3 * 7
    if (result.Q().toString() != "21")
    {
        std::cerr
            << "Q merge failed\n";

        return 1;
    }


    // T = 4 * 7 + 2 * 11
    //   = 50
    if (result.T().toString() != "50")
    {
        std::cerr
            << "T merge failed\n";

        return 1;
    }


    std::cout
        << "BinarySplitting OK\n";


    std::cout
        << "P="
        << result.P().toString()
        << " Q="
        << result.Q().toString()
        << " T="
        << result.T().toString()
        << '\n';


    BinaryNode splitResult =
        BinarySplitter::split(
            0,
            8
        );


    if (splitResult.start() != 0)
    {
        std::cerr
            << "Split start failed\n";

        return 1;
    }


    if (splitResult.end() != 8)
    {
        std::cerr
            << "Split end failed\n";

        return 1;
    }


    std::cout
        << "Recursive Split OK\n"; 
    

    
    BinaryNode sequentialResult =
        BinarySplitter::splitSequential(
            0,
            8
        );

    BinaryNode parallelResult =
        BinarySplitter::splitParallel(
            0,
            8
        );


    if (sequentialResult.start() != parallelResult.start())
    {
        std::cerr
            << "Parallel start mismatch\n";

        return 1;
    }


    if (sequentialResult.end() != parallelResult.end())
    {
        std::cerr
            << "Parallel end mismatch\n";

        return 1;
    }


    if (sequentialResult.P().toString() != parallelResult.P().toString())
    {
        std::cerr
            << "Parallel P mismatch\n";

        return 1;
    }


    if (sequentialResult.Q().toString() != parallelResult.Q().toString())
    {
        std::cerr
            << "Parallel Q mismatch\n";

        return 1;
    }


    if (sequentialResult.T().toString() != parallelResult.T().toString())
    {
        std::cerr
            << "Parallel T mismatch\n";

        return 1;
    }


    std::cout
        << "Parallel API OK\n";
    
    return 0;
}
