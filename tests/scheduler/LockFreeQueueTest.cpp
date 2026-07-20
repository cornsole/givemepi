#include "scheduler/LockFreeQueue.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>


namespace
{

using pi::scheduler::LockFreeQueue;
using pi::scheduler::Task;


void testCapacityContract()
{
    for (const std::size_t capacity : {0U, 1U})
    {
        bool rejected = false;

        try
        {
            LockFreeQueue queue(
                capacity
            );
        }
        catch (const std::invalid_argument&)
        {
            rejected = true;
        }


        assert(rejected);
    }


    if (
        std::numeric_limits<std::size_t>::max()
        >
        static_cast<std::size_t>(
            std::numeric_limits<std::ptrdiff_t>::max()
        )
    )
    {
        bool rejected = false;

        try
        {
            LockFreeQueue queue(
                static_cast<std::size_t>(
                    std::numeric_limits<std::ptrdiff_t>::max()
                )
                + 1
            );
        }
        catch (const std::invalid_argument&)
        {
            rejected = true;
        }


        assert(rejected);
    }


    LockFreeQueue queue(
        2
    );

    assert(queue.capacity() == 2);
    assert(queue.empty());
}


void testBoundedFifoAndSlotReuse()
{
    LockFreeQueue queue(
        2
    );

    std::vector<int> executionOrder;


    assert(
        queue.push(
            Task(
                [&executionOrder]()
                {
                    executionOrder.push_back(
                        1
                    );
                }
            )
        )
    );

    assert(
        queue.push(
            Task(
                [&executionOrder]()
                {
                    executionOrder.push_back(
                        2
                    );
                }
            )
        )
    );

    assert(
        !queue.push(
            Task(
                [&executionOrder]()
                {
                    executionOrder.push_back(
                        -1
                    );
                }
            )
        )
    );


    std::optional<Task> first =
        queue.pop();

    assert(first.has_value());

    first->execute();


    assert(
        queue.push(
            Task(
                [&executionOrder]()
                {
                    executionOrder.push_back(
                        3
                    );
                }
            )
        )
    );


    for (int expected = 2; expected <= 3; ++expected)
    {
        std::optional<Task> task =
            queue.pop();

        assert(task.has_value());

        task->execute();

        assert(executionOrder.back() == expected);
    }


    assert(
        executionOrder
        ==
        std::vector<int>({1, 2, 3})
    );

    assert(queue.empty());
    assert(!queue.pop().has_value());


    constexpr int reuseCount =
        20000;

    std::atomic<int> executed{
        0
    };


    for (int i = 0; i < reuseCount; ++i)
    {
        assert(
            queue.push(
                Task(
                    [&executed]()
                    {
                        executed.fetch_add(
                            1,
                            std::memory_order_relaxed
                        );
                    }
                )
            )
        );


        std::optional<Task> task =
            queue.pop();

        assert(task.has_value());

        task->execute();
    }


    assert(executed.load(std::memory_order_relaxed) == reuseCount);
    assert(queue.empty());
}


void testConcurrentProducersDoNotReportFalseFull()
{
    constexpr int producerCount =
        8;

    constexpr int tasksPerProducer =
        1000;

    constexpr int totalTaskCount =
        producerCount * tasksPerProducer;

    LockFreeQueue queue(
        totalTaskCount + 1
    );

    std::atomic<int> ready{
        0
    };

    std::atomic<bool> start{
        false
    };

    std::atomic<int> rejected{
        0
    };

    std::vector<std::atomic<int>> seen(
        totalTaskCount
    );

    std::vector<std::thread> producers;

    producers.reserve(
        producerCount
    );


    for (int producer = 0; producer < producerCount; ++producer)
    {
        producers.emplace_back(
            [&, producer]()
            {
                ready.fetch_add(
                    1,
                    std::memory_order_release
                );


                while (!start.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }


                for (int i = 0; i < tasksPerProducer; ++i)
                {
                    const int identifier =
                        producer * tasksPerProducer + i;


                    const bool accepted =
                        queue.push(
                            Task(
                                [&, identifier]()
                                {
                                    seen[identifier].fetch_add(
                                        1,
                                        std::memory_order_relaxed
                                    );
                                }
                            )
                        );


                    if (!accepted)
                    {
                        rejected.fetch_add(
                            1,
                            std::memory_order_relaxed
                        );
                    }
                }
            }
        );
    }


    while (ready.load(std::memory_order_acquire) != producerCount)
    {
        std::this_thread::yield();
    }

    start.store(
        true,
        std::memory_order_release
    );


    for (std::thread& producer : producers)
    {
        producer.join();
    }


    assert(rejected.load(std::memory_order_relaxed) == 0);


    int popped = 0;

    while (std::optional<Task> task = queue.pop())
    {
        task->execute();
        ++popped;
    }


    assert(popped == totalTaskCount);

    for (const std::atomic<int>& count : seen)
    {
        assert(count.load(std::memory_order_relaxed) == 1);
    }


    assert(queue.empty());
}


void testConcurrentProducersAndConsumersPreserveEveryTask()
{
    constexpr int producerCount =
        4;

    constexpr int consumerCount =
        4;

    constexpr int tasksPerProducer =
        3000;

    constexpr int totalTaskCount =
        producerCount * tasksPerProducer;

    LockFreeQueue queue(
        256
    );

    std::atomic<int> ready{
        0
    };

    std::atomic<bool> start{
        false
    };

    std::atomic<bool> timedOut{
        false
    };

    std::atomic<int> pushed{
        0
    };

    std::atomic<int> consumed{
        0
    };

    std::vector<std::atomic<int>> seen(
        totalTaskCount
    );

    const std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now()
        +
        std::chrono::seconds(
            10
        );

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    producers.reserve(
        producerCount
    );

    consumers.reserve(
        consumerCount
    );


    for (int producer = 0; producer < producerCount; ++producer)
    {
        producers.emplace_back(
            [&, producer]()
            {
                ready.fetch_add(
                    1,
                    std::memory_order_release
                );


                while (!start.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }


                for (int i = 0; i < tasksPerProducer; ++i)
                {
                    const int identifier =
                        producer * tasksPerProducer + i;


                    while (!timedOut.load(std::memory_order_acquire))
                    {
                        if (
                            queue.push(
                                Task(
                                    [&, identifier]()
                                    {
                                        seen[identifier].fetch_add(
                                            1,
                                            std::memory_order_relaxed
                                        );
                                    }
                                )
                            )
                        )
                        {
                            pushed.fetch_add(
                                1,
                                std::memory_order_relaxed
                            );

                            break;
                        }


                        if (std::chrono::steady_clock::now() >= deadline)
                        {
                            timedOut.store(
                                true,
                                std::memory_order_release
                            );

                            break;
                        }


                        std::this_thread::yield();
                    }
                }
            }
        );
    }


    for (int consumer = 0; consumer < consumerCount; ++consumer)
    {
        consumers.emplace_back(
            [&]()
            {
                ready.fetch_add(
                    1,
                    std::memory_order_release
                );


                while (!start.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }


                while (
                    consumed.load(std::memory_order_acquire)
                    <
                    totalTaskCount
                    &&
                    !timedOut.load(std::memory_order_acquire)
                )
                {
                    std::optional<Task> task =
                        queue.pop();


                    if (task.has_value())
                    {
                        task->execute();

                        consumed.fetch_add(
                            1,
                            std::memory_order_release
                        );

                        continue;
                    }


                    if (std::chrono::steady_clock::now() >= deadline)
                    {
                        timedOut.store(
                            true,
                            std::memory_order_release
                        );

                        break;
                    }


                    std::this_thread::yield();
                }
            }
        );
    }


    while (
        ready.load(std::memory_order_acquire)
        !=
        producerCount + consumerCount
    )
    {
        std::this_thread::yield();
    }

    start.store(
        true,
        std::memory_order_release
    );


    for (std::thread& producer : producers)
    {
        producer.join();
    }

    for (std::thread& consumer : consumers)
    {
        consumer.join();
    }


    assert(!timedOut.load(std::memory_order_acquire));
    assert(pushed.load(std::memory_order_relaxed) == totalTaskCount);
    assert(consumed.load(std::memory_order_relaxed) == totalTaskCount);

    for (const std::atomic<int>& count : seen)
    {
        assert(count.load(std::memory_order_relaxed) == 1);
    }


    assert(queue.empty());
    assert(!queue.pop().has_value());
}

} // namespace


int main()
{
    testCapacityContract();
    testBoundedFifoAndSlotReuse();
    testConcurrentProducersDoNotReportFalseFull();
    testConcurrentProducersAndConsumersPreserveEveryTask();


    std::cout
        << "LockFreeQueue correctness OK\n";


    return 0;
}
