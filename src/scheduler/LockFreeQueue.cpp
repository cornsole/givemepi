#include "scheduler/LockFreeQueue.hpp"

#include <utility>

namespace pi::scheduler
{

bool LockFreeQueue::push(
    Task task
)
{
    std::size_t position =
        enqueuePos_.fetch_add(
            1,
            std::memory_order_relaxed
        );

    Cell& cell =
        buffer_[
            position % capacity_
        ];

    std::size_t sequence =
        cell.sequence.load(
            std::memory_order_acquire
        );

    std::size_t expected =
        position;

    if (sequence != expected)
    {
        return false;
    }

    cell.data =
        std::move(task);

    cell.sequence.store(
        position + 1,
        std::memory_order_release
    );

    return true;
}


std::optional<Task> LockFreeQueue::pop()
{
    std::size_t position =
        dequeuePos_.fetch_add(
            1,
            std::memory_order_relaxed
        );

    Cell& cell =
        buffer_[
            position % capacity_
        ];

    std::size_t sequence =
        cell.sequence.load(
            std::memory_order_acquire
        );

    std::size_t expected =
        position + 1;

    if (sequence != expected)
    {
        return std::nullopt;
    }

    Task task =
        std::move(
            cell.data
        );

    cell.sequence.store(
        position + capacity_,
        std::memory_order_release
    );

    return task;
}


bool LockFreeQueue::empty() const noexcept
{
    return enqueuePos_.load(
               std::memory_order_relaxed
           )
        ==
           dequeuePos_.load(
               std::memory_order_relaxed
           );
}


std::size_t LockFreeQueue::capacity() const noexcept
{
    return capacity_;
}

} // namespace pi::scheduler