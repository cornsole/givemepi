#include "scheduler/ThreadPool.hpp"
#include "scheduler/TaskHandle.hpp"

#include <cassert>
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
                this,
                i,
                queue_.get(),
                &workers_
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
    std::unique_lock<std::mutex> lock(
        lifecycleMutex_
    );


    lifecycleCondition_.wait(
        lock,
        [this]()
        {
            return state_.load(
                       std::memory_order_acquire
                   ) != SchedulerState::Stopping;
        }
    );


    if (
        state_.load(
            std::memory_order_acquire
        ) == SchedulerState::Running
    )
    {
        return;
    }


    try
    {
        for (auto& worker : workers_)
        {
            worker->start();
        }
    }
    catch (...)
    {
        for (auto& worker : workers_)
        {
            worker->stop();
        }

        state_.store(
            SchedulerState::Stopped,
            std::memory_order_release
        );

        throw;
    }


    state_.store(
        SchedulerState::Running,
        std::memory_order_release
    );
}


void ThreadPool::stop()
{
    {
        std::unique_lock<std::mutex> lock(
            lifecycleMutex_
        );


        if (
            state_.load(
                std::memory_order_acquire
            ) == SchedulerState::Stopped
        )
        {
            return;
        }


        if (
            state_.load(
                std::memory_order_acquire
            ) == SchedulerState::Stopping
        )
        {
            const std::size_t stopGeneration =
                stopGeneration_;

            lifecycleCondition_.wait(
                lock,
                [this, stopGeneration]()
                {
                    return stopGeneration_
                        != stopGeneration;
                }
            );

            return;
        }


        state_.store(
            SchedulerState::Stopping,
            std::memory_order_release
        );
    }


    for (auto& worker : workers_)
    {
        worker->requestStop();
    }


    for (auto& worker : workers_)
    {
        worker->join();
    }


    {
        std::lock_guard<std::mutex> lock(
            lifecycleMutex_
        );

        state_.store(
            SchedulerState::Stopped,
            std::memory_order_release
        );

        ++stopGeneration_;
    }


    lifecycleCondition_.notify_all();
}


TaskHandle ThreadPool::submit(
    Task task
)
{
    std::lock_guard<std::mutex> lock(
        lifecycleMutex_
    );


    if (
        state_.load(
            std::memory_order_acquire
        ) != SchedulerState::Running
    )
    {
        return {};
    }


    if (
        workers_.empty()
        || !task.valid()
    )
    {
        return {};
    }


    TaskHandle handle =
        task.handle();


    outstandingTasks_.fetch_add(
        1,
        std::memory_order_acq_rel
    );


    try
    {
        Worker* currentWorker =
            Worker::currentWorker_;


        if (
            currentWorker != nullptr
            && currentWorker->owner_ == this
        )
        {
            currentWorker->push(
                std::move(task)
            );

            return handle;
        }


        if (
            !queue_->push(
                std::move(task)
            )
        )
        {
            taskCompleted();

            return {};
        }
    }
    catch (...)
    {
        taskCompleted();

        throw;
    }


    return handle;
}


std::size_t ThreadPool::workerCount() const noexcept
{
    return workers_.size();
}

Worker*
ThreadPool::workerAt(
    std::size_t index
)
{
    if (workers_.empty())
    {
        return nullptr;
    }


    return workers_[
        index % workers_.size()
    ].get();
}

bool ThreadPool::running() const noexcept
{
    return state() == SchedulerState::Running;
}


bool ThreadPool::ownsCurrentWorker() const noexcept
{
    return Worker::currentWorker_ != nullptr
        && Worker::currentWorker_->owner_ == this;
}


SchedulerState ThreadPool::state() const noexcept
{
    return state_.load(
        std::memory_order_acquire
    );
}


void ThreadPool::taskCompleted() noexcept
{
    [[maybe_unused]] const std::size_t previous =
        outstandingTasks_.fetch_sub(
            1,
            std::memory_order_acq_rel
        );


    assert(previous > 0);
}


bool ThreadPool::drainComplete() const noexcept
{
    return outstandingTasks_.load(
               std::memory_order_acquire
           ) == 0;
}


} // namespace pi::scheduler
