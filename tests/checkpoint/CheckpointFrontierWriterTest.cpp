#include "checkpoint/CheckpointFrontierWriter.hpp"

#include "binary/BinarySplitter.hpp"
#include "checkpoint/CheckpointResumePlanner.hpp"
#include "checkpoint/CheckpointStore.hpp"
#include "chudnovsky/PrecisionPolicy.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>


using pi::binary::BinaryNode;
using pi::binary::BinarySplitter;
using pi::checkpoint::AtomicCommitStage;
using pi::checkpoint::CheckpointFrontierWriter;
using pi::checkpoint::CheckpointManifest;
using pi::checkpoint::CheckpointResumePlan;
using pi::checkpoint::CheckpointResumePlanner;
using pi::checkpoint::CheckpointStore;
using pi::checkpoint::ComputationIdentity;
using pi::checkpoint::ManifestEntry;
using pi::checkpoint::RejectionReason;
using pi::chudnovsky::PrecisionPolicy;


namespace
{

int tempFilesCreated = 0;
int failAtCommit = 0;
AtomicCommitStage failStage = AtomicCommitStage::tempCreated;


void countingObserver(AtomicCommitStage stage)
{
    if (stage == AtomicCommitStage::tempCreated) ++tempFilesCreated;
    if (failAtCommit != 0 && tempFilesCreated == failAtCommit
        && stage == failStage)
    {
        throw std::runtime_error("injected checkpoint commit failure");
    }
}


void resetObserver(int commit, AtomicCommitStage stage)
{
    tempFilesCreated = 0;
    failAtCommit = commit;
    failStage = stage;
}


class TemporaryDirectory
{
public:
    TemporaryDirectory()
        : path_(std::filesystem::temp_directory_path()
            / ("givemepi-frontier-writer-"
                + std::to_string(static_cast<unsigned long long>(::getpid()))))
    {
        std::filesystem::remove_all(path_);
        std::filesystem::create_directory(path_);
    }
    ~TemporaryDirectory()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }
    const std::filesystem::path& path() const noexcept { return path_; }
private:
    std::filesystem::path path_;
};


struct Frontier
{
    std::vector<BinaryNode> nodes;
    std::vector<std::uint32_t> levels;
};


Frontier frontier(
    const std::vector<std::size_t>& bounds,
    const std::vector<std::uint32_t>& levels
)
{
    Frontier result;
    for (std::size_t index = 0; index + 1 < bounds.size(); ++index)
    {
        result.nodes.push_back(
            BinarySplitter::splitSequential(bounds[index], bounds[index + 1])
        );
    }
    result.levels = levels;
    return result;
}


void commit(CheckpointFrontierWriter& writer, const Frontier& value)
{
    writer.commitFrontier(value.nodes, value.levels);
}


std::size_t checkpointFileCount(const std::filesystem::path& directory)
{
    std::size_t count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
        const std::string name = entry.path().filename().string();
        if (entry.path().extension() == ".checkpoint"
            || name.find(".tmp.") != std::string::npos)
        {
            ++count;
        }
    }
    return count;
}


std::vector<ManifestEntry> acceptedEntries(const CheckpointResumePlan& plan)
{
    std::vector<ManifestEntry> entries;
    for (const auto& accepted : plan.acceptedBlocks)
        entries.push_back(accepted.manifestEntry);
    return entries;
}


bool cleanPlan(
    const CheckpointResumePlan& plan,
    const std::vector<ManifestEntry>& expected
)
{
    return plan.rejectedBlocks.empty()
        && plan.ignoredTemporaryFiles.empty()
        && plan.rangesToRecalculate.empty()
        && acceptedEntries(plan) == expected;
}


int fail(const char* message)
{
    std::cerr << message << '\n';
    return 1;
}

} // namespace


