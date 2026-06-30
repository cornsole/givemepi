#pragma once

#include "scheduler/Task.hpp"
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


    void start();


    void stop();


    bool submit(
        Task task
    );


    [[nodiscard]]
    std::size_t workerCount() const noexcept;


    [[nodiscard]]
    bool running() const noexcept;


private:

    std::unique_ptr<ThreadPool> pool_;

};


} // namespace pi::scheduler