#include "verification/DecimalHexDigit.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace pi::verification
{
namespace
{

pi::bigint::GMPInteger parseFraction(const CanonicalDecimal& decimal)
{
    pi::bigint::GMPInteger value;
    // CanonicalDecimal owns a null-terminated std::string, so its fractional
    // suffix can be parsed directly without another output-sized copy.
    if (mpz_set_str(*value.raw(), decimal.bytes().data() + 2, 10) != 0)
    {
        throw std::invalid_argument("Invalid canonical decimal digits");
    }
    return value;
}

struct ImportNode
{
    pi::bigint::GMPInteger value;
    std::uint64_t digits;
    std::size_t level;
    bool regular;
};

void publishImportWork(
    VerificationWorkObserver* observer,
    std::uint64_t units
) noexcept
{
    if (observer != nullptr && units != 0)
    {
        observer->workCompleted(units);
    }
}

const pi::bigint::GMPInteger& powerForLevel(
    std::vector<pi::bigint::GMPInteger>& powers,
    std::size_t level,
    std::size_t leafDigits
)
{
    while (powers.size() <= level)
    {
        pi::bigint::GMPInteger power;
        if (powers.empty())
        {
            power.setPowerOfTen(static_cast<std::uint64_t>(leafDigits));
        }
        else
        {
            power.set(powers.back());
            power.mul(powers.back());
        }
        powers.push_back(std::move(power));
    }
    return powers[level];
}

ImportNode mergeRegularNodes(
    ImportNode left,
    ImportNode right,
    std::vector<pi::bigint::GMPInteger>& powers,
    std::size_t leafDigits,
    VerificationWorkObserver* observer
)
{
    left.value.mul(powerForLevel(powers, left.level, leafDigits));
    left.value.add(right.value);
    left.digits += right.digits;
    ++left.level;
    publishImportWork(observer, left.digits);
    return left;
}

} // namespace
pi::bigint::GMPInteger importDecimalDigits(
    std::istream& input,
    std::uint64_t digitCount,
    std::size_t leafDigits,
    VerificationWorkObserver* observer
)
{
    constexpr auto maxRead = static_cast<std::uintmax_t>(
        std::numeric_limits<std::streamsize>::max()
    );
    if (leafDigits == 0
        || leafDigits == std::numeric_limits<std::size_t>::max()
        || static_cast<std::uintmax_t>(leafDigits) > maxRead)
    {
        throw std::invalid_argument("Decimal import leaf size is invalid");
    }

    std::string buffer(leafDigits + 1, '\0');
    std::vector<ImportNode> nodes;
    std::vector<pi::bigint::GMPInteger> powers;
    std::uint64_t remaining = digitCount;
    while (remaining != 0)
    {
        const auto count = static_cast<std::size_t>(
            std::min<std::uint64_t>(remaining, leafDigits)
        );
        input.read(buffer.data(), static_cast<std::streamsize>(count));
        if (input.gcount() != static_cast<std::streamsize>(count))
        {
            throw std::runtime_error("Output ended during decimal conversion");
        }
        if (!std::all_of(
                buffer.begin(),
                buffer.begin() + static_cast<std::ptrdiff_t>(count),
                [](char digit) { return digit >= '0' && digit <= '9'; }
            ))
        {
            throw std::runtime_error("Output changed after inspection");
        }
        buffer[count] = '\0';

        pi::bigint::GMPInteger value;
        if (mpz_set_str(*value.raw(), buffer.data(), 10) != 0)
        {
            throw std::runtime_error("Output changed after inspection");
        }
        nodes.push_back({
            std::move(value),
            static_cast<std::uint64_t>(count),
            0,
            count == leafDigits
        });
        publishImportWork(observer, static_cast<std::uint64_t>(count));
        remaining -= count;

        while (nodes.size() >= 2
            && nodes[nodes.size() - 2].regular
            && nodes.back().regular
            && nodes[nodes.size() - 2].level == nodes.back().level)
        {
            ImportNode right = std::move(nodes.back());
            nodes.pop_back();
            ImportNode left = std::move(nodes.back());
            nodes.pop_back();
            nodes.push_back(mergeRegularNodes(
                std::move(left),
                std::move(right),
                powers,
                leafDigits,
                observer
            ));
        }
    }

    if (nodes.empty())
    {
        return pi::bigint::GMPInteger{};
    }
    ImportNode right = std::move(nodes.back());
    nodes.pop_back();
    while (!nodes.empty())
    {
        ImportNode left = std::move(nodes.back());
        nodes.pop_back();
        pi::bigint::GMPInteger scale;
        scale.setPowerOfTen(right.digits);
        left.value.mul(scale);
        left.value.add(right.value);
        left.digits += right.digits;
        left.regular = false;
        publishImportWork(observer, left.digits);
        right = std::move(left);
    }
    return std::move(right.value);
}



bool DecimalHexDigit::resolved() const noexcept
{
    return status == BBPDigitStatus::resolved && digit.has_value();
}

DecimalHexDigitExtractor::DecimalHexDigitExtractor(
    const CanonicalDecimal& decimal
)
    : DecimalHexDigitExtractor(
        parseFraction(decimal),
        decimal.requestedDigits()
    )
{
}

DecimalHexDigitExtractor::DecimalHexDigitExtractor(
    pi::bigint::GMPInteger numerator,
    std::uint64_t requestedDigits
)
    : numerator_(std::move(numerator)),
      denominator_(),
      requestedDigits_(requestedDigits)
{
    if (requestedDigits_ == 0)
    {
        throw std::invalid_argument("Decimal hex extraction requires digits");
    }
    denominator_.setPowerOfTen(requestedDigits_);
}

DecimalHexDigitExtractor DecimalHexDigitExtractor::fromFile(
    const InspectedOutputFile& file,
    VerificationWorkObserver* observer
)
{
    if (file.requestedDigits == 0
        || file.requestedDigits > std::numeric_limits<std::uint64_t>::max() - 3
        || file.canonicalByteCount != file.requestedDigits + 2
        || file.fileSize != file.canonicalByteCount + 1)
    {
        throw std::invalid_argument("Invalid inspected output metadata");
    }

    std::ifstream input(file.path, std::ios::binary);
    char prefix[2]{};
    input.read(prefix, 2);
    if (!input || prefix[0] != '3' || prefix[1] != '.')
    {
        throw std::runtime_error("Output changed after inspection");
    }

    auto numerator = importDecimalDigits(
        input,
        file.requestedDigits,
        DEFAULT_IMPORT_LEAF_DIGITS,
        observer
    );
    char newline = '\0';
    input.get(newline);
    if (newline != '\n' || input.peek() != std::char_traits<char>::eof())
    {
        throw std::runtime_error("Output framing changed after inspection");
    }
    return DecimalHexDigitExtractor(std::move(numerator), file.requestedDigits);
}

DecimalHexDigit DecimalHexDigitExtractor::extract(
    std::uint64_t position
) const
{
    if (position == std::numeric_limits<std::uint64_t>::max()
        || position + 1 > std::numeric_limits<mp_bitcnt_t>::max() / 4)
    {
        throw std::out_of_range("Hexadecimal position is too large");
    }
    const mp_bitcnt_t shift = static_cast<mp_bitcnt_t>(4 * (position + 1));

    // center = D / 10^n, true value is conservatively in center +/- 0.5/10^n.
    // Multiplying all interval terms by two keeps the calculation integral.
    pi::bigint::GMPInteger center(numerator_);
    mpz_mul_2exp(*center.raw(), *center.raw(), shift + 1);
    pi::bigint::GMPInteger radius(1);
    mpz_mul_2exp(*radius.raw(), *radius.raw(), shift);
    pi::bigint::GMPInteger doubledDenominator(denominator_);
    mpz_mul_2exp(
        *doubledDenominator.raw(),
        *doubledDenominator.raw(),
        1
    );

    pi::bigint::GMPInteger lower(center);
    lower.sub(radius);
    lower.floorDivide(doubledDenominator);
    pi::bigint::GMPInteger upper(center);
    upper.add(radius);
    upper.floorDivide(doubledDenominator);
    if (lower.compare(upper) != 0)
    {
        return {position, BBPDigitStatus::inconclusive, std::nullopt};
    }
    const auto digit = static_cast<std::uint8_t>(
        mpz_fdiv_ui(*lower.raw(), 16)
    );
    return {position, BBPDigitStatus::resolved, digit};
}

std::uint64_t DecimalHexDigitExtractor::requestedDigits() const noexcept
{
    return requestedDigits_;
}

} // namespace pi::verification
