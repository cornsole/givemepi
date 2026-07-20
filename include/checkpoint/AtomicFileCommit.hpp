#pragma once

#include <cstdint>
#include <filesystem>
#include <span>


namespace pi::checkpoint
{

enum class AtomicCommitStage
{
    tempCreated,
    contentWritten,
    fileSynced,
    fileClosed,
    beforeRename,
    renamed,
    directorySynced
};

using AtomicCommitObserver = void (*)(AtomicCommitStage);


class AtomicFileCommit
{
public:

    /**
     * Durably replaces destination through a unique same-directory temp file.
     */
    static void write(
        const std::filesystem::path& destination,
        std::span<const std::uint8_t> bytes,
        AtomicCommitObserver observer = nullptr
    );
};

} // namespace pi::checkpoint
