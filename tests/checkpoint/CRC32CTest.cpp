#include "checkpoint/CRC32C.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <string_view>


using pi::checkpoint::CRC32C;


int main()
{
    constexpr std::string_view fixture = "123456789";
    const auto bytes = std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(fixture.data()),
        fixture.size()
    );

    if (CRC32C::calculate({}) != 0
        || CRC32C::calculate(bytes) != 0xE3069283U)
    {
        std::cerr << "CRC32C fixed fixture mismatch\n";
        return 1;
    }

    const auto first = bytes.first(4);
    const auto second = bytes.subspan(4);
    const std::uint32_t segmented = CRC32C::calculate(
        second,
        CRC32C::calculate(first)
    );

    if (segmented != CRC32C::calculate(bytes))
    {
        std::cerr << "CRC32C segmented calculation mismatch\n";
        return 1;
    }

    std::cout << "CRC32C OK\n";
    return 0;
}
