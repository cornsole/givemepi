#include "scheduler/LockFreeQueue.hpp"
#include "scheduler/ThreadPool.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
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

static_assert(
    std::is_same_v<
        decltype(
            std::declval<const pi::scheduler::ThreadPool&>().state()
        ),
        pi::scheduler::SchedulerState
    >
);


namespace
{

using pi::scheduler::LockFreeQueue;
using pi::scheduler::SchedulerState;
using pi::scheduler::Task;
using pi::scheduler::TaskHandle;
using pi::scheduler::ThreadPool;

constexpr int taskCount = 100;


void testStopDrainsEveryAcceptedTask()
{
    constexpr int drainTaskCount = 256;

    std::atomic<int> completedCount{
        0
    };

    ThreadPool pool(
        std::make_unique<LockFreeQueue>(512),
        4
    );

    pool.start();


    std::vector<TaskHandle> handles;

    handles.reserve(drainTaskCount);


    for (int i = 0; i < drainTaskCount; ++i)
    {
        TaskHandle submitted =
            pool.submit(
                Task(
                    [i, &completedCount]()
                    {
                        if (i == drainTaskCount - 1)
                        {
                            throw std::runtime_error(
                                "expected drain failure"
                            );
                        }

                        completedCount.fetch_add(
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


    pool.stop();


    assert(pool.state() == SchedulerState::Stopped);
    assert(!pool.running());
    assert(
        completedCount.load(
            std::memory_order_relaxed
        ) == drainTaskCount - 1
    );


    for (std::size_t i = 0; i < handles.size(); ++i)
    {
        assert(handles[i].isReady());

        if (i == handles.size() - 1)
        {
            assert(handles[i].isFailed());
        }
        else
        {
            assert(handles[i].isCompleted());
        }
    }
}


void testGlobalCapacityAndWorkerLocalRouting()
{
    constexpr int childTaskCount = 8;

    std::mutex gateMutex;
    std::condition_variable rootEntered;
    std::condition_variable rootRelease;

    bool entered = false;
    bool released = false;

    std::atomic<int> globalCount{
        0
    };

    std::atomic<int> childCount{
        0
    };

    ThreadPool pool(
        std::make_unique<LockFreeQueue>(2),
        1
    );

    std::vector<TaskHandle> childHandles;

    childHandles.reserve(childTaskCount);

    pool.start();


    TaskHandle root =
        pool.submit(
            Task(
                [&]()
                {
                    {
                        std::unique_lock<std::mutex> lock(
                            gateMutex
                        );

                        entered = true;
                        rootEntered.notify_one();

                        rootRelease.wait(
                            lock,
                            [&released]()
                            {
                                return released;
                            }
                        );
                    }


                    for (int i = 0; i < childTaskCount; ++i)
                    {
                        TaskHandle child =
                            pool.submit(
                                Task(
                                    [&childCount]()
                                    {
                                        childCount.fetch_add(
                                            1,
                                            std::memory_order_relaxed
                                        );
                                    }
                                )
                            );

                        assert(child.valid());

                        childHandles.push_back(
                            std::move(child)
                        );
                    }
                }
            )
        );

    assert(root.valid());


    {
        std::unique_lock<std::mutex> lock(
            gateMutex
        );

        rootEntered.wait(
            lock,
            [&entered]()
            {
                return entered;
            }
        );
    }


    TaskHandle globalOne =
        pool.submit(
            Task(
                [&globalCount]()
                {
                    globalCount.fetch_add(
                        1,
                        std::memory_order_relaxed
                    );
                }
            )
        );

    TaskHandle globalTwo =
        pool.submit(
            Task(
                [&globalCount]()
                {
                    globalCount.fetch_add(
                        1,
                        std::memory_order_relaxed
                    );
                }
            )
        );

    TaskHandle globalOverflow =
        pool.submit(
            Task(
                []()
                {
                }
            )
        );


    assert(globalOne.valid());
    assert(globalTwo.valid());
    assert(!globalOverflow.valid());


    {
        std::lock_guard<std::mutex> lock(
            gateMutex
        );

        released = true;
    }

    rootRelease.notify_one();

    root.wait();

    assert(root.isCompleted());
    assert(childHandles.size() == childTaskCount);


    pool.stop();


    assert(globalOne.isCompleted());
    assert(globalTwo.isCompleted());
    assert(globalCount.load(std::memory_order_relaxed) == 2);
    assert(
        childCount.load(
            std::memory_order_relaxed
        ) == childTaskCount
    );

    for (auto& child : childHandles)
    {
        assert(child.isCompleted());
    }
}


void testCrossPoolWorkerUsesTargetGlobalQueue()
{
    std::mutex gateMutex;
    std::condition_variable targetEntered;
    std::condition_variable targetRelease;

    bool entered = false;
    bool released = false;

    ThreadPool targetPool(
        std::make_unique<LockFreeQueue>(2),
        1
    );

    ThreadPool sourcePool(
        std::make_unique<LockFreeQueue>(4),
        1
    );

    targetPool.start();
    sourcePool.start();


    TaskHandle targetRoot =
        targetPool.submit(
            Task(
                [&]()
                {
                    std::unique_lock<std::mutex> lock(
                        gateMutex
                    );

                    entered = true;
                    targetEntered.notify_one();

                    targetRelease.wait(
                        lock,
                        [&released]()
                        {
                            return released;
                        }
                    );
                }
            )
        );

    assert(targetRoot.valid());


    {
        std::unique_lock<std::mutex> lock(
            gateMutex
        );

        targetEntered.wait(
            lock,
            [&entered]()
            {
                return entered;
            }
        );
    }


    TaskHandle targetGlobalOne =
        targetPool.submit(
            Task(
                []()
                {
                }
            )
        );

    TaskHandle targetGlobalTwo =
        targetPool.submit(
            Task(
                []()
                {
                }
            )
        );

    assert(targetGlobalOne.valid());
    assert(targetGlobalTwo.valid());


    std::atomic<bool> crossPoolAccepted{
        true
    };

    TaskHandle sourceTask =
        sourcePool.submit(
            Task(
                [&targetPool, &crossPoolAccepted]()
                {
                    TaskHandle crossPool =
                        targetPool.submit(
                            Task(
                                []()
                                {
                                }
                            )
                        );

                    crossPoolAccepted.store(
                        crossPool.valid(),
                        std::memory_order_release
                    );
                }
            )
        );

    assert(sourceTask.valid());

    sourceTask.wait();

    assert(sourceTask.isCompleted());
    assert(
        !crossPoolAccepted.load(
            std::memory_order_acquire
        )
    );


    sourcePool.stop();


    {
        std::lock_guard<std::mutex> lock(
            gateMutex
        );

        released = true;
    }

    targetRelease.notify_one();

    targetPool.stop();


    assert(targetRoot.isCompleted());
    assert(targetGlobalOne.isCompleted());
    assert(targetGlobalTwo.isCompleted());
}


bool waitForState(
    const ThreadPool& pool,
    SchedulerState expected
)
{
    const auto deadline =
        std::chrono::steady_clock::now()
        + std::chrono::seconds(2);


    while (pool.state() != expected)
    {
        if (std::chrono::steady_clock::now() >= deadline)
        {
            return false;
        }

        std::this_thread::yield();
    }


    return true;
}


void testLifecycleStateAndRestart()
{
    ThreadPool pool(
        std::make_unique<LockFreeQueue>(16),
        2
    );


    assert(pool.state() == SchedulerState::Stopped);
    assert(!pool.running());

    pool.stop();

    assert(pool.state() == SchedulerState::Stopped);


    pool.start();
    pool.start();

    assert(pool.state() == SchedulerState::Running);
    assert(pool.running());


    pool.stop();
    pool.stop();

    assert(pool.state() == SchedulerState::Stopped);
    assert(!pool.running());


    pool.start();

    TaskHandle restarted =
        pool.submit(
            Task(
                []()
                {
                }
            )
        );

    assert(restarted.valid());

    restarted.wait();

    assert(restarted.isCompleted());

    pool.stop();

    assert(pool.state() == SchedulerState::Stopped);
}


void testConcurrentStopRejectsSubmissionAndAllowsRestart()
{
    std::mutex gateMutex;
    std::condition_variable taskEntered;
    std::condition_variable taskRelease;

    bool entered = false;
    bool released = false;

    ThreadPool pool(
        std::make_unique<LockFreeQueue>(16),
        1
    );

    pool.start();


    TaskHandle blocking =
        pool.submit(
            Task(
                [&]()
                {
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
            )
        );

    assert(blocking.valid());


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


    std::thread stopper(
        [&pool]()
        {
            pool.stop();
        }
    );

    assert(
        waitForState(
            pool,
            SchedulerState::Stopping
        )
    );
    assert(!pool.running());


    TaskHandle duringStop =
        pool.submit(
            Task(
                []()
                {
                }
            )
        );

    assert(!duringStop.valid());


    std::thread restarter(
        [&pool]()
        {
            pool.start();
        }
    );


    {
        std::lock_guard<std::mutex> lock(
            gateMutex
        );

        released = true;
    }

    taskRelease.notify_one();

    stopper.join();
    restarter.join();

    blocking.wait();

    assert(blocking.isCompleted());
    assert(pool.state() == SchedulerState::Running);
    assert(pool.running());


    TaskHandle afterRestart =
        pool.submit(
            Task(
                []()
                {
                }
            )
        );

    assert(afterRestart.valid());

    afterRestart.wait();

    assert(afterRestart.isCompleted());

    pool.stop();
}


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
    testStopDrainsEveryAcceptedTask();
    testGlobalCapacityAndWorkerLocalRouting();
    testCrossPoolWorkerUsesTargetGlobalQueue();
    testLifecycleStateAndRestart();
    testConcurrentStopRejectsSubmissionAndAllowsRestart();
    testAcceptedTasksCanBeJoined();
    testRejectedSubmissionsReturnInvalidHandles();


    std::cout
        << "ThreadPool submission OK\n";


    return 0;
}
