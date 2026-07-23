#include "storage/ChunkIndex.hpp"

#include "chudnovsky/PrecisionPolicy.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace
{
template<typename F>
bool rejects(F&& f)
{
    try { f(); } catch (const std::invalid_argument&) { return true; }
    return false;
}
}

int main()
{
    using namespace pi::checkpoint;
    using namespace pi::chudnovsky;
    using namespace pi::storage;

    const auto computation = ComputationIdentity::fromPrecisionPlan(PrecisionPolicy::create(1000));
    const auto location = BlockLocation::create(0, 10, 1, computation);
    const auto identity = ChunkIdentity::create(computation, location);
    const ChunkIndexEntry entry{identity, identity.deterministicFilename(), 100, 80,
        ChunkChecksumAlgorithm::crc32c, 0x12345678U, chunkFormatVersion};

    auto index = ChunkIndex::create();
    index.add(entry);
    if (!index.contains(identity) || index.at(identity) != entry || index.storedBytes() != 80)
    {
        std::cerr << "ChunkIndex entry mismatch\n";
        return 1;
    }
    if (!rejects([&] { index.add(entry); }))
    {
        std::cerr << "Duplicate identity was accepted\n";
        return 1;
    }

    const auto overlappingLocation = BlockLocation::create(5, 15, 1, computation);
    const auto overlappingIdentity = ChunkIdentity::create(computation, overlappingLocation);
    const ChunkIndexEntry overlapping{overlappingIdentity, overlappingIdentity.deterministicFilename(), 20, 20,
        ChunkChecksumAlgorithm::crc32c, 1, chunkFormatVersion};
    if (!rejects([&] { index.add(overlapping); }))
    {
        std::cerr << "Overlapping range was accepted\n";
        return 1;
    }

    const auto encoded = index.serialize();
    if (ChunkIndex::deserialize(encoded).entries() != index.entries())
    {
        std::cerr << "ChunkIndex serialization mismatch\n";
        return 1;
    }
    auto corrupted = encoded;
    corrupted[corrupted.size() - 1] ^= 1;
    if (!rejects([&] { static_cast<void>(ChunkIndex::deserialize(corrupted)); }))
    {
        std::cerr << "Corrupted index was accepted\n";
        return 1;
    }

    const auto path = std::filesystem::temp_directory_path() / "givemepi-chunk-index-test.idx";
    index.save(path);
    if (ChunkIndex::load(path).entries() != index.entries())
    {
        std::cerr << "ChunkIndex file reload mismatch\n";
        return 1;
    }
    std::filesystem::remove(path);
    std::cout << "ChunkIndex OK\n";
    return 0;
}
