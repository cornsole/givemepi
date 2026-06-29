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
        4,
        128
    );


    scheduler.start();


    for (int i = 0; i < 100; ++i)
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


    scheduler.stop();


    assert(
        counter.load() == 100
    );



    std::cout
        << "Scheduler OK\n";


    std::cout
        << "Executed tasks: "
        << counter.load()
        << "\n";

    std::atomic<int> lockfreeCounter{
        0
    };


    Scheduler lockfree(
        std::make_unique<LockFreeQueue>(128),
        4
    );


    lockfree.start();


    for (int i = 0; i < 100; ++i)
    {
        bool submitted =
            lockfree.submit(
                Task(
                    [&lockfreeCounter]()
                    {
                        lockfreeCounter.fetch_add(
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


    while (lockfreeCounter.load() != 100)
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

    lockfree.stop();

    assert(
        lockfreeCounter.load() == 100
    );


    std::cout
        << "LockFreeQueue OK\n";


    std::cout
        << "Executed tasks: "
        << lockfreeCounter.load()
        << "\n";




    return 0;
}