#include "verification/FinalOutputInspector.hpp"
#include "verification/SHA256.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <sys/resource.h>
#include <unistd.h>

namespace
{
long maximumResidentKilobytes()
{
    rusage usage{};
    assert(::getrusage(RUSAGE_SELF, &usage) == 0);
    return usage.ru_maxrss;
}
}

int main()
{
    using namespace pi::verification;
    constexpr std::uint64_t digits = 128ULL * 1024ULL * 1024ULL;
    const auto path = std::filesystem::temp_directory_path()
        / ("pi-large-streaming-" + std::to_string(::getpid()) + ".txt");
    {
        std::ofstream output(path, std::ios::binary);
        output << "3.";
        std::array<char, 64 * 1024> chunk{};
        chunk.fill('7');
        std::uint64_t remaining = digits;
        while (remaining != 0)
        {
            const auto count = static_cast<std::streamsize>(
                std::min<std::uint64_t>(remaining, chunk.size())
            );
            output.write(chunk.data(), count);
            remaining -= static_cast<std::uint64_t>(count);
        }
        output << '\n';
        assert(output.good());
    }

    const long residentBefore = maximumResidentKilobytes();
    const auto inspection = FinalOutputInspector::inspectFile(path, digits);
    assert(inspection.accepted());
    assert(inspection.file->canonicalByteCount == digits + 2);
    const auto digest = SHA256::hashFilePrefix(
        path,
        inspection.file->canonicalByteCount
    );
    assert(toHex(digest).size() == 64);
    const long residentAfter = maximumResidentKilobytes();

    // Both passes use fixed 64 KiB buffers; allow allocator/runtime noise but
    // reject an accidental output-sized allocation.
    assert(residentAfter <= residentBefore + 16 * 1024);
    std::filesystem::remove(path);
    return 0;
}
