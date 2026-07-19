#include "scheduler/LockFreeQueue.hpp"
#include "scheduler/Scheduler.hpp"

#include <atomic>
#include <cassert>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>


static_assert(
    !std::is_copy_constructible_v<pi::scheduler::TaskHandle>
);

static_assert(
    !std::is_copy_assignable_v<pi::scheduler::TaskHandle>
);

static_assert(
    std::is_nothrow_move_constructible_v<pi::scheduler::TaskHandle>
);

static_assert(
    std::is_nothrow_move_assignable_v<pi::scheduler::TaskHandle>
);

static_assert(
    std::is_same_v<
        decltype(
            std::declval<pi::scheduler::Scheduler&>().submit(
                std::declval<pi::scheduler::Task>()
            )
        ),
        pi::scheduler::TaskHandle
    >
);


namespace
{

using pi::scheduler::LockFreeQueue;
using pi::scheduler::Scheduler;
using pi::scheduler::Task;
using pi::scheduler::TaskHandle;

constexpr int taskCount = 100;


void waitForSuccessfulTasks(
    std::vector<TaskHandle>& handles
)
{
    for (auto& handle : handles)
    {
        handle.wait();

        assert(handle.isReady());
        assert(handle.isCompleted());
        assert(!handle.isFailed());
    }
}


void testAcceptedTasksCanBeJoined()
{
    std::atomic<int> counter{
        0
    };

    Scheduler scheduler(
        4,
        128
    );

    scheduler.start();


    TaskHandle moveSource =
        scheduler.submit(
            Task(
                []()
                {
                }
            )
        );

    assert(moveSource.valid());


    TaskHandle moveTarget =
        std::move(moveSource);

    assert(!moveSource.valid());
    assert(moveTarget.valid());

    moveTarget.wait();

    assert(moveTarget.isCompleted());
    assert(!moveTarget.exception());


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


    waitForSuccessfulTasks(
        handles
    );

    scheduler.stop();


    assert(counter.load(std::memory_order_relaxed) == taskCount);
}


void testFailedTaskDoesNotStopWorker()
{
    Scheduler scheduler(
        2,
        16
    );

    scheduler.start();


    TaskHandle failed =
        scheduler.submit(
            Task(
                []()
                {
                    throw std::runtime_error(
                        "expected task failure"
                    );
                }
            )
        );

    assert(failed.valid());

    failed.wait();

    assert(failed.isReady());
    assert(failed.isFailed());
    assert(!failed.isCompleted());
    assert(failed.exception());


    bool failureRethrown = false;

    try
    {
        failed.rethrowIfFailed();
    }
    catch (const std::runtime_error& error)
    {
        failureRethrown =
            std::string(error.what())
            ==
            "expected task failure";
    }

    assert(failureRethrown);


    std::atomic<bool> workerSurvived{
        false
    };

    TaskHandle recovery =
        scheduler.submit(
            Task(
                [&workerSurvived]()
                {
                    workerSurvived.store(
                        true,
                        std::memory_order_relaxed
                    );
                }
            )
        );

    assert(recovery.valid());

    recovery.wait();

    assert(recovery.isCompleted());
    assert(workerSurvived.load(std::memory_order_relaxed));


    scheduler.stop();
}


void testRejectedSubmissionsReturnInvalidHandles()
{
    Scheduler scheduler(
        2,
        16
    );


    TaskHandle beforeStart =
        scheduler.submit(
            Task(
                []()
                {
                }
            )
        );

    assert(!beforeStart.valid());


    scheduler.start();

    TaskHandle invalidTask =
        scheduler.submit(
            Task{}
        );

    assert(!invalidTask.valid());

    scheduler.stop();


    TaskHandle afterStop =
        scheduler.submit(
            Task(
                []()
                {
                }
            )
        );

    assert(!afterStop.valid());


    Scheduler noWorkers(
        0,
        16
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


void testCustomQueueTasksCanBeJoined()
{
    std::atomic<int> counter{
        0
    };

    Scheduler scheduler(
        std::make_unique<LockFreeQueue>(128),
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


    waitForSuccessfulTasks(
        handles
    );

    scheduler.stop();


    assert(counter.load(std::memory_order_relaxed) == taskCount);
}

} // namespace


int main()
{
    testAcceptedTasksCanBeJoined();
    testFailedTaskDoesNotStopWorker();
    testRejectedSubmissionsReturnInvalidHandles();
    testCustomQueueTasksCanBeJoined();


    std::cout
        << "Scheduler synchronization OK\n";


    return 0;
}
