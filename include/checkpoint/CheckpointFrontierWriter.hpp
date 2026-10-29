#pragma once

#include "binary/BinarySplitter.hpp"
#include "checkpoint/AtomicFileCommit.hpp"
#include "checkpoint/CheckpointManifest.hpp"
#include "checkpoint/CheckpointStore.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>


namespace pi::checkpoint
{

/**
 * Publishes the staged Binary Splitting merge frontier as durable version-1
 * checkpoint blocks plus one manifest.
 *
 * The first frontier is always committed; later frontiers are committed only
 * once the interval has elapsed since the previous commit. Each commit writes
 * blocks for new frontier nodes, reuses the exact entries of unchanged nodes,
 * atomically replaces the manifest, then best-effort deletes superseded
 * block files. Runs on the coordinating thread only.
 */
class CheckpointFrontierWriter final : public binary::BinaryCheckpointCoordinator
{
public:
    using Clock = std::chrono::steady_clock;

    /**
     * @param acceptedEntries planner-accepted manifest entries whose blocks
     *        seed the initial frontier; any order, non-overlapping, complete.
     * @param commitObserver optional atomic-commit observer for every block
     *        and manifest write (failure injection in tests).
     * @throws std::invalid_argument for an invalid identity or entry set.
     */
    CheckpointFrontierWriter(
        std::filesystem::path checkpointDirectory,
        ComputationIdentity identity,
        std::chrono::seconds interval,
        std::vector<ManifestEntry> acceptedEntries,
        AtomicCommitObserver commitObserver = nullptr
    );

    /**
     * Commits the complete [0, termCount) frontier when the interval allows.
     *
     * @throws std::invalid_argument for a malformed frontier.
     * @throws on block or manifest write failure; the previously published
     *         manifest then remains the resume authority.
     */
    void commitFrontier(
        std::span<const binary::BinaryNode> nodes,
        std::span<const std::uint32_t> treeLevels
    ) override;

    /// Entries of the last published manifest, or the accepted entries
    /// before the first commit. Sorted canonically.
    [[nodiscard]]
    const std::vector<ManifestEntry>& entries() const noexcept;

    /// Number of manifests published by this writer.
    [[nodiscard]]
    std::uint64_t commitCount() const noexcept;

private:
    CheckpointStore store_;
    ComputationIdentity identity_;
    Clock::duration interval_;
    std::vector<ManifestEntry> entries_;
    std::optional<Clock::time_point> lastCommit_;
    std::uint64_t commitCount_ = 0;
    AtomicCommitObserver commitObserver_;
};

} // namespace pi::checkpoint
