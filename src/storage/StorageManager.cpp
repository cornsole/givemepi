#include "storage/StorageManager.hpp"

#include <stdexcept>

namespace pi::storage
{

std::filesystem::path StorageManager::makeIndexPath(const StoragePolicy& policy)
{
    return policy.directory / "chunk-index-v1.bin";
}

StorageManager::StorageManager(const StoragePolicy& policy)
    : policy_(policy)
    , store_(policy_)
    , index_(ChunkIndex::create())
    , indexPath_(makeIndexPath(policy_))
{
    if (std::filesystem::exists(indexPath_))
        index_ = ChunkIndex::load(indexPath_);
}

void StorageManager::publishIndex(const ChunkIndex& next)
{
    next.save(indexPath_);
    index_ = next;
}

ChunkId StorageManager::store(const Chunk& chunk)
{
    const auto& identity = chunk.metadata.identity;
    if (index_.contains(identity))
        throw std::invalid_argument("duplicate chunk identity in storage manager");

    const ChunkId id = store_.store(chunk);
    ChunkIndex next = index_;
    try
    {
        next.add(chunk.metadata);
        publishIndex(next);
    }
    catch (...)
    {
        store_.remove(id);
        throw;
    }
    return id;
}

std::optional<Chunk> StorageManager::load(const ChunkIdentity& identity) const
{
    if (!index_.contains(identity)) return std::nullopt;
    const auto entry = index_.at(identity);
    const auto loaded = store_.reloadAndVerify(entry.storageFile);
    if (!loaded.has_value()) return std::nullopt;
    if (loaded->metadata.identity != identity)
        throw std::runtime_error("chunk index identity does not match stored chunk");
    if (loaded->metadata.storedSize != entry.storedSize
        || loaded->metadata.checksumValue != entry.checksumValue)
        throw std::runtime_error("chunk index metadata does not match stored chunk");
    return loaded;
}

bool StorageManager::contains(const ChunkIdentity& identity) const noexcept
{
    if (!index_.contains(identity)) return false;
    return store_.contains(index_.at(identity).storageFile);
}

bool StorageManager::remove(const ChunkIdentity& identity)
{
    if (!index_.contains(identity)) return false;
    const auto id = index_.at(identity).storageFile;
    if (!store_.remove(id)) return false;
    ChunkIndex next = index_;
    next.remove(identity);
    publishIndex(next);
    return true;
}

StorageSnapshot StorageManager::snapshot(
    const std::vector<std::pair<ChunkId, std::uint64_t>>& residentChunks
) const
{
    StorageSnapshot snapshot;
    snapshot.indexedChunks = index_.size();
    snapshot.storedBytes = index_.storedBytes();
    snapshot.memoryBudgetBytes = policy_.memory_budget_bytes;
    snapshot.entries = index_.entries();
    for (const auto& [id, bytes] : residentChunks) { (void)id; snapshot.residentBytes += bytes; }
    return snapshot;
}

EvictionPlan StorageManager::planEvictions(
    const std::vector<std::pair<ChunkId, std::uint64_t>>& residentChunks,
    const std::set<ChunkId>& residentSet,
    const std::unordered_map<ChunkId, double>& mergeDistanceMap,
    std::uint64_t neededBytes
) const
{
    return store_.planEvictions(residentChunks, residentSet, mergeDistanceMap, neededBytes);
}

} // namespace pi::storage
