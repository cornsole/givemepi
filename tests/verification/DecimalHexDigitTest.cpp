#include "verification/DecimalHexDigit.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <unistd.h>

int main()
{
    using namespace pi::verification;
    constexpr std::string_view decimal =
        "14159265358979323846264338327950288419716939937510"
        "58209749445923078164062862089986280348253421170679";
    constexpr std::string_view hexadecimal =
        "243F6A8885A308D313198A2E03707344"
        "A4093822299F31D0082EFA98EC4E6C89";

    auto inspected = inspectCanonicalDecimal("3." + std::string(decimal), 100);
    assert(inspected.accepted());
    DecimalHexDigitExtractor extractor(*inspected.decimal);
    for (std::uint64_t position = 0; position < hexadecimal.size(); ++position)
    {
        const auto result = extractor.extract(position);
        assert(result.resolved());
        const char expected = hexadecimal[static_cast<std::size_t>(position)];
        assert(*result.digit == static_cast<std::uint8_t>(
            expected <= '9' ? expected - '0' : expected - 'A' + 10
        ));
    }

    auto coarse = inspectCanonicalDecimal("3.1", 1);
    DecimalHexDigitExtractor coarseExtractor(*coarse.decimal);
    assert(!coarseExtractor.extract(0).resolved());

    const auto path = std::filesystem::temp_directory_path()
        / ("pi-decimal-hex-" + std::to_string(::getpid()) + ".txt");
    {
        std::ofstream output(path, std::ios::binary);
        output << "3." << decimal << '\n';
    }
    const auto file = FinalOutputInspector::inspectFile(path, 100);
    assert(file.accepted());
    auto fileExtractor = DecimalHexDigitExtractor::fromFile(*file.file);
    assert(fileExtractor.extract(31).digit == extractor.extract(31).digit);
    std::filesystem::remove(path);

    bool rejected = false;
    try
    {
        (void)extractor.extract(std::numeric_limits<std::uint64_t>::max());
    }
    catch (const std::out_of_range&)
    {
        rejected = true;
    }
    assert(rejected);
    return 0;
}
