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

class Worker
{
public:

    Worker(
        std::size_t id,
        IQueue* queue,
        std::vector<std::unique_ptr<Worker>>* workers
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

    void push(
        Task task
    );


    std::optional<Task> steal();


private:

    void run();


    std::optional<Task> trySteal();

private:

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

};


} // namespace pi::scheduler