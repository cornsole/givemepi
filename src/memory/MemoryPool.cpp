#include "memory/MemoryPool.hpp"

#include <new>


namespace pi::memory
{

MemoryPool::MemoryPool(
    std::size_t blockSize,
    std::size_t blockCount
)
    : blockSize_(blockSize),
      blockCount_(blockCount)
{
    if (blockSize_ < sizeof(FreeBlock))
    {
        blockSize_ = sizeof(FreeBlock);
    }


    memory_ =
        static_cast<std::byte*>(
            ::operator new(
                blockSize_ * blockCount_,
                std::align_val_t(
                    alignof(std::max_align_t)
                )
            )
        );


    initialize();
}


MemoryPool::~MemoryPool()
{
    ::operator delete(
        memory_,
        std::align_val_t(
            alignof(std::max_align_t)
        )
    );
}


void MemoryPool::initialize()
{
    freeList_ = nullptr;


    for (std::size_t i = 0; i < blockCount_; ++i)
    {
        auto* block =
            reinterpret_cast<FreeBlock*>(
                blockAddress(i)
            );


        block->next = freeList_;

        freeList_ = block;
    }


    used_ = 0;
}


void* MemoryPool::allocate()
{
    if (freeList_ == nullptr)
    {
        throw std::bad_alloc();
    }


    FreeBlock* block =
        freeList_;


    freeList_ =
        freeList_->next;


    ++used_;


    return block;
}


void MemoryPool::deallocate(
    void* ptr
) noexcept
{
    if (ptr == nullptr)
    {
        return;
    }


    auto* block =
        static_cast<FreeBlock*>(
            ptr
        );


    block->next =
        freeList_;


    freeList_ = block;


    --used_;
}


void MemoryPool::reset() noexcept
{
    initialize();
}


std::size_t MemoryPool::blockSize() const noexcept
{
    return blockSize_;
}


std::size_t MemoryPool::capacity() const noexcept
{
    return blockCount_;
}


std::size_t MemoryPool::used() const noexcept
{
    return used_;
}


std::size_t MemoryPool::available() const noexcept
{
    return blockCount_ - used_;
}


std::byte* MemoryPool::blockAddress(
    std::size_t index
) const noexcept
{
    return memory_ + (index * blockSize_);
}


} // namespace pi::memory