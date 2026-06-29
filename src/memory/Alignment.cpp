#include "memory/Alignment.hpp"

#include <cstdint>
#include <new>


namespace pi::memory
{

void* Alignment::allocate(
    std::size_t size,
    std::size_t alignment
)
{
    return ::operator new(
        size,
        std::align_val_t(alignment)
    );
}


void Alignment::deallocate(
    void* ptr,
    std::size_t alignment
) noexcept
{
    ::operator delete(
        ptr,
        std::align_val_t(alignment)
    );
}


bool Alignment::isAligned(
    const void* ptr,
    std::size_t alignment
) noexcept
{
    if (ptr == nullptr)
    {
        return false;
    }


    const auto address =
        reinterpret_cast<std::uintptr_t>(
            ptr
        );


    return (address % alignment) == 0;
}


std::size_t Alignment::roundUp(
    std::size_t size,
    std::size_t alignment
) noexcept
{
    const std::size_t remainder =
        size % alignment;


    if (remainder == 0)
    {
        return size;
    }


    return size + (alignment - remainder);
}


} // namespace pi::memory