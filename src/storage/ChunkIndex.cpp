#include "storage/ChunkIndex.hpp"

#include "checkpoint/AtomicFileCommit.hpp"
#include "checkpoint/CRC32C.hpp"

#include <fstream>
#include <array>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace pi::storage
{
namespace
{
constexpr std::array<std::uint8_t, 8> magic{'P','I','C','H','I','D','X',0};
constexpr std::size_t identitySize = 53;
constexpr std::size_t checksumSize = 4;

void u16(std::vector<std::uint8_t>& out, std::uint16_t value)
{ out.push_back(static_cast<std::uint8_t>(value >> 8)); out.push_back(static_cast<std::uint8_t>(value)); }
void u32(std::vector<std::uint8_t>& out, std::uint32_t value)
{ for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<std::uint8_t>(value >> shift)); }
void u64(std::vector<std::uint8_t>& out, std::uint64_t value)
{ for (int shift = 56; shift >= 0; shift -= 8) out.push_back(static_cast<std::uint8_t>(value >> shift)); }

std::uint16_t read16(std::span<const std::uint8_t> b, std::size_t& p)
{ if (p + 2 > b.size()) throw std::invalid_argument("truncated chunk index"); const auto hi = b[p++]; const auto lo = b[p++]; return static_cast<std::uint16_t>((hi << 8) | lo); }
std::uint32_t read32(std::span<const std::uint8_t> b, std::size_t& p)
{ if (p + 4 > b.size()) throw std::invalid_argument("truncated chunk index"); std::uint32_t v=0; for(int i=0;i<4;++i)v=(v<<8)|b[p++]; return v; }
std::uint64_t read64(std::span<const std::uint8_t> b, std::size_t& p)
{ if (p + 8 > b.size()) throw std::invalid_argument("truncated chunk index"); std::uint64_t v=0; for(int i=0;i<8;++i)v=(v<<8)|b[p++]; return v; }

void encodeIdentity(std::vector<std::uint8_t>& out, const ChunkIdentity& id)
{
    const auto& c = id.computation;
    out.push_back(static_cast<std::uint8_t>(c.algorithm));
    u16(out, c.arithmeticVersion); u16(out, c.formulaVersion);
    u64(out, c.requestedDigits); u32(out, c.guardDigits); u64(out, c.workingDigits); u64(out, c.termCount);
    u32(out, id.location.treeLevel); u64(out, id.location.start); u64(out, id.location.end);
}

ChunkIdentity decodeIdentity(std::span<const std::uint8_t> b, std::size_t& p)
{
    if (p + identitySize > b.size()) throw std::invalid_argument("truncated chunk identity");
    checkpoint::ComputationIdentity c{
        static_cast<checkpoint::AlgorithmId>(b[p++]), read16(b,p), read16(b,p), read64(b,p),
        read32(b,p), read64(b,p), read64(b,p)
    };
    const auto treeLevel = read32(b,p);
    const auto start = read64(b,p);
    const auto end = read64(b,p);
    checkpoint::BlockLocation location{start, end, static_cast<std::uint32_t>(treeLevel)};
    return ChunkIdentity::create(c, location);
}

bool overlaps(const ChunkIdentity& a, const ChunkIdentity& b)
{
    return a.computation == b.computation
        && a.location.treeLevel == b.location.treeLevel
        && a.location.start < b.location.end
        && b.location.start < a.location.end;
}
}

ChunkIndex ChunkIndex::create() { return {}; }

void ChunkIndex::add(const ChunkIndexEntry& entry)
{
    entry.identity.validate();
    if (entry.storageFile.empty() || entry.storageFile.find_first_of("/\\\0") != std::string::npos)
        throw std::invalid_argument("chunk index storage file must be a plain filename");
    if (entry.formatVersion != chunkFormatVersion
        || entry.checksumAlgorithm != ChunkChecksumAlgorithm::crc32c)
        throw std::invalid_argument("unsupported chunk index metadata");
    if (entry.uncompressedSize > maxChunkPayloadBytes || entry.storedSize > maxChunkPayloadBytes)
        throw std::invalid_argument("chunk index size exceeds limit");
    if (entries_.contains(entry.identity))
        throw std::invalid_argument("duplicate chunk identity");
    for (const auto& [identity, existing] : entries_)
        if (overlaps(identity, entry.identity))
            throw std::invalid_argument("overlapping chunk ranges");
    entries_.emplace(entry.identity, entry);
}

