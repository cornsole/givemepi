#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

namespace pi::memory
{

class Arena
{
public:

    explicit Arena(
        std::size_t size
    );

    ~Arena();


    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    Arena(Arena&&) = delete;
    Arena& operator=(Arena&&) = delete;


    void* allocate(
        std::size_t size,
        std::size_t alignment = alignof(std::max_align_t)
    );


    template<typename T, typename... Args>
    T* create(
        Args&&... args
    )
    {
        void* memory =
            allocate(
                sizeof(T),
                alignof(T)
            );

        return new(memory) T(
            std::forward<Args>(args)...
        );
    }


    void reset() noexcept;


    [[nodiscard]]
    std::size_t capacity() const noexcept;


    [[nodiscard]]
    std::size_t used() const noexcept;


    [[nodiscard]]
    std::size_t remaining() const noexcept;


private:

    std::byte* buffer_ = nullptr;

    std::size_t capacity_ = 0;

    std::size_t offset_ = 0;


    static std::uintptr_t alignAddress(
        std::uintptr_t address,
        std::size_t alignment
    ) noexcept;
};


} // namespace pi::memory