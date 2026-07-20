#include "scheduler/LockFreeQueue.hpp"
#include "scheduler/Scheduler.hpp"

#include <atomic>
#include <cassert>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>


namespace
{

using pi::scheduler::LockFreeQueue;
using pi::scheduler::Scheduler;
using pi::scheduler::Task;
using pi::scheduler::TaskHandle;

constexpr int taskCount = 10000;


void testSubmittedLoadCompletesWithHandles()
{
    std::atomic<int> counter{
        0
    };


    Scheduler scheduler(
        std::make_unique<LockFreeQueue>(16384),
        4
    );


    scheduler.start();


    std::vector<TaskHandle> handles;

    handles.reserve(taskCount);


    for (int i = 0; i < taskCount; ++i)
    {
        TaskHandle submitted =
            scheduler.submit(
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

        assert(handle.isCompleted());
    }


    scheduler.stop();


    assert(
        counter.load()
        ==
        taskCount
    );
}

} // namespace


int main()
{
    testSubmittedLoadCompletesWithHandles();


    std::cout
        << "Scheduler load handles OK\n";


    return 0;
}
