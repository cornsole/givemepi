#include "verification/FinalOutputInspector.hpp"

#include <array>
#include <chrono>
#include <fstream>
#include <limits>
#include <string>
#include <utility>


namespace pi::verification
{
namespace
{

using Clock = std::chrono::steady_clock;


OutputFileInspection rejection(
    Clock::time_point started,
    VerificationReason reason,
    std::string detail,
    std::optional<std::uint64_t> offset = std::nullopt
)
{
    return OutputFileInspection{
        VerificationCheckResult(
            VerificationStage::outputStructure,
            VerificationStatus::failed,
            Clock::now() - started,
            {VerificationDiagnostic{
                VerificationStage::outputStructure,
                reason,
                std::move(detail),
                offset
            }}
        ),
        std::nullopt
    };
}

} // namespace


bool OutputFileInspection::accepted() const noexcept
{
    return verification.status() == VerificationStatus::passed
        && file.has_value();
}


CanonicalDecimalInspection FinalOutputInspector::inspectMemory(
    std::string_view candidate,
    std::uint64_t requestedDigits
)
{
    return inspectCanonicalDecimal(candidate, requestedDigits);
}


OutputFileInspection FinalOutputInspector::inspectFile(
    const std::filesystem::path& path,
    std::uint64_t requestedDigits
)
{
    const Clock::time_point started = Clock::now();
    if (requestedDigits == 0)
    {
        return rejection(
            started,
            VerificationReason::operationFailed,
            "Requested digit count must be greater than zero"
        );
    }
    if (requestedDigits > std::numeric_limits<std::uint64_t>::max() - 3)
    {
        return rejection(
            started,
            VerificationReason::invalidOutputLength,
            "Expected output file length overflows uint64"
        );
    }

    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return rejection(
            started,
            VerificationReason::fileReadFailed,
            "Cannot open final output file"
        );
    }

    const std::uint64_t canonicalBytes = requestedDigits + 2;
    const std::uint64_t expectedFileSize = canonicalBytes + 1;
    std::array<char, 64 * 1024> buffer{};
    std::uint64_t offset = 0;

    while (input)
    {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = input.gcount();
        for (std::streamsize index = 0; index < count; ++index, ++offset)
        {
            const char value = buffer[static_cast<std::size_t>(index)];
            if (offset == 0 && value != '3')
            {
                return rejection(
                    started,
                    VerificationReason::invalidOutputPrefix,
                    "Canonical output file must begin with 3.",
                    offset
                );
            }
            if (offset == 1 && value != '.')
            {
                return rejection(
                    started,
                    VerificationReason::invalidOutputPrefix,
                    "Canonical output file must begin with 3.",
                    offset
                );
            }
            if (offset >= 2 && offset < canonicalBytes
                && (value < '0' || value > '9'))
            {
                return rejection(
                    started,
                    VerificationReason::invalidOutputCharacter,
                    "Canonical output file contains a non-ASCII digit",
                    offset
                );
            }
            if (offset == canonicalBytes && value != '\n')
            {
                return rejection(
                    started,
                    VerificationReason::invalidOutputCharacter,
                    "Canonical output file must end with one newline",
                    offset
                );
            }
            if (offset >= expectedFileSize)
            {
                return rejection(
                    started,
                    VerificationReason::trailingData,
                    "Canonical output file contains trailing data",
                    offset
                );
            }
        }
    }

    if (!input.eof())
    {
        return rejection(
            started,
            VerificationReason::fileReadFailed,
            "Failed while reading final output file",
            offset
        );
    }
    if (offset != expectedFileSize)
    {
        return rejection(
            started,
            VerificationReason::invalidOutputLength,
            "Canonical output file has an unexpected length",
            offset
        );
    }

    return OutputFileInspection{
        VerificationCheckResult(
            VerificationStage::outputStructure,
            VerificationStatus::passed,
            Clock::now() - started
        ),
        InspectedOutputFile{
            path,
            offset,
            canonicalBytes,
            requestedDigits
        }
    };
}

} // namespace pi::verification
