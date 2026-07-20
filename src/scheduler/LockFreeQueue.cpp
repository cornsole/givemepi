#include "scheduler/LockFreeQueue.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace pi::scheduler
{

namespace
{

enum class SequenceOrder
{
    Behind,
    Equal,
    Ahead
};


SequenceOrder compareSequence(
    std::size_t sequence,
    std::size_t expected
) noexcept
{
    if (sequence == expected)
    {
        return SequenceOrder::Equal;
    }


    constexpr std::size_t maximumDifference =
        static_cast<std::size_t>(
            std::numeric_limits<std::ptrdiff_t>::max()
        );

    const std::size_t forward =
        sequence - expected;


    if (forward <= maximumDifference)
    {
        return SequenceOrder::Ahead;
    }


    return SequenceOrder::Behind;
}

} // namespace


LockFreeQueue::LockFreeQueue(
    std::size_t capacity
)
    : capacity_(capacity)
{
    constexpr std::size_t maximumCapacity =
        static_cast<std::size_t>(
            std::numeric_limits<std::ptrdiff_t>::max()
        );


    if (capacity_ < 2 || capacity_ > maximumCapacity)
    {
        throw std::invalid_argument(
            "LockFreeQueue capacity must be between 2 and PTRDIFF_MAX."
        );
    }


    buffer_ =
        std::make_unique<Cell[]>(
            capacity_
        );


    for (std::size_t i = 0; i < capacity_; ++i)
    {
        buffer_[i].sequence.store(
            i,
            std::memory_order_relaxed
        );
    }
}


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


        const SequenceOrder order =
            compareSequence(
                sequence,
                position
            );


        if (order == SequenceOrder::Equal)
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


        if (order == SequenceOrder::Behind)
        {
            return false;
        }


        position =
            enqueuePos_.load(
                std::memory_order_relaxed
            );
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


        const SequenceOrder order =
            compareSequence(
                sequence,
                expected
            );


        if (order == SequenceOrder::Equal)
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


        if (order == SequenceOrder::Behind)
        {
            return std::nullopt;
        }


        position =
            dequeuePos_.load(
                std::memory_order_relaxed
            );
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
    std::size_t position =
        dequeuePos_.load(
            std::memory_order_relaxed
        );


    while (true)
    {
        const Cell& cell =
            buffer_[
                position % capacity_
            ];


        const std::size_t sequence =
            cell.sequence.load(
                std::memory_order_acquire
            );


        const SequenceOrder order =
            compareSequence(
                sequence,
                position + 1
            );


        if (order == SequenceOrder::Equal)
        {
            return false;
        }


        if (order == SequenceOrder::Behind)
        {
            return true;
        }


        position =
            dequeuePos_.load(
                std::memory_order_relaxed
            );
    }
}


std::size_t LockFreeQueue::capacity() const noexcept
{
    return capacity_;
}

} // namespace pi::scheduler
