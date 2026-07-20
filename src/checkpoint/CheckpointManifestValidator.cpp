#include "checkpoint/CheckpointManifestValidator.hpp"

#include <algorithm>
#include <exception>
#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <utility>


namespace pi::checkpoint
{

namespace
{

auto key(const ManifestEntry& entry)
{
    return std::tuple{
        entry.location.start,
        entry.location.end,
        entry.location.treeLevel
    };
}


void add(
    std::vector<ValidationDiagnostic>& diagnostics,
    RejectionReason reason,
    std::string detail
)
{
    diagnostics.push_back(ValidationDiagnostic{
        ValidationStage::manifest,
        reason,
        std::move(detail)
    });
}


std::uint32_t maximumTreeLevel(std::uint64_t termCount) noexcept
{
    std::uint32_t level = 0;
    std::uint64_t capacity = 1;
    while (capacity < termCount)
    {
        if (capacity > std::numeric_limits<std::uint64_t>::max() / 2)
            return level + 1;
        capacity <<= 1;
        ++level;
    }
    return level;
}

} // namespace


CheckpointManifestValidation CheckpointManifestValidator::validate(
    const CheckpointManifest& manifest,
    const ComputationIdentity& expectedIdentity
)
{
    std::vector<ValidationDiagnostic> diagnostics;
    bool identityValid = true;
    try
    {
        expectedIdentity.validate();
        manifest.identity.validate();
    }
    catch (const std::exception& error)
    {
        identityValid = false;
        add(diagnostics, RejectionReason::manifestIdentityMismatch, error.what());
    }

    if (identityValid && manifest.identity != expectedIdentity)
    {
        identityValid = false;
        add(
            diagnostics,
            RejectionReason::manifestIdentityMismatch,
            "Manifest identity differs from the target computation"
        );
    }

    for (std::size_t index = 1; index < manifest.entries.size(); ++index)
    {
        if (key(manifest.entries[index - 1]) >= key(manifest.entries[index]))
        {
            add(
                diagnostics,
                RejectionReason::nonCanonicalOrder,
                "Manifest entries are not in strict canonical order"
            );
            break;
        }
    }

    std::vector<ManifestEntry> sorted = manifest.entries;
    std::sort(sorted.begin(), sorted.end(), [](const auto& left, const auto& right) {
        return key(left) < key(right);
    });
    std::vector<bool> rejected(sorted.size(), !identityValid);

    for (std::size_t index = 0; index < sorted.size(); ++index)
    {
        try
        {
            sorted[index].location.validate(expectedIdentity);
        }
        catch (const std::exception& error)
        {
            rejected[index] = true;
            add(diagnostics, RejectionReason::invalidRange, error.what());
        }

        if (sorted[index].location.treeLevel
            > maximumTreeLevel(expectedIdentity.termCount))
        {
            rejected[index] = true;
            add(
                diagnostics,
                RejectionReason::invalidTreeLevel,
                "Manifest entry tree level exceeds the computation tree"
            );
        }

        if (sorted[index].completion != CompletionState::complete)
        {
            rejected[index] = true;
            add(
                diagnostics,
                RejectionReason::incompleteEntry,
                "Incomplete manifest entry cannot join the accepted frontier"
            );
        }
    }

    std::optional<std::size_t> furthestIndex;
    std::uint64_t furthestEnd = 0;
    for (std::size_t index = 0; index < sorted.size(); ++index)
    {
        if (rejected[index])
        {
            continue;
        }

        if (furthestIndex.has_value()
            && sorted[index].location.start < furthestEnd)
        {
            rejected[index] = true;
            rejected[*furthestIndex] = true;
            const bool sameRange =
                sorted[index].location.start
                    == sorted[*furthestIndex].location.start
                && sorted[index].location.end
                    == sorted[*furthestIndex].location.end;
            add(
                diagnostics,
                sameRange
                    ? RejectionReason::duplicateRange
                    : RejectionReason::overlappingRange,
                sameRange
                    ? "Multiple entries represent the same range"
                    : "Manifest entry ranges overlap"
            );
        }

        if (!furthestIndex.has_value()
            || sorted[index].location.end > furthestEnd)
        {
            furthestEnd = sorted[index].location.end;
            furthestIndex = index;
        }
    }

    std::vector<ManifestEntry> accepted;
    for (std::size_t index = 0; index < sorted.size(); ++index)
    {
        if (!rejected[index]) accepted.push_back(sorted[index]);
    }

    std::vector<MissingRange> missing;
    std::uint64_t cursor = 0;
    for (const auto& entry : accepted)
    {
        if (cursor < entry.location.start)
            missing.push_back(MissingRange{cursor, entry.location.start});
        cursor = entry.location.end;
    }
    if (cursor < expectedIdentity.termCount)
        missing.push_back(MissingRange{cursor, expectedIdentity.termCount});

    for (const auto& range : missing)
    {
        add(
            diagnostics,
            RejectionReason::missingRange,
            "Missing manifest range ["
                + std::to_string(range.start) + ", "
                + std::to_string(range.end) + ")"
        );
    }

    ValidationResult result = diagnostics.empty()
        ? ValidationResult::accepted()
        : ValidationResult::rejected(std::move(diagnostics));
    return CheckpointManifestValidation{
        std::move(result), std::move(accepted), std::move(missing)
    };
}

} // namespace pi::checkpoint
