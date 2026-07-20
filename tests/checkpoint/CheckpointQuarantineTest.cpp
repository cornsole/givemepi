#include "checkpoint/CheckpointQuarantine.hpp"

#include "checkpoint/CheckpointFileInspector.hpp"
#include "checkpoint/CheckpointStore.hpp"
#include "chudnovsky/PrecisionPolicy.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <unistd.h>


using pi::bigint::GMPInteger;
using pi::checkpoint::BlockLocation;
using pi::checkpoint::CheckpointBlock;
using pi::checkpoint::CheckpointFileInspector;
using pi::checkpoint::CheckpointQuarantine;
using pi::checkpoint::CheckpointStore;
using pi::checkpoint::ComputationIdentity;
using pi::checkpoint::QuarantineOutcome;
using pi::checkpoint::RejectionReason;
using pi::checkpoint::ValidationDiagnostic;
using pi::checkpoint::ValidationResult;
using pi::checkpoint::ValidationStage;
using pi::chudnovsky::PrecisionPolicy;


namespace
{

class TemporaryDirectory
{
public:
    TemporaryDirectory()
        : path_(std::filesystem::temp_directory_path()
            / ("givemepi-quarantine-"
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


std::string readText(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()
    );
}


ValidationResult sampleRejection()
{
    return ValidationResult::rejected(ValidationDiagnostic{
        ValidationStage::checksum,
        RejectionReason::checksumMismatch,
        "fixture CRC mismatch"
    });
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
    const std::filesystem::path source = store.save(CheckpointBlock{
        identity, location, GMPInteger(256), GMPInteger(1000), negative
    });

    std::fstream corrupt(source, std::ios::in | std::ios::out | std::ios::binary);
    corrupt.seekp(-1, std::ios::end);
    corrupt.put(43);
    corrupt.close();
    const auto inspection = CheckpointFileInspector::inspect(source, identity);
    if (!inspection.validation.isRejected())
    {
        std::cerr << "Corrupt fixture was not rejected\n";
        return 1;
    }

    const CheckpointQuarantine quarantine(temporary.path() / "quarantine");
    const QuarantineOutcome moved = quarantine.quarantine(
        source, inspection.validation
    );
    if (!moved.operation.isAccepted()
        || !moved.quarantinedPath.has_value()
        || !moved.diagnosticPath.has_value()
        || std::filesystem::exists(source)
        || !std::filesystem::is_regular_file(*moved.quarantinedPath)
        || !std::filesystem::is_regular_file(*moved.diagnosticPath)
        || moved.quarantinedPath->filename().string().find(
            "checksum_mismatch"
        ) == std::string::npos)
    {
        std::cerr << "Rejected checkpoint was not quarantined\n";
        return 1;
    }

    const std::string record = readText(*moved.diagnosticPath);
    if (record.find("reason=checksum_mismatch") == std::string::npos
        || record.find("stage=checksum") == std::string::npos
        || record.find(source.string()) == std::string::npos)
    {
        std::cerr << "Quarantine diagnostic record mismatch\n";
        return 1;
    }

    const std::filesystem::path tempFile =
        temporary.path() / "block.checkpoint.tmp.123";
    std::ofstream(tempFile) << "partial";
    const QuarantineOutcome ignored = quarantine.quarantine(
        tempFile, sampleRejection()
    );
    if (!ignored.operation.isIgnored() || !std::filesystem::exists(tempFile))
    {
        std::cerr << "Temporary checkpoint was quarantined\n";
        return 1;
    }

    const std::filesystem::path acceptedFile = temporary.path() / "accepted.checkpoint";
    std::ofstream(acceptedFile) << "accepted";
    const QuarantineOutcome refused = quarantine.quarantine(
        acceptedFile, ValidationResult::accepted()
    );
    if (!refused.operation.isRejected()
        || !std::filesystem::exists(acceptedFile))
    {
        std::cerr << "Accepted file entered quarantine\n";
        return 1;
    }

    const std::filesystem::path blockedDirectory = temporary.path() / "not-directory";
    std::ofstream(blockedDirectory) << "file";
    const std::filesystem::path preserved = temporary.path() / "preserved.checkpoint";
    std::ofstream(preserved) << "preserved";
    const QuarantineOutcome failed = CheckpointQuarantine(
        blockedDirectory
    ).quarantine(preserved, sampleRejection());
    if (!failed.operation.isRejected() || !std::filesystem::exists(preserved))
    {
        std::cerr << "Failed quarantine did not preserve source\n";
        return 1;
    }

    std::cout << "CheckpointQuarantine OK\n";
    return 0;
}
