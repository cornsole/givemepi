#include "scheduler/ThreadPool.hpp"
#include "scheduler/LockFreeQueue.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>


int main()
{
    using namespace pi::scheduler;


    std::atomic<int> counter{
        0
    };


    ThreadPool pool(
        std::make_unique<LockFreeQueue>(
            128
        ),
        4
    );


    pool.start();


    for (int i = 0; i < 100; ++i)
    {
        bool submitted =
            pool.submit(
                Task(
                    [&counter]()
                    {
                        counter.fetch_add(
                            1,
                            std::memory_order_relaxed
                        );
                    }
                )
            );


        assert(submitted);
    }


    auto start =
        std::chrono::steady_clock::now();


    while (counter.load() != 100)
    {
        if (
            std::chrono::steady_clock::now() - start
            >
            std::chrono::seconds(2)
        )
        {
            break;
        }

        std::this_thread::yield();
    }


    pool.stop();


    assert(
        counter.load() == 100
    );


    std::cout
        << "ThreadPool OK\n";


    std::cout
        << "Executed tasks: "
        << counter.load()
        << "\n";


    return 0;
}