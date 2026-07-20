#include "checkpoint/CheckpointQuarantine.hpp"

#include "checkpoint/AtomicFileCommit.hpp"

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <exception>
#include <fcntl.h>
#include <stdexcept>
#include <span>
#include <string>
#include <system_error>
#include <unistd.h>
#include <utility>


namespace pi::checkpoint
{

namespace
{

std::atomic<std::uint64_t> quarantineSequence{0};


ValidationResult failure(std::string detail)
{
    return ValidationResult::rejected(ValidationDiagnostic{
        ValidationStage::quarantine,
        RejectionReason::quarantineFailed,
        std::move(detail)
    });
}


std::string oneLine(std::string value)
{
    for (char& character : value)
    {
        if (character == '\n' || character == '\r') character = ' ';
    }
    return value;
}


void syncDirectory(const std::filesystem::path& directory)
{
    const int descriptor = ::open(
        directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC
    );
    if (descriptor < 0)
    {
        throw std::system_error(
            errno, std::generic_category(), "open quarantine directory"
        );
    }

    const int syncResult = ::fsync(descriptor);
    const int syncError = errno;
    const int closeResult = ::close(descriptor);
    const int closeError = errno;

    if (syncResult != 0)
        throw std::system_error(
            syncError, std::generic_category(), "sync quarantine directory"
        );
    if (closeResult != 0)
        throw std::system_error(
            closeError, std::generic_category(), "close quarantine directory"
        );
}


std::filesystem::path uniqueDestination(
    const std::filesystem::path& directory,
    const std::filesystem::path& source,
    RejectionReason reason
)
{
    constexpr std::uint32_t maximumAttempts = 128;
    for (std::uint32_t attempt = 0; attempt < maximumAttempts; ++attempt)
    {
        const std::uint64_t sequence = quarantineSequence.fetch_add(
            1, std::memory_order_relaxed
        );
        const std::filesystem::path candidate = directory / (
            source.filename().string()
            + ".rejected."
            + std::string(toString(reason))
            + "."
            + std::to_string(static_cast<unsigned long long>(::getpid()))
            + "."
            + std::to_string(sequence)
        );
        if (!std::filesystem::exists(candidate)) return candidate;
    }
    throw std::runtime_error("Unable to allocate a unique quarantine path");
}

} // namespace


CheckpointQuarantine::CheckpointQuarantine(std::filesystem::path directory)
    : directory_(std::move(directory))
{
    if (directory_.empty())
        throw std::invalid_argument("Quarantine directory must not be empty");
}


QuarantineOutcome CheckpointQuarantine::quarantine(
    const std::filesystem::path& source,
    const ValidationResult& rejection
) const
{
    if (source.filename().string().find(".tmp.") != std::string::npos)
    {
        return QuarantineOutcome{
            ValidationResult::ignored(
                RejectionReason::temporaryFile,
                "Temporary checkpoint file is ignored, not quarantined"
            ),
            std::nullopt,
            std::nullopt
        };
    }

    if (!rejection.isRejected() || rejection.diagnostics().empty())
    {
        return QuarantineOutcome{
            failure("Only a rejected checkpoint can be quarantined"),
            std::nullopt,
            std::nullopt
        };
    }

    const ValidationDiagnostic& primary = rejection.diagnostics().front();

    std::filesystem::path destination;
    std::filesystem::path diagnostic;
    bool moved = false;

    try
    {
        std::filesystem::create_directories(directory_);
        destination = uniqueDestination(
            directory_, source, primary.reason
        );
        diagnostic = destination.string() + ".reason";
        const std::string record =
            "original=" + oneLine(source.string()) + "\n"
            + "stage=" + std::string(toString(primary.stage)) + "\n"
            + "reason=" + std::string(toString(primary.reason)) + "\n"
            + "detail=" + oneLine(primary.detail) + "\n";
        const auto recordBytes = std::span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(record.data()),
            record.size()
        );
        AtomicFileCommit::write(diagnostic, recordBytes);

        std::error_code renameError;
        std::filesystem::rename(source, destination, renameError);
        if (renameError)
        {
            std::error_code ignored;
            std::filesystem::remove(diagnostic, ignored);
            throw std::system_error(renameError, "rename checkpoint to quarantine");
        }
        moved = true;

        const std::filesystem::path sourceDirectory = source.parent_path().empty()
            ? std::filesystem::path(".")
            : source.parent_path();
        syncDirectory(sourceDirectory);
        if (sourceDirectory != directory_) syncDirectory(directory_);

        return QuarantineOutcome{
            ValidationResult::accepted(),
            destination,
            diagnostic
        };
    }
    catch (const std::exception& error)
    {
        return QuarantineOutcome{
            failure(error.what()),
            moved ? std::optional(destination) : std::nullopt,
            moved ? std::optional(diagnostic) : std::nullopt
        };
    }
}

} // namespace pi::checkpoint
