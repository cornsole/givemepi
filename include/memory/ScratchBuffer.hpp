#pragma once

#include <cstddef>
#include <vector>

namespace pi::memory
{

class ScratchBuffer
{
public:

    explicit ScratchBuffer(
        std::size_t size
    );

    ~ScratchBuffer() = default;


    ScratchBuffer(const ScratchBuffer&) = delete;
    ScratchBuffer& operator=(const ScratchBuffer&) = delete;

    ScratchBuffer(ScratchBuffer&&) noexcept = default;
    ScratchBuffer& operator=(ScratchBuffer&&) noexcept = default;


    [[nodiscard]]
    std::byte* data() noexcept;


    [[nodiscard]]
    const std::byte* data() const noexcept;


    [[nodiscard]]
    std::size_t size() const noexcept;


    void clear() noexcept;


    template<typename T>
    [[nodiscard]]
    T* as() noexcept
    {
        return reinterpret_cast<T*>(
            buffer_.data()
        );
    }


    template<typename T>
    [[nodiscard]]
    const T* as() const noexcept
    {
        return reinterpret_cast<const T*>(
            buffer_.data()
        );
    }


private:

    std::vector<std::byte> buffer_;

};


} // namespace pi::memory