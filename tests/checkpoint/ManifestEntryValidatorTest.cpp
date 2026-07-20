#include "checkpoint/ManifestEntryValidator.hpp"

#include "checkpoint/CheckpointStore.hpp"
#include "chudnovsky/PrecisionPolicy.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>


using pi::bigint::GMPInteger;
using pi::checkpoint::BlockLocation;
using pi::checkpoint::CheckpointBlock;
using pi::checkpoint::CheckpointFileInspector;
using pi::checkpoint::CheckpointStore;
using pi::checkpoint::CompletionState;
using pi::checkpoint::ComputationIdentity;
using pi::checkpoint::ManifestEntry;
using pi::checkpoint::ManifestEntryValidator;
using pi::checkpoint::RejectionReason;
using pi::checkpoint::ValidationResult;
using pi::chudnovsky::PrecisionPolicy;


namespace
{

class TemporaryDirectory
{
public:
    TemporaryDirectory()
        : path_(std::filesystem::temp_directory_path()
            / ("givemepi-entry-validator-"
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


bool hasReason(const ValidationResult& result, RejectionReason reason)
{
    return std::any_of(
        result.diagnostics().begin(),
        result.diagnostics().end(),
        [reason](const auto& diagnostic) { return diagnostic.reason == reason; }
    );
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
    const std::filesystem::path path = store.save(CheckpointBlock{
        identity, location, GMPInteger(256), GMPInteger(1000), negative
    });
    const auto inspection = CheckpointFileInspector::inspect(path, identity);
    if (!inspection.hasAcceptedFile())
    {
        std::cerr << "Fixture block inspection failed\n";
        return 1;
    }

    const ManifestEntry validEntry{
        location,
        path.filename().string(),
        inspection.file->payloadSize,
        inspection.file->checksumAlgorithm,
        inspection.file->checksum,
        CompletionState::complete
    };
    if (!ManifestEntryValidator::validate(
            validEntry, *inspection.file, identity
        ).isAccepted())
    {
        std::cerr << "Valid manifest entry was rejected\n";
        return 1;
    }

    ManifestEntry invalidEntry = validEntry;
    invalidEntry.location = BlockLocation::create(10, 20, 3, identity);
    invalidEntry.filename = "wrong.checkpoint";
    ++invalidEntry.payloadSize;
    invalidEntry.checksum ^= 1;
    invalidEntry.completion = CompletionState::incomplete;
    const ValidationResult invalid = ManifestEntryValidator::validate(
        invalidEntry, *inspection.file, identity
    );

    const ComputationIdentity otherIdentity =
        ComputationIdentity::fromPrecisionPlan(PrecisionPolicy::create(2000));
    const ValidationResult incompatible = ManifestEntryValidator::validate(
        validEntry, *inspection.file, otherIdentity
    );

    if (!invalid.isRejected()
        || !hasReason(invalid, RejectionReason::locationMismatch)
        || !hasReason(invalid, RejectionReason::filenameMismatch)
        || !hasReason(invalid, RejectionReason::sizeMismatch)
        || !hasReason(invalid, RejectionReason::entryChecksumMismatch)
        || !hasReason(invalid, RejectionReason::incompleteEntry)
        || !hasReason(
            incompatible,
            RejectionReason::manifestIdentityMismatch
        ))
    {
        std::cerr << "Manifest entry mismatch diagnostics are incomplete\n";
        return 1;
    }

    std::cout << "ManifestEntryValidator OK\n";
    return 0;
}
