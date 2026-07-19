#include "scheduler/Task.hpp"

#include <exception>
#include <stdexcept>


namespace pi::scheduler
{

namespace
{

void finishTask(
    const std::shared_ptr<detail::TaskSharedState>& state,
    TaskState result,
    std::exception_ptr exception = nullptr
)
{
    {
        std::lock_guard<std::mutex> lock(
            state->mutex
        );

        state->exception =
            std::move(exception);

        state->state.store(
            result,
            std::memory_order_release
        );
    }


    state->condition.notify_all();
}

} // namespace


void Task::execute()
{
    if (!shared_state_)
    {
        return;
    }


    TaskState expected =
        TaskState::Pending;


    if (
        !shared_state_->state.compare_exchange_strong(
            expected,
            TaskState::Running,
            std::memory_order_acq_rel
        )
    )
    {
        return;
    }


    if (!function_)
    {
        finishTask(
            shared_state_,
            TaskState::Failed,
            std::make_exception_ptr(
                std::logic_error(
                    "Cannot execute an invalid task."
                )
            )
        );

        return;
    }

    try
    {
        function_();

        finishTask(
            shared_state_,
            TaskState::Completed
        );
    }
    catch (...)
    {
        finishTask(
            shared_state_,
            TaskState::Failed,
            std::current_exception()
        );
    }
}


TaskState Task::state() const noexcept
{
    if (!shared_state_)
    {
        return TaskState::Failed;
    }


    return shared_state_->state.load(
        std::memory_order_acquire
    );
}


bool Task::valid() const noexcept
{
    return shared_state_
        && static_cast<bool>(
            function_
        );
}


TaskHandle Task::handle() const noexcept
{
    return TaskHandle(
        shared_state_
    );
}


} // namespace pi::scheduler
