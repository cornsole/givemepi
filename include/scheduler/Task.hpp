#pragma once

#include <cstdint>
#include <functional>
#include <utility>


namespace pi::scheduler
{

enum class TaskState : std::uint8_t
{
    Pending,
    Running,
    Completed,
    Failed
};


class Task
{
public:

    using Function =
        std::function<void()>;


    Task() = default;


    explicit Task(
        Function function
    )
        : function_(std::move(function))
    {
    }


    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;


    Task(Task&&) noexcept = default;
    Task& operator=(Task&&) noexcept = default;


    void execute();


    [[nodiscard]]
    TaskState state() const noexcept;


    [[nodiscard]]
    bool valid() const noexcept;


private:

    Function function_;

    TaskState state_ =
        TaskState::Pending;

};


} // namespace pi::scheduler