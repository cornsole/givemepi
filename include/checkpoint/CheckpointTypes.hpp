#pragma once

#include "chudnovsky/PrecisionPolicy.hpp"

#include <cstdint>
#include <optional>
#include <functional>


namespace pi::checkpoint
{

enum class AlgorithmId : std::uint16_t
{
    chudnovskyBinarySplitting = 1
};


inline constexpr std::uint16_t arithmeticFormatVersion = 1;
inline constexpr std::uint16_t binarySplittingFormulaVersion = 1;


struct ComputationIdentity
{
    AlgorithmId algorithm;
    std::uint16_t arithmeticVersion;
    std::uint16_t formulaVersion;
    std::uint64_t requestedDigits;
    std::uint32_t guardDigits;
    std::uint64_t workingDigits;
    std::uint64_t termCount;

    [[nodiscard]]
    static ComputationIdentity fromPrecisionPlan(
        const chudnovsky::PrecisionPlan& plan
    );

    void validate() const;

    [[nodiscard]]
    bool operator==(const ComputationIdentity&) const noexcept = default;

    [[nodiscard]]
    bool operator<(const ComputationIdentity& other) const noexcept {
        if (algorithm != other.algorithm) return algorithm < other.algorithm;
        if (arithmeticVersion != other.arithmeticVersion) return arithmeticVersion < other.arithmeticVersion;
        if (formulaVersion != other.formulaVersion) return formulaVersion < other.formulaVersion;
        if (requestedDigits != other.requestedDigits) return requestedDigits < other.requestedDigits;
        if (guardDigits != other.guardDigits) return guardDigits < other.guardDigits;
        if (workingDigits != other.workingDigits) return workingDigits < other.workingDigits;
        return termCount < other.termCount;
    }
};


struct BlockLocation
{
    std::uint64_t start;
    std::uint64_t end;
    std::uint32_t treeLevel;

    [[nodiscard]]
    static BlockLocation create(
        std::uint64_t start,
        std::uint64_t end,
        std::uint32_t treeLevel,
        const ComputationIdentity& identity
    );

    void validate(const ComputationIdentity& identity) const;

    [[nodiscard]]
    bool operator==(const BlockLocation&) const noexcept = default;

    [[nodiscard]]
    bool operator<(const BlockLocation& other) const noexcept {
        if (start != other.start) return start < other.start;
        if (end != other.end) return end < other.end;
        return treeLevel < other.treeLevel;
    }
};


// Diagnostic execution settings are intentionally separate from
// ComputationIdentity: changing them cannot invalidate a mathematical block.
struct ExecutionProvenance
{
    std::optional<std::uint32_t> workerCount;
    std::optional<std::uint64_t> sequentialCutoff;
    std::optional<std::uint32_t> tasksPerWorker;
    std::optional<std::uint64_t> queueCapacity;

    [[nodiscard]]
    bool operator==(const ExecutionProvenance&) const noexcept = default;
};

} // namespace pi::checkpoint
