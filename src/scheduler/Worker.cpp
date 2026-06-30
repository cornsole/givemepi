#include "scheduler/Worker.hpp"

#include <chrono>
#include <thread>
#include <utility>
#include <vector>
#include <memory>


namespace pi::scheduler
{

Worker::Worker(
    std::size_t id,
    IQueue* queue,
    std::vector<std::unique_ptr<Worker>>* workers
)
    : id_(id),
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
    running_ = true;


    thread_ =
        std::thread(
            &Worker::run,
            this
        );
}


void Worker::stop() noexcept
{
    if (!running_)
    {
        return;
    }


    stopRequested_ = true;


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
    while (!stopRequested_)
    {
        auto task =
            localQueue_.pop();


        if (task.has_value())
        {
            task->execute();
            continue;
        }


        task =
            globalQueue_->pop();


        if (task.has_value())
        {
            task->execute();
            continue;
        }


        task =
            steal();


        if (task.has_value())
        {
            task->execute();
            continue;
        }


        std::this_thread::yield();
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