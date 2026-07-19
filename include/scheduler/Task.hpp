#pragma once

#include "scheduler/TaskHandle.hpp"

#include <functional>
#include <memory>
#include <utility>


namespace pi::scheduler
{

class Task
{
public:

    using Function =
        std::function<void()>;


    /// Create an invalid task with no callable or shared state.
    Task() = default;


    /// Create a task and the shared state observed by its handle.
    explicit Task(
        Function function
    )
        : function_(std::move(function)),
          shared_state_(
              std::make_shared<detail::TaskSharedState>()
          )
    {
    }


    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;


    Task(Task&&) noexcept = default;
    Task& operator=(Task&&) noexcept = default;


    /// Execute the callable at most once and publish its terminal state.
    /// Callable exceptions are captured in the shared state.
    void execute();


    /// Return the task's current lifecycle state.
    [[nodiscard]]
    TaskState state() const noexcept;


    /// Check whether the task has an executable callable and shared state.
    [[nodiscard]]
    bool valid() const noexcept;


    /// Create a move-only handle observing this task's shared state.
    [[nodiscard]]
    TaskHandle handle() const noexcept;


private:

    Function function_;

    std::shared_ptr<detail::TaskSharedState> shared_state_;

};


} // namespace pi::scheduler
