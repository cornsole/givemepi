#include "checkpoint/CheckpointManifest.hpp"

#include "checkpoint/CRC32C.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <tuple>


namespace pi::checkpoint
{

namespace
{

constexpr std::array<std::uint8_t, 8> manifestMagic{
    'G', 'M', 'P', 'I', 'M', 'A', 'N', 0
};
constexpr std::size_t checksumOffset = 68;
constexpr std::size_t checksumSize = 4;
constexpr std::size_t fixedEntrySize = 48;


void appendU8(std::vector<std::uint8_t>& out, std::uint8_t value)
{
    out.push_back(value);
}

void appendU16(std::vector<std::uint8_t>& out, std::uint16_t value)
{
    out.push_back(static_cast<std::uint8_t>(value >> 8));
    out.push_back(static_cast<std::uint8_t>(value));
}

void appendU32(std::vector<std::uint8_t>& out, std::uint32_t value)
{
    for (int shift = 24; shift >= 0; shift -= 8)
    {
        out.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

void appendU64(std::vector<std::uint8_t>& out, std::uint64_t value)
{
    for (int shift = 56; shift >= 0; shift -= 8)
    {
        out.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

void storeU32(std::vector<std::uint8_t>& out, std::size_t offset,
              std::uint32_t value)
{
    for (int shift = 24; shift >= 0; shift -= 8)
    {
        out[offset++] = static_cast<std::uint8_t>(value >> shift);
    }
}

std::uint32_t manifestChecksum(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() < CheckpointManifestCodec::version1HeaderSize)
    {
        throw std::invalid_argument("Truncated checkpoint manifest");
    }

    std::uint32_t checksum = CRC32C::calculate(bytes.first(checksumOffset));
    constexpr std::array<std::uint8_t, checksumSize> zeros{};
    checksum = CRC32C::calculate(zeros, checksum);
    return CRC32C::calculate(
        bytes.subspan(checksumOffset + checksumSize), checksum
    );
}

void validateEntry(
    const ManifestEntry& entry,
    const ComputationIdentity& identity
)
{
    entry.location.validate(identity);

    if (entry.filename.empty()
        || entry.filename.find_first_of("/\\") != std::string::npos
        || entry.filename.find('\0') != std::string::npos)
    {
        throw std::invalid_argument("Manifest filename must be a plain filename");
    }

    if (entry.filename.size() > std::numeric_limits<std::uint32_t>::max())
    {
        throw std::length_error("Manifest filename is too long");
    }

    if (entry.checksumAlgorithm != ChecksumAlgorithm::crc32c)
    {
        throw std::invalid_argument("Manifest entry requires CRC32C metadata");
    }

    if (entry.completion != CompletionState::incomplete
        && entry.completion != CompletionState::complete)
    {
        throw std::invalid_argument("Unknown manifest completion state");
    }
}

auto entryKey(const ManifestEntry& entry)
{
    return std::tuple{
        entry.location.start,
        entry.location.end,
        entry.location.treeLevel
    };
}


class Reader
{
public:
    explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    std::span<const std::uint8_t> take(std::size_t count)
    {
        if (count > bytes_.size() - position_)
        {
            throw std::invalid_argument("Truncated checkpoint manifest");
        }
        const auto result = bytes_.subspan(position_, count);
        position_ += count;
        return result;
    }

    std::uint8_t u8() { return take(1).front(); }
    std::uint16_t u16()
    {
        const auto b = take(2);
        return static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(b[0]) << 8) | b[1]
        );
    }
    std::uint32_t u32()
    {
        std::uint32_t value = 0;
        for (const auto byte : take(4)) value = (value << 8) | byte;
        return value;
    }
    std::uint64_t u64()
    {
        std::uint64_t value = 0;
        for (const auto byte : take(8)) value = (value << 8) | byte;
        return value;
    }
    std::size_t remaining() const noexcept { return bytes_.size() - position_; }

private:
    std::span<const std::uint8_t> bytes_;
    std::size_t position_ = 0;
};

} // namespace


std::vector<std::uint8_t> CheckpointManifestCodec::encode(
    const CheckpointManifest& manifest
)
{
    manifest.identity.validate();
    std::vector<ManifestEntry> entries = manifest.entries;

    for (const auto& entry : entries) validateEntry(entry, manifest.identity);
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        return entryKey(a) < entryKey(b);
    });

    for (std::size_t index = 1; index < entries.size(); ++index)
    {
        if (entryKey(entries[index - 1]) == entryKey(entries[index]))
        {
            throw std::invalid_argument("Duplicate manifest block location");
        }
    }

    std::size_t totalSize = version1HeaderSize;
    for (const auto& entry : entries)
    {
        if (entry.filename.size()
            > std::numeric_limits<std::size_t>::max() - fixedEntrySize
            || fixedEntrySize + entry.filename.size()
                > std::numeric_limits<std::size_t>::max() - totalSize)
        {
            throw std::overflow_error("Checkpoint manifest size overflow");
        }
        totalSize += fixedEntrySize + entry.filename.size();
    }

