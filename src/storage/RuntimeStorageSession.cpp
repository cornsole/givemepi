#include "storage/RuntimeStorageSession.hpp"

#include <cerrno>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <system_error>
#include <sys/file.h>
#include <unistd.h>

namespace pi::storage
{
namespace
{

[[noreturn]] void throwSystemError(const std::string& operation, int error)
{
    throw std::runtime_error(
        operation + ": " + std::error_code(error, std::generic_category()).message());
}

} // namespace

RuntimeStorageSession::RuntimeStorageSession(
    std::filesystem::path baseDirectory)
    : baseDirectory_(std::move(baseDirectory))
{
    if (baseDirectory_.empty())
        throw std::invalid_argument("runtime storage base directory is empty");

    std::error_code error;
    std::filesystem::create_directories(baseDirectory_, error);
    if (error)
        throw std::runtime_error(
            "cannot create runtime storage base directory: " + error.message());
    if (!std::filesystem::is_directory(baseDirectory_, error) || error)
        throw std::runtime_error("runtime storage base path is not a directory");

    const auto lockPath = baseDirectory_ / ".pi-engine-runtime.lock";
    lockDescriptor_ = ::open(
        lockPath.c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (lockDescriptor_ < 0)
        throwSystemError("cannot open runtime storage lock", errno);

    for (;;)
    {
        if (::flock(lockDescriptor_, LOCK_EX | LOCK_NB) == 0) break;
        const int lockError = errno;
        if (lockError == EINTR) continue;
        ::close(lockDescriptor_);
        lockDescriptor_ = -1;
        if (lockError == EWOULDBLOCK || lockError == EAGAIN)
        {
            throw std::runtime_error(
                "runtime storage base is already owned by another process");
        }
        throwSystemError("cannot lock runtime storage base", lockError);
    }

    activeDirectory_ = baseDirectory_ / ".pi-engine-active";
    try
    {
        std::filesystem::remove_all(activeDirectory_, error);
        if (error)
            throw std::runtime_error(
                "cannot remove stale runtime storage namespace: "
                + error.message());
        if (!std::filesystem::create_directory(activeDirectory_, error) || error)
        {
            throw std::runtime_error(
                "cannot create active runtime storage namespace: "
                + (error ? error.message() : std::string("path already exists")));
        }
    }
    catch (...)
    {
        static_cast<void>(::flock(lockDescriptor_, LOCK_UN));
        ::close(lockDescriptor_);
        lockDescriptor_ = -1;
        throw;
    }
}

RuntimeStorageSession::~RuntimeStorageSession() noexcept
{
    std::error_code ignored;
    if (!activeDirectory_.empty())
        std::filesystem::remove_all(activeDirectory_, ignored);
    if (lockDescriptor_ >= 0)
    {
        static_cast<void>(::flock(lockDescriptor_, LOCK_UN));
        static_cast<void>(::close(lockDescriptor_));
    }
}

} // namespace pi::storage
