#include "storage/VersionedChunkIndex.hpp"

#include "checkpoint/CheckpointTypes.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>


namespace pi::storage::test
{

namespace
{

// Helper to create a unique computation identity
checkpoint::ComputationIdentity createComputationIdentity(
    std::uint64_t requestedDigits,
    std::uint32_t guardDigits,
    std::uint64_t workingDigits,
    std::uint64_t termCount
)
{
    (void)workingDigits;
    const auto plan = chudnovsky::PrecisionPolicy::create(requestedDigits, guardDigits);
    auto identity = checkpoint::ComputationIdentity::fromPrecisionPlan(plan);
    // Preserve the test's request for distinct identities without violating
    // the durable identity/precision contract.
    if (termCount % 2 != 0)
        identity = checkpoint::ComputationIdentity::fromPrecisionPlan(
            chudnovsky::PrecisionPolicy::create(requestedDigits + 1, guardDigits));
    return identity;
}

// Helper to create a unique block location
checkpoint::BlockLocation createBlockLocation(
    std::uint64_t start,
    std::uint64_t end,
    std::uint32_t treeLevel
)
{
    return checkpoint::BlockLocation::create(
        start,
        end,
        treeLevel,
        createComputationIdentity(100000, 100, 200000, 1000000)
    );
}

} // namespace


void testAddAndGetVersion()
{
    std::cout << "Running testAddAndGetVersion..." << std::endl;
    
    auto index = VersionedChunkIndex::create();
    
    // Create a unique identity
    checkpoint::ComputationIdentity computation =
        createComputationIdentity(100000, 100, 200000, 1000000);
    checkpoint::BlockLocation location =
        createBlockLocation(0, 100, 5);
    ChunkIdentity identity = ChunkIdentity::create(computation, location);
    
    // Add a version
    auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    index.addVersion(identity, 1, 0x12345678, 1024, timestamp);
    
    // Verify it exists
    assert(index.hasVersions(identity));
    assert(index.getVersionCount(identity) == 1);
    assert(index.identityCount() == 1);
    assert(index.totalVersionCount() == 1);
    
    // Get all versions
    auto versions = index.getAllVersions(identity);
    assert(versions.size() == 1);
    assert(versions[0].formatVersion == 1);
    assert(versions[0].checksumValue == 0x12345678);
    assert(versions[0].storedSize == 1024);
    assert(versions[0].timestampNs == timestamp);
    
    // Get latest version
    auto latest = index.getLatestVersion(identity);
    assert(latest.has_value());
    assert(latest->formatVersion == 1);
    assert(latest->checksumValue == 0x12345678);
    
    std::cout << "  PASSED" << std::endl;
}


void testAddMultipleVersions()
{
    std::cout << "Running testAddMultipleVersions..." << std::endl;
    
    auto index = VersionedChunkIndex::create();
    
    checkpoint::ComputationIdentity computation =
        createComputationIdentity(100000, 100, 200000, 1000000);
    checkpoint::BlockLocation location =
        createBlockLocation(0, 100, 5);
    ChunkIdentity identity = ChunkIdentity::create(computation, location);
    
    // Add multiple versions
    auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    
    index.addVersion(identity, 1, 0x11111111, 512, timestamp);
    index.addVersion(identity, 1, 0x22222222, 640, timestamp + 1000);
    index.addVersion(identity, 2, 0x33333333, 480, timestamp + 2000);
    index.addVersion(identity, 1, 0x44444444, 700, timestamp + 3000);
    
    // Verify counts
    assert(index.getVersionCount(identity) == 4);
    assert(index.totalVersionCount() == 4);
    
    // Get all versions - should be sorted by timestamp
    auto versions = index.getAllVersions(identity);
    assert(versions.size() == 4);
    
    // Verify timestamps are in ascending order
    assert(versions[0].timestampNs == timestamp);
    assert(versions[1].timestampNs == timestamp + 1000);
    assert(versions[2].timestampNs == timestamp + 2000);
    assert(versions[3].timestampNs == timestamp + 3000);
    
    // Get latest version (should be the last one added)
    auto latest = index.getLatestVersion(identity);
    assert(latest.has_value());
    assert(latest->formatVersion == 1);
    assert(latest->checksumValue == 0x44444444);
    assert(latest->storedSize == 700);
    
    std::cout << "  PASSED" << std::endl;
}


void testGetLatestVersionWithFormat()
{
    std::cout << "Running testGetLatestVersionWithFormat..." << std::endl;
    
    auto index = VersionedChunkIndex::create();
    
    checkpoint::ComputationIdentity computation =
        createComputationIdentity(100000, 100, 200000, 1000000);
    checkpoint::BlockLocation location =
        createBlockLocation(0, 100, 5);
    ChunkIdentity identity = ChunkIdentity::create(computation, location);
    
    auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    
    index.addVersion(identity, 1, 0x11111111, 512, timestamp);
    index.addVersion(identity, 2, 0x22222222, 640, timestamp + 1000);
    index.addVersion(identity, 1, 0x33333333, 480, timestamp + 2000);
    
    // Get latest version with format version 1
    auto latestV1 = index.getLatestVersionWithFormat(identity, 1);
    assert(latestV1.has_value());
    assert(latestV1->formatVersion == 1);
    assert(latestV1->checksumValue == 0x33333333); // Latest format 1
    assert(latestV1->storedSize == 480);
    
    // Get latest version with format version 2
    auto latestV2 = index.getLatestVersionWithFormat(identity, 2);
    assert(latestV2.has_value());
    assert(latestV2->formatVersion == 2);
    assert(latestV2->checksumValue == 0x22222222);
    
    // Try to get non-existent format version
    auto latestV3 = index.getLatestVersionWithFormat(identity, 3);
    assert(!latestV3.has_value());
    
    std::cout << "  PASSED" << std::endl;
}


void testMultipleIdentities()
{
    std::cout << "Running testMultipleIdentities..." << std::endl;
    
    auto index = VersionedChunkIndex::create();
    
    // Create two different identities
    checkpoint::ComputationIdentity computation1 =
        createComputationIdentity(100000, 100, 200000, 1000000);
    checkpoint::ComputationIdentity computation2 =
        createComputationIdentity(100000, 100, 200000, 1000001);
    
    checkpoint::BlockLocation location1 =
        createBlockLocation(0, 100, 5);
    checkpoint::BlockLocation location2 =
        createBlockLocation(0, 100, 5);
    
    ChunkIdentity identity1 = ChunkIdentity::create(computation1, location1);
    ChunkIdentity identity2 = ChunkIdentity::create(computation2, location2);
    
    auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    
    // Add versions for both identities
    index.addVersion(identity1, 1, 0x11111111, 512, timestamp);
    index.addVersion(identity2, 1, 0x22222222, 640, timestamp);
    index.addVersion(identity1, 2, 0x33333333, 480, timestamp + 1000);
    index.addVersion(identity2, 2, 0x44444444, 550, timestamp + 2000);
    
    // Verify identity count
    assert(index.identityCount() == 2);
    
    // Verify total version count
    assert(index.totalVersionCount() == 4);
    
    // Verify each identity has correct version count
    assert(index.getVersionCount(identity1) == 2);
    assert(index.getVersionCount(identity2) == 2);
    
    // Get all identities
    auto identities = index.getAllIdentities();
    assert(identities.size() == 2);
    
    std::cout << "  PASSED" << std::endl;
}


void testNotFound()
{
    std::cout << "Running testNotFound..." << std::endl;
    
    auto index = VersionedChunkIndex::create();
    
    checkpoint::ComputationIdentity computation =
        createComputationIdentity(100000, 100, 200000, 1000000);
    checkpoint::BlockLocation location =
        createBlockLocation(0, 100, 5);
    ChunkIdentity identity = ChunkIdentity::create(computation, location);
    
    // Try to get version for identity that doesn't exist
    assert(!index.hasVersions(identity));
    assert(index.getVersionCount(identity) == 0);
    
    auto versions = index.getAllVersions(identity);
    assert(versions.empty());
    
    auto latest = index.getLatestVersion(identity);
    assert(!latest.has_value());
    
    auto latestWithFormat = index.getLatestVersionWithFormat(identity, 1);
    assert(!latestWithFormat.has_value());
    
    std::cout << "  PASSED" << std::endl;
}


void testClear()
{
    std::cout << "Running testClear..." << std::endl;
    
    auto index = VersionedChunkIndex::create();
    
    checkpoint::ComputationIdentity computation =
        createComputationIdentity(100000, 100, 200000, 1000000);
    checkpoint::BlockLocation location =
        createBlockLocation(0, 100, 5);
    ChunkIdentity identity = ChunkIdentity::create(computation, location);
    
    auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    index.addVersion(identity, 1, 0x12345678, 1024, timestamp);
    
    // Add more
    index.addVersion(identity, 2, 0x23456789, 800, timestamp + 1000);
    
    assert(index.identityCount() == 1);
    assert(index.totalVersionCount() == 2);
    
    // Clear
    index.clear();
    
    assert(index.identityCount() == 0);
    assert(index.totalVersionCount() == 0);
    assert(!index.hasVersions(identity));
    
    std::cout << "  PASSED" << std::endl;
}


void testSerializeDeserialize()
{
    std::cout << "Running testSerializeDeserialize..." << std::endl;
    
    auto index = VersionedChunkIndex::create();
    
    checkpoint::ComputationIdentity computation =
        createComputationIdentity(100000, 100, 200000, 1000000);
    checkpoint::BlockLocation location =
        createBlockLocation(0, 100, 5);
    ChunkIdentity identity = ChunkIdentity::create(computation, location);
    
    auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    
    // Add multiple versions
    index.addVersion(identity, 1, 0x11111111, 512, timestamp);
    index.addVersion(identity, 2, 0x22222222, 640, timestamp + 1000);
    index.addVersion(identity, 1, 0x33333333, 480, timestamp + 2000);
    index.addVersion(identity, 1, 0x44444444, 700, timestamp + 3000);
    
    // Serialize
    auto bytes = index.serialize();
    assert(!bytes.empty());
    
    std::cout << "  Serialized size: " << bytes.size() << " bytes" << std::endl;
    
    // Deserialize
    auto deserializedIndex = VersionedChunkIndex::deserialize(std::vector<std::uint8_t>(bytes));
    
    // Verify deserialized index
    assert(deserializedIndex.identityCount() == 1);
    assert(deserializedIndex.totalVersionCount() == 4);
    assert(deserializedIndex.hasVersions(identity));
    assert(deserializedIndex.getVersionCount(identity) == 4);
    
    auto versions = deserializedIndex.getAllVersions(identity);
    assert(versions.size() == 4);
    
    // Verify checksums
    assert(versions[0].checksumValue == 0x11111111);
    assert(versions[1].checksumValue == 0x22222222);
    assert(versions[2].checksumValue == 0x33333333);
    assert(versions[3].checksumValue == 0x44444444);
    
    // Verify stored sizes
    assert(versions[0].storedSize == 512);
    assert(versions[1].storedSize == 640);
    assert(versions[2].storedSize == 480);
    assert(versions[3].storedSize == 700);
    
    std::cout << "  PASSED" << std::endl;
}


void testSerializationIdempotency()
{
    std::cout << "Running testSerializationIdempotency..." << std::endl;
    
    auto index = VersionedChunkIndex::create();
    
    checkpoint::ComputationIdentity computation =
        createComputationIdentity(100000, 100, 200000, 1000000);
    checkpoint::BlockLocation location =
        createBlockLocation(0, 100, 5);
    ChunkIdentity identity = ChunkIdentity::create(computation, location);
    
    auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    
    index.addVersion(identity, 1, 0x11111111, 512, timestamp);
    index.addVersion(identity, 2, 0x22222222, 640, timestamp + 1000);
    index.addVersion(identity, 1, 0x33333333, 480, timestamp + 2000);
    
    // Serialize twice
    auto bytes1 = index.serialize();
    auto bytes2 = index.serialize();
    
    assert(bytes1 == bytes2);
    
    // Deserialize and verify
    auto index2 = VersionedChunkIndex::deserialize(bytes1);
    auto index3 = VersionedChunkIndex::deserialize(bytes2);
    
    assert(index2.getAllVersions(identity) == index3.getAllVersions(identity));
    
    std::cout << "  PASSED" << std::endl;
}


void runAllTests()
{
    std::cout << "=== VersionedChunkIndex Tests ===" << std::endl;
    std::cout << std::endl;
    
    try
    {
        testAddAndGetVersion();
        testAddMultipleVersions();
        testGetLatestVersionWithFormat();
        testMultipleIdentities();
        testNotFound();
        testClear();
        testSerializeDeserialize();
        testSerializationIdempotency();
        
        std::cout << std::endl;
        std::cout << "=== All tests PASSED ===" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << std::endl;
        std::cerr << "=== Test FAILED: " << e.what() << std::endl;
        std::terminate();
    }
}


} // namespace pi::storage::test

int main()
{
    namespace test = pi::storage::test;
    test::runAllTests();
    return 0;
}
