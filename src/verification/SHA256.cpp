#include "verification/SHA256.hpp"

#include <algorithm>
#include <bit>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>


namespace pi::verification
{
namespace
{

constexpr std::array<std::uint32_t, 64> ROUND_CONSTANTS{
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};


std::uint32_t loadBigEndian(const std::uint8_t* bytes) noexcept
{
    return (static_cast<std::uint32_t>(bytes[0]) << 24)
        | (static_cast<std::uint32_t>(bytes[1]) << 16)
        | (static_cast<std::uint32_t>(bytes[2]) << 8)
        | static_cast<std::uint32_t>(bytes[3]);
}

} // namespace


SHA256::SHA256() noexcept
    : state_{
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U
    }
{
}


void SHA256::update(std::span<const std::uint8_t> bytes)
{
    if (finalized_)
    {
        throw std::logic_error("Cannot update a finalized SHA-256 context");
    }
    constexpr std::uint64_t maximumBytes =
        std::numeric_limits<std::uint64_t>::max() / 8;
    if (bytes.size() > maximumBytes - totalBytes_)
    {
        throw std::length_error("SHA-256 input bit length overflows uint64");
    }
    totalBytes_ += static_cast<std::uint64_t>(bytes.size());

    std::size_t offset = 0;
    if (bufferSize_ != 0)
    {
        const std::size_t copied = std::min(
            bytes.size(),
            buffer_.size() - bufferSize_
        );
        if (copied != 0U)
        {
            std::memcpy(buffer_.data() + bufferSize_, bytes.data(), copied);
        }
        bufferSize_ += copied;
        offset += copied;
        if (bufferSize_ == buffer_.size())
        {
            transform(buffer_.data());
            bufferSize_ = 0;
        }
    }

    while (bytes.size() - offset >= buffer_.size())
    {
        transform(bytes.data() + offset);
        offset += buffer_.size();
    }
    if (offset < bytes.size())
    {
        bufferSize_ = bytes.size() - offset;
        std::memcpy(buffer_.data(), bytes.data() + offset, bufferSize_);
    }
}


void SHA256::update(std::string_view bytes)
{
    update(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(bytes.data()),
        bytes.size()
    ));
}


SHA256Digest SHA256::finalize()
{
    if (finalized_)
    {
        return digest_;
    }

    const std::uint64_t bitLength = totalBytes_ * 8;
    buffer_[bufferSize_++] = 0x80;
    if (bufferSize_ > 56)
    {
        std::fill(buffer_.begin() + bufferSize_, buffer_.end(), 0);
        transform(buffer_.data());
        bufferSize_ = 0;
    }
    std::fill(buffer_.begin() + bufferSize_, buffer_.begin() + 56, 0);
    for (std::size_t index = 0; index < 8; ++index)
    {
        buffer_[63 - index] = static_cast<std::uint8_t>(
            bitLength >> (index * 8)
        );
    }
    transform(buffer_.data());
    bufferSize_ = 0;

    for (std::size_t word = 0; word < state_.size(); ++word)
    {
        for (std::size_t byte = 0; byte < 4; ++byte)
        {
            digest_[word * 4 + byte] = static_cast<std::uint8_t>(
                state_[word] >> (24 - byte * 8)
            );
        }
    }
    finalized_ = true;
    return digest_;
}


bool SHA256::finalized() const noexcept
{
    return finalized_;
}


SHA256Digest SHA256::hash(std::string_view bytes)
{
    SHA256 context;
    context.update(bytes);
    return context.finalize();
}


SHA256Digest SHA256::hashFilePrefix(
    const std::filesystem::path& path,
    std::uint64_t byteCount
)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("Cannot open file for SHA-256 hashing");
    }

    SHA256 context;
    std::array<std::uint8_t, 64 * 1024> buffer{};
    std::uint64_t remaining = byteCount;
    while (remaining != 0)
    {
        const std::size_t requested = static_cast<std::size_t>(std::min<
            std::uint64_t
        >(remaining, buffer.size()));
        input.read(
            reinterpret_cast<char*>(buffer.data()),
            static_cast<std::streamsize>(requested)
        );
        const std::streamsize count = input.gcount();
        if (count <= 0)
        {
            throw std::runtime_error(
                "File ended before the SHA-256 prefix was complete"
            );
        }
        context.update(std::span<const std::uint8_t>(
            buffer.data(),
            static_cast<std::size_t>(count)
        ));
        remaining -= static_cast<std::uint64_t>(count);
        if (static_cast<std::size_t>(count) != requested && remaining != 0)
        {
            throw std::runtime_error(
                "Failed while reading SHA-256 file prefix"
            );
        }
    }
    return context.finalize();
}


void SHA256::transform(const std::uint8_t* block) noexcept
{
    std::array<std::uint32_t, 64> schedule{};
    for (std::size_t index = 0; index < 16; ++index)
    {
        schedule[index] = loadBigEndian(block + index * 4);
    }
    for (std::size_t index = 16; index < schedule.size(); ++index)
    {
        const std::uint32_t s0 = std::rotr(schedule[index - 15], 7)
            ^ std::rotr(schedule[index - 15], 18)
            ^ (schedule[index - 15] >> 3);
        const std::uint32_t s1 = std::rotr(schedule[index - 2], 17)
            ^ std::rotr(schedule[index - 2], 19)
            ^ (schedule[index - 2] >> 10);
        schedule[index] = schedule[index - 16] + s0
            + schedule[index - 7] + s1;
    }

    std::uint32_t a = state_[0];
    std::uint32_t b = state_[1];
    std::uint32_t c = state_[2];
    std::uint32_t d = state_[3];
    std::uint32_t e = state_[4];
    std::uint32_t f = state_[5];
    std::uint32_t g = state_[6];
    std::uint32_t h = state_[7];

    for (std::size_t index = 0; index < schedule.size(); ++index)
    {
        const std::uint32_t sum1 = std::rotr(e, 6) ^ std::rotr(e, 11)
            ^ std::rotr(e, 25);
        const std::uint32_t choice = (e & f) ^ (~e & g);
        const std::uint32_t temporary1 = h + sum1 + choice
            + ROUND_CONSTANTS[index] + schedule[index];
        const std::uint32_t sum0 = std::rotr(a, 2) ^ std::rotr(a, 13)
            ^ std::rotr(a, 22);
        const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t temporary2 = sum0 + majority;

        h = g;
        g = f;
        f = e;
        e = d + temporary1;
        d = c;
        c = b;
        b = a;
        a = temporary1 + temporary2;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}


std::string toHex(const SHA256Digest& digest)
{
    constexpr char hex[] = "0123456789abcdef";
    std::string encoded(digest.size() * 2, '0');
    for (std::size_t index = 0; index < digest.size(); ++index)
    {
        encoded[index * 2] = hex[digest[index] >> 4];
        encoded[index * 2 + 1] = hex[digest[index] & 0x0f];
    }
    return encoded;
}

} // namespace pi::verification
