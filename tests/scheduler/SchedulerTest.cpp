#include "scheduler/Scheduler.hpp"

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


    std::this_thread::sleep_for(
        std::chrono::milliseconds(100)
    );


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


    return 0;
}