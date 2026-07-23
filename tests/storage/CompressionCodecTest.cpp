#include "storage/CompressionCodec.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>


int main()
{
    using pi::storage::CompressionAlgorithm;
    using pi::storage::CompressionCodecs;

    const auto& codec = CompressionCodecs::forAlgorithm(
        CompressionAlgorithm::none
    );
    const std::vector<std::uint8_t> input{1, 2, 3, 4};
    const std::vector<std::uint8_t> compressed = codec.compress(input, 4);
    const std::vector<std::uint8_t> decompressed = codec.decompress(
        compressed,
        4,
        4
    );

    if (compressed != input || decompressed != input
        || codec.algorithm() != CompressionAlgorithm::none)
    {
        std::cerr << "None compression round trip mismatch\n";
        return 1;
    }

    bool rejected = false;
    try
    {
        static_cast<void>(codec.compress(input, 3));
    }
    catch (const std::length_error&)
    {
        rejected = true;
    }
    if (!rejected)
    {
        std::cerr << "Compression allocation bound was ignored\n";
        return 1;
    }

    rejected = false;
    try
    {
        static_cast<void>(codec.decompress(input, 3, 4));
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    if (!rejected)
    {
        std::cerr << "Decompression size contract was ignored\n";
        return 1;
    }

    rejected = false;
    try
    {
        static_cast<void>(CompressionCodecs::forAlgorithm(
            CompressionAlgorithm::lz4
        ));
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    if (!rejected)
    {
        std::cerr << "Unsupported compression codec was accepted\n";
        return 1;
    }

    std::cout << "CompressionCodec OK\n";
    return 0;
}
