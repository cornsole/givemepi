#include "storage/StorageMergeCoordinator.hpp"

#include "storage/BinaryNodeChunkAdapter.hpp"
#include "progress/ProgressTracker.hpp"

#include <set>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace pi::storage
{

StorageMergeCoordinator::StorageMergeCoordinator(
    StorageManager& manager,
    checkpoint::ComputationIdentity computation,
    progress::ProgressTracker* progress
)
    : manager_(manager),
      computation_(std::move(computation)),
      progress_(progress)
{
    computation_.validate();
}

void StorageMergeCoordinator::observeResidentNodes(
    std::span<const binary::BinaryNode> nodes,
    std::uint32_t level
)
{
    std::vector<ResidentMergeNode> next;
    next.reserve(nodes.size());
    std::set<ChunkId> identities;

    for (const binary::BinaryNode& node : nodes)
    {
        if (node.start() >= node.end())
        {
            throw std::invalid_argument(
                "merge coordinator received an invalid BinaryNode range");
        }

        const auto location = checkpoint::BlockLocation::create(
            static_cast<std::uint64_t>(node.start()),
            static_cast<std::uint64_t>(node.end()),
            level,
            computation_);
        const auto identity = ChunkIdentity::create(computation_, location);
        const auto metadata = ChunkCodec::createMetadata(
            identity, node.P(), node.Q(), node.T());
        const ChunkId chunkId = identity.deterministicFilename();
        if (!identities.insert(chunkId).second)
        {
            throw std::invalid_argument(
                "merge coordinator received duplicate resident identity");
        }

        next.push_back(ResidentMergeNode{
            identity,
            chunkId,
            metadata.uncompressedSize,
            level,
            NodeLifecycle{},
            nodes.size() > 1 ? 1.0 : 0.0});
    }

    for (const auto& previous : residentNodes_)
    {
        if (manager_.contains(previous.identity)
            && !manager_.remove(previous.identity))
        {
            throw std::runtime_error(
                "cannot retire consumed spilled merge node");
        }
    }
    residentNodes_ = std::move(next);
    publishProgress(level);
}

void StorageMergeCoordinator::publishProgress(std::uint32_t level)
{
    if (progress_ == nullptr) return;
    const auto current = snapshot();
    static_cast<void>(progress_->setCurrentMergeLevel(level));
    progress_->setStorageProgress(
        current.residentBytes,
        current.storedBytes,
        static_cast<std::uint64_t>(current.indexedChunks));
}

void StorageMergeCoordinator::prepareMergeNodes(
    std::span<binary::BinaryNode> nodes,
    std::uint32_t level
)
{
    if (nodes.size() != residentNodes_.size())
    {
        throw std::logic_error(
            "merge coordinator resident set does not match merge nodes");
    }

    for (std::size_t index = 0; index < nodes.size(); ++index)
    {
        auto& record = residentNodes_[index];
        record.mergeDistance = 0.0;
        if (record.level != level || record.lifecycle.state()
            != NodeLifecycleState::stored)
        {
            continue;
        }
        if (!record.lifecycle.beginLoad())
        {
            throw std::logic_error("cannot begin spilled node load");
        }

        try
        {
            const auto loaded = manager_.load(record.identity);
            if (!loaded.has_value())
            {
                static_cast<void>(record.lifecycle.failLoad(false));
                throw std::runtime_error("spilled merge node is missing");
            }
            const auto restored = BinaryNodeChunkAdapter::toBinaryNode(*loaded);
            nodes[index].P().set(restored.P());
            nodes[index].Q().set(restored.Q());
            nodes[index].T().set(restored.T());
            if (!record.lifecycle.completeLoad())
            {
                throw std::logic_error("cannot complete spilled node load");
            }
            ++reloadCount_;
            publishProgress(level);
        }
        catch (...)
        {
            if (record.lifecycle.state() == NodeLifecycleState::loading)
            {
                static_cast<void>(record.lifecycle.failLoad(true));
            }
            throw;
        }
    }
}

void StorageMergeCoordinator::spillAfterMergeLevel(
    std::span<binary::BinaryNode> nodes,
    std::uint32_t level
)
{
    if (nodes.size() != residentNodes_.size())
    {
        throw std::logic_error(
            "merge coordinator resident set does not match spill nodes");
    }

    const auto current = snapshot();
    if (current.residentBytes <= current.memoryBudgetBytes)
    {
        return;
    }

    std::vector<std::pair<ChunkId, std::uint64_t>> resident;
    std::set<ChunkId> residentSet;
    std::unordered_map<ChunkId, double> mergeDistances;
    resident.reserve(residentNodes_.size());
    for (const auto& record : residentNodes_)
    {
        resident.emplace_back(record.chunkId, record.residentBytes);
        residentSet.insert(record.chunkId);
        mergeDistances.emplace(record.chunkId, record.mergeDistance);
    }

    const std::uint64_t neededBytes =
        current.residentBytes - current.memoryBudgetBytes;
    const auto plan = manager_.planEvictions(
        resident, residentSet, mergeDistances, neededBytes);
    if (!plan.budgetSatisfied)
    {
        throw std::runtime_error(
            "merge resident set cannot satisfy storage memory budget");
    }

    for (const auto& candidate : plan.candidates)
    {
        for (std::size_t index = 0; index < residentNodes_.size(); ++index)
        {
            auto& record = residentNodes_[index];
            if (record.chunkId != candidate.chunkId) continue;
            if (record.level != level || !nodes[index].hasValues()) continue;
            if (!record.lifecycle.beginSpill())
            {
                throw std::logic_error("cannot begin merge node spill");
            }
            try
            {
                const Chunk chunk = BinaryNodeChunkAdapter::toChunk(
                    nodes[index], computation_, level,
                    manager_.policy().compression);
                static_cast<void>(manager_.store(chunk));
                nodes[index].clearValues();
                ++spillCount_;
                spilledBytes_ += chunk.metadata.storedSize;
                if (!record.lifecycle.completeSpill())
                {
                    throw std::logic_error("cannot complete merge node spill");
                }
            }
            catch (...)
            {
                if (record.lifecycle.state() == NodeLifecycleState::spilling)
                {
                    static_cast<void>(record.lifecycle.failSpill());
                }
                throw;
            }
            break;
        }
    }
    publishProgress(level);
}

StorageSnapshot StorageMergeCoordinator::snapshot() const
{
    std::vector<std::pair<ChunkId, std::uint64_t>> resident;
    resident.reserve(residentNodes_.size());
    for (const auto& node : residentNodes_)
    {
        if (node.lifecycle.state() == NodeLifecycleState::resident
            || node.lifecycle.state() == NodeLifecycleState::spilling)
        {
            resident.emplace_back(node.chunkId, node.residentBytes);
        }
    }
    return manager_.snapshot(resident);
}

const std::vector<ResidentMergeNode>&
StorageMergeCoordinator::residentNodes() const noexcept
{
    return residentNodes_;
}

std::uint64_t StorageMergeCoordinator::spillCount() const noexcept
{
    return spillCount_;
}

std::uint64_t StorageMergeCoordinator::reloadCount() const noexcept
{
    return reloadCount_;
}

std::uint64_t StorageMergeCoordinator::spilledBytes() const noexcept
{
    return spilledBytes_;
}

} // namespace pi::storage
