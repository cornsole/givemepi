#include "memory/ScratchBuffer.hpp"

#include <algorithm>
#include <cstring>


namespace pi::memory
{

ScratchBuffer::ScratchBuffer(
    std::size_t size
)
    : buffer_(size)
{
}


std::byte* ScratchBuffer::data() noexcept
{
    return buffer_.data();
}


const std::byte* ScratchBuffer::data() const noexcept
{
    return buffer_.data();
}


std::size_t ScratchBuffer::size() const noexcept
{
    return buffer_.size();
}


void ScratchBuffer::clear() noexcept
{
    std::fill(
        buffer_.begin(),
        buffer_.end(),
        std::byte{0}
    );
}


} // namespace pi::memory