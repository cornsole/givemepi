#pragma once

#include <optional>

#include "scheduler/Task.hpp"

namespace pi::scheduler
{

class IQueue
{
public:

    virtual ~IQueue() = default;


    virtual bool push(
        Task task
    ) = 0;


    virtual std::optional<Task> pop() = 0;


    [[nodiscard]]
    virtual bool empty() const noexcept = 0;


    [[nodiscard]]
    virtual std::size_t capacity() const noexcept = 0;

};

} // namespace pi::scheduler