#include "memory/Arena.hpp"

#include <algorithm>
#include <new>
#include <stdexcept>


namespace pi::memory
{

Arena::Arena(
    std::size_t size
)
    : capacity_(size)
{
    buffer_ =
        static_cast<std::byte*>(
            ::operator new(
                capacity_,
                std::align_val_t(
                    alignof(std::max_align_t)
                )
            )
        );
}


Arena::~Arena()
{
    ::operator delete(
        buffer_,
        std::align_val_t(
            alignof(std::max_align_t)
        )
    );
}


void* Arena::allocate(
    std::size_t size,
    std::size_t alignment
)
{
    const std::uintptr_t current =
        reinterpret_cast<std::uintptr_t>(
            buffer_ + offset_
        );


    const std::uintptr_t aligned =
        alignAddress(
            current,
            alignment
        );


    const std::size_t padding =
        aligned - current;


    if (offset_ + padding + size > capacity_)
    {
        throw std::bad_alloc();
    }


    offset_ += padding;


    void* result =
        buffer_ + offset_;


    offset_ += size;


    return result;
}


void Arena::reset() noexcept
{
    offset_ = 0;
}


std::size_t Arena::capacity() const noexcept
{
    return capacity_;
}


std::size_t Arena::used() const noexcept
{
    return offset_;
}


std::size_t Arena::remaining() const noexcept
{
    return capacity_ - offset_;
}


std::uintptr_t Arena::alignAddress(
    std::uintptr_t address,
    std::size_t alignment
) noexcept
{
    const std::uintptr_t mask =
        alignment - 1;


    return (address + mask) & ~mask;
}


} // namespace pi::memory