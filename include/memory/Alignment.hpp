#pragma once

#include <cstddef>

namespace pi::memory
{

class Alignment
{
public:

    static constexpr std::size_t CACHE_LINE = 64;


    [[nodiscard]]
    static void* allocate(
        std::size_t size,
        std::size_t alignment = CACHE_LINE
    );


    static void deallocate(
        void* ptr,
        std::size_t alignment = CACHE_LINE
    ) noexcept;


    [[nodiscard]]
    static bool isAligned(
        const void* ptr,
        std::size_t alignment
    ) noexcept;


    [[nodiscard]]
    static std::size_t roundUp(
        std::size_t size,
        std::size_t alignment
    ) noexcept;


private:

    Alignment() = delete;

};

} // namespace pi::memory