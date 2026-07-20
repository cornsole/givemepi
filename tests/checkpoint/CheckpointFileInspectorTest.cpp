#include "checkpoint/CheckpointFileInspector.hpp"
#include "checkpoint/CheckpointStore.hpp"

#include "chudnovsky/PrecisionPolicy.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>


using pi::bigint::GMPInteger;
using pi::checkpoint::BlockLocation;
using pi::checkpoint::CheckpointBlock;
using pi::checkpoint::CheckpointFileInspection;
using pi::checkpoint::CheckpointFileInspector;
using pi::checkpoint::CheckpointStore;
using pi::checkpoint::ChecksumAlgorithm;
using pi::checkpoint::ComputationIdentity;
using pi::checkpoint::RejectionReason;
using pi::chudnovsky::PrecisionPolicy;


namespace
{

class TemporaryDirectory
{
public:
    TemporaryDirectory()
        : path_(std::filesystem::temp_directory_path()
            / ("givemepi-inspector-"
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


bool hasReason(
    const CheckpointFileInspection& inspection,
    RejectionReason reason
)
{
    return !inspection.validation.diagnostics().empty()
        && inspection.validation.diagnostics().front().reason == reason;
}


void overwriteByte(
    const std::filesystem::path& path,
    std::streamoff offset,
    char value
)
{
    std::fstream file(path, std::ios::in | std::ios::out | std::ios::binary);
    file.seekp(offset);
    file.put(value);
}

} // namespace


int main()
{
    TemporaryDirectory temporary;
    const CheckpointStore store(temporary.path() / "blocks");
    const ComputationIdentity identity =
        ComputationIdentity::fromPrecisionPlan(PrecisionPolicy::create(1000));
    const BlockLocation location = BlockLocation::create(0, 10, 3, identity);
    GMPInteger negative(42);
    negative.negate();
    const CheckpointBlock block{
        identity, location, GMPInteger(256), GMPInteger(1000), negative
    };
    const std::filesystem::path saved = store.save(block);

    const CheckpointFileInspection accepted =
        CheckpointFileInspector::inspect(saved, identity);
    if (!accepted.hasAcceptedFile()
        || accepted.file->path != saved
        || accepted.file->fileSize != std::filesystem::file_size(saved)
        || accepted.file->checksumAlgorithm != ChecksumAlgorithm::crc32c
        || accepted.file->block.location != location)
    {
        std::cerr << "Accepted checkpoint inspection mismatch\n";
        return 1;
    }

    const CheckpointFileInspection temporaryFile =
        CheckpointFileInspector::inspect(
            saved.parent_path() / "block.checkpoint.tmp.1", identity
        );
    const CheckpointFileInspection missing = CheckpointFileInspector::inspect(
        saved.parent_path() / "missing.checkpoint", identity
    );
    const CheckpointFileInspection unexpected = CheckpointFileInspector::inspect(
        saved.parent_path() / "manifest.bin", identity
    );
    const ComputationIdentity otherIdentity =
        ComputationIdentity::fromPrecisionPlan(PrecisionPolicy::create(2000));
    const CheckpointFileInspection incompatible =
        CheckpointFileInspector::inspect(saved, otherIdentity);

    const std::filesystem::path badMagic = temporary.path() / "bad-magic.checkpoint";
    std::filesystem::copy_file(saved, badMagic);
    overwriteByte(badMagic, 0, 'X');

    const std::filesystem::path badVersion = temporary.path() / "bad-version.checkpoint";
    std::filesystem::copy_file(saved, badVersion);
    overwriteByte(badVersion, 9, 2);

    const std::filesystem::path badChecksum = temporary.path() / "bad-crc.checkpoint";
    std::filesystem::copy_file(saved, badChecksum);
    overwriteByte(
        badChecksum,
        static_cast<std::streamoff>(std::filesystem::file_size(badChecksum) - 1),
        43
    );

    if (!temporaryFile.validation.isIgnored()
        || !hasReason(temporaryFile, RejectionReason::temporaryFile)
        || !missing.validation.isRejected()
        || !hasReason(missing, RejectionReason::fileReadFailed)
        || !hasReason(unexpected, RejectionReason::unexpectedBlockFile)
        || !hasReason(incompatible, RejectionReason::invalidIdentity)
        || !hasReason(
            CheckpointFileInspector::inspect(badMagic, identity),
            RejectionReason::invalidMagic
        )
        || !hasReason(
            CheckpointFileInspector::inspect(badVersion, identity),
            RejectionReason::unsupportedVersion
        )
        || !hasReason(
            CheckpointFileInspector::inspect(badChecksum, identity),
            RejectionReason::checksumMismatch
        ))
    {
        std::cerr << "Checkpoint inspection rejection mismatch\n";
        return 1;
    }

    std::cout << "CheckpointFileInspector OK\n";
    return 0;
}
