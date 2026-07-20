#include "checkpoint/GMPSerialization.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>


using pi::bigint::GMPInteger;
using pi::checkpoint::GMPSerialization;
using pi::checkpoint::IntegerSign;
using pi::checkpoint::SerializedGMPInteger;


namespace
{

bool roundTrips(const GMPInteger& value)
{
    const SerializedGMPInteger encoded = GMPSerialization::encode(value);
    const GMPInteger decoded = GMPSerialization::decode(
        encoded.sign,
        encoded.magnitude
    );

    return decoded.compare(value) == 0
        && GMPSerialization::encode(decoded) == encoded;
}


template<typename Function>
bool rejects(Function&& function)
{
    try
    {
        function();
    }
    catch (const std::invalid_argument&)
    {
        return true;
    }

    return false;
}

} // namespace


int main()
{
    const SerializedGMPInteger zero = GMPSerialization::encode(GMPInteger{});

    if (zero.sign != IntegerSign::zero || !zero.magnitude.empty())
    {
        std::cerr << "Canonical zero encoding mismatch\n";
        return 1;
    }

    const SerializedGMPInteger positive =
        GMPSerialization::encode(GMPInteger{256});

    if (positive.sign != IntegerSign::positive
        || positive.magnitude != std::vector<std::uint8_t>({0x01, 0x00}))
    {
        std::cerr << "Canonical positive encoding mismatch\n";
        return 1;
    }

    GMPInteger negative(128);
    negative.negate();
    const SerializedGMPInteger encodedNegative =
        GMPSerialization::encode(negative);

    if (encodedNegative.sign != IntegerSign::negative
        || encodedNegative.magnitude != std::vector<std::uint8_t>({0x80}))
    {
        std::cerr << "Canonical negative encoding mismatch\n";
        return 1;
    }

    GMPInteger large;
    large.setPowerOfTen(10000);

    if (!roundTrips(GMPInteger{})
        || !roundTrips(GMPInteger{1})
        || !roundTrips(negative)
        || !roundTrips(large))
    {
        std::cerr << "Canonical GMP round trip failed\n";
        return 1;
    }

    const std::vector<std::uint8_t> one{1};
    const std::vector<std::uint8_t> leadingZero{0, 1};

    if (!rejects([&one]() {
            static_cast<void>(
                GMPSerialization::decode(IntegerSign::zero, one)
            );
        })
        || !rejects([]() {
            static_cast<void>(GMPSerialization::decode(
                IntegerSign::positive,
                {}
            ));
        })
        || !rejects([&leadingZero]() {
            static_cast<void>(GMPSerialization::decode(
                IntegerSign::negative,
                leadingZero
            ));
        })
        || !rejects([&one]() {
            static_cast<void>(GMPSerialization::decode(
                static_cast<IntegerSign>(255),
                one
            ));
        }))
    {
        std::cerr << "Non-canonical GMP encoding was accepted\n";
        return 1;
    }

    std::cout << "GMPSerialization OK\n";
    return 0;
}
