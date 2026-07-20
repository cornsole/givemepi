#include "checkpoint/CheckpointResumePlanner.hpp"

#include "binary/BinarySplitter.hpp"
#include "checkpoint/CheckpointFileInspector.hpp"
#include "checkpoint/CheckpointStore.hpp"
#include "chudnovsky/PrecisionPolicy.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>


using pi::binary::BinaryNode;
using pi::binary::BinarySplitter;
using pi::checkpoint::BlockLocation;
using pi::checkpoint::CheckpointBlock;
using pi::checkpoint::CheckpointManifest;
using pi::checkpoint::CheckpointResumePlanner;
using pi::checkpoint::CheckpointStore;
using pi::checkpoint::CompletionState;
using pi::checkpoint::ComputationIdentity;
using pi::checkpoint::ManifestEntry;
using pi::checkpoint::MissingRange;
using pi::checkpoint::RejectionReason;
using pi::chudnovsky::PrecisionPolicy;


namespace
{

class TemporaryDirectory
{
public:
    TemporaryDirectory()
        : path_(std::filesystem::temp_directory_path()
            / ("givemepi-resume-plan-"
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


CheckpointBlock makeBlock(
    const ComputationIdentity& identity,
    std::uint64_t start,
    std::uint64_t end,
    std::uint32_t level
)
{
    const BinaryNode node = BinarySplitter::splitSequential(
        static_cast<std::size_t>(start),
        static_cast<std::size_t>(end)
    );
    return CheckpointBlock{
        identity,
        BlockLocation::create(start, end, level, identity),
        node.P(), node.Q(), node.T()
    };
}


ManifestEntry entryFor(
    const CheckpointStore& store,
    const CheckpointBlock& block
)
{
    const auto path = store.blockPath(block.identity, block.location);
    const auto inspection = pi::checkpoint::CheckpointFileInspector::inspect(
        path, block.identity
    );
    if (!inspection.hasAcceptedFile())
        throw std::runtime_error("Resume fixture inspection failed");
    return ManifestEntry{
        block.location,
        path.filename().string(),
        inspection.file->payloadSize,
        inspection.file->checksumAlgorithm,
        inspection.file->checksum,
        CompletionState::complete
    };
}


bool rejectedFor(
    const pi::checkpoint::CheckpointResumePlan& plan,
    RejectionReason reason
)
{
    for (const auto& rejected : plan.rejectedBlocks)
    {
        for (const auto& diagnostic : rejected.validation.diagnostics())
        {
            if (diagnostic.reason == reason) return true;
        }
    }
    return false;
}

} // namespace


int main()
{
    TemporaryDirectory temporary;
    const std::filesystem::path blockDirectory = temporary.path() / "blocks";
    const CheckpointStore store(blockDirectory);
    const ComputationIdentity identity =
        ComputationIdentity::fromPrecisionPlan(PrecisionPolicy::create(1000));

    const CheckpointBlock valid = makeBlock(identity, 0, 10, 4);
    const CheckpointBlock corrupt = makeBlock(identity, 10, 20, 4);
    CheckpointBlock mathematicallyWrong = makeBlock(identity, 20, 74, 6);
    mathematicallyWrong.t.add(pi::bigint::GMPInteger(1));

    const auto validPath = store.save(valid);
    const auto corruptPath = store.save(corrupt);
    const auto wrongPath = store.save(mathematicallyWrong);
    const ManifestEntry validEntry = entryFor(store, valid);
    const ManifestEntry corruptEntry = entryFor(store, corrupt);
    const ManifestEntry wrongEntry = entryFor(store, mathematicallyWrong);
    static_cast<void>(store.saveManifest(CheckpointManifest{
        identity, {validEntry, corruptEntry, wrongEntry}
    }));

    std::fstream corruptFile(
        corruptPath, std::ios::in | std::ios::out | std::ios::binary
    );
    corruptFile.seekg(-1, std::ios::end);
    char lastByte = 0;
    corruptFile.get(lastByte);
    corruptFile.seekp(-1, std::ios::end);
    corruptFile.put(static_cast<char>(lastByte ^ 1));
    corruptFile.close();

    const std::filesystem::path orphan = blockDirectory / "orphan.checkpoint";
    std::filesystem::copy_file(validPath, orphan);
    const std::filesystem::path temporaryFile =
        blockDirectory / "partial.checkpoint.tmp.1";
    std::ofstream(temporaryFile) << "partial";

    const auto plan = CheckpointResumePlanner(
        blockDirectory,
        temporary.path() / "quarantine"
    ).create(identity);

    if (plan.acceptedBlocks.size() != 1
        || plan.acceptedBlocks.front().block.location != valid.location
        || plan.rangesToRecalculate
            != std::vector<MissingRange>({{10, 74}})
        || plan.rejectedBlocks.size() != 3
        || plan.ignoredTemporaryFiles
            != std::vector<std::filesystem::path>({temporaryFile})
        || !rejectedFor(plan, RejectionReason::checksumMismatch)
        || !rejectedFor(plan, RejectionReason::modularMismatch)
        || !rejectedFor(plan, RejectionReason::unexpectedBlockFile))
    {
        std::cerr << "Mixed checkpoint resume plan mismatch\n";
        return 1;
    }

    for (const auto& rejected : plan.rejectedBlocks)
    {
        if (!rejected.quarantinedPath.has_value()
            || std::filesystem::exists(rejected.path))
        {
            std::cerr << "Rejected resume block was not quarantined\n";
            return 1;
        }
    }

    if (!std::filesystem::exists(validPath)
        || !std::filesystem::exists(temporaryFile)
        || std::filesystem::exists(corruptPath)
        || std::filesystem::exists(wrongPath)
        || std::filesystem::exists(orphan))
    {
        std::cerr << "Resume planner final namespace mismatch\n";
        return 1;
    }

    std::cout << "CheckpointResumePlanner OK\n";
    return 0;
}
