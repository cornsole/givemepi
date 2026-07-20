#pragma once

#include <cstdint>


namespace pi::scheduler
{

/// Observable lifecycle states shared by Scheduler and ThreadPool.
enum class SchedulerState : std::uint8_t
{
    Stopped,
    Running,
    Stopping
};


} // namespace pi::scheduler
