#include "scheduler/LockFreeQueue.hpp"

#include <utility>

namespace pi::scheduler
{

bool LockFreeQueue::push(
    Task task
)
{
    std::size_t position =
        enqueuePos_.load(
            std::memory_order_relaxed
        );


    Cell* target = nullptr;


    while (true)
    {
        Cell& cell =
            buffer_[
                position % capacity_
            ];


        std::size_t sequence =
            cell.sequence.load(
                std::memory_order_acquire
            );


        if (sequence == position)
        {
            if (
                enqueuePos_.compare_exchange_weak(
                    position,
                    position + 1,
                    std::memory_order_relaxed
                )
            )
            {
                target = &cell;
                break;
            }

            continue;
        }


        return false;
    }


    target->data =
        std::move(task);


    target->sequence.store(
        position + 1,
        std::memory_order_release
    );


    return true;
}


std::optional<Task> LockFreeQueue::pop()
{
    std::size_t position =
        dequeuePos_.load(
            std::memory_order_relaxed
        );


    Cell* target = nullptr;


    while (true)
    {
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


        if (sequence == expected)
        {
            if (
                dequeuePos_.compare_exchange_weak(
                    position,
                    position + 1,
                    std::memory_order_relaxed
                )
            )
            {
                target = &cell;
                break;
            }

            continue;
        }


        return std::nullopt;
    }


    Task task =
        std::move(
            target->data
        );


    target->sequence.store(
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

} // namespace pi::scheduler청소 안한지 9년된 커피 머신 열었다가 기절할 뻔했습니다 (시청 주의)
