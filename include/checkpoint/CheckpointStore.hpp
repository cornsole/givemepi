#pragma once

#include "checkpoint/AtomicFileCommit.hpp"
#include "checkpoint/CheckpointBlockCodec.hpp"
#include "checkpoint/CheckpointManifest.hpp"

#include <cstdint>
#include <filesystem>


namespace pi::checkpoint
{

/// Durable block path plus the metadata produced while encoding it.
struct StoredCheckpointBlock
{
    std::filesystem::path path;
    std::uint64_t fileSize;
    std::uint64_t payloadSize;
    ChecksumAlgorithm checksumAlgorithm;
    std::uint32_t checksum;
};


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

    /**
     * Atomically writes a block from borrowed P/Q/T values and returns its
     * manifest metadata without rereading or decoding the written file.
     */
    [[nodiscard]]
    StoredCheckpointBlock save(
        const ComputationIdentity& identity,
        const BlockLocation& location,
        const bigint::GMPInteger& p,
        const bigint::GMPInteger& q,
        const bigint::GMPInteger& t,
        AtomicCommitObserver observer = nullptr
    ) const;

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
        const CheckpointManifest& manifest,
        AtomicCommitObserver observer = nullptr
    ) const;

    [[nodiscard]]
    CheckpointManifest loadManifest(
        const ComputationIdentity& identity
    ) const;

private:
    std::filesystem::path directory_;
};

} // namespace pi::checkpoint
