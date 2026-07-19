#include "scheduler/Task.hpp"

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>


namespace
{

using pi::scheduler::Task;
using pi::scheduler::TaskHandle;
using pi::scheduler::TaskState;


void testSuccessfulLifecycleAndWaiter()
{
    std::mutex gateMutex;
    std::condition_variable taskEntered;
    std::condition_variable taskRelease;

    bool entered = false;
    bool released = false;

    std::atomic<int> executionCount{
        0
    };


    Task task(
        [&]()
        {
            executionCount.fetch_add(
                1,
                std::memory_order_relaxed
            );


            std::unique_lock<std::mutex> lock(
                gateMutex
            );

            entered = true;
            taskEntered.notify_one();

            taskRelease.wait(
                lock,
                [&released]()
                {
                    return released;
                }
            );
        }
    );


    TaskHandle handle =
        task.handle();


    assert(task.valid());
    assert(handle.valid());
    assert(task.state() == TaskState::Pending);
    assert(!handle.isReady());


    std::thread executor(
        [&task]()
        {
            task.execute();
        }
    );


    {
        std::unique_lock<std::mutex> lock(
            gateMutex
        );

        taskEntered.wait(
            lock,
            [&entered]()
            {
                return entered;
            }
        );
    }


    assert(task.state() == TaskState::Running);
    assert(!handle.isReady());


    std::atomic<bool> waiterReturned{
        false
    };

    std::thread waiter(
        [&handle, &waiterReturned]()
        {
            handle.wait();

            waiterReturned.store(
                true,
                std::memory_order_release
            );
        }
    );


    assert(!waiterReturned.load(std::memory_order_acquire));


    {
        std::lock_guard<std::mutex> lock(
            gateMutex
        );

        released = true;
    }

    taskRelease.notify_one();

    executor.join();
    waiter.join();


    assert(task.state() == TaskState::Completed);
    assert(handle.isReady());
    assert(handle.isCompleted());
    assert(!handle.isFailed());
    assert(!handle.exception());
    assert(waiterReturned.load(std::memory_order_acquire));
    assert(executionCount.load(std::memory_order_relaxed) == 1);


    task.execute();

    assert(task.state() == TaskState::Completed);
    assert(executionCount.load(std::memory_order_relaxed) == 1);
}


void testFailedLifecycleAndExceptionCapture()
{
    std::atomic<int> executionCount{
        0
    };

    Task task(
        [&executionCount]()
        {
            executionCount.fetch_add(
                1,
                std::memory_order_relaxed
            );

            throw std::runtime_error(
                "expected direct task failure"
            );
        }
    );

    TaskHandle handle =
        task.handle();


    bool escaped = false;

    try
    {
        task.execute();
    }
    catch (...)
    {
        escaped = true;
    }


    assert(!escaped);
    assert(task.state() == TaskState::Failed);
    assert(handle.isReady());
    assert(handle.isFailed());
    assert(!handle.isCompleted());
    assert(handle.exception());
    assert(executionCount.load(std::memory_order_relaxed) == 1);


    bool failureRethrown = false;

    try
    {
        handle.rethrowIfFailed();
    }
    catch (const std::runtime_error& error)
    {
        failureRethrown =
            std::string(error.what())
            ==
            "expected direct task failure";
    }


    assert(failureRethrown);


    task.execute();

    assert(task.state() == TaskState::Failed);
    assert(executionCount.load(std::memory_order_relaxed) == 1);
}

} // namespace


int main()
{
    testSuccessfulLifecycleAndWaiter();
    testFailedLifecycleAndExceptionCapture();


    std::cout
        << "Task lifecycle OK\n";


    return 0;
}
