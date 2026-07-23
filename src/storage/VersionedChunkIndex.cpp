#include "storage/VersionedChunkIndex.hpp"

#include "storage/ChunkCodec.hpp"
#include "checkpoint/CheckpointTypes.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>


namespace pi::storage
{

VersionedChunkIndex VersionedChunkIndex::create()
{
    return VersionedChunkIndex{std::map<ChunkIdentity, std::vector<ChunkVersionEntry>>{}};
}

VersionedChunkIndex::VersionedChunkIndex() = default;


void VersionedChunkIndex::addVersion(
    const ChunkIdentity& identity,
    std::uint16_t formatVersion,
    std::uint32_t checksumValue,
    std::uint64_t storedSize,
    std::uint64_t timestampNs
)
{
    auto& versions = versions_[identity];
    
    // Maintain sorted order by timestamp
    versions.push_back(ChunkVersionEntry::create(
        formatVersion,
        checksumValue,
        storedSize,
        timestampNs
    ));
    
    std::sort(versions.begin(), versions.end(),
        [](const ChunkVersionEntry& a, const ChunkVersionEntry& b)
    {
        return a.timestampNs < b.timestampNs;
    });
}


std::vector<ChunkVersionEntry> VersionedChunkIndex::getAllVersions(
    const ChunkIdentity& identity
) const
{
    auto it = versions_.find(identity);
    if (it == versions_.end())
    {
        return {};
    }
    return it->second;
}


std::optional<ChunkVersionEntry> VersionedChunkIndex::getLatestVersion(
    const ChunkIdentity& identity
) const
{
    auto it = versions_.find(identity);
    if (it == versions_.end())
    {
        return std::nullopt;
    }
    
    const auto& versions = it->second;
    if (versions.empty())
    {
        return std::nullopt;
    }
    
    return versions.back(); // Latest is at the end due to sorting
}


std::optional<ChunkVersionEntry> VersionedChunkIndex::getLatestVersionWithFormat(
    const ChunkIdentity& identity,
    std::uint16_t formatVersion
) const
{
    auto it = versions_.find(identity);
    if (it == versions_.end())
    {
        return std::nullopt;
    }
    
    const auto& versions = it->second;
    if (versions.empty())
    {
        return std::nullopt;
    }
    
    // Find the latest version with matching format version
    const auto itLatest = std::find_if(
        versions.rbegin(),
        versions.rend(),
        [formatVersion](const ChunkVersionEntry& entry)
    {
        return entry.formatVersion == formatVersion;
    });

    if (itLatest == versions.rend())
    {
        return std::nullopt;
    }
    
    return *itLatest;
}


bool VersionedChunkIndex::hasVersions(const ChunkIdentity& identity) const
{
    return versions_.find(identity) != versions_.end() &&
           !versions_.at(identity).empty();
}


std::size_t VersionedChunkIndex::getVersionCount(
    const ChunkIdentity& identity
) const
{
    auto it = versions_.find(identity);
    if (it == versions_.end())
    {
        return 0;
    }
    return it->second.size();
}


std::vector<ChunkIdentity> VersionedChunkIndex::getAllIdentities() const
{
    std::vector<ChunkIdentity> identities;
    identities.reserve(versions_.size());
    for (const auto& [identity, versions] : versions_)
    {
        identities.push_back(identity);
    }
    return identities;
}


std::size_t VersionedChunkIndex::identityCount() const
{
    return versions_.size();
}


std::size_t VersionedChunkIndex::totalVersionCount() const
{
    std::size_t total = 0;
    for (const auto& [identity, versions] : versions_)
    {
        total += versions.size();
    }
    return total;
}


void VersionedChunkIndex::clear()
{
    versions_.clear();
}


std::vector<std::uint8_t> VersionedChunkIndex::serialize() const
{
    return serializeInternal();
}


VersionedChunkIndex VersionedChunkIndex::deserialize(std::span<const std::uint8_t> bytes)
{
    VersionedChunkIndex index;
    index.deserializeInternal(bytes);
    return index;
}


std::vector<std::uint8_t> VersionedChunkIndex::serializeInternal() const
{
    std::vector<std::uint8_t> data;
    
    // Magic number
    data.push_back(magicNumberByte1);
    data.push_back(magicNumberByte2);
    data.push_back(magicNumberByte3);
    
    // Format version
    data.push_back(static_cast<std::uint8_t>(version));
    
    // Number of identities
    auto identities = getAllIdentities();
    std::size_t identityCount = identities.size();
    data.push_back(static_cast<std::uint8_t>(identityCount & 0xFF));
    data.push_back(static_cast<std::uint8_t>((identityCount >> 8) & 0xFF));
    data.push_back(static_cast<std::uint8_t>((identityCount >> 16) & 0xFF));
    data.push_back(static_cast<std::uint8_t>((identityCount >> 24) & 0xFF));
    
    // Number of total versions
    auto totalVersions = totalVersionCount();
    data.push_back(static_cast<std::uint8_t>(totalVersions & 0xFF));
    data.push_back(static_cast<std::uint8_t>((totalVersions >> 8) & 0xFF));
    data.push_back(static_cast<std::uint8_t>((totalVersions >> 16) & 0xFF));
    data.push_back(static_cast<std::uint8_t>((totalVersions >> 24) & 0xFF));
    
    // Write each identity (in sorted order for deterministic serialization)
    for (const auto& identity : identities)
    {
        // Fixed identity encoding:
        // algorithm(1), arithmetic(2), formula(2), requested(8), guard(4),
        // working(8), terms(8), level(4), start(8), end(8) = 53 bytes.
        std::vector<std::uint8_t> identityBytes(53, 0);
        
        // ComputationIdentity serialization (18 bytes)
        identityBytes[0] = static_cast<std::uint8_t>(identity.computation.algorithm);
        identityBytes[1] = static_cast<std::uint8_t>(identity.computation.arithmeticVersion & 0xFF);
        identityBytes[2] = static_cast<std::uint8_t>((identity.computation.arithmeticVersion >> 8) & 0xFF);
        identityBytes[3] = static_cast<std::uint8_t>(identity.computation.formulaVersion & 0xFF);
        identityBytes[4] = static_cast<std::uint8_t>((identity.computation.formulaVersion >> 8) & 0xFF);
        identityBytes[5] = static_cast<std::uint8_t>(identity.computation.requestedDigits & 0xFF);
        identityBytes[6] = static_cast<std::uint8_t>((identity.computation.requestedDigits >> 8) & 0xFF);
        identityBytes[7] = static_cast<std::uint8_t>((identity.computation.requestedDigits >> 16) & 0xFF);
        identityBytes[8] = static_cast<std::uint8_t>((identity.computation.requestedDigits >> 24) & 0xFF);
        identityBytes[9] = static_cast<std::uint8_t>((identity.computation.requestedDigits >> 32) & 0xFF);
        identityBytes[10] = static_cast<std::uint8_t>((identity.computation.requestedDigits >> 40) & 0xFF);
        identityBytes[11] = static_cast<std::uint8_t>((identity.computation.requestedDigits >> 48) & 0xFF);
        identityBytes[12] = static_cast<std::uint8_t>((identity.computation.requestedDigits >> 56) & 0xFF);
        identityBytes[13] = static_cast<std::uint8_t>(identity.computation.guardDigits & 0xFF);
        identityBytes[14] = static_cast<std::uint8_t>((identity.computation.guardDigits >> 8) & 0xFF);
        identityBytes[15] = static_cast<std::uint8_t>((identity.computation.guardDigits >> 16) & 0xFF);
        identityBytes[16] = static_cast<std::uint8_t>((identity.computation.guardDigits >> 24) & 0xFF);
        identityBytes[17] = static_cast<std::uint8_t>(identity.computation.workingDigits & 0xFF);
        identityBytes[18] = static_cast<std::uint8_t>((identity.computation.workingDigits >> 8) & 0xFF);
        identityBytes[19] = static_cast<std::uint8_t>((identity.computation.workingDigits >> 16) & 0xFF);
        identityBytes[20] = static_cast<std::uint8_t>((identity.computation.workingDigits >> 24) & 0xFF);
        identityBytes[21] = static_cast<std::uint8_t>((identity.computation.workingDigits >> 32) & 0xFF);
        identityBytes[22] = static_cast<std::uint8_t>((identity.computation.workingDigits >> 40) & 0xFF);
        identityBytes[23] = static_cast<std::uint8_t>((identity.computation.workingDigits >> 48) & 0xFF);
        identityBytes[24] = static_cast<std::uint8_t>((identity.computation.workingDigits >> 56) & 0xFF);
        identityBytes[25] = static_cast<std::uint8_t>(identity.computation.termCount & 0xFF);
        identityBytes[26] = static_cast<std::uint8_t>((identity.computation.termCount >> 8) & 0xFF);
        identityBytes[27] = static_cast<std::uint8_t>((identity.computation.termCount >> 16) & 0xFF);
        identityBytes[28] = static_cast<std::uint8_t>((identity.computation.termCount >> 24) & 0xFF);
        identityBytes[29] = static_cast<std::uint8_t>((identity.computation.termCount >> 32) & 0xFF);
        identityBytes[30] = static_cast<std::uint8_t>((identity.computation.termCount >> 40) & 0xFF);
        identityBytes[31] = static_cast<std::uint8_t>((identity.computation.termCount >> 48) & 0xFF);
        identityBytes[32] = static_cast<std::uint8_t>((identity.computation.termCount >> 56) & 0xFF);
        
        // BlockLocation serialization (20 bytes)
        for (std::size_t shift = 0; shift < 32; shift += 8)
            identityBytes[33 + shift / 8] = static_cast<std::uint8_t>(identity.location.treeLevel >> shift);
        for (std::size_t shift = 0; shift < 64; shift += 8) {
            identityBytes[37 + shift / 8] = static_cast<std::uint8_t>(identity.location.start >> shift);
            identityBytes[45 + shift / 8] = static_cast<std::uint8_t>(identity.location.end >> shift);
        }
        
        // Write identity bytes length prefix (4 bytes, big-endian)
        auto identityBytesLen = identityBytes.size();
        data.push_back(static_cast<std::uint8_t>(identityBytesLen & 0xFF));
        data.push_back(static_cast<std::uint8_t>((identityBytesLen >> 8) & 0xFF));
        data.push_back(static_cast<std::uint8_t>((identityBytesLen >> 16) & 0xFF));
        data.push_back(static_cast<std::uint8_t>((identityBytesLen >> 24) & 0xFF));
        
        // Write identity bytes
        data.insert(data.end(), identityBytes.begin(), identityBytes.end());
        
        // Write version count for this identity (2 bytes, big-endian)
        auto versionCount = getVersionCount(identity);
        data.push_back(static_cast<std::uint8_t>(versionCount & 0xFF));
        data.push_back(static_cast<std::uint8_t>((versionCount >> 8) & 0xFF));
        
        // Write each version
        for (const auto& version : versions_.at(identity))
        {
            // formatVersion (2 bytes, big-endian)
            data.push_back(static_cast<std::uint8_t>(version.formatVersion & 0xFF));
            data.push_back(static_cast<std::uint8_t>((version.formatVersion >> 8) & 0xFF));
            
            // checksumValue (4 bytes, big-endian)
            data.push_back(static_cast<std::uint8_t>(version.checksumValue & 0xFF));
            data.push_back(static_cast<std::uint8_t>((version.checksumValue >> 8) & 0xFF));
            data.push_back(static_cast<std::uint8_t>((version.checksumValue >> 16) & 0xFF));
            data.push_back(static_cast<std::uint8_t>((version.checksumValue >> 24) & 0xFF));
            
            // storedSize (8 bytes, big-endian)
            data.push_back(static_cast<std::uint8_t>(version.storedSize & 0xFF));
            data.push_back(static_cast<std::uint8_t>((version.storedSize >> 8) & 0xFF));
            data.push_back(static_cast<std::uint8_t>((version.storedSize >> 16) & 0xFF));
            data.push_back(static_cast<std::uint8_t>((version.storedSize >> 24) & 0xFF));
            data.push_back(static_cast<std::uint8_t>((version.storedSize >> 32) & 0xFF));
            data.push_back(static_cast<std::uint8_t>((version.storedSize >> 40) & 0xFF));
            data.push_back(static_cast<std::uint8_t>((version.storedSize >> 48) & 0xFF));
            data.push_back(static_cast<std::uint8_t>((version.storedSize >> 56) & 0xFF));
            
            // timestampNs (8 bytes, big-endian)
            data.push_back(static_cast<std::uint8_t>(version.timestampNs & 0xFF));
            data.push_back(static_cast<std::uint8_t>((version.timestampNs >> 8) & 0xFF));
            data.push_back(static_cast<std::uint8_t>((version.timestampNs >> 16) & 0xFF));
            data.push_back(static_cast<std::uint8_t>((version.timestampNs >> 24) & 0xFF));
            data.push_back(static_cast<std::uint8_t>((version.timestampNs >> 32) & 0xFF));
            data.push_back(static_cast<std::uint8_t>((version.timestampNs >> 40) & 0xFF));
            data.push_back(static_cast<std::uint8_t>((version.timestampNs >> 48) & 0xFF));
            data.push_back(static_cast<std::uint8_t>((version.timestampNs >> 56) & 0xFF));
        }
    }
    
    return data;
}


void VersionedChunkIndex::deserializeInternal(std::span<const std::uint8_t> bytes)
{
    // Validate magic number
    if (bytes.size() < 11)
    {
        throw std::invalid_argument("VersionedChunkIndex: insufficient data for magic number");
    }
    
    if (bytes[0] != magicNumberByte1 || bytes[1] != magicNumberByte2 || bytes[2] != magicNumberByte3)
    {
        throw std::invalid_argument(
            "VersionedChunkIndex: invalid magic number " +
            std::to_string(bytes[0]) + " " + std::to_string(bytes[1]) + " " + std::to_string(bytes[2])
        );
    }
    
    // Read format version (just validate for now)
    if (bytes[3] != static_cast<std::uint8_t>(version))
    {
        throw std::invalid_argument(
            "VersionedChunkIndex: unsupported format version " +
            std::to_string(bytes[3])
        );
    }
    
    // Read number of identities (4 bytes, big-endian)
    auto identityCount = static_cast<std::size_t>(bytes[4]) |
                         (static_cast<std::size_t>(bytes[5]) << 8) |
                         (static_cast<std::size_t>(bytes[6]) << 16) |
                         (static_cast<std::size_t>(bytes[7]) << 24);
    
    // Read number of total versions (4 bytes, big-endian) - skip for now
    (void)bytes[11]; // Skip format version bytes
    (void)bytes[8];
    (void)bytes[9];
    (void)bytes[10];
    (void)bytes[11];
    
    std::size_t offset = 12; // Skip magic, version, and counts
    
    // Parse each identity and its versions
    for (std::size_t i = 0; i < identityCount; ++i)
    {
        // Read identity bytes length (4 bytes, big-endian)
        if (offset + 4 > bytes.size())
        {
            throw std::invalid_argument(
                "VersionedChunkIndex: insufficient data for identity bytes length"
            );
        }
        
        auto identityBytesLen = static_cast<std::size_t>(bytes[offset]) |
                                (static_cast<std::size_t>(bytes[offset + 1]) << 8) |
                                (static_cast<std::size_t>(bytes[offset + 2]) << 16) |
                                (static_cast<std::size_t>(bytes[offset + 3]) << 24);
        offset += 4;
        
        if (identityBytesLen != 53)
        {
            throw std::invalid_argument(
                "VersionedChunkIndex: unexpected identity bytes length " +
                std::to_string(identityBytesLen)
            );
        }
        
        if (offset + identityBytesLen > bytes.size())
        {
            throw std::invalid_argument(
                "VersionedChunkIndex: insufficient data for identity bytes"
            );
        }
        
        // Parse ChunkIdentity from bytes
        std::vector<std::uint8_t> identityBytes(
            bytes.begin() + offset,
            bytes.begin() + offset + identityBytesLen
        );
        offset += identityBytesLen;
        
        ChunkIdentity identity;
        identity.computation.algorithm =
            static_cast<checkpoint::AlgorithmId>(identityBytes[0]);
        identity.computation.arithmeticVersion =
            static_cast<std::uint16_t>(identityBytes[1] | (identityBytes[2] << 8));
        identity.computation.formulaVersion =
            static_cast<std::uint16_t>(identityBytes[3] | (identityBytes[4] << 8));
        identity.computation.requestedDigits =
            static_cast<std::uint64_t>(identityBytes[5]) |
            (static_cast<std::uint64_t>(identityBytes[6]) << 8) |
            (static_cast<std::uint64_t>(identityBytes[7]) << 16) |
            (static_cast<std::uint64_t>(identityBytes[8]) << 24) |
            (static_cast<std::uint64_t>(identityBytes[9]) << 32) |
            (static_cast<std::uint64_t>(identityBytes[10]) << 40) |
            (static_cast<std::uint64_t>(identityBytes[11]) << 48) |
            (static_cast<std::uint64_t>(identityBytes[12]) << 56);
        identity.computation.guardDigits =
            static_cast<std::uint32_t>(identityBytes[13]) |
            (static_cast<std::uint32_t>(identityBytes[14]) << 8) |
            (static_cast<std::uint32_t>(identityBytes[15]) << 16) |
            (static_cast<std::uint32_t>(identityBytes[16]) << 24);
        identity.computation.workingDigits =
            static_cast<std::uint64_t>(identityBytes[17]) |
            (static_cast<std::uint64_t>(identityBytes[18]) << 8) |
            (static_cast<std::uint64_t>(identityBytes[19]) << 16) |
            (static_cast<std::uint64_t>(identityBytes[20]) << 24) |
            (static_cast<std::uint64_t>(identityBytes[21]) << 32) |
            (static_cast<std::uint64_t>(identityBytes[22]) << 40) |
            (static_cast<std::uint64_t>(identityBytes[23]) << 48) |
            (static_cast<std::uint64_t>(identityBytes[24]) << 56);
        identity.computation.termCount = 0;
        for (std::size_t i = 0; i < 8; ++i)
            identity.computation.termCount |= static_cast<std::uint64_t>(identityBytes[25 + i]) << (8 * i);
        
        identity.location.treeLevel = 0;
        for (std::size_t i = 0; i < 4; ++i)
            identity.location.treeLevel |= static_cast<std::uint32_t>(identityBytes[33 + i]) << (8 * i);
        identity.location.start = 0;
        identity.location.end = 0;
        for (std::size_t i = 0; i < 8; ++i) {
            identity.location.start |= static_cast<std::uint64_t>(identityBytes[37 + i]) << (8 * i);
            identity.location.end |= static_cast<std::uint64_t>(identityBytes[45 + i]) << (8 * i);
        }
        
        identity.validate();
        
        // Read version count (2 bytes, big-endian)
        if (offset + 2 > bytes.size())
        {
            throw std::invalid_argument(
                "VersionedChunkIndex: insufficient data for version count"
            );
        }
        
        auto versionCount = static_cast<std::size_t>(bytes[offset]) |
                            (static_cast<std::size_t>(bytes[offset + 1]) << 8);
        offset += 2;
        
        // Read each version
        for (std::size_t j = 0; j < versionCount; ++j)
        {
            // formatVersion (2 bytes, big-endian)
            if (offset + 2 > bytes.size())
            {
                throw std::invalid_argument(
                    "VersionedChunkIndex: insufficient data for format version"
                );
            }
            
            auto formatVersion = static_cast<std::uint16_t>(bytes[offset]) |
                                 (static_cast<std::uint16_t>(bytes[offset + 1]) << 8);
            offset += 2;
            
            // checksumValue (4 bytes, big-endian)
            if (offset + 4 > bytes.size())
            {
                throw std::invalid_argument(
                    "VersionedChunkIndex: insufficient data for checksum value"
                );
            }
            
            auto checksumValue = static_cast<std::uint32_t>(bytes[offset]) |
                                 (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
                                 (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
                                 (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
            offset += 4;
            
            // storedSize (8 bytes, big-endian)
            if (offset + 8 > bytes.size())
            {
                throw std::invalid_argument(
                    "VersionedChunkIndex: insufficient data for stored size"
                );
            }
            
            auto storedSize = static_cast<std::uint64_t>(bytes[offset]) |
                              (static_cast<std::uint64_t>(bytes[offset + 1]) << 8) |
                              (static_cast<std::uint64_t>(bytes[offset + 2]) << 16) |
                              (static_cast<std::uint64_t>(bytes[offset + 3]) << 24) |
                              (static_cast<std::uint64_t>(bytes[offset + 4]) << 32) |
                              (static_cast<std::uint64_t>(bytes[offset + 5]) << 40) |
                              (static_cast<std::uint64_t>(bytes[offset + 6]) << 48) |
                              (static_cast<std::uint64_t>(bytes[offset + 7]) << 56);
            offset += 8;
            
            // timestampNs (8 bytes, big-endian)
            if (offset + 8 > bytes.size())
            {
                throw std::invalid_argument(
                    "VersionedChunkIndex: insufficient data for timestamp"
                );
            }
            
            auto timestampNs = static_cast<std::uint64_t>(bytes[offset]) |
                               (static_cast<std::uint64_t>(bytes[offset + 1]) << 8) |
                               (static_cast<std::uint64_t>(bytes[offset + 2]) << 16) |
                               (static_cast<std::uint64_t>(bytes[offset + 3]) << 24) |
                               (static_cast<std::uint64_t>(bytes[offset + 4]) << 32) |
                               (static_cast<std::uint64_t>(bytes[offset + 5]) << 40) |
                               (static_cast<std::uint64_t>(bytes[offset + 6]) << 48) |
                               (static_cast<std::uint64_t>(bytes[offset + 7]) << 56);
            offset += 8;
            
            addVersion(
                identity,
                formatVersion,
                checksumValue,
                storedSize,
                timestampNs
            );
        }
    }
    
    // Validate offset matches expected size
    if (offset != bytes.size())
    {
        throw std::invalid_argument(
            "VersionedChunkIndex: incomplete data (expected " +
            std::to_string(bytes.size()) + " bytes, read " +
            std::to_string(offset) + " bytes)"
        );
    }
}

} // namespace pi::storage
