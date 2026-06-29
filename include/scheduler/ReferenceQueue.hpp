#pragma once

#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>

#include "scheduler/IQueue.hpp"

namespace pi::scheduler
{

class ReferenceQueue final : public IQueue
{
public:

    explicit ReferenceQueue(
        std::size_t capacity
    );


    ~ReferenceQueue() override = default;


    ReferenceQueue(const ReferenceQueue&) = delete;
    ReferenceQueue& operator=(const ReferenceQueue&) = delete;


    bool push(
        Task task
    ) override;


    std::optional<Task> pop() override;


    [[nodiscard]]
    bool empty() const noexcept override;


    [[nodiscard]]
    std::size_t capacity() const noexcept override;


private:

    mutable std::mutex mutex_;

    std::queue<Task> queue_;

    std::size_t capacity_;

};

} // namespace pi::scheduler