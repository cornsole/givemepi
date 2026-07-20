#include "scheduler/LockFreeQueue.hpp"
#include "scheduler/ThreadPool.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>


namespace
{

using pi::scheduler::LockFreeQueue;
using pi::scheduler::SchedulerState;
using pi::scheduler::Task;
using pi::scheduler::TaskHandle;
using pi::scheduler::ThreadPool;


template<typename Predicate>
bool waitUntil(
    Predicate predicate
)
{
    const std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now()
        +
        std::chrono::seconds(
            5
        );


    while (!predicate())
    {
        if (std::chrono::steady_clock::now() >= deadline)
        {
            return false;
        }

        std::this_thread::yield();
    }


    return true;
}


template<typename Function>
void runConcurrently(
    std::size_t callerCount,
    Function function
)
{
    std::atomic<std::size_t> ready{
        0
    };

    std::atomic<bool> begin{
        false
    };

    std::vector<std::thread> callers;

    callers.reserve(
        callerCount
    );


    for (std::size_t i = 0; i < callerCount; ++i)
    {
        callers.emplace_back(
            [&ready, &begin, function]()
            {
                ready.fetch_add(
                    1,
                    std::memory_order_release
                );


                while (!begin.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }


                function();
            }
        );
    }


    assert(
        waitUntil(
            [&ready, callerCount]()
            {
                return ready.load(
                           std::memory_order_acquire
                       ) == callerCount;
            }
        )
    );


    begin.store(
        true,
        std::memory_order_release
    );


    for (std::thread& caller : callers)
    {
        caller.join();
    }
}


struct ProducerResult
{
    std::vector<TaskHandle> acceptedBeforeStop;
    std::vector<TaskHandle> acceptedDuringStop;

