#include "scheduler/WorkStealingQueue.hpp"


namespace pi::scheduler
{

void WorkStealingQueue::push(
    Task task
)
{
    std::lock_guard<std::mutex> lock(
        mutex_
    );


    queue_.push_back(
        std::move(task)
    );
}


std::optional<Task>
WorkStealingQueue::pop()
{
    std::lock_guard<std::mutex> lock(
        mutex_
    );


    if (queue_.empty())
    {
        return std::nullopt;
    }


    Task task =
        std::move(
            queue_.back()
        );


    queue_.pop_back();


    return task;
}


std::optional<Task>
WorkStealingQueue::steal()
{
    std::lock_guard<std::mutex> lock(
        mutex_
    );


    if (queue_.empty())
    {
        return std::nullopt;
    }


    Task task =
        std::move(
            queue_.front()
        );


    queue_.pop_front();


    return task;
}


bool WorkStealingQueue::empty() const noexcept
{
    std::lock_guard<std::mutex> lock(
        mutex_
    );


    return queue_.empty();
}


} // namespace pi::scheduler