#include "storage/RuntimeStorageSession.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace
{

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void writeSentinel(const std::filesystem::path& path)
{
    std::ofstream output(path);
    output << "sentinel";
    if (!output) throw std::runtime_error("cannot write test sentinel");
}

} // namespace

int main()
{
    using pi::storage::RuntimeStorageSession;

    const auto base = std::filesystem::temp_directory_path()
        / ("givemepi-runtime-storage-session-" + std::to_string(::getpid()));
    std::error_code ignored;
    std::filesystem::remove_all(base, ignored);
    std::filesystem::create_directories(base / ".pi-engine-active");
    writeSentinel(base / ".pi-engine-active" / "stale.chunk");
    writeSentinel(base / "checkpoint-sentinel");

    std::filesystem::path active;
    {
        RuntimeStorageSession first(base);
        active = first.activeDirectory();
        require(std::filesystem::is_directory(active),
                "active runtime namespace was not created");
        require(!std::filesystem::exists(active / "stale.chunk"),
                "stale runtime artifact survived acquisition");
        require(std::filesystem::is_regular_file(base / "checkpoint-sentinel"),
                "unrelated sibling was removed");

        bool rejected = false;
        try
        {
            RuntimeStorageSession second(base);
            (void)second;
        }
        catch (const std::runtime_error& error)
        {
            rejected = std::string(error.what()).find("already owned")
                != std::string::npos;
        }
        require(rejected, "concurrent runtime storage owner was accepted");
        writeSentinel(active / "current.chunk");
    }

    require(!std::filesystem::exists(active),
            "active runtime namespace survived owner destruction");
    require(std::filesystem::is_regular_file(base / "checkpoint-sentinel"),
            "destruction removed unrelated sibling");
    require(std::filesystem::is_regular_file(
                base / ".pi-engine-runtime.lock"),
            "persistent lock inode was removed");

    {
        RuntimeStorageSession reacquired(base);
        require(std::filesystem::is_directory(reacquired.activeDirectory()),
                "runtime storage base could not be reacquired");
    }

    std::filesystem::remove_all(base, ignored);
    std::cout << "RuntimeStorageSession OK\n";
    return 0;
}
