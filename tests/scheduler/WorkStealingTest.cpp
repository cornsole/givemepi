#include "scheduler/Scheduler.hpp"
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


    Scheduler scheduler(
        std::make_unique<LockFreeQueue>(1024),
        4
    );


    scheduler.start();


    for (int i = 0; i < 10000; ++i)
    {
        bool submitted =
            scheduler.submit(
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


    while (
        counter.load(
            std::memory_order_relaxed
        )
        !=
        10000
    )
    {
        if (
            std::chrono::steady_clock::now()
            -
            start
            >
            std::chrono::seconds(5)
        )
        {
            break;
        }


        std::this_thread::yield();
    }


    scheduler.stop();


    assert(
        counter.load()
        ==
        10000
    );


    std::cout
        << "WorkStealing OK\n";


    std::cout
        << "Executed tasks: "
        << counter.load()
        << "\n";


    return 0;
}