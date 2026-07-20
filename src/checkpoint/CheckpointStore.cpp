#include "checkpoint/CheckpointStore.hpp"

#include "checkpoint/AtomicFileCommit.hpp"

#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>


namespace pi::checkpoint
{

CheckpointStore::CheckpointStore(std::filesystem::path directory)
    : directory_(std::move(directory))
{
    if (directory_.empty())
    {
        throw std::invalid_argument("Checkpoint directory must not be empty");
    }
}


std::filesystem::path CheckpointStore::blockPath(
    const ComputationIdentity& identity,
    const BlockLocation& location
) const
{
    identity.validate();
    location.validate(identity);

    const std::string filename =
        "pqt-v" + std::to_string(CheckpointBlockCodec::formatVersion)
        + "-a" + std::to_string(static_cast<std::uint16_t>(identity.algorithm))
        + "-av" + std::to_string(identity.arithmeticVersion)
        + "-fv" + std::to_string(identity.formulaVersion)
        + "-d" + std::to_string(identity.requestedDigits)
        + "-g" + std::to_string(identity.guardDigits)
        + "-w" + std::to_string(identity.workingDigits)
        + "-n" + std::to_string(identity.termCount)
        + "-r" + std::to_string(location.start)
        + "-" + std::to_string(location.end)
        + "-l" + std::to_string(location.treeLevel)
        + ".checkpoint";

    return directory_ / filename;
}


std::filesystem::path CheckpointStore::save(
    const CheckpointBlock& block
) const
{
    const std::filesystem::path destination = blockPath(
        block.identity, block.location
    );
    std::filesystem::create_directories(directory_);
    const std::vector<std::uint8_t> bytes = CheckpointBlockCodec::encode(block);
    AtomicFileCommit::write(destination, bytes);
    return destination;
}


CheckpointBlock CheckpointStore::load(
    const ComputationIdentity& identity,
    const BlockLocation& location
) const
{
    const std::filesystem::path source = blockPath(identity, location);
    const std::uintmax_t fileSize = std::filesystem::file_size(source);

    if (fileSize > std::numeric_limits<std::size_t>::max())
    {
        throw std::length_error("Checkpoint file is too large to load");
    }

    if (fileSize
        > static_cast<std::uintmax_t>(
            std::numeric_limits<std::streamsize>::max()
        ))
    {
        throw std::length_error("Checkpoint file exceeds stream limits");
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(fileSize));
    std::ifstream input(source, std::ios::binary);

    if (!input || (fileSize != 0
        && !input.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size())
        )))
    {
        throw std::runtime_error("Failed to read checkpoint file");
    }

    CheckpointBlock block = CheckpointBlockCodec::decode(bytes);

    if (block.identity != identity || block.location != location)
    {
        throw std::invalid_argument(
            "Checkpoint content does not match its deterministic path"
        );
    }

    return block;
}


std::filesystem::path CheckpointStore::manifestPath(
    const ComputationIdentity& identity
) const
{
    identity.validate();
    return directory_ / (
        "manifest-v" + std::to_string(CheckpointManifestCodec::formatVersion)
        + "-a" + std::to_string(static_cast<std::uint16_t>(identity.algorithm))
        + "-av" + std::to_string(identity.arithmeticVersion)
        + "-fv" + std::to_string(identity.formulaVersion)
        + "-d" + std::to_string(identity.requestedDigits)
        + "-g" + std::to_string(identity.guardDigits)
        + "-w" + std::to_string(identity.workingDigits)
        + "-n" + std::to_string(identity.termCount)
        + ".manifest"
    );
}


std::filesystem::path CheckpointStore::saveManifest(
    const CheckpointManifest& manifest
) const
{
    const std::filesystem::path destination = manifestPath(manifest.identity);
    std::filesystem::create_directories(directory_);
    const std::vector<std::uint8_t> bytes =
        CheckpointManifestCodec::encode(manifest);
    AtomicFileCommit::write(destination, bytes);
    return destination;
}


CheckpointManifest CheckpointStore::loadManifest(
    const ComputationIdentity& identity
) const
{
    const std::filesystem::path source = manifestPath(identity);
    const std::uintmax_t fileSize = std::filesystem::file_size(source);

    if (fileSize > std::numeric_limits<std::size_t>::max()
        || fileSize > static_cast<std::uintmax_t>(
            std::numeric_limits<std::streamsize>::max()))
    {
        throw std::length_error("Checkpoint manifest is too large to load");
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(fileSize));
    std::ifstream input(source, std::ios::binary);
    if (!input || (fileSize != 0
        && !input.read(reinterpret_cast<char*>(bytes.data()),
                       static_cast<std::streamsize>(bytes.size()))))
    {
        throw std::runtime_error("Failed to read checkpoint manifest");
    }

    CheckpointManifest manifest = CheckpointManifestCodec::decode(bytes);
    if (manifest.identity != identity)
    {
        throw std::invalid_argument(
            "Checkpoint manifest does not match its deterministic path"
        );
    }
    return manifest;
}

} // namespace pi::checkpoint
