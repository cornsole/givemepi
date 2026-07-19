#include "scheduler/TaskHandle.hpp"

#include <utility>


namespace pi::scheduler
{

namespace
{

bool isTerminal(
    TaskState state
) noexcept
{
    return state == TaskState::Completed
        || state == TaskState::Failed;
}

} // namespace


TaskHandle::TaskHandle(
    std::shared_ptr<detail::TaskSharedState> state
) noexcept
    : shared_state_(
        std::move(state)
    )
{
}


bool TaskHandle::valid() const noexcept
{
    return static_cast<bool>(
        shared_state_
    );
}


void TaskHandle::wait() const
{
    if (!shared_state_)
    {
        return;
    }


    std::unique_lock<std::mutex> lock(
        shared_state_->mutex
    );


    shared_state_->condition.wait(
        lock,
        [this]()
        {
            return isTerminal(
                shared_state_->state.load(
                    std::memory_order_acquire
                )
            );
        }
    );
}


bool TaskHandle::isReady() const noexcept
{
    if (!shared_state_)
    {
        return false;
    }


    return isTerminal(
        shared_state_->state.load(
            std::memory_order_acquire
        )
    );
}


bool TaskHandle::isCompleted() const noexcept
{
    return shared_state_
        && shared_state_->state.load(
               std::memory_order_acquire
           ) == TaskState::Completed;
}


bool TaskHandle::isFailed() const noexcept
{
    return shared_state_
        && shared_state_->state.load(
               std::memory_order_acquire
           ) == TaskState::Failed;
}


std::exception_ptr TaskHandle::exception() const
{
    if (!shared_state_)
    {
        return nullptr;
    }


    std::lock_guard<std::mutex> lock(
        shared_state_->mutex
    );


    return shared_state_->exception;
}


void TaskHandle::rethrowIfFailed() const
{
    wait();


    std::exception_ptr captured =
        exception();


    if (captured)
    {
        std::rethrow_exception(
            captured
        );
    }
}


} // namespace pi::scheduler
