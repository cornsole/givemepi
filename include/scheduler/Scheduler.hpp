#pragma once

#include "scheduler/SchedulerState.hpp"
#include "scheduler/Task.hpp"
#include "scheduler/TaskHandle.hpp"
#include "scheduler/ThreadPool.hpp"

#include <cstddef>
#include <memory>


namespace pi::scheduler
{

class Scheduler
{
public:

    Scheduler(
        std::size_t workerCount,
        std::size_t queueCapacity
    );

    Scheduler(
        std::unique_ptr<IQueue> queue,
        std::size_t workerCount
    );


    ~Scheduler();


    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;


    Scheduler(Scheduler&&) = delete;
    Scheduler& operator=(Scheduler&&) = delete;


    /// Start scheduler workers or restart after a completed stop.
    void start();


    /// Reject new work and drain every accepted task before returning.
    void stop();


    /// Submit one task according to the scheduler's global/local routing.
    /// Returns an invalid handle when the scheduler cannot accept the task.
    [[nodiscard]]
    TaskHandle submit(
        Task task
    );


    [[nodiscard]]
    std::size_t workerCount() const noexcept;


    [[nodiscard]]
    bool running() const noexcept;


    /// Return the current scheduler lifecycle state.
    [[nodiscard]]
    SchedulerState state() const noexcept;


private:

    std::unique_ptr<ThreadPool> pool_;

};


} // namespace pi::scheduler
