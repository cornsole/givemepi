#include "scheduler/Task.hpp"

namespace pi::scheduler
{

void Task::execute()
{
    if (!function_)
    {
        state_ = TaskState::Failed;
        return;
    }


    state_ = TaskState::Running;


    try
    {
        function_();

        state_ = TaskState::Completed;
    }
    catch (...)
    {
        state_ = TaskState::Failed;
    }
}


TaskState Task::state() const noexcept
{
    return state_;
}


bool Task::valid() const noexcept
{
    return static_cast<bool>(
        function_
    );
}


} // namespace pi::scheduler