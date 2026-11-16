#include "verification/DecimalHexDigit.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

namespace
{

class RecordingWork final
    : public pi::verification::VerificationWorkObserver
{
public:
    void workCompleted(std::uint64_t units) noexcept override
    {
        assert(units != 0);
        total += units;
        ++calls;
    }

    std::uint64_t total = 0;
    std::uint64_t calls = 0;
};

std::string testDigits(std::size_t count)
{
    std::string digits;
    digits.reserve(count);
    for (std::size_t index = 0; index < count; ++index)
    {
        digits.push_back(static_cast<char>('0' + (index * 7) % 10));
    }
    return digits;
}

void assertBalancedImport(std::size_t count)
{
    const std::string digits = testDigits(count);
    std::istringstream input(digits);
    const auto actual = pi::verification::importDecimalDigits(
        input,
        count,
        8
    );
    pi::bigint::GMPInteger expected;
    if (digits.empty())
    {
        assert(actual.isZero());
        return;
    }
    assert(mpz_set_str(*expected.raw(), digits.c_str(), 10) == 0);
    assert(actual.compare(expected) == 0);
}

} // namespace


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
    for (const std::size_t count : std::vector<std::size_t>{
            0, 1, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65
        })
    {
        assertBalancedImport(count);
    }

    RecordingWork work;
    std::istringstream balancedInput(testDigits(33));
    const auto balanced = importDecimalDigits(balancedInput, 33, 8, &work);
    assert(!balanced.isZero());
    // Five parsed leaves (33 units), two 16-digit merges, one 32-digit
    // merge, and the final 33-digit merge.
    assert(work.total == 130);
    assert(work.calls == 9);

    bool invalidLeafRejected = false;
    try
    {
        std::istringstream input("1");
        (void)importDecimalDigits(input, 1, 0);
    }
    catch (const std::invalid_argument&)
    {
        invalidLeafRejected = true;
    }
    assert(invalidLeafRejected);

    bool truncatedRejected = false;
    try
    {
        std::istringstream input("123");
        (void)importDecimalDigits(input, 5, 8);
    }
    catch (const std::runtime_error& error)
    {
        truncatedRejected =
            std::string_view(error.what())
            == "Output ended during decimal conversion";
    }
    assert(truncatedRejected);

    bool nonDigitRejected = false;
    try
    {
        std::istringstream input("12x45");
        (void)importDecimalDigits(input, 5, 8);
    }
    catch (const std::runtime_error& error)
    {
        nonDigitRejected =
            std::string_view(error.what())
            == "Output changed after inspection";
    }
    assert(nonDigitRejected);


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
