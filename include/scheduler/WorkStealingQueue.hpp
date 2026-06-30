#pragma once

#include "scheduler/Task.hpp"

#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>


namespace pi::scheduler
{

class WorkStealingQueue
{
public:

    WorkStealingQueue() = default;


    ~WorkStealingQueue() = default;


    WorkStealingQueue(
        const WorkStealingQueue&
    ) = delete;


    WorkStealingQueue& operator=(
        const WorkStealingQueue&
    ) = delete;


    WorkStealingQueue(
        WorkStealingQueue&&
    ) = delete;


    WorkStealingQueue& operator=(
        WorkStealingQueue&&
    ) = delete;


    void push(
        Task task
    );


    std::optional<Task> pop();


    std::optional<Task> steal();


    [[nodiscard]]
    bool empty() const noexcept;


private:

    mutable std::mutex mutex_;


    std::deque<Task> queue_;

};


} // namespace pi::scheduler