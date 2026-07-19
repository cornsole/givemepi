#include "scheduler/LockFreeQueue.hpp"
#include "scheduler/ThreadPool.hpp"

#include <atomic>
#include <cassert>
#include <iostream>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>


static_assert(
    std::is_same_v<
        decltype(
            std::declval<pi::scheduler::ThreadPool&>().submit(
                std::declval<pi::scheduler::Task>()
            )
        ),
        pi::scheduler::TaskHandle
    >
);


namespace
{

using pi::scheduler::LockFreeQueue;
using pi::scheduler::Task;
using pi::scheduler::TaskHandle;
using pi::scheduler::ThreadPool;

constexpr int taskCount = 100;


void testAcceptedTasksCanBeJoined()
{
    std::atomic<int> counter{
        0
    };

    ThreadPool pool(
        std::make_unique<LockFreeQueue>(128),
        4
    );

    pool.start();


    std::vector<TaskHandle> handles;

    handles.reserve(taskCount);


    for (int i = 0; i < taskCount; ++i)
    {
        TaskHandle submitted =
            pool.submit(
                Task(
                    [&counter]()
                    {
                        counter.fetch_add(
                            1,
                            std::memory_order_relaxed
                        );
                    }
                )
            );

        assert(submitted.valid());

        handles.push_back(
            std::move(submitted)
        );
    }


    for (auto& handle : handles)
    {
        handle.wait();

        assert(handle.isReady());
        assert(handle.isCompleted());
        assert(!handle.isFailed());
    }

    pool.stop();


    assert(counter.load(std::memory_order_relaxed) == taskCount);
}


void testRejectedSubmissionsReturnInvalidHandles()
{
    ThreadPool pool(
        std::make_unique<LockFreeQueue>(16),
        2
    );


    TaskHandle beforeStart =
        pool.submit(
            Task(
                []()
                {
                }
            )
        );

    assert(!beforeStart.valid());


    pool.start();

    TaskHandle invalidTask =
        pool.submit(
            Task{}
        );

    assert(!invalidTask.valid());

    pool.stop();


    TaskHandle afterStop =
        pool.submit(
            Task(
                []()
                {
                }
            )
        );

    assert(!afterStop.valid());


    ThreadPool noWorkers(
        std::make_unique<LockFreeQueue>(16),
        0
    );

    noWorkers.start();

    TaskHandle noWorkerHandle =
        noWorkers.submit(
            Task(
                []()
                {
                }
            )
        );

    assert(!noWorkerHandle.valid());

    noWorkers.stop();
}

} // namespace


int main()
{
    testAcceptedTasksCanBeJoined();
    testRejectedSubmissionsReturnInvalidHandles();


    std::cout
        << "ThreadPool submission OK\n";


    return 0;
}
