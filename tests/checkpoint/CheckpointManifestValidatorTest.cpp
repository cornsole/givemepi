#include "checkpoint/CheckpointManifestValidator.hpp"

#include "chudnovsky/PrecisionPolicy.hpp"

#include <algorithm>
#include <iostream>


using pi::checkpoint::BlockLocation;
using pi::checkpoint::CheckpointManifest;
using pi::checkpoint::CheckpointManifestValidation;
using pi::checkpoint::CheckpointManifestValidator;
using pi::checkpoint::ChecksumAlgorithm;
using pi::checkpoint::CompletionState;
using pi::checkpoint::ComputationIdentity;
using pi::checkpoint::ManifestEntry;
using pi::checkpoint::MissingRange;
using pi::checkpoint::RejectionReason;
using pi::chudnovsky::PrecisionPolicy;


namespace
{

ManifestEntry entry(
    const ComputationIdentity& identity,
    std::uint64_t start,
    std::uint64_t end,
    std::uint32_t level,
    CompletionState completion = CompletionState::complete
)
{
    return ManifestEntry{
        BlockLocation::create(start, end, level, identity),
        "block-" + std::to_string(start) + "-" + std::to_string(end)
            + ".checkpoint",
        100,
        ChecksumAlgorithm::crc32c,
        0x12345678U,
        completion
    };
}


bool hasReason(
    const CheckpointManifestValidation& validation,
    RejectionReason reason
)
{
    return std::any_of(
        validation.validation.diagnostics().begin(),
        validation.validation.diagnostics().end(),
        [reason](const auto& diagnostic) { return diagnostic.reason == reason; }
    );
}

} // namespace


int main()
{
    const ComputationIdentity identity =
        ComputationIdentity::fromPrecisionPlan(PrecisionPolicy::create(1000));
    const ManifestEntry first = entry(identity, 0, 10, 3);
    const ManifestEntry second = entry(identity, 10, 74, 7);

    const CheckpointManifestValidation complete =
        CheckpointManifestValidator::validate(
            CheckpointManifest{identity, {first, second}}, identity
        );
    if (!complete.validation.isAccepted()
        || complete.acceptedEntries.size() != 2
        || !complete.missingRanges.empty())
    {
        std::cerr << "Complete manifest frontier was rejected\n";
        return 1;
    }

    const CheckpointManifestValidation gap =
        CheckpointManifestValidator::validate(
            CheckpointManifest{
                identity,
                {first, entry(identity, 20, 74, 7)}
            },
            identity
        );
    if (!hasReason(gap, RejectionReason::missingRange)
        || gap.missingRanges != std::vector<MissingRange>({{10, 20}})
        || gap.acceptedEntries.size() != 2)
    {
        std::cerr << "Manifest gap calculation mismatch\n";
        return 1;
    }

    const CheckpointManifestValidation overlap =
        CheckpointManifestValidator::validate(
            CheckpointManifest{
                identity,
                {entry(identity, 0, 20, 4), entry(identity, 10, 74, 7)}
            },
            identity
        );
    if (!hasReason(overlap, RejectionReason::overlappingRange)
        || !overlap.acceptedEntries.empty()
        || overlap.missingRanges
            != std::vector<MissingRange>({{0, identity.termCount}}))
    {
        std::cerr << "Manifest overlap handling mismatch\n";
        return 1;
    }

    const CheckpointManifestValidation duplicate =
        CheckpointManifestValidator::validate(
            CheckpointManifest{
                identity,
                {entry(identity, 0, 10, 2), entry(identity, 0, 10, 3)}
            },
            identity
        );
    if (!hasReason(duplicate, RejectionReason::duplicateRange))
    {
        std::cerr << "Duplicate range was accepted\n";
        return 1;
    }

    const CheckpointManifestValidation incomplete =
        CheckpointManifestValidator::validate(
            CheckpointManifest{
                identity,
                {first, entry(identity, 10, 74, 7, CompletionState::incomplete)}
            },
            identity
        );
    if (!hasReason(incomplete, RejectionReason::incompleteEntry)
        || incomplete.acceptedEntries != std::vector<ManifestEntry>({first})
        || incomplete.missingRanges
            != std::vector<MissingRange>({{10, 74}}))
    {
        std::cerr << "Incomplete entry frontier mismatch\n";
        return 1;
    }

    const CheckpointManifestValidation unsorted =
        CheckpointManifestValidator::validate(
            CheckpointManifest{identity, {second, first}}, identity
        );
    if (!hasReason(unsorted, RejectionReason::nonCanonicalOrder)
        || unsorted.acceptedEntries.size() != 2
        || !unsorted.missingRanges.empty())
    {
        std::cerr << "Non-canonical manifest order was not diagnosed\n";
        return 1;
    }

    std::cout << "CheckpointManifestValidator OK\n";
    return 0;
}
