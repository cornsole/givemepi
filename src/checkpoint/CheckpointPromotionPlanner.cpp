#include "checkpoint/CheckpointPromotionPlanner.hpp"

#include "checkpoint/CheckpointFileInspector.hpp"
#include "checkpoint/CheckpointManifest.hpp"
#include "checkpoint/CheckpointManifestValidator.hpp"
#include "checkpoint/ManifestEntryValidator.hpp"
#include "checkpoint/ModularPQTVerifier.hpp"

#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace pi::checkpoint
{
namespace
{

std::vector<std::uint8_t> readFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input)
    {
        throw std::runtime_error(
            "cannot open promotion manifest: " + path.string());
    }

    const std::streamoff size = input.tellg();
    if (size < 0
        || static_cast<std::uintmax_t>(size)
            > std::numeric_limits<std::size_t>::max())
    {
        throw std::runtime_error(
            "promotion manifest size is not representable");
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    input.seekg(0, std::ios::beg);
    if (!bytes.empty())
    {
        input.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    }
    if (!input)
    {
        throw std::runtime_error(
            "cannot read promotion manifest: " + path.string());
    }
    return bytes;
}

std::string validationDetail(
    const char* context,
    const ValidationResult& validation)
{
    std::string message(context);
    const auto& diagnostics = validation.diagnostics();
    if (!diagnostics.empty())
    {
        message.append(": ");
        message.append(diagnostics.front().detail);
    }
    return message;
}

void validateCompatibility(
    const ComputationIdentity& source,
    const ComputationIdentity& target)
{
    source.validate();
    target.validate();
    if (source.algorithm != target.algorithm
        || source.arithmeticVersion != target.arithmeticVersion
        || source.formulaVersion != target.formulaVersion)
    {
        throw std::runtime_error(
            "promotion source uses an incompatible computation format");
    }
    if (source.requestedDigits >= target.requestedDigits
        || source.termCount >= target.termCount)
    {
        throw std::runtime_error(
            "promotion source must have fewer digits and terms than target");
    }
}

} // namespace

CheckpointPromotionPlanner::CheckpointPromotionPlanner(
    std::filesystem::path manifestPath)
    : manifestPath_(std::move(manifestPath))
{
    if (manifestPath_.empty())
    {
        throw std::invalid_argument(
            "promotion manifest path must not be empty");
    }
}

CheckpointPromotionPlan CheckpointPromotionPlanner::create(
    const ComputationIdentity& targetIdentity) const
{
    const CheckpointManifest manifest = CheckpointManifestCodec::decode(
        readFile(manifestPath_));
    validateCompatibility(manifest.identity, targetIdentity);

    const CheckpointManifestValidation manifestValidation =
        CheckpointManifestValidator::validate(manifest, manifest.identity);
    if (!manifestValidation.validation.isAccepted()
        || manifestValidation.acceptedEntries.size() != manifest.entries.size()
        || !manifestValidation.missingRanges.empty())
    {
        throw std::runtime_error(validationDetail(
            "promotion source manifest is incomplete or invalid",
            manifestValidation.validation));
    }

    CheckpointPromotionPlan plan{manifest.identity, {}, 0};
    plan.promotedBlocks.reserve(manifestValidation.acceptedEntries.size());
    std::filesystem::path sourceDirectory = manifestPath_.parent_path();
    if (sourceDirectory.empty())
    {
        sourceDirectory = ".";
    }

    for (const ManifestEntry& entry : manifestValidation.acceptedEntries)
    {
        entry.location.validate(targetIdentity);
        const std::filesystem::path path = sourceDirectory / entry.filename;
        CheckpointFileInspection inspection =
            CheckpointFileInspector::inspect(path, manifest.identity);
        if (!inspection.hasAcceptedFile())
        {
            throw std::runtime_error(validationDetail(
                "promotion source block is invalid", inspection.validation));
        }

        const ValidationResult entryValidation = ManifestEntryValidator::validate(
            entry, *inspection.file, manifest.identity);
        if (!entryValidation.isAccepted())
        {
            throw std::runtime_error(validationDetail(
                "promotion source entry does not match its block",
                entryValidation));
        }

        const ValidationResult modular = ModularPQTVerifier::validate(
            inspection.file->block);
        if (!modular.isAccepted())
        {
            throw std::runtime_error(validationDetail(
                "promotion source block failed mathematical verification",
                modular));
        }

        const std::uint64_t terms = entry.location.end - entry.location.start;
        if (plan.promotedTerms
            > std::numeric_limits<std::uint64_t>::max() - terms)
        {
            throw std::runtime_error("promotion term count overflow");
        }
        plan.promotedTerms += terms;
        plan.promotedBlocks.push_back(PromotedCheckpoint{
            path,
            inspection.file->fileSize,
            std::move(inspection.file->block)
        });
    }

    if (plan.promotedTerms != manifest.identity.termCount)
    {
        throw std::runtime_error(
            "promotion source frontier does not cover every source term");
    }
    return plan;
}

} // namespace pi::checkpoint