    std::vector<std::uint8_t> output;
    output.reserve(totalSize);
    output.insert(output.end(), manifestMagic.begin(), manifestMagic.end());
    appendU16(output, formatVersion);
    appendU16(output, version1HeaderSize);
    appendU32(output, 0);
    appendU16(output, static_cast<std::uint16_t>(manifest.identity.algorithm));
    appendU16(output, manifest.identity.arithmeticVersion);
    appendU16(output, manifest.identity.formulaVersion);
    appendU16(output, 0);
    appendU64(output, manifest.identity.requestedDigits);
    appendU32(output, manifest.identity.guardDigits);
    appendU32(output, 0);
    appendU64(output, manifest.identity.workingDigits);
    appendU64(output, manifest.identity.termCount);
    appendU64(output, entries.size());
    appendU16(output, static_cast<std::uint16_t>(ChecksumAlgorithm::crc32c));
    appendU16(output, checksumSize);
    appendU32(output, 0);

    for (const auto& entry : entries)
    {
        appendU64(output, entry.location.start);
        appendU64(output, entry.location.end);
        appendU32(output, entry.location.treeLevel);
        appendU8(output, static_cast<std::uint8_t>(entry.completion));
        appendU8(output, 0); appendU16(output, 0);
        appendU64(output, entry.payloadSize);
        appendU16(output, static_cast<std::uint16_t>(entry.checksumAlgorithm));
        appendU16(output, checksumSize);
        appendU32(output, entry.checksum);
        appendU32(output, static_cast<std::uint32_t>(entry.filename.size()));
        appendU32(output, 0);
        output.insert(output.end(), entry.filename.begin(), entry.filename.end());
    }

    storeU32(output, checksumOffset, manifestChecksum(output));
    return output;
}


CheckpointManifest CheckpointManifestCodec::decode(
    std::span<const std::uint8_t> bytes
)
{
    Reader reader(bytes);
    const auto magic = reader.take(manifestMagic.size());
    if (!std::equal(magic.begin(), magic.end(), manifestMagic.begin()))
        throw std::invalid_argument("Invalid checkpoint manifest magic");
    if (reader.u16() != formatVersion || reader.u16() != version1HeaderSize)
        throw std::invalid_argument("Unsupported checkpoint manifest version");
    if (reader.u32() != 0) throw std::invalid_argument("Unsupported manifest flags");

    const AlgorithmId algorithm = static_cast<AlgorithmId>(reader.u16());
    const std::uint16_t arithmetic = reader.u16();
    const std::uint16_t formula = reader.u16();
    if (reader.u16() != 0) throw std::invalid_argument("Non-zero manifest reserved field");
    const std::uint64_t requested = reader.u64();
    const std::uint32_t guard = reader.u32();
    if (reader.u32() != 0) throw std::invalid_argument("Non-zero manifest reserved field");
    ComputationIdentity identity{
        algorithm, arithmetic, formula, requested, guard, reader.u64(), reader.u64()
    };
    identity.validate();
    const std::uint64_t entryCount = reader.u64();
    if (reader.u16() != static_cast<std::uint16_t>(ChecksumAlgorithm::crc32c)
        || reader.u16() != checksumSize)
        throw std::invalid_argument("Unsupported manifest checksum metadata");
    const std::uint32_t storedChecksum = reader.u32();
    if (storedChecksum != manifestChecksum(bytes))
        throw std::invalid_argument("Checkpoint manifest CRC32C mismatch");

    if (entryCount > reader.remaining() / fixedEntrySize
        || entryCount > std::numeric_limits<std::size_t>::max())
        throw std::invalid_argument("Invalid checkpoint manifest entry count");

    std::vector<ManifestEntry> entries;
    entries.reserve(static_cast<std::size_t>(entryCount));
    for (std::uint64_t index = 0; index < entryCount; ++index)
    {
        BlockLocation location{reader.u64(), reader.u64(), reader.u32()};
        const CompletionState completion = static_cast<CompletionState>(reader.u8());
        if (reader.u8() != 0 || reader.u16() != 0)
            throw std::invalid_argument("Non-zero manifest reserved field");
        const std::uint64_t payloadSize = reader.u64();
        const auto algorithmId = static_cast<ChecksumAlgorithm>(reader.u16());
        if (reader.u16() != checksumSize)
            throw std::invalid_argument("Invalid entry checksum length");
        const std::uint32_t checksum = reader.u32();
        const std::uint32_t filenameLength = reader.u32();
        if (reader.u32() != 0) throw std::invalid_argument("Non-zero manifest reserved field");
        const auto filenameBytes = reader.take(filenameLength);
        ManifestEntry entry{
            location,
            std::string(filenameBytes.begin(), filenameBytes.end()),
            payloadSize, algorithmId, checksum, completion
        };
        validateEntry(entry, identity);
        if (!entries.empty() && entryKey(entries.back()) >= entryKey(entry))
            throw std::invalid_argument("Manifest entries are not canonically sorted");
        entries.push_back(std::move(entry));
    }
    if (reader.remaining() != 0)
        throw std::invalid_argument("Trailing checkpoint manifest bytes");
    return CheckpointManifest{identity, std::move(entries)};
}

} // namespace pi::checkpoint
