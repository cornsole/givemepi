#include "scheduler/ReferenceQueue.hpp"

namespace pi::scheduler
{

ReferenceQueue::ReferenceQueue(
    std::size_t capacity
)
    : capacity_(capacity)
{
}


bool ReferenceQueue::push(
    Task task
)
{
    std::lock_guard<std::mutex> lock(
        mutex_
    );


    if (queue_.size() >= capacity_)
    {
        return false;
    }


    queue_.push(
        std::move(task)
    );


    return true;
}


std::optional<Task> ReferenceQueue::pop()
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


    queue_.pop();


    return task;
}


bool ReferenceQueue::empty() const noexcept
{
    std::lock_guard<std::mutex> lock(
        mutex_
    );


    return queue_.empty();
}


std::size_t ReferenceQueue::capacity() const noexcept
{
    return capacity_;
}


} // namespace pi::scheduler