#pragma once

#include "scheduler/IQueue.hpp"

#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>


namespace pi::scheduler
{

class LockFreeQueue final : public IQueue
{
public:

    explicit LockFreeQueue(
        std::size_t capacity
    )
        : capacity_(capacity),
          buffer_(std::make_unique<Cell[]>(capacity))
    {
        for (std::size_t i = 0; i < capacity_; ++i)
        {
            buffer_[i].sequence.store(
                i,
                std::memory_order_relaxed
            );
        }
    }


    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;


    bool push(
        Task task
    ) override;


    std::optional<Task> pop() override;


    [[nodiscard]]
    bool empty() const noexcept override;


    [[nodiscard]]
    std::size_t capacity() const noexcept override;



private:

    struct Cell
    {
        std::atomic<std::size_t> sequence{0};

        Task data{};
    };


    std::size_t capacity_;

    std::unique_ptr<Cell[]> buffer_;


    alignas(64)
    std::atomic<std::size_t> enqueuePos_{0};


    alignas(64)
    std::atomic<std::size_t> dequeuePos_{0};

};


} // namespace pi::scheduler