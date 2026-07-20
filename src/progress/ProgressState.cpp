#include "progress/ProgressState.hpp"


namespace pi::progress
{

bool isValidTransition(ProgressPhase from, ProgressPhase to) noexcept
{
    if (from == to)
    {
        return true;
    }

    if (isTerminal(from))
    {
        return false;
    }

    if (to == ProgressPhase::failed)
    {
        return true;
    }

    switch (from)
    {
        case ProgressPhase::initializing:
            return to == ProgressPhase::validatingCheckpoints
                || to == ProgressPhase::splitting;
        case ProgressPhase::validatingCheckpoints:
            return to == ProgressPhase::splitting;
        case ProgressPhase::splitting:
            return to == ProgressPhase::merging
                || to == ProgressPhase::finalizing;
        case ProgressPhase::merging:
            return to == ProgressPhase::finalizing;
        case ProgressPhase::finalizing:
            return to == ProgressPhase::writingOutput
                || to == ProgressPhase::completed;
        case ProgressPhase::writingOutput:
            return to == ProgressPhase::verifyingOutput
                || to == ProgressPhase::completed;
        case ProgressPhase::verifyingOutput:
            return to == ProgressPhase::completed;
        case ProgressPhase::completed:
        case ProgressPhase::failed:
            return false;
    }
    return false;
}


std::string_view toString(ProgressPhase phase) noexcept
{
    switch (phase)
    {
        case ProgressPhase::initializing: return "initializing";
        case ProgressPhase::validatingCheckpoints:
            return "validating_checkpoints";
        case ProgressPhase::splitting: return "splitting";
        case ProgressPhase::merging: return "merging";
        case ProgressPhase::finalizing: return "finalizing";
        case ProgressPhase::writingOutput: return "writing_output";
        case ProgressPhase::verifyingOutput: return "verifying_output";
        case ProgressPhase::completed: return "completed";
        case ProgressPhase::failed: return "failed";
    }
    return "unknown";
}


std::string_view toString(ProgressTerminalState state) noexcept
{
    switch (state)
    {
        case ProgressTerminalState::running: return "running";
        case ProgressTerminalState::completed: return "completed";
        case ProgressTerminalState::failed: return "failed";
    }
    return "unknown";
}

} // namespace pi::progress
