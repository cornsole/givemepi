#pragma once

#include <cstdint>
#include <span>


namespace pi::checkpoint
{

class CRC32C
{
public:

    /**
     * Calculates CRC32C (Castagnoli). Passing a prior returned checksum allows
     * discontiguous input segments to be processed without concatenation.
     */
    [[nodiscard]]
    static std::uint32_t calculate(
        std::span<const std::uint8_t> bytes,
        std::uint32_t previous = 0
    ) noexcept;
};

} // namespace pi::checkpoint
