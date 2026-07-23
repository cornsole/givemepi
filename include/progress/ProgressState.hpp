#pragma once

#include <cstdint>
#include <string_view>


namespace pi::progress
{

/// Version of the public progress snapshot and reporter schema.
inline constexpr std::uint32_t PROGRESS_SCHEMA_VERSION = 3;


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
