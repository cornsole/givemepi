#include "verification/KnownDigitsVerifier.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <limits>
#include <string>
#include <vector>


namespace pi::verification
{
namespace
{

// Fractional digits are deliberately defined once. Expanding the reference
// requires a version bump so manifests can identify the exact comparison set.
constexpr std::string_view REFERENCE_FRACTIONAL_DIGITS =
    "14159265358979323846264338327950288419716939937510"
    "582097494459230781640628620899862803482534211706798";

constexpr std::uint64_t PUBLISHED_REFERENCE_DIGITS = 100;

std::string expectedPrefix(std::uint64_t requestedDigits)
{
    const auto compared = std::min(requestedDigits, PUBLISHED_REFERENCE_DIGITS);
    std::string expected(REFERENCE_FRACTIONAL_DIGITS.substr(0, compared));
    if (requestedDigits <= PUBLISHED_REFERENCE_DIGITS
        && REFERENCE_FRACTIONAL_DIGITS[static_cast<std::size_t>(requestedDigits)] >= '5')
    {
        for (std::size_t index = expected.size(); index != 0; --index)
        {
            if (expected[index - 1] != '9')
            {
                ++expected[index - 1];
                break;
            }
            expected[index - 1] = '0';
        }
    }
    return expected;
}


KnownDigitsVerification compare(
    std::string_view fractionalDigits,
    std::uint64_t requestedDigits,
    std::chrono::steady_clock::time_point started
)
{
    const auto compared = std::min<std::uint64_t>(
        requestedDigits,
        PUBLISHED_REFERENCE_DIGITS
    );
    const std::string expected = expectedPrefix(requestedDigits);
    std::vector<VerificationDiagnostic> diagnostics;
    for (std::uint64_t index = 0; index < compared; ++index)
    {
        if (fractionalDigits[static_cast<std::size_t>(index)]
            != expected[static_cast<std::size_t>(index)])
        {
            diagnostics.push_back({
                VerificationStage::knownDigits,
                VerificationReason::knownDigitsMismatch,
                "Canonical decimal differs from known pi prefix",
                index + 2
            });
            break;
        }
    }
    const auto duration = std::chrono::steady_clock::now() - started;
    const auto status = diagnostics.empty()
        ? VerificationStatus::passed
        : VerificationStatus::failed;
    return {
        VerificationCheckResult(
            VerificationStage::knownDigits,
            status,
            duration,
            std::move(diagnostics)
        ),
        KNOWN_DIGITS_REFERENCE_VERSION,
        PUBLISHED_REFERENCE_DIGITS,
        compared
    };
}


KnownDigitsVerification readFailure(
    std::string detail,
    std::chrono::steady_clock::time_point started
)
{
    return {
        VerificationCheckResult(
            VerificationStage::knownDigits,
            VerificationStatus::failed,
            std::chrono::steady_clock::now() - started,
            {{
                VerificationStage::knownDigits,
                VerificationReason::fileReadFailed,
                std::move(detail),
                std::nullopt
            }}
        ),
        KNOWN_DIGITS_REFERENCE_VERSION,
        PUBLISHED_REFERENCE_DIGITS,
        0
    };
}

} // namespace


bool KnownDigitsVerification::matched() const noexcept
{
    return verification.status() == VerificationStatus::passed;
}


std::string_view KnownDigitsVerifier::referenceFractionalDigits() noexcept
{
    return REFERENCE_FRACTIONAL_DIGITS.substr(0, PUBLISHED_REFERENCE_DIGITS);
}


KnownDigitsVerification KnownDigitsVerifier::verify(
    const CanonicalDecimal& decimal
)
{
    const auto started = std::chrono::steady_clock::now();
    return compare(
        decimal.fractionalDigits(),
        decimal.requestedDigits(),
        started
    );
}


KnownDigitsVerification KnownDigitsVerifier::verify(
    const InspectedOutputFile& file
)
{
    const auto started = std::chrono::steady_clock::now();
    if (file.requestedDigits == 0
        || file.requestedDigits > std::numeric_limits<std::uint64_t>::max() - 3
        || file.canonicalByteCount != file.requestedDigits + 2
        || file.fileSize != file.canonicalByteCount + 1)
    {
        return readFailure("Invalid inspected output metadata", started);
    }

    const auto compared = std::min<std::uint64_t>(
        file.requestedDigits,
        PUBLISHED_REFERENCE_DIGITS
    );
    const auto requiredBytes = static_cast<std::size_t>(compared + 2);
    std::string prefix(requiredBytes, '\0');
    std::ifstream input(file.path, std::ios::binary);
    if (!input)
    {
        return readFailure("Cannot open output file for known-digits verification", started);
    }
    input.read(prefix.data(), static_cast<std::streamsize>(prefix.size()));
    if (input.gcount() != static_cast<std::streamsize>(prefix.size()))
    {
        return readFailure("Output file ended before the known prefix", started);
    }

    if (prefix[0] != '3' || prefix[1] != '.')
    {
        std::uint64_t offset = prefix[0] == '3' ? 1 : 0;
        return {
            VerificationCheckResult(
                VerificationStage::knownDigits,
                VerificationStatus::failed,
                std::chrono::steady_clock::now() - started,
                {{
                    VerificationStage::knownDigits,
                    VerificationReason::knownDigitsMismatch,
                    "Canonical decimal prefix changed after inspection",
                    offset
                }}
            ),
            KNOWN_DIGITS_REFERENCE_VERSION,
            PUBLISHED_REFERENCE_DIGITS,
            compared
        };
    }
    return compare(std::string_view(prefix).substr(2), file.requestedDigits, started);
}

} // namespace pi::verification
