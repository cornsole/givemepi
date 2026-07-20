#pragma once

#include "checkpoint/CheckpointBlockCodec.hpp"
#include "checkpoint/CheckpointManifest.hpp"

#include <filesystem>


namespace pi::checkpoint
{

class CheckpointStore
{
public:
    explicit CheckpointStore(std::filesystem::path directory);

    [[nodiscard]]
    std::filesystem::path blockPath(
        const ComputationIdentity& identity,
        const BlockLocation& location
    ) const;

    [[nodiscard]]
    std::filesystem::path save(const CheckpointBlock& block) const;

    [[nodiscard]]
    CheckpointBlock load(
        const ComputationIdentity& identity,
        const BlockLocation& location
    ) const;

    [[nodiscard]]
    std::filesystem::path manifestPath(
        const ComputationIdentity& identity
    ) const;

    [[nodiscard]]
    std::filesystem::path saveManifest(
        const CheckpointManifest& manifest
    ) const;

    [[nodiscard]]
    CheckpointManifest loadManifest(
        const ComputationIdentity& identity
    ) const;

private:
    std::filesystem::path directory_;
};

} // namespace pi::checkpoint
