#pragma once

#include "checkpoint/CheckpointBlockCodec.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>


namespace pi::checkpoint
{

enum class CompletionState : std::uint8_t
{
    incomplete = 0,
    complete = 1
};


struct ManifestEntry
{
    BlockLocation location;
    std::string filename;
    std::uint64_t payloadSize;
    ChecksumAlgorithm checksumAlgorithm;
    std::uint32_t checksum;
    CompletionState completion;

    [[nodiscard]]
    bool operator==(const ManifestEntry&) const noexcept = default;
};


struct CheckpointManifest
{
    ComputationIdentity identity;
    std::vector<ManifestEntry> entries;

    [[nodiscard]]
    bool operator==(const CheckpointManifest&) const noexcept = default;
};


class CheckpointManifestCodec
{
public:
    static constexpr std::uint16_t formatVersion = 1;
    static constexpr std::uint16_t version1HeaderSize = 72;

    [[nodiscard]]
    static std::vector<std::uint8_t> encode(
        const CheckpointManifest& manifest
    );

    [[nodiscard]]
    static CheckpointManifest decode(std::span<const std::uint8_t> bytes);
};

} // namespace pi::checkpoint
