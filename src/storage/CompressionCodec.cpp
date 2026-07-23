#include "storage/CompressionCodec.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>


namespace pi::storage
{

namespace
{

std::size_t checkedSize(std::uint64_t value, std::uint64_t limit)
{
    if (value > limit
        || (sizeof(std::size_t) < sizeof(std::uint64_t)
            && value > std::numeric_limits<std::size_t>::max()))
    {
        throw std::length_error("Compression output size is too large");
    }

    return static_cast<std::size_t>(value);
}


class NoneCompressionCodec final : public CompressionCodec
{
public:
    [[nodiscard]]
    CompressionAlgorithm algorithm() const noexcept override
    {
        return CompressionAlgorithm::none;
    }

    [[nodiscard]]
    std::vector<std::uint8_t> compress(
        std::span<const std::uint8_t> input,
        std::uint64_t maxOutputSize
    ) const override
    {
        static_cast<void>(checkedSize(
            input.size(),
            maxOutputSize
        ));
        return std::vector<std::uint8_t>(input.begin(), input.end());
    }

    [[nodiscard]]
    std::vector<std::uint8_t> decompress(
        std::span<const std::uint8_t> input,
        std::uint64_t expectedOutputSize,
        std::uint64_t maxOutputSize
    ) const override
    {
        const std::size_t outputSize = checkedSize(
            expectedOutputSize,
            maxOutputSize
        );
        if (input.size() != outputSize)
        {
            throw std::invalid_argument(
                "Uncompressed payload size does not match metadata"
            );
        }

        return std::vector<std::uint8_t>(input.begin(), input.end());
    }
};

} // namespace


const CompressionCodec& CompressionCodecs::forAlgorithm(
    CompressionAlgorithm algorithm
)
{
    static const NoneCompressionCodec noneCodec;

    if (algorithm == CompressionAlgorithm::none)
    {
        return noneCodec;
    }

    throw std::invalid_argument(
        "Compression codec is not implemented: "
        + std::string(
            algorithm == CompressionAlgorithm::lz4 ? "lz4" : "unknown"
        )
    );
}

} // namespace pi::storage
