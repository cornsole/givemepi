#include "checkpoint/AtomicFileCommit.hpp"

#include <atomic>
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <fcntl.h>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unistd.h>


namespace pi::checkpoint
{

namespace
{

std::atomic<std::uint64_t> tempSequence{0};


class FileDescriptor
{
public:
    explicit FileDescriptor(int value = -1) noexcept : value_(value) {}

    ~FileDescriptor()
    {
        if (value_ >= 0)
        {
            ::close(value_);
        }
    }

    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;

    FileDescriptor(FileDescriptor&& other) noexcept
        : value_(other.release())
    {
    }

    FileDescriptor& operator=(FileDescriptor&& other) noexcept
    {
        if (this != &other)
        {
            if (value_ >= 0)
            {
                ::close(value_);
            }
            value_ = other.release();
        }
        return *this;
    }

    [[nodiscard]] int get() const noexcept { return value_; }

    int release() noexcept
    {
        const int value = value_;
        value_ = -1;
        return value;
    }

private:
    int value_;
};


[[noreturn]] void throwSystemError(const char* operation)
{
    throw std::system_error(errno, std::generic_category(), operation);
}


std::filesystem::path createTempPath(
    const std::filesystem::path& destination,
    FileDescriptor& descriptor
)
{
    constexpr std::uint32_t maximumAttempts = 128;

    for (std::uint32_t attempt = 0; attempt < maximumAttempts; ++attempt)
    {
        const std::uint64_t sequence = tempSequence.fetch_add(
            1, std::memory_order_relaxed
        );
        const std::filesystem::path candidate = destination.parent_path()
            / (destination.filename().string()
                + ".tmp."
                + std::to_string(static_cast<unsigned long long>(::getpid()))
                + "."
                + std::to_string(sequence));

        const int file = ::open(
            candidate.c_str(),
            O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC,
            0600
        );

        if (file >= 0)
        {
            descriptor = FileDescriptor(file);
            return candidate;
        }

        if (errno != EEXIST)
        {
            throwSystemError("create checkpoint temp file");
        }
    }

    throw std::runtime_error("Unable to allocate a unique checkpoint temp file");
}


void writeAll(int file, std::span<const std::uint8_t> bytes)
{
    std::size_t offset = 0;

    while (offset < bytes.size())
    {
        const std::size_t requestSize = std::min(
            bytes.size() - offset,
            static_cast<std::size_t>(std::numeric_limits<ssize_t>::max())
        );
        const ssize_t written = ::write(
            file, bytes.data() + offset, requestSize
        );

        if (written < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            throwSystemError("write checkpoint temp file");
        }

        if (written == 0)
        {
            throw std::runtime_error("Checkpoint write made no progress");
        }

        offset += static_cast<std::size_t>(written);
    }
}

} // namespace


void AtomicFileCommit::write(
    const std::filesystem::path& destination,
    std::span<const std::uint8_t> bytes,
    AtomicCommitObserver observer
)
{
    if (destination.empty() || destination.filename().empty())
    {
        throw std::invalid_argument("Checkpoint destination is not a file path");
    }

    FileDescriptor tempFile;
    const std::filesystem::path tempPath = createTempPath(
        destination, tempFile
    );
    bool renamed = false;

    try
    {
        if (observer != nullptr) observer(AtomicCommitStage::tempCreated);
        writeAll(tempFile.get(), bytes);
        if (observer != nullptr) observer(AtomicCommitStage::contentWritten);

        if (::fsync(tempFile.get()) != 0)
        {
            throwSystemError("sync checkpoint temp file");
        }
        if (observer != nullptr) observer(AtomicCommitStage::fileSynced);

        const int rawFile = tempFile.release();

        if (::close(rawFile) != 0)
        {
            throwSystemError("close checkpoint temp file");
        }
        if (observer != nullptr) observer(AtomicCommitStage::fileClosed);
        if (observer != nullptr) observer(AtomicCommitStage::beforeRename);

        if (::rename(tempPath.c_str(), destination.c_str()) != 0)
        {
            throwSystemError("rename checkpoint temp file");
        }
        renamed = true;
        if (observer != nullptr) observer(AtomicCommitStage::renamed);

        FileDescriptor directory(::open(
            destination.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC
        ));

        if (directory.get() < 0)
        {
            throwSystemError("open checkpoint directory");
        }

        if (::fsync(directory.get()) != 0)
        {
            throwSystemError("sync checkpoint directory");
        }
        if (observer != nullptr) observer(AtomicCommitStage::directorySynced);
    }
    catch (...)
    {
        if (!renamed)
        {
            ::unlink(tempPath.c_str());
        }
        throw;
    }
}

} // namespace pi::checkpoint
