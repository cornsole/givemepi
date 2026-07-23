#include "storage/StorageManager.hpp"

#include "bigint/GMPInteger.hpp"
#include "chudnovsky/PrecisionPolicy.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{

pi::storage::Chunk makeChunk(
    const pi::checkpoint::ComputationIdentity& computation,
    std::uint64_t start
)
{
    const auto identity = pi::storage::ChunkIdentity::create(
        computation,
        pi::checkpoint::BlockLocation::create(
            start, start + 4, 0, computation));
    pi::bigint::GMPInteger p(12 + start), q(5 + start), t(7 + start);
    return pi::storage::Chunk{
        pi::storage::ChunkCodec::createMetadata(identity, p, q, t),
        p, q, t};
}

void flipLastByte(const std::filesystem::path& path)
{
    const auto size = std::filesystem::file_size(path);
    if (size == 0) throw std::runtime_error("test chunk unexpectedly empty");
    std::fstream file(path, std::ios::in | std::ios::out | std::ios::binary);
    file.seekg(static_cast<std::streamoff>(size - 1));
    char value = 0;
    file.read(&value, 1);
    file.seekp(static_cast<std::streamoff>(size - 1));
    value ^= static_cast<char>(0x01);
    file.write(&value, 1);
}

} // namespace

int main()
{
    using namespace pi::checkpoint;
    using namespace pi::chudnovsky;
    using namespace pi::storage;

    const auto directory = std::filesystem::temp_directory_path() / "givemepi-pr0025-manager-test";
    std::error_code ec;
    std::filesystem::remove_all(directory, ec);
    StoragePolicy policy;
    policy.directory = directory;

    const auto computation = ComputationIdentity::fromPrecisionPlan(PrecisionPolicy::create(1000));
    const Chunk chunk = makeChunk(computation, 0);
    const auto identity = chunk.metadata.identity;

    {
        StorageManager manager(policy);
        const auto id = manager.store(chunk);
        const auto snapshot = manager.snapshot({{id, 128}});
        if (!manager.contains(identity) || snapshot.indexedChunks != 1
            || snapshot.residentBytes != 128 || !manager.load(identity).has_value())
        {
            std::cerr << "StorageManager initial publish failed\n";
            return 1;
        }
    }

    {
        StorageManager restarted(policy);
        const auto loaded = restarted.load(identity);
        if (!loaded.has_value() || loaded->p.compare(chunk.p) != 0
            || loaded->q.compare(chunk.q) != 0
            || loaded->t.compare(chunk.t) != 0)
        {
            std::cerr << "StorageManager restart reload failed\n";
            return 1;
        }
        if (!restarted.remove(identity) || restarted.contains(identity)
            || restarted.snapshot().indexedChunks != 0)
        {
            std::cerr << "StorageManager removal failed\n";
            return 1;
        }
    }

    // A restart must not make a damaged payload readable. The index remains
    // present so the failure is visible to the load path rather than turning
    // into a silent cache miss.
    {
        const auto corruptDirectory = directory.string() + "-corrupt";
        const auto corruptPath = std::filesystem::path(corruptDirectory);
        std::filesystem::remove_all(corruptPath, ec);
        StoragePolicy corruptPolicy = policy;
        corruptPolicy.directory = corruptPath;
        const Chunk corruptChunk = makeChunk(computation, 0);
        StorageManager manager(corruptPolicy);
        const auto id = manager.store(corruptChunk);
        ChunkStore rawStore(corruptPolicy);
        flipLastByte(rawStore.chunkPath(id));

        bool rejected = false;
        try
        {
            StorageManager restarted(corruptPolicy);
            static_cast<void>(restarted.load(corruptChunk.metadata.identity));
        }
        catch (const std::runtime_error&)
        {
            rejected = true;
        }
        if (!rejected)
        {
            std::cerr << "Corrupted chunk survived restart/load\n";
            return 1;
        }
        std::filesystem::remove_all(corruptPath, ec);
    }

    // A damaged durable index must fail startup; rebuilding an index from a
    // partially trusted file would violate the restart integrity contract.
    {
        const auto indexDirectory = directory.string() + "-index-corrupt";
        const auto indexPath = std::filesystem::path(indexDirectory);
        std::filesystem::remove_all(indexPath, ec);
        StoragePolicy indexPolicy = policy;
        indexPolicy.directory = indexPath;
        StorageManager manager(indexPolicy);
        static_cast<void>(manager.store(makeChunk(computation, 0)));
        flipLastByte(indexPath / "chunk-index-v1.bin");

        bool rejected = false;
        try
        {
            StorageManager restarted(indexPolicy);
            (void)restarted;
        }
        catch (const std::exception&)
        {
            rejected = true;
        }
        if (!rejected)
        {
            std::cerr << "Corrupted chunk index survived restart\n";
            return 1;
        }
        std::filesystem::remove_all(indexPath, ec);
    }

    std::filesystem::remove_all(directory, ec);
    std::cout << "StorageManager OK\n";
    return 0;
}
