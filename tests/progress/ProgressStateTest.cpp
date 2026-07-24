#include "progress/ProgressState.hpp"

#include <array>
#include <cassert>
#include <cstdint>


using pi::progress::ProgressPhase;
using pi::progress::ProgressTerminalState;


int main()
{
    using pi::progress::PROGRESS_SCHEMA_VERSION;
    using pi::progress::isTerminal;
    using pi::progress::isValidTransition;
    using pi::progress::terminalState;
    using pi::progress::toString;

    static_assert(PROGRESS_SCHEMA_VERSION == 3);
    static_assert(!isTerminal(ProgressPhase::initializing));
    static_assert(isTerminal(ProgressPhase::completed));
    static_assert(isTerminal(ProgressPhase::failed));
    static_assert(
        terminalState(ProgressPhase::merging)
        == ProgressTerminalState::running
    );
    static_assert(
        terminalState(ProgressPhase::completed)
        == ProgressTerminalState::completed
    );
    static_assert(
        terminalState(ProgressPhase::failed)
        == ProgressTerminalState::failed
    );

    const std::array orderedPhases{
        ProgressPhase::initializing,
        ProgressPhase::validatingCheckpoints,
        ProgressPhase::splitting,
        ProgressPhase::merging,
        ProgressPhase::finalizing,
        ProgressPhase::writingOutput,
        ProgressPhase::verifyingOutput,
        ProgressPhase::completed
    };

    for (std::size_t index = 1; index < orderedPhases.size(); ++index)
    {
        assert(isValidTransition(
            orderedPhases[index - 1],
            orderedPhases[index]
        ));
    }

    for (const ProgressPhase phase : orderedPhases)
    {
        assert(isValidTransition(phase, phase));
    }
    assert(isValidTransition(ProgressPhase::failed, ProgressPhase::failed));

    assert(isValidTransition(
        ProgressPhase::initializing,
        ProgressPhase::splitting
    ));
    assert(isValidTransition(
        ProgressPhase::splitting,
        ProgressPhase::finalizing
    ));
    assert(isValidTransition(
        ProgressPhase::finalizing,
        ProgressPhase::completed
    ));

    for (const ProgressPhase phase : orderedPhases)
    {
        if (!isTerminal(phase))
        {
            assert(isValidTransition(phase, ProgressPhase::failed));
        }
    }

    assert(!isValidTransition(
        ProgressPhase::splitting,
        ProgressPhase::initializing
    ));
    assert(!isValidTransition(
        ProgressPhase::completed,
        ProgressPhase::failed
    ));
    assert(!isValidTransition(
        ProgressPhase::failed,
        ProgressPhase::initializing
    ));
    assert(!isValidTransition(
        ProgressPhase::writingOutput,
        ProgressPhase::finalizing
    ));

    assert(toString(ProgressPhase::initializing) == "initializing");
    assert(toString(ProgressPhase::validatingCheckpoints)
        == "validating_checkpoints");
    assert(toString(ProgressPhase::writingOutput) == "writing_output");
    assert(toString(ProgressPhase::verifyingOutput) == "verifying_output");
    assert(toString(ProgressPhase::completed) == "completed");
    assert(toString(ProgressPhase::failed) == "failed");
    assert(toString(ProgressTerminalState::running) == "running");
    assert(toString(ProgressTerminalState::completed) == "completed");
    assert(toString(ProgressTerminalState::failed) == "failed");

    assert(toString(static_cast<ProgressPhase>(255)) == "unknown");
    assert(toString(static_cast<ProgressTerminalState>(255)) == "unknown");

    return 0;
}
