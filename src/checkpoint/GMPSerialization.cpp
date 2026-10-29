#include "checkpoint/GMPSerialization.hpp"

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>


namespace pi::checkpoint
{

SerializedGMPInteger GMPSerialization::encode(
    const bigint::GMPInteger& value
)
{
    std::vector<std::uint8_t> magnitude(magnitudeSize(value));
    exportMagnitude(value, magnitude);
    return SerializedGMPInteger{sign(value), std::move(magnitude)};
}


IntegerSign GMPSerialization::sign(const bigint::GMPInteger& value) noexcept
{
    const int rawSign = mpz_sgn(*value.raw());
    if (rawSign == 0) return IntegerSign::zero;
    return rawSign > 0 ? IntegerSign::positive : IntegerSign::negative;
}


std::size_t GMPSerialization::magnitudeSize(const bigint::GMPInteger& value)
{
    if (mpz_sgn(*value.raw()) == 0) return 0;

    // Base 2 is exact in mpz_sizeinbase, so this is the minimal byte count.
    const std::size_t bitCount = mpz_sizeinbase(*value.raw(), 2);
    if (bitCount > std::numeric_limits<std::size_t>::max() - 7)
        throw std::overflow_error("Canonical GMP magnitude size overflow");
    return (bitCount + 7) / 8;
}


void GMPSerialization::exportMagnitude(
    const bigint::GMPInteger& value,
    std::span<std::uint8_t> destination
)
{
    if (destination.size() != magnitudeSize(value))
    {
        throw std::invalid_argument(
            "Canonical magnitude destination has the wrong size"
        );
    }
    if (destination.empty()) return;

    // mpz_export ignores the sign and emits |value|, so negative values need
    // no temporary GMP copy.
    std::size_t written = 0;
    mpz_export(destination.data(), &written, 1, 1, 1, 0, *value.raw());
    if (written != destination.size())
    {
        throw std::logic_error("Canonical magnitude export size mismatch");
    }
}


bigint::GMPInteger GMPSerialization::decode(
    IntegerSign sign,
    std::span<const std::uint8_t> magnitude
)
{
    switch (sign)
    {
        case IntegerSign::zero:
            if (!magnitude.empty())
            {
                throw std::invalid_argument(
                    "Canonical zero must have an empty magnitude"
                );
            }

            return bigint::GMPInteger{};

        case IntegerSign::positive:
        case IntegerSign::negative:
            if (magnitude.empty())
            {
                throw std::invalid_argument(
                    "Canonical non-zero integer requires a magnitude"
                );
            }

            if (magnitude.front() == 0)
            {
                throw std::invalid_argument(
                    "Canonical magnitude must not contain a leading zero"
                );
            }

            break;

        default:
            throw std::invalid_argument(
                "Unknown canonical integer sign"
            );
    }

    bigint::GMPInteger value;

    mpz_import(
        *value.raw(),
        magnitude.size(),
        1,
        1,
        1,
        0,
        magnitude.data()
    );

    if (sign == IntegerSign::negative)
    {
        value.negate();
    }

    return value;
}

} // namespace pi::checkpoint
