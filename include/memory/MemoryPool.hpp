#pragma once

#include <cstddef>
#include <vector>

namespace pi::memory
{

class MemoryPool
{
public:

    MemoryPool(
        std::size_t blockSize,
        std::size_t blockCount
    );

    ~MemoryPool();


    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    MemoryPool(MemoryPool&&) = delete;
    MemoryPool& operator=(MemoryPool&&) = delete;


    [[nodiscard]]
    void* allocate();


    void deallocate(
        void* ptr
    ) noexcept;


    void reset() noexcept;


    [[nodiscard]]
    std::size_t blockSize() const noexcept;


    [[nodiscard]]
    std::size_t capacity() const noexcept;


    [[nodiscard]]
    std::size_t used() const noexcept;


    [[nodiscard]]
    std::size_t available() const noexcept;


private:

    struct FreeBlock
    {
        FreeBlock* next = nullptr;
    };


    std::byte* memory_ = nullptr;

    FreeBlock* freeList_ = nullptr;


    std::size_t blockSize_ = 0;
    std::size_t blockCount_ = 0;
    std::size_t used_ = 0;


    void initialize();


    [[nodiscard]]
    std::byte* blockAddress(
        std::size_t index
    ) const noexcept;
};


} // namespace pi::memory