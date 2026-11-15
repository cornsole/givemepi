#pragma once

#include "progress/ProgressSnapshot.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>


namespace pi::progress
{

/// Weighted finalization steps published by the calculator.
///
/// Weights are in multiples of one working-precision multiplication: the
/// fixed-point square root costs about three, scaling by Q and dividing by T
/// about five, and decimal conversion about half the bit length of the
/// working digit count.
struct FinalizationWork
{
    std::uint64_t squareRoot;
    std::uint64_t scaling;
    std::uint64_t conversion;

    [[nodiscard]]
    constexpr std::uint64_t total() const noexcept
    {
        return squareRoot + scaling + conversion;
    }
};


/// Returns the finalization step weights for a working precision.
///
/// Time complexity: O(1). Memory complexity: O(1).
[[nodiscard]]
constexpr FinalizationWork finalizationWork(std::uint64_t workingDigits) noexcept
{
    const auto bits = static_cast<std::uint64_t>(
        std::bit_width(std::max<std::uint64_t>(workingDigits, 1))
    );
    return FinalizationWork{3, 5, std::max<std::uint64_t>(1, bits / 2)};
}


/// Inputs of the end-to-end normalized cost plan.
struct PipelineCostInputs
{
    /// Requested plus guard digits.
    std::uint64_t workingDigits;
    std::uint64_t totalTerms;
    /// Planned frontier nodes entering the merge stage (accepted resume
    /// blocks plus leaf blocks); one or zero means no merge work.
    std::uint64_t frontierNodes;
    std::uint64_t workerCount;
    /// Bytes written to the decimal output file.
    std::uint64_t outputBytes;
    /// Verification work in byte-pass units; zero when verification is off.
    std::uint64_t verificationUnits;
};


/// Plans normalized per-phase costs for the total ETA.
///
/// Purpose: weight dissimilar phase units so remaining work can be summed.
/// The unit is one sequential pass over one output byte. One working-precision
/// multiplication M costs 4 * D * bit_width(D) for D working digits, and one
/// Binary Splitting tree level costs 2M. Leaf splitting covers the tree levels
/// below the merge frontier and runs on min(workers, frontier) threads; each
/// merge level runs on min(workers, pairs) threads; finalization runs on one
/// thread and costs M times its step weights; output writing costs four units
/// per byte; verification costs its declared units. Checkpoint validation is
/// finished before the tracker exists and is not planned.
/// Output: costs indexed by phaseIndex(); a phase without work costs zero and
/// any planned phase costs at least one. When the sum would exceed 2^60 all
/// costs are scaled down proportionally, so only ratios carry meaning.
/// The model is a heuristic; observed rates correct it while running.
/// Time complexity: O(log frontierNodes). Memory complexity: O(1).
[[nodiscard]]
inline ProgressPhaseCosts planPipelineCosts(
    const PipelineCostInputs& inputs
) noexcept
{
    const long double digits = static_cast<long double>(
        std::max<std::uint64_t>(inputs.workingDigits, 1)
    );
    const long double bits = static_cast<long double>(
        std::bit_width(std::max<std::uint64_t>(inputs.workingDigits, 1))
    );
    const long double multiplication = 4.0L * digits * bits;
    const long double level = 2.0L * multiplication;
    const std::uint64_t workers = std::max<std::uint64_t>(inputs.workerCount, 1);
    const std::uint64_t frontier =
        std::max<std::uint64_t>(inputs.frontierNodes, 1);

    const std::uint64_t mergeDepth = frontier > 1
        ? static_cast<std::uint64_t>(std::bit_width(frontier - 1))
        : 0;
    const std::uint64_t treeDepth = std::max<std::uint64_t>(
        static_cast<std::uint64_t>(
            std::bit_width(std::max<std::uint64_t>(inputs.totalTerms, 1))
        ),
        mergeDepth + 1
    );

    std::array<long double, PROGRESS_PHASE_COUNT> costs{};
    costs[phaseIndex(ProgressPhase::splitting)] =
        level * static_cast<long double>(treeDepth - mergeDepth)
        / static_cast<long double>(std::min(workers, frontier));

    long double merging = 0.0L;
    for (std::uint64_t nodes = frontier; nodes > 1; nodes = nodes / 2 + nodes % 2)
    {
        const std::uint64_t pairs = nodes / 2;
        merging += level / static_cast<long double>(std::min(workers, pairs));
    }
    costs[phaseIndex(ProgressPhase::merging)] = merging;
    costs[phaseIndex(ProgressPhase::finalizing)] = multiplication
        * static_cast<long double>(
            finalizationWork(inputs.workingDigits).total()
        );
    costs[phaseIndex(ProgressPhase::writingOutput)] =
        4.0L * static_cast<long double>(inputs.outputBytes);
    costs[phaseIndex(ProgressPhase::verifyingOutput)] =
        static_cast<long double>(inputs.verificationUnits);

    long double sum = 0.0L;
    for (const long double cost : costs)
    {
        sum += cost;
    }
    constexpr long double limit = 1152921504606846976.0L; // 2^60
    const long double scale = sum > limit ? limit / sum : 1.0L;

    ProgressPhaseCosts planned{};
    for (std::size_t index = 0; index < PROGRESS_PHASE_COUNT; ++index)
    {
        if (costs[index] > 0.0L)
        {
            planned[index] = std::max<std::uint64_t>(
                1,
                static_cast<std::uint64_t>(costs[index] * scale)
            );
        }
    }
    return planned;
}

} // namespace pi::progress
