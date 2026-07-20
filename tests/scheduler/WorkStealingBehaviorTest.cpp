#include "scheduler/IQueue.hpp"
#include "scheduler/Scheduler.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <thread>
#include <utility>
#include <vector>


namespace
{

using pi::scheduler::IQueue;
using pi::scheduler::Scheduler;
using pi::scheduler::Task;
using pi::scheduler::TaskHandle;


class RootOnlyQueue final : public IQueue
{
public:

    bool push(
        Task task
    ) override
    {
        pushAttempts_.fetch_add(
            1,
            std::memory_order_relaxed
        );


        std::lock_guard<std::mutex> lock(
            mutex_
        );


        if (rootAccepted_)
        {
            return false;
        }


        rootAccepted_ = true;

        pending_.emplace(
            std::move(task)
        );


        return true;
    }


    std::optional<Task> pop() override
    {
        std::lock_guard<std::mutex> lock(
            mutex_
        );


        if (!pending_.has_value())
        {
            return std::nullopt;
        }


        Task task =
            std::move(
                *pending_
            );

        pending_.reset();


        return task;
    }


    [[nodiscard]]
    bool empty() const noexcept override
    {
        std::lock_guard<std::mutex> lock(
            mutex_
        );

        return !pending_.has_value();
    }


    [[nodiscard]]
    std::size_t capacity() const noexcept override
    {
        return 1;
    }


    [[nodiscard]]
    std::size_t pushAttempts() const noexcept
    {
        return pushAttempts_.load(
            std::memory_order_relaxed
        );
    }


private:

    mutable std::mutex mutex_;

    bool rootAccepted_ = false;

    std::optional<Task> pending_;

    std::atomic<std::size_t> pushAttempts_{
        0
    };
};


void testWorkerLocalChildrenAreStolenByMultipleWorkers()
{
    constexpr std::size_t workerCount =
        4;

    constexpr std::size_t childTaskCount =
        64;

    auto rootQueue =
        std::make_unique<RootOnlyQueue>();

    RootOnlyQueue* queueObserver =
        rootQueue.get();

    Scheduler scheduler(
        std::move(rootQueue),
        workerCount
    );

    std::mutex stateMutex;
    std::condition_variable stateChanged;

    std::thread::id rootThread;
    std::set<std::thread::id> childThreads;

    bool childrenPublished = false;
    bool allChildrenAccepted = false;
    bool releaseChildren = false;
    bool releaseRoot = false;

    std::size_t completedChildren = 0;

    std::vector<TaskHandle> childHandles;

    std::vector<std::atomic<int>> childExecutions(
        childTaskCount
    );


    scheduler.start();


    TaskHandle root =
        scheduler.submit(
            Task(
                [&]()
                {
                    const std::thread::id executingThread =
                        std::this_thread::get_id();

                    std::vector<TaskHandle> localHandles;

                    localHandles.reserve(
                        childTaskCount
                    );

                    bool accepted = true;


                    for (
                        std::size_t child = 0;
                        child < childTaskCount;
                        ++child
                    )
                    {
                        TaskHandle handle =
                            scheduler.submit(
                                Task(
                                    [&, child]()
                                    {
                                        {
                                            std::unique_lock<std::mutex> lock(
                                                stateMutex
                                            );

                                            childThreads.insert(
                                                std::this_thread::get_id()
                                            );

                                            stateChanged.notify_all();

                                            stateChanged.wait(
                                                lock,
                                                [&releaseChildren]()
                                                {
                                                    return releaseChildren;
                                                }
                                            );
                                        }


                                        childExecutions[child].fetch_add(
                                            1,
                                            std::memory_order_relaxed
                                        );


                                        {
                                            std::lock_guard<std::mutex> lock(
                                                stateMutex
                                            );

                                            ++completedChildren;
                                        }

                                        stateChanged.notify_all();
                                    }
                                )
                            );


                        if (!handle.valid())
                        {
                            accepted = false;
                            continue;
                        }


                        localHandles.push_back(
                            std::move(handle)
                        );
                    }


                    std::unique_lock<std::mutex> lock(
                        stateMutex
                    );

                    rootThread =
                        executingThread;

                    allChildrenAccepted =
                        accepted
                        &&
                        localHandles.size() == childTaskCount;

                    childHandles =
                        std::move(localHandles);

                    childrenPublished = true;

                    stateChanged.notify_all();

                    stateChanged.wait(
                        lock,
                        [&releaseRoot]()
                        {
                            return releaseRoot;
                        }
                    );
                }
            )
        );


    assert(root.valid());


    {
        std::unique_lock<std::mutex> lock(
            stateMutex
        );

        const bool published =
            stateChanged.wait_for(
                lock,
                std::chrono::seconds(
                    5
                ),
                [&childrenPublished]()
                {
                    return childrenPublished;
                }
            );

        assert(published);
        assert(allChildrenAccepted);
        assert(childHandles.size() == childTaskCount);


        const bool multipleThievesEntered =
            stateChanged.wait_for(
                lock,
                std::chrono::seconds(
                    5
                ),
                [&childThreads]()
                {
                    return childThreads.size() >= 2;
                }
            );

        assert(multipleThievesEntered);
        assert(childThreads.find(rootThread) == childThreads.end());


        releaseChildren = true;
    }

    stateChanged.notify_all();


    {
        std::unique_lock<std::mutex> lock(
            stateMutex
        );

        const bool allChildrenCompleted =
            stateChanged.wait_for(
                lock,
                std::chrono::seconds(
                    5
                ),
                [&completedChildren]()
                {
                    return completedChildren == childTaskCount;
                }
            );

        assert(allChildrenCompleted);
        assert(childThreads.size() >= 2);
        assert(childThreads.find(rootThread) == childThreads.end());

        releaseRoot = true;
    }

    stateChanged.notify_all();


    root.wait();

    assert(root.isCompleted());


    for (TaskHandle& child : childHandles)
    {
        child.wait();

        assert(child.isCompleted());
    }


    scheduler.stop();


    assert(queueObserver->pushAttempts() == 1);

    for (const std::atomic<int>& executions : childExecutions)
    {
        assert(executions.load(std::memory_order_relaxed) == 1);
    }
}

} // namespace


int main()
{
    testWorkerLocalChildrenAreStolenByMultipleWorkers();


    std::cout
        << "Observable work stealing OK\n";


    return 0;
}
