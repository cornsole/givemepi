#pragma once

#include "scheduler/IQueue.hpp"
#include "scheduler/Task.hpp"
#include "scheduler/TaskHandle.hpp"
#include "scheduler/Worker.hpp"

#include <atomic>
#include <cstddef>
#include <memory>
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


    void start();


    void stop();


    /// Submit one task and return a handle when the task is accepted.
    /// Returns an invalid handle when the pool cannot accept the task.
    [[nodiscard]]
    TaskHandle submit(
        Task task
    );


    [[nodiscard]]
    std::size_t workerCount() const noexcept;


    [[nodiscard]]
    bool running() const noexcept;


    Worker* workerAt(
        std::size_t index
    );


private:

    std::unique_ptr<IQueue> queue_;


    std::vector<
        std::unique_ptr<Worker>
    > workers_;


    std::atomic<std::size_t> nextWorker_{
        0
    };


    std::atomic<bool> running_{
        false
    };

};

} // namespace pi::scheduler
