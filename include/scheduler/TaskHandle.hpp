#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>


namespace pi::scheduler
{

class Task;


enum class TaskState : std::uint8_t
{
    Pending,
    Running,
    Completed,
    Failed
};


namespace detail
{

struct TaskSharedState
{
    std::atomic<TaskState> state{
        TaskState::Pending
    };

    std::exception_ptr exception;

    mutable std::mutex mutex;

    std::condition_variable condition;
};

} // namespace detail


class TaskHandle
{
public:

    /// Create an invalid handle that does not observe a task.
    TaskHandle() noexcept = default;


    /// Transfer ownership of the observed shared state.
    TaskHandle(TaskHandle&&) noexcept = default;

    /// Transfer ownership of the observed shared state.
    TaskHandle& operator=(TaskHandle&&) noexcept = default;


    TaskHandle(const TaskHandle&) = delete;
    TaskHandle& operator=(const TaskHandle&) = delete;


    /// Check whether this handle observes an accepted task.
    [[nodiscard]]
    bool valid() const noexcept;


    /// Wait until the observed task reaches a terminal state.
    void wait() const;


    /// Check whether the observed task reached a terminal state.
    [[nodiscard]]
    bool isReady() const noexcept;


    /// Check whether the observed task completed successfully.
    [[nodiscard]]
    bool isCompleted() const noexcept;


    /// Check whether the observed task failed.
    [[nodiscard]]
    bool isFailed() const noexcept;


    /// Return the captured task exception when one is currently available.
    [[nodiscard]]
    std::exception_ptr exception() const;


    /// Rethrow the exception captured from a failed task, if one exists.
    void rethrowIfFailed() const;


private:

    explicit TaskHandle(
        std::shared_ptr<detail::TaskSharedState> state
    ) noexcept;


    std::shared_ptr<detail::TaskSharedState> shared_state_;

    friend class Task;

};


} // namespace pi::scheduler
