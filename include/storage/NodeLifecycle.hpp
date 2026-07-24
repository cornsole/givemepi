#pragma once

#include <string_view>

namespace pi::storage
{

/// Runtime ownership state of one BinaryNode/chunk record.
enum class NodeLifecycleState
{
    resident,
    spilling,
    stored,
    loading,
    corrupted,
    removed
};

[[nodiscard]]
std::string_view toString(NodeLifecycleState state) noexcept;

/// Small state machine for the synchronous PR-0026 storage boundary.
///
/// This class tracks state only; it does not own a BinaryNode or perform I/O.
/// In particular, a failed spill returns to resident so the caller can retain
/// the original value, while a failed load distinguishes a corrupt artifact
/// from an absent artifact.
class NodeLifecycle
{
public:
    explicit NodeLifecycle(
        NodeLifecycleState initial = NodeLifecycleState::resident
    ) noexcept;

    [[nodiscard]] NodeLifecycleState state() const noexcept;

    [[nodiscard]] bool beginSpill() noexcept;
    [[nodiscard]] bool completeSpill() noexcept;
    [[nodiscard]] bool failSpill() noexcept;

    [[nodiscard]] bool beginLoad() noexcept;
    [[nodiscard]] bool completeLoad() noexcept;
    /// `artifactExists=true` means validation failed; false means it vanished.
    [[nodiscard]] bool failLoad(bool artifactExists) noexcept;

    [[nodiscard]] bool remove() noexcept;

private:
    NodeLifecycleState state_;
};

} // namespace pi::storage
