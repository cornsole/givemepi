#pragma once

#include "verification/CanonicalDecimal.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>


namespace pi::verification
{

/// Metadata for one structurally accepted decimal output file.
struct InspectedOutputFile
{
    std::filesystem::path path;
    std::uint64_t fileSize;
    std::uint64_t canonicalByteCount;
    std::uint64_t requestedDigits;

    [[nodiscard]]
    bool operator==(const InspectedOutputFile&) const noexcept = default;
};


/// Result of bounded-memory final output file inspection.
struct OutputFileInspection
{
    VerificationCheckResult verification;
    std::optional<InspectedOutputFile> file;

    [[nodiscard]] bool accepted() const noexcept;
};


/// Separates memory-resident canonical inspection from stored-file inspection.
class FinalOutputInspector
{
public:
    /// Inspects a memory value using the canonical newline-free contract.
    ///
    /// Time complexity: O(n). Memory complexity: O(n) only on success because
    /// the accepted canonical value owns its bytes.
    [[nodiscard]]
    static CanonicalDecimalInspection inspectMemory(
        std::string_view candidate,
        std::uint64_t requestedDigits
    );

    /// Inspects a stored output as canonical bytes plus exactly one `\n`.
    ///
    /// The file is read in fixed-size chunks and is never copied into one large
    /// string. Accepted metadata identifies the canonical prefix for subsequent
    /// streaming hash verification.
    /// Time complexity: O(n). Memory complexity: O(1).
    [[nodiscard]]
    static OutputFileInspection inspectFile(
        const std::filesystem::path& path,
        std::uint64_t requestedDigits
    );
};

} // namespace pi::verification
