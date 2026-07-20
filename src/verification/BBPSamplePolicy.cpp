#include "verification/BBPSamplePolicy.hpp"

#include "verification/BBPHexDigit.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace pi::verification
{
namespace
{

std::uint64_t mix(std::uint64_t value) noexcept
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

std::uint64_t identitySeed(const VerificationIdentity& identity) noexcept
{
    std::uint64_t seed = mix(identity.requestedDigits);
    seed ^= mix(identity.guardDigits + 0x100000001ULL);
    seed ^= mix(identity.workingDigits + 0x200000003ULL);
    seed ^= mix(identity.termCount + 0x300000007ULL);
    return mix(seed);
}

std::uint64_t fractionOf(
    std::uint64_t value,
    std::uint64_t numerator,
    std::uint64_t denominator
) noexcept
{
    return (value / denominator) * numerator
        + ((value % denominator) * numerator) / denominator;
}

void addUnique(
    std::vector<std::uint64_t>& positions,
    std::uint64_t position,
    std::uint64_t safeCount,
    std::size_t maximumSamples
)
{
    if (position >= safeCount || positions.size() >= maximumSamples)
    {
        return;
    }
    if (std::find(positions.begin(), positions.end(), position)
        == positions.end())
    {
        positions.push_back(position);
    }
}

} // namespace

BBPSamplePolicy::BBPSamplePolicy(std::size_t maximumSamples)
    : maximumSamples_(maximumSamples)
{
    if (maximumSamples_ == 0
        || maximumSamples_ > MAXIMUM_BBP_SAMPLE_COUNT)
    {
        throw std::invalid_argument("BBP sample count must be between 1 and 32");
    }
}

std::size_t BBPSamplePolicy::maximumSamples() const noexcept
{
    return maximumSamples_;
}

std::uint64_t BBPSamplePolicy::safePositionCount(
    std::uint64_t requestedDigits
) noexcept
{
    if (requestedDigits <= 2)
    {
        return 0;
    }
    // 0.83 is a strict lower approximation of log_16(10). Two decimal
    // positions are reserved so rounding uncertainty remains away from the
    // nominal precision limit; exact extraction can still reject a boundary.
    const std::uint64_t usable = requestedDigits - 2;
    const std::uint64_t precisionBound =
        (usable / 100U) * 83U + ((usable % 100U) * 83U) / 100U;
    return std::min(
        precisionBound,
        BBPHexDigitCalculator::maximumPosition() + 1
    );
}

std::vector<std::uint64_t> BBPSamplePolicy::select(
    const VerificationIdentity& identity
) const
{
    const std::uint64_t safeCount = safePositionCount(identity.requestedDigits);
    std::vector<std::uint64_t> positions;
    positions.reserve(maximumSamples_);
    if (safeCount == 0)
    {
        return positions;
    }

    for (const std::uint64_t early : std::array<std::uint64_t, 3>{0, 1, 2})
    {
        addUnique(positions, early, safeCount, maximumSamples_);
    }

    const std::uint64_t last = safeCount - 1;
    addUnique(positions, fractionOf(last, 1, 4), safeCount, maximumSamples_);
    addUnique(positions, fractionOf(last, 1, 2), safeCount, maximumSamples_);
    addUnique(positions, fractionOf(last, 3, 4), safeCount, maximumSamples_);

    std::uint64_t state = identitySeed(identity);
    for (std::size_t attempt = 0;
         positions.size() < maximumSamples_ && attempt < maximumSamples_ * 4;
         ++attempt)
    {
        state = mix(state + attempt);
        addUnique(positions, state % safeCount, safeCount, maximumSamples_);
    }

    // For very small ranges, deterministic probing guarantees completion even
    // when identity-derived positions repeatedly collide.
    for (std::uint64_t position = 0;
         positions.size() < maximumSamples_ && position < safeCount;
         ++position)
    {
        addUnique(positions, position, safeCount, maximumSamples_);
    }
    std::sort(positions.begin(), positions.end());
    return positions;
}

} // namespace pi::verification
