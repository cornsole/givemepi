#pragma once

#include <filesystem>

namespace pi::storage
{

/// Owns one production runtime-storage namespace for a process lifetime.
///
/// The session exclusively locks the configured base directory, removes stale
/// temporary runtime data, and exposes a clean active directory. Checkpoint
/// data is outside this namespace and is never read or modified.
///
/// Construction performs O(n) filesystem work for n stale active entries and
/// uses O(1) memory apart from filesystem-library state. Destruction performs
/// the same bounded cleanup without throwing.
class RuntimeStorageSession
{
public:
    /// Acquires exclusive ownership of `baseDirectory` and creates a clean
    /// active runtime-storage directory.
    ///
    /// @throws std::invalid_argument if the base path is empty.
    /// @throws std::runtime_error if ownership or filesystem setup fails.
    explicit RuntimeStorageSession(std::filesystem::path baseDirectory);

    ~RuntimeStorageSession() noexcept;

    RuntimeStorageSession(const RuntimeStorageSession&) = delete;
    RuntimeStorageSession& operator=(const RuntimeStorageSession&) = delete;
    RuntimeStorageSession(RuntimeStorageSession&&) = delete;
    RuntimeStorageSession& operator=(RuntimeStorageSession&&) = delete;

    /// Returns the clean directory reserved for runtime chunks and their index.
    /// Time and memory complexity: O(1).
    [[nodiscard]] const std::filesystem::path& activeDirectory() const noexcept
    {
        return activeDirectory_;
    }

private:
    std::filesystem::path baseDirectory_;
    std::filesystem::path activeDirectory_;
    int lockDescriptor_ = -1;
};

} // namespace pi::storage
