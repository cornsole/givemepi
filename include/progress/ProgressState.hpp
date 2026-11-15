#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>


namespace pi::progress
{

/// Version of the public progress snapshot and reporter schema.
///
/// Version 5 replaces the single ETA with independently nullable total and
/// current-phase ETA values and adds raw current-phase work counters.
inline constexpr std::uint32_t PROGRESS_SCHEMA_VERSION = 5;


/// High-level phase of one pi computation.
enum class ProgressPhase
{
    initializing,
    validatingCheckpoints,
    splitting,
    merging,
    finalizing,
    writingOutput,
    verifyingOutput,
    completed,
    failed
};


/// Number of ProgressPhase enumerators; phases are declared in lifecycle
/// order so their ordinal also orders the end-to-end pipeline.
inline constexpr std::size_t PROGRESS_PHASE_COUNT = 9;


/// Returns the zero-based ordinal of a phase.
///
/// Time complexity: O(1).
/// Memory complexity: O(1).
[[nodiscard]]
constexpr std::size_t phaseIndex(ProgressPhase phase) noexcept
{
    return static_cast<std::size_t>(phase);
}


/// Returns whether a phase can carry measurable pipeline work.
///
/// Initialization and the terminal phases carry no work units; every phase
/// from checkpoint validation through output verification may.
/// Time complexity: O(1).
/// Memory complexity: O(1).
[[nodiscard]]
constexpr bool isWorkPhase(ProgressPhase phase) noexcept
{
    return phase != ProgressPhase::initializing
        && phase != ProgressPhase::completed
        && phase != ProgressPhase::failed;
}


/// Terminal state derived from a progress phase.
enum class ProgressTerminalState
{
    running,
    completed,
    failed
};


/// Returns the terminal state represented by a phase.
///
/// Time complexity: O(1).
/// Memory complexity: O(1).
[[nodiscard]]
constexpr ProgressTerminalState terminalState(ProgressPhase phase) noexcept
{
    switch (phase)
    {
        case ProgressPhase::completed:
            return ProgressTerminalState::completed;
        case ProgressPhase::failed:
            return ProgressTerminalState::failed;
        default:
            return ProgressTerminalState::running;
    }
}


/// Returns whether a phase is terminal.
///
/// Time complexity: O(1).
/// Memory complexity: O(1).
[[nodiscard]]
constexpr bool isTerminal(ProgressPhase phase) noexcept
{
    return terminalState(phase) != ProgressTerminalState::running;
}


/// Returns whether moving from one phase to another obeys the lifecycle.
///
/// Repeating a phase is valid so callers can apply idempotent updates. Any
/// running phase may fail. Terminal phases cannot transition elsewhere.
///
/// Time complexity: O(1).
/// Memory complexity: O(1).
[[nodiscard]]
bool isValidTransition(ProgressPhase from, ProgressPhase to) noexcept;


/// Returns the stable schema spelling of a phase.
///
/// Time complexity: O(1).
/// Memory complexity: O(1).
[[nodiscard]]
std::string_view toString(ProgressPhase phase) noexcept;


/// Returns the stable schema spelling of a terminal state.
///
/// Time complexity: O(1).
/// Memory complexity: O(1).
[[nodiscard]]
std::string_view toString(ProgressTerminalState state) noexcept;

} // namespace pi::progress
