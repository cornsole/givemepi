#include "checkpoint/CheckpointFrontierWriter.hpp"

#include "checkpoint/CheckpointManifestValidator.hpp"

#include <algorithm>
#include <stdexcept>
#include <system_error>
#include <utility>


namespace pi::checkpoint
{

namespace
{

bool locationLess(const ManifestEntry& entry, const BlockLocation& location)
{
    return entry.location < location;
}

} // namespace


CheckpointFrontierWriter::CheckpointFrontierWriter(
    std::filesystem::path checkpointDirectory,
    ComputationIdentity identity,
    std::chrono::seconds interval,
    std::vector<ManifestEntry> acceptedEntries,
    AtomicCommitObserver commitObserver
)
    : store_(std::move(checkpointDirectory)),
      identity_(identity),
      interval_(interval),
      entries_(std::move(acceptedEntries)),
      commitObserver_(commitObserver)
{
    identity_.validate();
    if (interval.count() < 0)
        throw std::invalid_argument("Checkpoint interval must not be negative");

    std::sort(entries_.begin(), entries_.end(), [](const auto& left, const auto& right) {
        return left.location < right.location;
    });

    std::uint64_t cursor = 0;
    for (const ManifestEntry& entry : entries_)
    {
        entry.location.validate(identity_);
        if (entry.location.start < cursor)
            throw std::invalid_argument("Accepted checkpoint entries overlap");
        if (entry.completion != CompletionState::complete)
            throw std::invalid_argument("Accepted checkpoint entry is incomplete");
        if (entry.filename
            != store_.blockPath(identity_, entry.location).filename().string())
        {
            throw std::invalid_argument(
                "Accepted checkpoint entry filename is not deterministic"
            );
        }
        cursor = entry.location.end;
    }
}


void CheckpointFrontierWriter::commitFrontier(
    std::span<const binary::BinaryNode> nodes,
    std::span<const std::uint32_t> treeLevels
)
{
    if (nodes.size() != treeLevels.size())
        throw std::invalid_argument("Frontier node and tree-level counts differ");
    if (nodes.empty())
        throw std::invalid_argument("Checkpoint frontier must not be empty");

    if (nodes.size() != 1 && lastCommit_.has_value()
        && Clock::now() - *lastCommit_ < interval_)
    {
        return;
    }

    // Build the complete candidate manifest before any I/O. Unchanged nodes
    // reuse their exact published entry; new nodes get metadata on write.
    CheckpointManifest manifest{identity_, {}};
    manifest.entries.reserve(nodes.size());
    std::vector<std::size_t> newNodes;
    std::uint64_t cursor = 0;
    auto existing = entries_.cbegin();

    for (std::size_t index = 0; index < nodes.size(); ++index)
    {
        const std::uint64_t start = nodes[index].start();
        const std::uint64_t end = nodes[index].end();
        if (start != cursor)
        {
            throw std::invalid_argument(
                "Checkpoint frontier is not sorted and contiguous from zero"
            );
        }
        cursor = end;

        const BlockLocation location = BlockLocation::create(
            start, end, treeLevels[index], identity_
        );
        existing = std::lower_bound(
            existing, entries_.cend(), location, locationLess
        );
        if (existing != entries_.cend() && existing->location == location)
        {
            manifest.entries.push_back(*existing);
            continue;
        }

        if (!nodes[index].hasValues())
        {
            throw std::logic_error(
                "New checkpoint frontier node has no resident P/Q/T payload"
            );
        }
        manifest.entries.push_back(ManifestEntry{
            location, {}, 0, ChecksumAlgorithm::crc32c, 0,
            CompletionState::complete
        });
        newNodes.push_back(index);
    }

    if (cursor != identity_.termCount)
        throw std::invalid_argument("Checkpoint frontier does not cover every term");

    if (commitCount_ != 0 && newNodes.empty()
        && manifest.entries.size() == entries_.size())
    {
        return; // Identical to the published frontier.
    }

    // Structural gate only: block metadata is filled in below.
    if (!CheckpointManifestValidator::validate(manifest, identity_)
            .validation.isAccepted())
    {
        throw std::invalid_argument(
            "Checkpoint frontier would not form a resumable manifest"
        );
    }

    for (const std::size_t index : newNodes)
    {
        const binary::BinaryNode& node = nodes[index];
        ManifestEntry& entry = manifest.entries[index];
        StoredCheckpointBlock stored = store_.save(
            identity_, entry.location, node.P(), node.Q(), node.T(),
            commitObserver_
        );
        entry.filename = stored.path.filename().string();
        entry.payloadSize = stored.payloadSize;
        entry.checksumAlgorithm = stored.checksumAlgorithm;
        entry.checksum = stored.checksum;
    }

    static_cast<void>(store_.saveManifest(manifest, commitObserver_));

    // The new manifest is authoritative; superseded children are now
    // unreferenced. Failure to delete only leaves files the planner
    // quarantines as unexpected on resume.
    auto published = manifest.entries.cbegin();
    for (const ManifestEntry& previous : entries_)
    {
        published = std::lower_bound(
            published, manifest.entries.cend(), previous.location, locationLess
        );
        if (published != manifest.entries.cend()
            && published->location == previous.location)
        {
            continue;
        }
        std::error_code ignored;
        std::filesystem::remove(
            store_.blockPath(identity_, previous.location), ignored
        );
    }

    entries_ = std::move(manifest.entries);
    lastCommit_ = Clock::now();
    ++commitCount_;
}


const std::vector<ManifestEntry>& CheckpointFrontierWriter::entries() const noexcept
{
    return entries_;
}


std::uint64_t CheckpointFrontierWriter::commitCount() const noexcept
{
    return commitCount_;
}

} // namespace pi::checkpoint