int main()
{
    TemporaryDirectory temporary;
    const std::filesystem::path directory = temporary.path() / "blocks";
    const CheckpointStore store(directory);
    const CheckpointResumePlanner planner(
        directory, temporary.path() / "quarantine"
    );
    const ComputationIdentity identity =
        ComputationIdentity::fromPrecisionPlan(PrecisionPolicy::create(1000));
    const std::size_t n = static_cast<std::size_t>(identity.termCount);
    const std::vector<std::size_t> leaves{
        0, n / 5, 2 * n / 5, 3 * n / 5, 4 * n / 5, n
    };

    const Frontier leafStage = frontier(leaves, {0, 0, 0, 0, 0});
    const Frontier firstMerge = frontier(
        {0, leaves[2], leaves[4], n}, {1, 1, 0}
    );
    const Frontier secondMerge = frontier({0, leaves[4], n}, {2, 0});
    const Frontier root = frontier({0, n}, {3});

    // The first stage always commits; a later stage inside the interval is
    // skipped and leaves the published frontier untouched.
    CheckpointFrontierWriter gated(
        directory, identity, std::chrono::hours(1), {}
    );
    commit(gated, leafStage);
    const CheckpointManifest leafManifest = store.loadManifest(identity);
    if (gated.commitCount() != 1 || leafManifest.entries != gated.entries()
        || leafManifest.entries.size() != 5
        || checkpointFileCount(directory) != 5)
    {
        return fail("First completed stage was not committed");
    }
    if (!cleanPlan(planner.create(identity), leafManifest.entries))
        return fail("Planner rejected the first committed frontier");

    commit(gated, firstMerge);
    if (gated.commitCount() != 1
        || store.loadManifest(identity) != leafManifest
        || checkpointFileCount(directory) != 5)
    {
        return fail("Interval-gated stage replaced the durable frontier");
    }

    // Resume from planner-accepted entries; the merged frontier replaces
    // consumed children and keeps the odd node's exact file and entry.
    const std::filesystem::path oddPath = directory / leafManifest.entries[4].filename;
    const auto oddWriteTime = std::filesystem::last_write_time(oddPath);
    CheckpointFrontierWriter resumed(
        directory, identity, std::chrono::seconds(0),
        acceptedEntries(planner.create(identity)), countingObserver
    );
    resetObserver(0, AtomicCommitStage::tempCreated);
    commit(resumed, firstMerge);
    const CheckpointManifest mergedManifest = store.loadManifest(identity);
    if (tempFilesCreated != 3 || resumed.commitCount() != 1
        || mergedManifest.entries != resumed.entries()
        || mergedManifest.entries.size() != 3
        || mergedManifest.entries[2] != leafManifest.entries[4]
        || std::filesystem::last_write_time(oddPath) != oddWriteTime
        || checkpointFileCount(directory) != 3)
    {
        return fail("Changed frontier did not replace only consumed children");
    }
    for (std::size_t index = 0; index < 4; ++index)
    {
        if (std::filesystem::exists(directory / leafManifest.entries[index].filename))
            return fail("Consumed child block was not deleted");
    }
    if (!cleanPlan(planner.create(identity), mergedManifest.entries))
        return fail("Planner rejected the merged frontier");

    resetObserver(0, AtomicCommitStage::tempCreated);
    commit(resumed, firstMerge);
    if (tempFilesCreated != 0 || resumed.commitCount() != 1)
        return fail("Unchanged frontier was rewritten");

    // A failed block write leaves the last manifest authoritative.
    CheckpointFrontierWriter blockFailure(
        directory, identity, std::chrono::seconds(0),
        mergedManifest.entries, countingObserver
    );
    resetObserver(1, AtomicCommitStage::contentWritten);
    bool threw = false;
    try { commit(blockFailure, secondMerge); }
    catch (const std::runtime_error&) { threw = true; }
    if (!threw || blockFailure.commitCount() != 0
        || blockFailure.entries() != mergedManifest.entries
        || store.loadManifest(identity) != mergedManifest
        || !cleanPlan(planner.create(identity), mergedManifest.entries))
    {
        return fail("Block write failure did not preserve the last manifest");
    }

    // A failed manifest publication leaves the new parent unreferenced; the
    // planner keeps the old frontier and quarantines the orphan.
    CheckpointFrontierWriter manifestFailure(
        directory, identity, std::chrono::seconds(0),
        mergedManifest.entries, countingObserver
    );
    resetObserver(2, AtomicCommitStage::beforeRename);
    threw = false;
    try { commit(manifestFailure, secondMerge); }
    catch (const std::runtime_error&) { threw = true; }
    if (!threw || tempFilesCreated != 2
        || manifestFailure.entries() != mergedManifest.entries
        || store.loadManifest(identity) != mergedManifest)
    {
        return fail("Manifest failure replaced the last manifest");
    }
    const CheckpointResumePlan afterManifestFailure = planner.create(identity);
    bool orphanRejected = false;
    for (const auto& rejected : afterManifestFailure.rejectedBlocks)
    {
        for (const auto& diagnostic : rejected.validation.diagnostics())
        {
            if (diagnostic.reason == RejectionReason::unexpectedBlockFile)
                orphanRejected = true;
        }
    }
    if (!orphanRejected || afterManifestFailure.rejectedBlocks.size() != 1
        || !afterManifestFailure.rangesToRecalculate.empty()
        || acceptedEntries(afterManifestFailure) != mergedManifest.entries)
    {
        return fail("Last manifest was not recoverable after manifest failure");
    }

    // Recovery from the planner commits the root and removes every child.
    CheckpointFrontierWriter recovered(
        directory, identity, std::chrono::seconds(0),
        acceptedEntries(afterManifestFailure)
    );
    commit(recovered, root);
    const CheckpointManifest rootManifest = store.loadManifest(identity);
    if (rootManifest.entries.size() != 1 || checkpointFileCount(directory) != 1
        || !cleanPlan(planner.create(identity), rootManifest.entries))
    {
        return fail("Root frontier was not committed cleanly");
    }

    // Malformed frontiers fail before any write.
    const Frontier gap = frontier({0, leaves[1]}, {0});
    const Frontier unaligned = frontier({0, n}, {});
    for (const Frontier* invalid : {&gap, &unaligned})
    {
        threw = false;
        try { commit(recovered, *invalid); }
        catch (const std::invalid_argument&) { threw = true; }
        if (!threw || recovered.commitCount() != 1
            || store.loadManifest(identity) != rootManifest)
        {
            return fail("Malformed frontier was accepted");
        }
    }

    return 0;
}
