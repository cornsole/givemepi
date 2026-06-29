#include "memory/Arena.hpp"
#include "memory/Alignment.hpp"
#include "memory/MemoryPool.hpp"
#include "memory/ScratchBuffer.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>


int main()
{
    using namespace pi::memory;


    // ------------------------------------------------------------
    // Arena Test
    // ------------------------------------------------------------

    Arena arena(1024);


    void* a =
        arena.allocate(128);


    assert(a != nullptr);
    assert(arena.used() >= 128);


    std::size_t usedBeforeReset =
        arena.used();


    arena.reset();


    assert(arena.used() == 0);

    std::cout
        << "Arena OK ("
        << usedBeforeReset
        << " bytes used)\n";



    // ------------------------------------------------------------
    // Alignment Test
    // ------------------------------------------------------------

    void* aligned =
        Alignment::allocate(
            256,
            Alignment::CACHE_LINE
        );


    assert(
        Alignment::isAligned(
            aligned,
            Alignment::CACHE_LINE
        )
    );


    Alignment::deallocate(
        aligned,
        Alignment::CACHE_LINE
    );


    std::cout
        << "Alignment OK\n";



    // ------------------------------------------------------------
    // Memory Pool Test
    // ------------------------------------------------------------

    MemoryPool pool(
        sizeof(std::uint64_t),
        16
    );


    void* block1 =
        pool.allocate();


    void* block2 =
        pool.allocate();


    assert(block1 != nullptr);
    assert(block2 != nullptr);

    assert(pool.used() == 2);


    pool.deallocate(block1);
    pool.deallocate(block2);


    assert(pool.used() == 0);


    std::cout
        << "MemoryPool OK\n";



    // ------------------------------------------------------------
    // Scratch Buffer Test
    // ------------------------------------------------------------

    ScratchBuffer buffer(
        4096
    );


    assert(
        buffer.size() == 4096
    );


    auto* data =
        buffer.as<std::uint64_t>();


    data[0] = 12345;


    assert(data[0] == 12345);


    buffer.clear();


    assert(data[0] == 0);


    std::cout
        << "ScratchBuffer OK\n";



    std::cout
        << "\nAll memory tests passed.\n";


    return 0;
}