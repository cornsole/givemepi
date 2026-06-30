#include "scheduler/ThreadPool.hpp"

#include <utility>


namespace pi::scheduler
{

ThreadPool::ThreadPool(
    std::unique_ptr<IQueue> queue,
    std::size_t workerCount
)
    : queue_(
        std::move(queue)
    )
{
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


ThreadPool::~ThreadPool()
{
    stop();
}


void ThreadPool::start()
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


void ThreadPool::stop()
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


bool ThreadPool::submit(
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


std::size_t ThreadPool::workerCount() const noexcept
{
    return workers_.size();
}


bool ThreadPool::running() const noexcept
{
    return running_.load(
        std::memory_order_relaxed
    );
}


} // namespace pi::scheduler