    std::size_t rejectedBeforeStop = 0;
    std::size_t rejectedDuringStop = 0;
};


void testConcurrentSubmitAndStopHasOneAcceptanceCutoff()
{
    constexpr std::size_t workerCount =
        4;

    constexpr std::size_t producerCount =
        4;

    constexpr std::size_t submissionsBeforeStop =
        128;

    constexpr std::size_t submissionsDuringStop =
        128;

    constexpr std::size_t submissionsPerProducer =
        submissionsBeforeStop + submissionsDuringStop;

    constexpr std::size_t totalSubmissionCount =
        producerCount * submissionsPerProducer;

    ThreadPool pool(
        std::make_unique<LockFreeQueue>(
            totalSubmissionCount
        ),
        workerCount
    );

    std::mutex blockerMutex;
    std::condition_variable blockersEntered;
    std::condition_variable blockersRelease;

    std::size_t enteredCount = 0;
    bool releaseBlockers = false;

    std::vector<TaskHandle> blockerHandles;

    blockerHandles.reserve(
        workerCount
    );

    std::vector<std::atomic<int>> executions(
        totalSubmissionCount
    );


    pool.start();


    for (std::size_t i = 0; i < workerCount; ++i)
    {
        TaskHandle blocker =
            pool.submit(
                Task(
                    [&]()
                    {
                        std::unique_lock<std::mutex> lock(
                            blockerMutex
                        );

                        ++enteredCount;
                        blockersEntered.notify_one();

                        blockersRelease.wait(
                            lock,
                            [&releaseBlockers]()
                            {
                                return releaseBlockers;
                            }
                        );
                    }
                )
            );

        assert(blocker.valid());

        blockerHandles.push_back(
            std::move(blocker)
        );
    }


    {
        std::unique_lock<std::mutex> lock(
            blockerMutex
        );

        const bool allWorkersEntered =
            blockersEntered.wait_for(
                lock,
                std::chrono::seconds(
                    5
                ),
                [&enteredCount]()
                {
                    return enteredCount == workerCount;
                }
            );

        assert(allWorkersEntered);
    }


    std::atomic<std::size_t> producersReadyForStop{
        0
    };

    std::atomic<bool> submitDuringStop{
        false
    };

    std::vector<ProducerResult> results(
        producerCount
    );

    std::vector<std::thread> producers;

    producers.reserve(
        producerCount
    );


    for (std::size_t producer = 0; producer < producerCount; ++producer)
    {
        results[producer].acceptedBeforeStop.reserve(
            submissionsBeforeStop
        );

        results[producer].acceptedDuringStop.reserve(
            submissionsDuringStop
        );


        producers.emplace_back(
            [&, producer]()
            {
                ProducerResult& result =
                    results[producer];


                for (
                    std::size_t i = 0;
                    i < submissionsBeforeStop;
                    ++i
                )
                {
                    const std::size_t identifier =
                        producer * submissionsPerProducer + i;

                    TaskHandle handle =
                        pool.submit(
                            Task(
                                [&, identifier]()
                                {
                                    executions[identifier].fetch_add(
                                        1,
                                        std::memory_order_relaxed
                                    );
                                }
                            )
                        );


                    if (handle.valid())
                    {
                        result.acceptedBeforeStop.push_back(
                            std::move(handle)
                        );
                    }
                    else
                    {
                        ++result.rejectedBeforeStop;
                    }
                }


                producersReadyForStop.fetch_add(
                    1,
                    std::memory_order_release
                );


                while (
                    !submitDuringStop.load(
                        std::memory_order_acquire
                    )
                )
                {
                    std::this_thread::yield();
                }


                for (
                    std::size_t i = submissionsBeforeStop;
                    i < submissionsPerProducer;
                    ++i
                )
                {
                    const std::size_t identifier =
                        producer * submissionsPerProducer + i;

                    TaskHandle handle =
                        pool.submit(
                            Task(
                                [&, identifier]()
                                {
                                    executions[identifier].fetch_add(
                                        1,
                                        std::memory_order_relaxed
                                    );
                                }
                            )
                        );


                    if (handle.valid())
                    {
                        result.acceptedDuringStop.push_back(
                            std::move(handle)
                        );
                    }
                    else
                    {
                        ++result.rejectedDuringStop;
                    }
                }
            }
        );
    }


    assert(
        waitUntil(
            [&producersReadyForStop]()
            {
                return producersReadyForStop.load(
                           std::memory_order_acquire
                       ) == producerCount;
            }
        )
    );


    std::thread stopper(
        [&pool]()
        {
            pool.stop();
        }
    );


    assert(
        waitUntil(
            [&pool]()
            {
                return pool.state()
                    == SchedulerState::Stopping;
            }
        )
    );


    submitDuringStop.store(
        true,
        std::memory_order_release
    );


    for (std::thread& producer : producers)
    {
        producer.join();
    }


    {
        std::lock_guard<std::mutex> lock(
            blockerMutex
        );

        releaseBlockers = true;
    }

    blockersRelease.notify_all();

    stopper.join();


    assert(pool.state() == SchedulerState::Stopped);
    assert(!pool.running());


    for (TaskHandle& blocker : blockerHandles)
    {
        assert(blocker.isCompleted());
    }


    for (std::size_t producer = 0; producer < producerCount; ++producer)
    {
        const ProducerResult& result =
            results[producer];

        assert(result.rejectedBeforeStop == 0);
        assert(
            result.acceptedBeforeStop.size()
            == submissionsBeforeStop
        );
        assert(result.acceptedDuringStop.empty());
        assert(
            result.rejectedDuringStop
            == submissionsDuringStop
        );


        for (const TaskHandle& handle : result.acceptedBeforeStop)
        {
            assert(handle.isCompleted());
        }


        for (
            std::size_t i = 0;
            i < submissionsPerProducer;
            ++i
        )
        {
            const std::size_t identifier =
                producer * submissionsPerProducer + i;

            const int expectedExecutions =
                i < submissionsBeforeStop
                    ? 1
                    : 0;

            assert(
                executions[identifier].load(
                    std::memory_order_relaxed
                ) == expectedExecutions
            );
        }
    }
}


void testConcurrentRepeatedStartAndStopCalls()
{
    constexpr std::size_t lifecycleCycles =
        12;

    constexpr std::size_t concurrentCallers =
        8;

    constexpr std::size_t taskCountPerCycle =
        64;

    constexpr std::size_t workerCount =
        4;

    ThreadPool pool(
        std::make_unique<LockFreeQueue>(
            taskCountPerCycle * 2
        ),
        workerCount
    );

    std::atomic<std::size_t> executionCount{
        0
    };


    for (std::size_t cycle = 0; cycle < lifecycleCycles; ++cycle)
    {
        runConcurrently(
            concurrentCallers,
            [&pool]()
            {
                pool.start();
            }
        );


        assert(pool.state() == SchedulerState::Running);
        assert(pool.running());

        for (std::size_t worker = 0; worker < workerCount; ++worker)
        {
            assert(pool.workerAt(worker)->running());
        }


        std::vector<TaskHandle> handles;

        handles.reserve(
            taskCountPerCycle
        );


        for (std::size_t i = 0; i < taskCountPerCycle; ++i)
        {
            TaskHandle handle =
                pool.submit(
                    Task(
                        [&executionCount]()
                        {
                            std::this_thread::yield();

                            executionCount.fetch_add(
                                1,
                                std::memory_order_relaxed
                            );
                        }
                    )
                );

            assert(handle.valid());

            handles.push_back(
                std::move(handle)
            );
        }


        runConcurrently(
            concurrentCallers,
            [&pool]()
            {
                pool.stop();
            }
        );


        assert(pool.state() == SchedulerState::Stopped);
        assert(!pool.running());

        for (std::size_t worker = 0; worker < workerCount; ++worker)
        {
            assert(!pool.workerAt(worker)->running());
        }


        for (const TaskHandle& handle : handles)
        {
            assert(handle.isCompleted());
        }
    }


    assert(
        executionCount.load(
            std::memory_order_relaxed
        ) == lifecycleCycles * taskCountPerCycle
    );
}

} // namespace


int main()
{
    testConcurrentSubmitAndStopHasOneAcceptanceCutoff();
    testConcurrentRepeatedStartAndStopCalls();


    std::cout
        << "Scheduler direct concurrency OK\n";


    return 0;
}
