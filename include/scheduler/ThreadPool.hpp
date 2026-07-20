#pragma once

#include "scheduler/IQueue.hpp"
#include "scheduler/SchedulerState.hpp"
#include "scheduler/Task.hpp"
#include "scheduler/TaskHandle.hpp"
#include "scheduler/Worker.hpp"

#include <atomic>
#include <cstddef>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>


namespace pi::scheduler
{

class ThreadPool
{
public:

    ThreadPool(
        std::unique_ptr<IQueue> queue,
        std::size_t workerCount
    );


    ~ThreadPool();


    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;


    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;


    /// Start workers, waiting for an overlapping stop to finish if necessary.
    void start();


    /// Reject new work, drain every accepted task, and join all workers.
    void stop();


    /// Submit external work globally or current-worker work locally.
    /// Returns an invalid handle when the pool or global queue rejects it.
    [[nodiscard]]
    TaskHandle submit(
        Task task
    );


    [[nodiscard]]
    std::size_t workerCount() const noexcept;


    [[nodiscard]]
    bool running() const noexcept;


    /// Check whether the current thread is a worker owned by this pool.
    [[nodiscard]]
    bool ownsCurrentWorker() const noexcept;


    /// Return the current scheduler lifecycle state.
    [[nodiscard]]
    SchedulerState state() const noexcept;


    Worker* workerAt(
        std::size_t index
    );


private:

    std::unique_ptr<IQueue> queue_;


    std::vector<
        std::unique_ptr<Worker>
    > workers_;


    std::atomic<std::size_t> outstandingTasks_{
        0
    };


    std::atomic<SchedulerState> state_{
        SchedulerState::Stopped
    };


    mutable std::mutex lifecycleMutex_;


    std::condition_variable lifecycleCondition_;


    std::size_t stopGeneration_ = 0;


    void taskCompleted() noexcept;


    [[nodiscard]]
    bool drainComplete() const noexcept;


    friend class Worker;

};

} // namespace pi::scheduler
