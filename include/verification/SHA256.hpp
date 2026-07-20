#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>


namespace pi::verification
{

using SHA256Digest = std::array<std::uint8_t, 32>;


/// Incremental FIPS 180-4 SHA-256 context.
class SHA256
{
public:
    SHA256() noexcept;

    /// Adds bytes to the digest state.
    ///
    /// Throws std::logic_error after finalization and std::length_error if the
    /// SHA-256 64-bit bit-length field would overflow. Time complexity: O(n).
    /// Additional memory complexity: O(1).
    void update(std::span<const std::uint8_t> bytes);

    /// Adds the exact bytes of a string view. Time O(n), memory O(1).
    void update(std::string_view bytes);

    /// Finalizes and returns the digest. Repeated calls return the same digest.
    ///
    /// Time and additional memory complexity: O(1).
    [[nodiscard]] SHA256Digest finalize();

    [[nodiscard]] bool finalized() const noexcept;

    /// Hashes one memory range in O(n) time and O(1) additional memory.
    [[nodiscard]] static SHA256Digest hash(std::string_view bytes);

    /// Hashes exactly byteCount bytes from the beginning of a file.
    ///
    /// The file is read in fixed-size chunks. Throws on open/read failure or if
    /// the file ends before byteCount. Time O(byteCount), memory O(1).
    [[nodiscard]]
    static SHA256Digest hashFilePrefix(
        const std::filesystem::path& path,
        std::uint64_t byteCount
    );

private:
    void transform(const std::uint8_t* block) noexcept;

    std::array<std::uint32_t, 8> state_;
    std::array<std::uint8_t, 64> buffer_{};
    std::size_t bufferSize_ = 0;
    std::uint64_t totalBytes_ = 0;
    bool finalized_ = false;
    SHA256Digest digest_{};
};


/// Encodes a digest as 64 lowercase hexadecimal characters.
/// Time complexity: O(1). Memory complexity: O(1).
[[nodiscard]] std::string toHex(const SHA256Digest& digest);

} // namespace pi::verification
