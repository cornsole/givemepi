#pragma once

#include "bigint/GMPInteger.hpp"

#include <cstddef>
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
     * Returns the canonical sign of value.
     */
    [[nodiscard]]
    static IntegerSign sign(const bigint::GMPInteger& value) noexcept;

    /**
     * Returns the minimal canonical magnitude length of value in bytes.
     */
    [[nodiscard]]
    static std::size_t magnitudeSize(const bigint::GMPInteger& value);

    /**
     * Writes the canonical magnitude of value directly into destination
     * without copying the GMP limbs.
     *
     * @throws std::invalid_argument unless destination has magnitudeSize().
     */
    static void exportMagnitude(
        const bigint::GMPInteger& value,
        std::span<std::uint8_t> destination
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
