#include "checkpoint/CheckpointResumePlanner.hpp"

#include "checkpoint/CheckpointFileInspector.hpp"
#include "checkpoint/CheckpointStore.hpp"
#include "checkpoint/ManifestEntryValidator.hpp"
#include "checkpoint/ModularPQTVerifier.hpp"

#include <algorithm>
#include <exception>
#include <set>
#include <string>
#include <utility>


namespace pi::checkpoint
{

namespace
{

ValidationResult rejection(
    ValidationStage stage,
    RejectionReason reason,
    std::string detail
)
{
    return ValidationResult::rejected(ValidationDiagnostic{
        stage, reason, std::move(detail)
    });
}


std::vector<MissingRange> complement(
    const std::vector<AcceptedCheckpoint>& accepted,
    std::uint64_t termCount
)
{
    std::vector<BlockLocation> locations;
    locations.reserve(accepted.size());
    for (const auto& checkpoint : accepted)
        locations.push_back(checkpoint.block.location);
    std::sort(locations.begin(), locations.end(), [](const auto& left, const auto& right) {
        return left.start < right.start;
    });

    std::vector<MissingRange> missing;
    std::uint64_t cursor = 0;
    for (const auto& location : locations)
    {
        if (cursor < location.start)
            missing.push_back(MissingRange{cursor, location.start});
        cursor = std::max(cursor, location.end);
    }
    if (cursor < termCount) missing.push_back(MissingRange{cursor, termCount});
    return missing;
}


void appendDiagnostics(
    std::vector<ValidationDiagnostic>& destination,
    const ValidationResult& source
)
{
    destination.insert(
        destination.end(),
        source.diagnostics().begin(),
        source.diagnostics().end()
    );
}

} // namespace


CheckpointResumePlanner::CheckpointResumePlanner(
    std::filesystem::path checkpointDirectory,
    std::filesystem::path quarantineDirectory
)
    : checkpointDirectory_(std::move(checkpointDirectory)),
      quarantineDirectory_(std::move(quarantineDirectory))
{
    if (checkpointDirectory_.empty() || quarantineDirectory_.empty())
        throw std::invalid_argument("Resume planner directories must not be empty");
}


CheckpointResumePlan CheckpointResumePlanner::create(
    const ComputationIdentity& expectedIdentity
) const
{
    CheckpointResumePlan plan;
    const CheckpointStore store(checkpointDirectory_);
    const CheckpointQuarantine quarantine(quarantineDirectory_);
    CheckpointManifest manifest{expectedIdentity, {}};
    bool manifestLoaded = false;

    try
    {
        manifest = store.loadManifest(expectedIdentity);
        manifestLoaded = true;
    }
    catch (const std::exception& error)
    {
        plan.diagnostics.push_back(ValidationDiagnostic{
            ValidationStage::manifest,
            RejectionReason::invalidManifest,
            error.what()
        });
    }

    CheckpointManifestValidation manifestValidation =
        CheckpointManifestValidator::validate(manifest, expectedIdentity);
    appendDiagnostics(plan.diagnostics, manifestValidation.validation);

    std::set<std::string> manifestFilenames;
    for (const auto& entry : manifest.entries)
        manifestFilenames.insert(entry.filename);

    std::set<std::filesystem::path> processedPaths;

    auto rejectFile = [&](const std::filesystem::path& path,
                          ValidationResult validation,
                          bool mayQuarantine) {
        if (!processedPaths.insert(path).second) return;
        RejectedCheckpoint rejected{
            path, std::move(validation), std::nullopt, std::nullopt
        };
        if (mayQuarantine && std::filesystem::exists(path))
        {
            QuarantineOutcome outcome = quarantine.quarantine(
                path, rejected.validation
            );
            rejected.quarantinedPath = outcome.quarantinedPath;
            if (!outcome.operation.isAccepted())
                appendDiagnostics(plan.diagnostics, outcome.operation);
            rejected.quarantineOperation = std::move(outcome.operation);
        }
        appendDiagnostics(plan.diagnostics, rejected.validation);
        plan.rejectedBlocks.push_back(std::move(rejected));
    };

    if (manifestLoaded)
    {
        for (const auto& entry : manifest.entries)
        {
            if (std::find(
                    manifestValidation.acceptedEntries.begin(),
                    manifestValidation.acceptedEntries.end(),
                    entry
                ) == manifestValidation.acceptedEntries.end())
            {
                rejectFile(
                    checkpointDirectory_ / entry.filename,
                    manifestValidation.validation,
                    true
                );
            }
        }

        for (const auto& entry : manifestValidation.acceptedEntries)
        {
            const std::filesystem::path path = checkpointDirectory_ / entry.filename;
            CheckpointFileInspection inspection =
                CheckpointFileInspector::inspect(path, expectedIdentity);
            if (!inspection.hasAcceptedFile())
            {
                rejectFile(path, std::move(inspection.validation), true);
                continue;
            }

            ValidationResult entryValidation = ManifestEntryValidator::validate(
                entry, *inspection.file, expectedIdentity
            );
            if (!entryValidation.isAccepted())
            {
                rejectFile(path, std::move(entryValidation), true);
                continue;
            }

            ValidationResult modular = ModularPQTVerifier::validate(
                inspection.file->block
            );
            if (!modular.isAccepted())
            {
                rejectFile(path, std::move(modular), true);
                continue;
            }

            plan.acceptedBlocks.push_back(AcceptedCheckpoint{
                path, std::move(inspection.file->block)
            });
            processedPaths.insert(path);
        }
    }

    std::vector<std::filesystem::path> candidates;
    if (std::filesystem::exists(checkpointDirectory_))
    {
        for (const auto& entry : std::filesystem::directory_iterator(
                checkpointDirectory_))
            candidates.push_back(entry.path());
    }

    for (const auto& path : candidates)
    {
        const std::string filename = path.filename().string();
        if (filename.find(".tmp.") != std::string::npos)
        {
            plan.ignoredTemporaryFiles.push_back(path);
            continue;
        }
        if (path.extension() == ".checkpoint"
            && manifestFilenames.find(filename) == manifestFilenames.end())
        {
            rejectFile(
                path,
                rejection(
                    ValidationStage::manifest,
                    RejectionReason::unexpectedBlockFile,
                    "Checkpoint file is not a member of the manifest"
                ),
                true
            );
        }
    }

    std::sort(
        plan.acceptedBlocks.begin(),
        plan.acceptedBlocks.end(),
        [](const auto& left, const auto& right) {
            return left.block.location.start < right.block.location.start;
        }
    );
    plan.rangesToRecalculate = complement(
        plan.acceptedBlocks, expectedIdentity.termCount
    );
    return plan;
}

} // namespace pi::checkpoint
