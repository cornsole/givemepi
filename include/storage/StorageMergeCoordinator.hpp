#pragma once

#include "binary/BinarySplitter.hpp"
#include "checkpoint/CheckpointTypes.hpp"
#include "storage/StorageManager.hpp"
#include "storage/NodeLifecycle.hpp"
#include "storage/AsyncWriter.hpp"
#include "storage/AsyncReader.hpp"

#include <cstdint>
#include <vector>

namespace pi::progress
{
class ProgressTracker;
}

namespace pi::storage
{

struct ResidentMergeNode
{
    ChunkIdentity identity;
    ChunkId chunkId;
    std::uint64_t residentBytes = 0;
    std::uint32_t level = 0;
    NodeLifecycle lifecycle;
    double mergeDistance = 1.0;
};

/// Synchronous bridge from BinarySplitter's merge coordinator callbacks to
/// StorageManager's resident/storage accounting.
class StorageMergeCoordinator final : public binary::BinaryMergeCoordinator
{
public:
    StorageMergeCoordinator(
        StorageManager& manager,
        checkpoint::ComputationIdentity computation,
        progress::ProgressTracker* progress = nullptr,
        AsyncChunkWriter* asyncWriter = nullptr,
        AsyncChunkReader* asyncReader = nullptr
    );

    void observeResidentNodes(
        std::span<const binary::BinaryNode> nodes,
        std::uint32_t level
    ) override;

    void prepareMergeNodes(
        std::span<binary::BinaryNode> nodes,
        std::uint32_t level
    ) override;

    void spillAfterMergeLevel(
        std::span<binary::BinaryNode> nodes,
        std::uint32_t level
    ) override;

    [[nodiscard]] StorageSnapshot snapshot() const;
    [[nodiscard]] const std::vector<ResidentMergeNode>& residentNodes()
        const noexcept;
    [[nodiscard]] std::uint64_t spillCount() const noexcept;
    [[nodiscard]] std::uint64_t reloadCount() const noexcept;
    [[nodiscard]] std::uint64_t spilledBytes() const noexcept;

private:
    void publishProgress(std::uint32_t level);

    StorageManager& manager_;
    checkpoint::ComputationIdentity computation_;
    progress::ProgressTracker* progress_ = nullptr;
    AsyncChunkWriter* asyncWriter_ = nullptr;
    AsyncChunkReader* asyncReader_ = nullptr;
    std::vector<ResidentMergeNode> residentNodes_;
    std::uint64_t spillCount_ = 0;
    std::uint64_t reloadCount_ = 0;
    std::uint64_t spilledBytes_ = 0;
};

} // namespace pi::storage