void ChunkIndex::add(const ChunkMetadata& metadata)
{
    add(ChunkIndexEntry{metadata.identity, metadata.identity.deterministicFilename(), metadata.uncompressedSize,
        metadata.storedSize, metadata.checksumAlgorithm, metadata.checksumValue, metadata.formatVersion});
}

void ChunkIndex::remove(const ChunkIdentity& identity) { entries_.erase(identity); }
bool ChunkIndex::contains(const ChunkIdentity& identity) const noexcept { return entries_.contains(identity); }
const ChunkIndexEntry& ChunkIndex::at(const ChunkIdentity& identity) const
{
    const auto it = entries_.find(identity);
    if (it == entries_.end()) throw std::out_of_range("chunk identity not found");
    return it->second;
}
std::vector<ChunkIndexEntry> ChunkIndex::entries() const
{
    std::vector<ChunkIndexEntry> result;
    for (const auto& [identity, entry] : entries_) { (void)identity; result.push_back(entry); }
    return result;
}
std::uint64_t ChunkIndex::storedBytes() const noexcept
{
    std::uint64_t total = 0;
    for (const auto& [identity, entry] : entries_) {
        (void)identity;
        if (std::numeric_limits<std::uint64_t>::max() - total < entry.storedSize) return std::numeric_limits<std::uint64_t>::max();
        total += entry.storedSize;
    }
    return total;
}
std::size_t ChunkIndex::size() const noexcept { return entries_.size(); }

std::vector<std::uint8_t> ChunkIndex::serialize() const
{
    if (entries_.size() > std::numeric_limits<std::uint32_t>::max()) throw std::length_error("too many chunk index entries");
    std::vector<std::uint8_t> out(magic.begin(), magic.end());
    u16(out, formatVersion); u32(out, static_cast<std::uint32_t>(entries_.size()));
    for (const auto& [identity, entry] : entries_) {
        (void)identity;
        encodeIdentity(out, entry.identity);
        if (entry.storageFile.size() > std::numeric_limits<std::uint32_t>::max()) throw std::length_error("chunk filename too long");
        u32(out, static_cast<std::uint32_t>(entry.storageFile.size()));
        out.insert(out.end(), entry.storageFile.begin(), entry.storageFile.end());
        u64(out, entry.uncompressedSize); u64(out, entry.storedSize);
        u16(out, static_cast<std::uint16_t>(entry.checksumAlgorithm)); u32(out, entry.checksumValue); u16(out, entry.formatVersion);
    }
    u32(out, checkpoint::CRC32C::calculate(out));
    return out;
}

ChunkIndex ChunkIndex::deserialize(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() < magic.size() + 2 + 4 + checksumSize) throw std::invalid_argument("truncated chunk index");
    const auto checksumOffset = bytes.size() - checksumSize;
    if (checkpoint::CRC32C::calculate(bytes.first(checksumOffset))
        != (static_cast<std::uint32_t>(bytes[checksumOffset]) << 24
            | static_cast<std::uint32_t>(bytes[checksumOffset+1]) << 16
            | static_cast<std::uint32_t>(bytes[checksumOffset+2]) << 8
            | bytes[checksumOffset+3])) throw std::invalid_argument("chunk index checksum mismatch");
    if (!std::equal(magic.begin(), magic.end(), bytes.begin())) throw std::invalid_argument("invalid chunk index magic");
    std::size_t p = magic.size();
    if (read16(bytes,p) != formatVersion) throw std::invalid_argument("unsupported chunk index version");
    const auto count = read32(bytes,p);
    ChunkIndex result;
    for (std::uint32_t i=0; i<count; ++i) {
        auto identity = decodeIdentity(bytes,p);
        const auto length = read32(bytes,p);
        if (length > bytes.size() - p - checksumSize) throw std::invalid_argument("invalid chunk filename length");
        std::string filename(reinterpret_cast<const char*>(bytes.data()+p), length); p += length;
        ChunkIndexEntry entry{identity, filename, read64(bytes,p), read64(bytes,p),
            static_cast<ChunkChecksumAlgorithm>(read16(bytes,p)), read32(bytes,p), read16(bytes,p)};
        result.add(entry);
    }
    if (p != checksumOffset) throw std::invalid_argument("trailing chunk index data");
    return result;
}

void ChunkIndex::save(const std::filesystem::path& path) const
{
    if (path.empty()) throw std::invalid_argument("chunk index path is empty");
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    const auto bytes = serialize();
    checkpoint::AtomicFileCommit::write(path, bytes);
}

ChunkIndex ChunkIndex::load(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open chunk index");
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    return deserialize(bytes);
}

} // namespace pi::storage
