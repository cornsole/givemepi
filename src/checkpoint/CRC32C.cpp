#include "checkpoint/CRC32C.hpp"

#include <array>
#include <cstddef>


namespace pi::checkpoint
{

namespace
{

constexpr std::uint32_t reflectedCastagnoliPolynomial = 0x82F63B78U;


constexpr std::array<std::uint32_t, 256> createTable() noexcept
{
    std::array<std::uint32_t, 256> table{};

    for (std::size_t index = 0; index < table.size(); ++index)
    {
        std::uint32_t remainder = static_cast<std::uint32_t>(index);

        for (int bit = 0; bit < 8; ++bit)
        {
            remainder = (remainder >> 1)
                ^ ((remainder & 1U) != 0
                    ? reflectedCastagnoliPolynomial
                    : 0U);
        }

        table[index] = remainder;
    }

    return table;
}


constexpr auto crcTable = createTable();

} // namespace


std::uint32_t CRC32C::calculate(
    std::span<const std::uint8_t> bytes,
    std::uint32_t previous
) noexcept
{
    std::uint32_t checksum = previous ^ 0xFFFFFFFFU;

    for (const std::uint8_t byte : bytes)
    {
        const std::uint8_t index = static_cast<std::uint8_t>(checksum ^ byte);
        checksum = crcTable[index] ^ (checksum >> 8);
    }

    return checksum ^ 0xFFFFFFFFU;
}

} // namespace pi::checkpoint
