#pragma once

#include "bigint/GMPInteger.hpp"

#include <cstdint>
#include <span>
#include <vector>


namespace pi::checkpoint
{

enum class IntegerSign : std::uint8_t
{
    zero = 0,
    positive = 1,
    negative = 2
};


struct SerializedGMPInteger
{
    IntegerSign sign;
    std::vector<std::uint8_t> magnitude;

    [[nodiscard]]
    bool operator==(const SerializedGMPInteger&) const noexcept = default;
};


class GMPSerialization
{
public:

    /**
     * Exports a GMP integer as canonical signed magnitude.
     *
     * Zero has an empty magnitude. Non-zero magnitudes are minimal unsigned
     * big-endian byte sequences without a leading zero byte.
     */
    [[nodiscard]]
    static SerializedGMPInteger encode(
        const bigint::GMPInteger& value
    );

    /**
     * Imports canonical signed magnitude and rejects non-canonical forms.
     *
     * @throws std::invalid_argument for an invalid sign/magnitude pair.
     */
    [[nodiscard]]
    static bigint::GMPInteger decode(
        IntegerSign sign,
        std::span<const std::uint8_t> magnitude
    );
};

} // namespace pi::checkpoint
