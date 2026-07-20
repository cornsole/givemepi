#pragma once

#include "scheduler/IQueue.hpp"
#include "scheduler/Task.hpp"
#include "scheduler/WorkStealingQueue.hpp"

#include <atomic>
#include <cstddef>
#include <thread>
#include <optional>
#include <vector>


namespace pi::scheduler
{

class ThreadPool;


class Worker
{
public:

    Worker(
        ThreadPool* owner,
        std::size_t id,
        IQueue* queue,
        std::vector<std::unique_ptr<Worker>>* workers
    );


    ~Worker();


    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;


    Worker(Worker&&) = delete;
    Worker& operator=(Worker&&) = delete;


    /// Start this worker's execution loop.
    void start();


    /// Request stop and join after all pool-accepted work drains.
    void stop() noexcept;


    [[nodiscard]]
    bool running() const noexcept;


    [[nodiscard]]
    std::size_t id() const noexcept;

    /// Push worker-created work to this worker's local queue.
    void push(
        Task task
    );


    /// Steal one task from another worker's local queue.
    std::optional<Task> steal();


private:

    void requestStop() noexcept;


    void join() noexcept;


    void run();


    void executeTask(
        Task& task
    );

private:

    ThreadPool* owner_ = nullptr;


    std::size_t id_ = 0;


    IQueue* globalQueue_ = nullptr;


    std::vector<std::unique_ptr<Worker>>* workers_ = nullptr;


    WorkStealingQueue localQueue_;


    std::thread thread_;


    std::atomic<bool> running_{
        false
    };


    std::atomic<bool> stopRequested_{
        false
    };


    static thread_local Worker* currentWorker_;


    friend class ThreadPool;

};


} // namespace pi::scheduler
