#pragma once

#include "storage/ChunkTypes.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <span>


namespace pi::storage
{

/// @brief Entry representing a specific version of a chunk
struct ChunkVersionEntry
{
    std::uint16_t formatVersion;      /// Format version of this chunk
    std::uint32_t checksumValue;      /// CRC32C checksum
    std::uint64_t storedSize;         /// Compressed size in bytes
    std::uint64_t timestampNs;        /// Timestamp in nanoseconds

    [[nodiscard]]
    static ChunkVersionEntry create(
        std::uint16_t formatVersion,
        std::uint32_t checksumValue,
        std::uint64_t storedSize,
        std::uint64_t timestampNs
    )
    {
        return ChunkVersionEntry{
            formatVersion,
            checksumValue,
            storedSize,
            timestampNs
        };
    }

    [[nodiscard]]
    bool operator==(const ChunkVersionEntry&) const noexcept = default;
};

/// @brief Index tracking all versions of chunks for a given identity
class VersionedChunkIndex
{
public:
    /// @brief Create a new empty versioned chunk index
    [[nodiscard]]
    static VersionedChunkIndex create();

    /// @brief Add a new chunk version to the index
    /// @param identity Chunk identity (computation + location)
    /// @param formatVersion Format version of the chunk
    /// @param checksumValue CRC32C checksum value
    /// @param storedSize Compressed size in bytes
    /// @param timestampNs Timestamp in nanoseconds
    void addVersion(
        const ChunkIdentity& identity,
        std::uint16_t formatVersion,
        std::uint32_t checksumValue,
        std::uint64_t storedSize,
        std::uint64_t timestampNs
    );

    /// @brief Get all versions of a chunk by identity
    /// @return Vector of ChunkVersionEntry sorted by timestamp (ascending)
    [[nodiscard]]
    std::vector<ChunkVersionEntry> getAllVersions(
        const ChunkIdentity& identity
    ) const;

    /// @brief Get the latest version of a chunk
    /// @param identity Chunk identity
    /// @return Latest version entry, or std::nullopt if not found
    [[nodiscard]]
    std::optional<ChunkVersionEntry> getLatestVersion(
        const ChunkIdentity& identity
    ) const;

    /// @brief Get the latest version with exact format version
    /// @param identity Chunk identity
    /// @param formatVersion Required format version
    /// @return Matching version entry, or std::nullopt if not found
    [[nodiscard]]
    std::optional<ChunkVersionEntry> getLatestVersionWithFormat(
        const ChunkIdentity& identity,
        std::uint16_t formatVersion
    ) const;

    /// @brief Check if any version of a chunk exists
    [[nodiscard]]
    bool hasVersions(const ChunkIdentity& identity) const;

    /// @brief Get the count of versions for a chunk
    [[nodiscard]]
    std::size_t getVersionCount(const ChunkIdentity& identity) const;

    /// @brief Get all unique chunk identities in the index
    [[nodiscard]]
    std::vector<ChunkIdentity> getAllIdentities() const;

    /// @brief Get total number of chunk identities in the index
    [[nodiscard]]
    std::size_t identityCount() const;

    /// @brief Get total number of versions in the index
    [[nodiscard]]
    std::size_t totalVersionCount() const;

    /// @brief Clear all entries from the index
    void clear();

    /// @brief Serialize the index to a vector of bytes
    [[nodiscard]]
    std::vector<std::uint8_t> serialize() const;

    /// @brief Deserialize the index from bytes
    static VersionedChunkIndex deserialize(std::span<const std::uint8_t> bytes);

private:
    /// Map from ChunkIdentity to list of versions (sorted by timestamp)
    std::map<ChunkIdentity, std::vector<ChunkVersionEntry>> versions_;

    /// Serialization helpers
    static constexpr std::uint8_t magicNumberByte1 = 0x56;
    static constexpr std::uint8_t magicNumberByte2 = 0x43;
    static constexpr std::uint8_t magicNumberByte3 = 0x49; // 'VCI'
    static constexpr std::uint16_t version = 1;

    /// Default constructor for empty index
    VersionedChunkIndex();

    /// Private constructor with initial map
    VersionedChunkIndex(std::map<ChunkIdentity, std::vector<ChunkVersionEntry>> versions)
        : versions_(std::move(versions)) {}

    /// Serialization internal method
    [[nodiscard]]
    std::vector<std::uint8_t> serializeInternal() const;

    /// Read bytes from span
    static std::vector<std::uint8_t> readBytes(
        std::span<const std::uint8_t> bytes,
        std::size_t count
    );

    /// Read uint64 from bytes (little-endian)
    static std::uint64_t readU64(std::span<const std::uint8_t> bytes);

    /// Read uint32 from bytes (little-endian)
    static std::uint32_t readU32(std::span<const std::uint8_t> bytes);

    /// Read uint16 from bytes (little-endian)
    static std::uint16_t readU16(std::span<const std::uint8_t> bytes);

    /// Deserialize internal method
    void deserializeInternal(std::span<const std::uint8_t> bytes);
};

} // namespace pi::storage
