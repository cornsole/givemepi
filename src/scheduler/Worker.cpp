#include "scheduler/Worker.hpp"

#include <chrono>
#include <thread>


namespace pi::scheduler
{

Worker::Worker(
    std::size_t id,
    LockFreeQueue* queue
)
    : id_(id),
      queue_(queue)
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


void Worker::run()
{
    while (!stopRequested_)
    {
        auto task =
            queue_->pop();


        if (task.has_value())
        {
            task->execute();
            continue;
        }


        std::this_thread::yield();
    }
}


} // namespace pi::scheduler