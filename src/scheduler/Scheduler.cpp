#include "scheduler/Scheduler.hpp"

#include <utility>


namespace pi::scheduler
{

Scheduler::Scheduler(
    std::size_t workerCount,
    std::size_t queueCapacity
)
{
    queue_ =
        std::make_unique<
            LockFreeQueue
        >(
            queueCapacity
        );


    workers_.reserve(
        workerCount
    );


    for (std::size_t i = 0; i < workerCount; ++i)
    {
        workers_.push_back(
            std::make_unique<Worker>(
                i,
                queue_.get()
            )
        );
    }
}


Scheduler::~Scheduler()
{
    stop();
}


void Scheduler::start()
{
    if (running_)
    {
        return;
    }


    running_ = true;


    for (auto& worker : workers_)
    {
        worker->start();
    }
}


void Scheduler::stop()
{
    if (!running_)
    {
        return;
    }


    for (auto& worker : workers_)
    {
        worker->stop();
    }


    running_ = false;
}


bool Scheduler::submit(
    Task task
)
{
    if (!running_)
    {
        return false;
    }


    return queue_->push(
        std::move(task)
    );
}


std::size_t Scheduler::workerCount() const noexcept
{
    return workers_.size();
}


bool Scheduler::running() const noexcept
{
    return running_;
}


} // namespace pi::scheduler