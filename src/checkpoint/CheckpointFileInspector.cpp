#include "checkpoint/CheckpointFileInspector.hpp"

#include "checkpoint/CheckpointBlockValidator.hpp"

#include <exception>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>


namespace pi::checkpoint
{

namespace
{

CheckpointFileInspection rejected(
    ValidationStage stage,
    RejectionReason reason,
    std::string detail
)
{
    return CheckpointFileInspection{
        ValidationResult::rejected(
            ValidationDiagnostic{stage, reason, std::move(detail)}
        ),
        std::nullopt
    };
}

} // namespace


CheckpointFileInspection CheckpointFileInspector::inspect(
    const std::filesystem::path& path,
    const ComputationIdentity& expectedIdentity
)
{
    if (path.filename().string().find(".tmp.") != std::string::npos)
    {
        return CheckpointFileInspection{
            ValidationResult::ignored(
                RejectionReason::temporaryFile,
                "Incomplete checkpoint temp file"
            ),
            std::nullopt
        };
    }

    if (path.extension() != ".checkpoint")
    {
        return rejected(
            ValidationStage::discovery,
            RejectionReason::unexpectedBlockFile,
            "Candidate does not use the final checkpoint extension"
        );
    }

    try
    {
        const std::uintmax_t rawSize = std::filesystem::file_size(path);
        if (rawSize > std::numeric_limits<std::size_t>::max()
            || rawSize > std::numeric_limits<std::uint64_t>::max()
            || rawSize > static_cast<std::uintmax_t>(
                std::numeric_limits<std::streamsize>::max()))
        {
            return rejected(
                ValidationStage::discovery,
                RejectionReason::fileReadFailed,
                "Checkpoint file exceeds supported memory limits"
            );
        }

        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(rawSize));
        std::ifstream input(path, std::ios::binary);
        if (!input || (rawSize != 0
            && !input.read(
                reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()))))
        {
            return rejected(
                ValidationStage::discovery,
                RejectionReason::fileReadFailed,
                "Checkpoint file could not be read completely"
            );
        }

        CheckpointBlockValidation blockValidation =
            CheckpointBlockValidator::validate(bytes, expectedIdentity);
        if (!blockValidation.hasAcceptedBlock())
        {
            return CheckpointFileInspection{
                std::move(blockValidation.validation),
                std::nullopt
            };
        }

        ValidatedCheckpointBlock validated =
            std::move(*blockValidation.block);
        return CheckpointFileInspection{
            ValidationResult::accepted(),
            InspectedCheckpointFile{
                path,
                validated.encodedSize,
                validated.payloadSize,
                validated.checksumAlgorithm,
                validated.checksum,
                std::move(validated.block)
            }
        };
    }
    catch (const std::exception& error)
    {
        return rejected(
            ValidationStage::discovery,
            RejectionReason::fileReadFailed,
            error.what()
        );
    }
}

} // namespace pi::checkpoint
