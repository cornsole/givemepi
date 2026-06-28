#pragma once

namespace pi::scheduler
{

class ThreadPool
{
public:
    ThreadPool() = default;
    ~ThreadPool() = default;

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;
};

} // namespace pi::scheduler