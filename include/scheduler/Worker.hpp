#pragma once

#include "scheduler/LockFreeQueue.hpp"
#include "scheduler/Task.hpp"

#include <atomic>
#include <cstddef>
#include <thread>


namespace pi::scheduler
{

class Worker
{
public:

    Worker(
        std::size_t id,
        LockFreeQueue* queue
    );


    ~Worker();


    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;


    Worker(Worker&&) = delete;
    Worker& operator=(Worker&&) = delete;


    void start();


    void stop() noexcept;


    [[nodiscard]]
    bool running() const noexcept;


    [[nodiscard]]
    std::size_t id() const noexcept;


private:

    void run();


private:

    std::size_t id_ = 0;


    LockFreeQueue* queue_ = nullptr;


    std::thread thread_;


    std::atomic<bool> running_{
        false
    };


    std::atomic<bool> stopRequested_{
        false
    };

};


} // namespace pi::scheduler