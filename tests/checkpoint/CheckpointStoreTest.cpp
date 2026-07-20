#include "checkpoint/CheckpointStore.hpp"
#include "checkpoint/AtomicFileCommit.hpp"

#include "chudnovsky/PrecisionPolicy.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <unistd.h>


using pi::bigint::GMPInteger;
using pi::checkpoint::BlockLocation;
using pi::checkpoint::AtomicCommitStage;
using pi::checkpoint::AtomicFileCommit;
using pi::checkpoint::CheckpointBlock;
using pi::checkpoint::CheckpointStore;
using pi::checkpoint::ComputationIdentity;
using pi::chudnovsky::PrecisionPolicy;


namespace
{

AtomicCommitStage injectedStage = AtomicCommitStage::tempCreated;

void injectFailure(AtomicCommitStage stage)
{
    if (stage == injectedStage)
    {
        throw std::runtime_error("injected atomic commit failure");
    }
}

class TemporaryDirectory
{
public:
    TemporaryDirectory()
        : path_(std::filesystem::temp_directory_path()
            / ("givemepi-checkpoint-store-"
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

    [[nodiscard]] const std::filesystem::path& path() const noexcept
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};


bool sameBlock(const CheckpointBlock& left, const CheckpointBlock& right)
{
    return left.identity == right.identity
        && left.location == right.location
        && left.p.compare(right.p) == 0
        && left.q.compare(right.q) == 0
        && left.t.compare(right.t) == 0;
}


std::string readText(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()
    );
}

} // namespace


int main()
{
    TemporaryDirectory temporary;
    const CheckpointStore store(temporary.path() / "blocks");
    const ComputationIdentity identity =
        ComputationIdentity::fromPrecisionPlan(PrecisionPolicy::create(1000));
    const BlockLocation location = BlockLocation::create(10, 20, 3, identity);

    GMPInteger large;
    large.setPowerOfTen(10000);
    GMPInteger negative(123456789);
    negative.mul(large);
    negative.negate();

    const CheckpointBlock block{
        identity, location, large, GMPInteger(987654321), negative
    };

    const std::filesystem::path expected = temporary.path() / "blocks"
        / "pqt-v1-a1-av1-fv1-d1000-g32-w1032-n74-r10-20-l3.checkpoint";

    if (store.blockPath(identity, location) != expected)
    {
        std::cerr << "Deterministic checkpoint path mismatch\n";
        return 1;
    }

    const std::filesystem::path saved = store.save(block);
    const CheckpointBlock loaded = store.load(identity, location);

    if (saved != expected || !std::filesystem::is_regular_file(saved)
        || !sameBlock(block, loaded))
    {
        std::cerr << "Checkpoint store round trip failed\n";
        return 1;
    }

    const auto firstSize = std::filesystem::file_size(saved);
    static_cast<void>(store.save(block));

    if (std::filesystem::file_size(saved) != firstSize)
    {
        std::cerr << "Deterministic checkpoint replacement failed\n";
        return 1;
    }

    std::size_t entryCount = 0;
    for (const auto& entry : std::filesystem::directory_iterator(saved.parent_path()))
    {
        ++entryCount;
        if (entry.path().extension() != ".checkpoint")
        {
            std::cerr << "Checkpoint temp file remained visible\n";
            return 1;
        }
    }

    if (entryCount != 1)
    {
        std::cerr << "Unexpected checkpoint store entry count\n";
        return 1;
    }

    const std::filesystem::path atomicPath = temporary.path() / "atomic-final";
    const std::string original = "original durable content";
    const std::string replacement = "replacement content";
    AtomicFileCommit::write(
        atomicPath,
        std::span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(original.data()),
            original.size()
        )
    );

    for (const AtomicCommitStage stage : {
            AtomicCommitStage::tempCreated,
            AtomicCommitStage::contentWritten,
            AtomicCommitStage::fileSynced,
            AtomicCommitStage::fileClosed,
            AtomicCommitStage::beforeRename})
    {
        injectedStage = stage;
        bool failed = false;
        try
        {
            AtomicFileCommit::write(
                atomicPath,
                std::span<const std::uint8_t>(
                    reinterpret_cast<const std::uint8_t*>(replacement.data()),
                    replacement.size()
                ),
                injectFailure
            );
        }
        catch (const std::runtime_error&)
        {
            failed = true;
        }

        if (!failed || readText(atomicPath) != original)
        {
            std::cerr << "Atomic failure replaced the existing final file\n";
            return 1;
        }
    }

    for (const auto& entry : std::filesystem::directory_iterator(temporary.path()))
    {
        if (entry.path().filename().string().find(".tmp.") != std::string::npos)
        {
            std::cerr << "Atomic failure left a temp file behind\n";
            return 1;
        }
    }

    std::cout << "CheckpointStore OK\n";
    return 0;
}
