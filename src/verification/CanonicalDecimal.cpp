#include "verification/CanonicalDecimal.hpp"

#include <chrono>
#include <limits>
#include <utility>


namespace pi::verification
{
namespace
{

using Clock = std::chrono::steady_clock;


CanonicalDecimalInspection rejection(
    Clock::time_point started,
    VerificationReason reason,
    std::string detail,
    std::optional<std::uint64_t> offset = std::nullopt
)
{
    return CanonicalDecimalInspection{
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


CanonicalDecimal::CanonicalDecimal(
    std::string bytes,
    std::uint64_t requestedDigits
)
    : bytes_(std::move(bytes)),
      requestedDigits_(requestedDigits)
{
}


std::string_view CanonicalDecimal::bytes() const noexcept
{
    return bytes_;
}


std::string_view CanonicalDecimal::fractionalDigits() const noexcept
{
    return std::string_view(bytes_).substr(2);
}


std::uint64_t CanonicalDecimal::requestedDigits() const noexcept
{
    return requestedDigits_;
}


bool CanonicalDecimalInspection::accepted() const noexcept
{
    return verification.status() == VerificationStatus::passed
        && decimal.has_value();
}


CanonicalDecimalInspection inspectCanonicalDecimal(
    std::string_view candidate,
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
    if (requestedDigits
        > std::numeric_limits<std::size_t>::max() - 2)
    {
        return rejection(
            started,
            VerificationReason::invalidOutputLength,
            "Canonical decimal length does not fit this platform"
        );
    }

    const std::size_t expectedSize =
        static_cast<std::size_t>(requestedDigits) + 2;
    if (candidate.size() < 2 || candidate[0] != '3' || candidate[1] != '.')
    {
        std::uint64_t offset = 0;
        if (!candidate.empty() && candidate[0] == '3')
        {
            offset = 1;
        }
        return rejection(
            started,
            VerificationReason::invalidOutputPrefix,
            "Canonical decimal must begin with 3.",
            offset
        );
    }
    if (candidate.size() > expectedSize)
    {
        return rejection(
            started,
            VerificationReason::trailingData,
            "Canonical decimal contains trailing data",
            expectedSize
        );
    }
    if (candidate.size() < expectedSize)
    {
        return rejection(
            started,
            VerificationReason::invalidOutputLength,
            "Canonical decimal has fewer digits than requested",
            candidate.size()
        );
    }

    for (std::size_t offset = 2; offset < candidate.size(); ++offset)
    {
        if (candidate[offset] < '0' || candidate[offset] > '9')
        {
            return rejection(
                started,
                VerificationReason::invalidOutputCharacter,
                "Canonical decimal contains a non-ASCII digit",
                offset
            );
        }
    }

    return CanonicalDecimalInspection{
        VerificationCheckResult(
            VerificationStage::outputStructure,
            VerificationStatus::passed,
            Clock::now() - started
        ),
        CanonicalDecimal(std::string(candidate), requestedDigits)
    };
}

} // namespace pi::verification
