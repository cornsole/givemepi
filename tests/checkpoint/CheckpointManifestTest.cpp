#include "checkpoint/CheckpointManifest.hpp"
#include "checkpoint/CheckpointStore.hpp"

#include "chudnovsky/PrecisionPolicy.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>


using pi::checkpoint::BlockLocation;
using pi::checkpoint::CheckpointManifest;
using pi::checkpoint::CheckpointManifestCodec;
using pi::checkpoint::CheckpointStore;
using pi::checkpoint::ChecksumAlgorithm;
using pi::checkpoint::CompletionState;
using pi::checkpoint::ComputationIdentity;
using pi::checkpoint::ManifestEntry;
using pi::chudnovsky::PrecisionPolicy;


namespace
{

class TemporaryDirectory
{
public:
    TemporaryDirectory()
        : path_(std::filesystem::temp_directory_path()
            / ("givemepi-manifest-"
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


bool rejects(const std::vector<std::uint8_t>& bytes)
{
    try
    {
        static_cast<void>(CheckpointManifestCodec::decode(bytes));
    }
    catch (const std::invalid_argument&)
    {
        return true;
    }
    return false;
}

} // namespace


int main()
{
    const ComputationIdentity identity =
        ComputationIdentity::fromPrecisionPlan(PrecisionPolicy::create(1000));
    const ManifestEntry later{
        BlockLocation::create(10, 20, 3, identity),
        "block-later.checkpoint",
        200,
        ChecksumAlgorithm::crc32c,
        0x12345678U,
        CompletionState::complete
    };
    const ManifestEntry earlier{
        BlockLocation::create(0, 10, 3, identity),
        "block-earlier.checkpoint",
        100,
        ChecksumAlgorithm::crc32c,
        0x90ABCDEFU,
        CompletionState::incomplete
    };

    const CheckpointManifest insertionA{identity, {later, earlier}};
    const CheckpointManifest insertionB{identity, {earlier, later}};
    const auto bytesA = CheckpointManifestCodec::encode(insertionA);
    const auto bytesB = CheckpointManifestCodec::encode(insertionB);
    const CheckpointManifest decoded = CheckpointManifestCodec::decode(bytesA);

    if (bytesA != bytesB
        || decoded != insertionB
        || CheckpointManifestCodec::encode(decoded) != bytesA)
    {
        std::cerr << "Canonical manifest round trip failed\n";
        return 1;
    }

    bool rejectedDuplicate = false;
    try
    {
        static_cast<void>(CheckpointManifestCodec::encode(
            CheckpointManifest{identity, {earlier, earlier}}
        ));
    }
    catch (const std::invalid_argument&)
    {
        rejectedDuplicate = true;
    }

    std::vector<std::uint8_t> corrupted = bytesA;
    corrupted.back() ^= 1;
    if (!rejectedDuplicate || !rejects(corrupted))
    {
        std::cerr << "Invalid manifest was accepted\n";
        return 1;
    }

    TemporaryDirectory temporary;
    const CheckpointStore store(temporary.path() / "checkpoint");
    const auto manifestPath = store.saveManifest(insertionA);
    const CheckpointManifest loaded = store.loadManifest(identity);

    if (loaded != insertionB
        || manifestPath != store.manifestPath(identity)
        || !std::filesystem::is_regular_file(manifestPath))
    {
        std::cerr << "Atomic manifest store round trip failed\n";
        return 1;
    }

    std::cout << "CheckpointManifest OK\n";
    return 0;
}
