#include "scheduler/Scheduler.hpp"

#include "scheduler/LockFreeQueue.hpp"
#include "scheduler/ReferenceQueue.hpp"

#include <utility>


namespace pi::scheduler
{

Scheduler::Scheduler(
    std::size_t workerCount,
    std::size_t queueCapacity
)
{
    pool_ =
        std::make_unique<ThreadPool>(
            std::make_unique<LockFreeQueue>(
                queueCapacity
            ),
            workerCount
        );
}


Scheduler::Scheduler(
    std::unique_ptr<IQueue> queue,
    std::size_t workerCount
)
{
    pool_ =
        std::make_unique<ThreadPool>(
            std::move(queue),
            workerCount
        );
}


Scheduler::~Scheduler()
{
    stop();
}


void Scheduler::start()
{
    pool_->start();
}


void Scheduler::stop()
{
    pool_->stop();
}


TaskHandle Scheduler::submit(
    Task task
)
{
    return pool_->submit(
        std::move(task)
    );
}


std::size_t Scheduler::workerCount() const noexcept
{
    return pool_->workerCount();
}


bool Scheduler::running() const noexcept
{
    return pool_->running();
}


SchedulerState Scheduler::state() const noexcept
{
    return pool_->state();
}


} // namespace pi::scheduler
