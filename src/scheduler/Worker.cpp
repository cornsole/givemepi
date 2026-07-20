#include "scheduler/Worker.hpp"
#include "scheduler/ThreadPool.hpp"

#include <thread>
#include <utility>


namespace pi::scheduler
{

thread_local Worker* Worker::currentWorker_ =
    nullptr;


Worker::Worker(
    ThreadPool* owner,
    std::size_t id,
    IQueue* queue,
    std::vector<std::unique_ptr<Worker>>* workers
)
    : owner_(owner),
      id_(id),
      globalQueue_(queue),
      workers_(workers)
{
}


Worker::~Worker()
{
    stop();
}


void Worker::start()
{
    if (running_)
    {
        return;
    }


    stopRequested_ = false;


    thread_ =
        std::thread(
            &Worker::run,
            this
        );


    running_ = true;
}


void Worker::stop() noexcept
{
    requestStop();
    join();
}


void Worker::requestStop() noexcept
{
    stopRequested_.store(
        true,
        std::memory_order_release
    );
}


void Worker::join() noexcept
{
    if (!running_)
    {
        return;
    }


    if (thread_.joinable())
    {
        thread_.join();
    }


    running_ = false;
}


bool Worker::running() const noexcept
{
    return running_.load(
        std::memory_order_relaxed
    );
}


std::size_t Worker::id() const noexcept
{
    return id_;
}


void Worker::push(
    Task task
)
{
    localQueue_.push(
        std::move(task)
    );
}


void Worker::run()
{
    currentWorker_ =
        this;


    while (true)
    {
        auto task =
            localQueue_.pop();


        if (task.has_value())
        {
            executeTask(
                *task
            );
            continue;
        }


        task =
            globalQueue_->pop();


        if (task.has_value())
        {
            executeTask(
                *task
            );
            continue;
        }


        task =
            steal();


        if (task.has_value())
        {
            executeTask(
                *task
            );
            continue;
        }


        if (
            stopRequested_.load(
                std::memory_order_acquire
            )
            &&
            (
                owner_ == nullptr
                || owner_->drainComplete()
            )
        )
        {
            break;
        }


        std::this_thread::yield();
    }


    currentWorker_ =
        nullptr;
}


void Worker::executeTask(
    Task& task
)
{
    task.execute();


    if (owner_ != nullptr)
    {
        owner_->taskCompleted();
    }
}

std::optional<Task>
Worker::steal()
{
    if (workers_ == nullptr)
    {
        return std::nullopt;
    }


    for (auto& worker : *workers_)
    {
        if (worker.get() == this)
        {
            continue;
        }


        auto task =
            worker->localQueue_.steal();


        if (task.has_value())
        {
            return task;
        }
    }


    return std::nullopt;
}

} // namespace pi::scheduler
