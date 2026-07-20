#pragma once

#include <cstddef>
#include <cstdint>


namespace pi::chudnovsky
{

struct PrecisionPlan
{
    std::uint64_t requestedDigits;
    std::uint32_t guardDigits;
    std::uint64_t workingDigits;
    std::size_t termCount;
    std::uint64_t estimatedBits;
};


class PrecisionPolicy
{
public:

    static constexpr std::uint32_t minimumGuardDigits = 16;
    static constexpr std::uint32_t defaultGuardDigits = 32;

    /**
     * Creates a deterministic integer-only precision plan.
     *
     * Input: non-zero requested digits and at least minimumGuardDigits guards.
     * Output: guarded decimal precision, sufficient term count, and bit estimate.
     * Time complexity: O(1).
     * Memory complexity: O(1).
     *
     * @throws std::invalid_argument for an invalid digit or guard request.
     * @throws std::overflow_error when the derived plan cannot be represented.
     */
    [[nodiscard]]
    static PrecisionPlan create(
        std::uint64_t requestedDigits,
        std::uint32_t guardDigits = defaultGuardDigits
    );
};

} // namespace pi::chudnovsky
