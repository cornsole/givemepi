#include "chudnovsky/PrecisionPolicy.hpp"
#include "progress/ProgressTracker.hpp"
#include "scheduler/Scheduler.hpp"
#include "storage/StorageMergeCoordinator.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <vector>

int main()
{
    using namespace pi::checkpoint;
    using namespace pi::chudnovsky;
    using namespace pi::storage;

    const auto directory = std::filesystem::temp_directory_path()
        / "givemepi-pr0026-merge-coordinator-test";
    std::error_code ec;
    std::filesystem::remove_all(directory, ec);

    StoragePolicy policy;
    policy.directory = directory;
    policy.memory_budget_bytes = 1;
    policy.target_chunk_size_bytes = 1;
    StorageManager manager(policy);
    const auto computation = ComputationIdentity::fromPrecisionPlan(
        PrecisionPolicy::create(1000));
    pi::progress::ProgressTracker progress({
        computation.requestedDigits,
        computation.termCount,
        0});
    StorageMergeCoordinator coordinator(manager, computation, &progress);

    pi::scheduler::Scheduler baselineScheduler(2, 32);
    baselineScheduler.start();
    const auto baseline = pi::binary::BinarySplitter::splitParallel(
        0,
        8,
        baselineScheduler,
        pi::binary::ParallelSplitOptions{2, 2},
        nullptr);
    baselineScheduler.stop();

    pi::scheduler::Scheduler scheduler(2, 32);
    scheduler.start();
    const auto result = pi::binary::BinarySplitter::splitParallel(
        0,
        8,
        scheduler,
        pi::binary::ParallelSplitOptions{2, 2},
        nullptr,
        &coordinator);
    scheduler.stop();

    assert(result.start() == 0);
    assert(result.end() == 8);
    assert(result.P().compare(baseline.P()) == 0);
    assert(result.Q().compare(baseline.Q()) == 0);
    assert(result.T().compare(baseline.T()) == 0);
    assert(coordinator.residentNodes().size() == 1);
    assert(coordinator.residentNodes().front().level == 2);
    assert(coordinator.residentNodes().front().identity.location.start == 0);
    assert(coordinator.residentNodes().front().identity.location.end == 8);

    const auto snapshot = coordinator.snapshot();
    assert(coordinator.spillCount() > 0);
    assert(coordinator.reloadCount() > 0);
    assert(snapshot.indexedChunks == 0);
    assert(coordinator.residentNodes().front().mergeDistance == 0.0);
    const auto progressSnapshot = progress.snapshot();
    assert(progressSnapshot.currentMergeLevel() == 2);
    assert(progressSnapshot.storageResidentBytes() > 0);
    assert(progressSnapshot.storageStoredBytes() == 0);
    assert(progressSnapshot.storageChunkCount() == 0);

    // If every node is protected by the active merge state, the planner must
    // reject the budget request before any payload is cleared.
    const auto protectedDirectory = directory.string() + "-protected";
    const auto protectedPath = std::filesystem::path(protectedDirectory);
    std::filesystem::remove_all(protectedPath, ec);
    StoragePolicy protectedPolicy = policy;
    protectedPolicy.directory = protectedPath;
    StorageManager protectedManager(protectedPolicy);
    StorageMergeCoordinator protectedCoordinator(
        protectedManager, computation);
    std::vector<pi::binary::BinaryNode> protectedLeaves;
    protectedLeaves.push_back(
        pi::binary::BinarySplitter::splitSequential(0, 2));
    protectedLeaves.push_back(
        pi::binary::BinarySplitter::splitSequential(2, 4));
    protectedCoordinator.observeResidentNodes(protectedLeaves, 0);
    protectedCoordinator.prepareMergeNodes(protectedLeaves, 0);
    bool protectedRejected = false;
    try
    {
        protectedCoordinator.spillAfterMergeLevel(protectedLeaves, 0);
    }
    catch (const std::runtime_error&)
    {
        protectedRejected = true;
    }
    assert(protectedRejected);
    assert(protectedLeaves[0].hasValues());
    assert(protectedLeaves[1].hasValues());
    std::filesystem::remove_all(protectedPath, ec);

    // A missing durable payload must fail reload instead of returning a
    // partially initialized BinaryNode.
    const auto missingDirectory = directory.string() + "-missing";
    const auto missingPath = std::filesystem::path(missingDirectory);
    std::filesystem::remove_all(missingPath, ec);
    StoragePolicy missingPolicy = policy;
    missingPolicy.directory = missingPath;
    StorageManager missingManager(missingPolicy);
    StorageMergeCoordinator missingCoordinator(missingManager, computation);
    std::vector<pi::binary::BinaryNode> leaves;
    leaves.push_back(pi::binary::BinarySplitter::splitSequential(0, 2));
    leaves.push_back(pi::binary::BinarySplitter::splitSequential(2, 4));
    missingCoordinator.observeResidentNodes(leaves, 0);
    missingCoordinator.spillAfterMergeLevel(leaves, 0);
    const auto& records = missingCoordinator.residentNodes();
    bool removedStored = false;
    for (const auto& record : records)
    {
        if (record.lifecycle.state() == NodeLifecycleState::stored)
        {
            removedStored = missingManager.remove(record.identity);
            break;
        }
    }
    assert(removedStored);
    bool rejectedMissing = false;
    try
    {
        missingCoordinator.prepareMergeNodes(leaves, 0);
    }
    catch (const std::runtime_error&)
    {
        rejectedMissing = true;
    }
    assert(rejectedMissing);
    std::filesystem::remove_all(missingPath, ec);
    assert(snapshot.residentBytes
        == coordinator.residentNodes().front().residentBytes);

    std::filesystem::remove_all(directory, ec);
    std::cout << "StorageMergeCoordinator OK\n";
    return 0;
}
