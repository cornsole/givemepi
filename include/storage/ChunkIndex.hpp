#pragma once

#include "storage/ChunkTypes.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace pi::storage
{

struct ChunkIndexEntry
{
    ChunkIdentity identity;
    std::string storageFile;
    std::uint64_t uncompressedSize = 0;
    std::uint64_t storedSize = 0;
    ChunkChecksumAlgorithm checksumAlgorithm = ChunkChecksumAlgorithm::crc32c;
    std::uint32_t checksumValue = 0;
    std::uint16_t formatVersion = chunkFormatVersion;

    [[nodiscard]] bool operator==(const ChunkIndexEntry&) const noexcept = default;
};

class ChunkIndex
{
public:
    static constexpr std::uint16_t formatVersion = 1;

    static ChunkIndex create();

    // Adds one identity. Duplicate identities and overlapping ranges at the
    // same computation/tree level are rejected.
    void add(const ChunkIndexEntry& entry);
    void add(const ChunkMetadata& metadata);
    void remove(const ChunkIdentity& identity);

    [[nodiscard]] bool contains(const ChunkIdentity& identity) const noexcept;
    [[nodiscard]] const ChunkIndexEntry& at(const ChunkIdentity& identity) const;
    [[nodiscard]] std::vector<ChunkIndexEntry> entries() const;
    [[nodiscard]] std::uint64_t storedBytes() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;

    [[nodiscard]] std::vector<std::uint8_t> serialize() const;
    static ChunkIndex deserialize(std::span<const std::uint8_t> bytes);

    void save(const std::filesystem::path& path) const;
    static ChunkIndex load(const std::filesystem::path& path);

private:
    std::map<ChunkIdentity, ChunkIndexEntry> entries_;
};

} // namespace pi::storage
