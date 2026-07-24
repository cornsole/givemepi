#include "storage/NodeLifecycle.hpp"

namespace pi::storage
{

std::string_view toString(NodeLifecycleState state) noexcept
{
    switch (state)
    {
        case NodeLifecycleState::resident: return "resident";
        case NodeLifecycleState::spilling: return "spilling";
        case NodeLifecycleState::stored: return "stored";
        case NodeLifecycleState::loading: return "loading";
        case NodeLifecycleState::corrupted: return "corrupted";
        case NodeLifecycleState::removed: return "removed";
    }
    return "unknown";
}

NodeLifecycle::NodeLifecycle(NodeLifecycleState initial) noexcept
    : state_(initial)
{
}

NodeLifecycleState NodeLifecycle::state() const noexcept
{
    return state_;
}

bool NodeLifecycle::beginSpill() noexcept
{
    if (state_ != NodeLifecycleState::resident) return false;
    error_.reset();
    state_ = NodeLifecycleState::spilling;
    return true;
}

bool NodeLifecycle::completeSpill() noexcept
{
    if (state_ != NodeLifecycleState::spilling) return false;
    state_ = NodeLifecycleState::stored;
    return true;
}

bool NodeLifecycle::failSpill() noexcept
{
    if (state_ != NodeLifecycleState::spilling) return false;
    state_ = NodeLifecycleState::resident;
    return true;
}

bool NodeLifecycle::finishAsyncSpill(
    AsyncWriteState result,
    std::string_view detail
)
{
    if (state_ != NodeLifecycleState::spilling) return false;
    if (result == AsyncWriteState::stored)
    {
        state_ = NodeLifecycleState::stored;
        error_.reset();
        return true;
    }
    if (result != AsyncWriteState::failed
        && result != AsyncWriteState::cancelled)
    {
        return false;
    }

    state_ = NodeLifecycleState::resident;
    if (!detail.empty()) error_ = std::string(detail);
    return false;
}

const std::optional<std::string>& NodeLifecycle::error() const noexcept
{
    return error_;
}

bool NodeLifecycle::beginLoad() noexcept
{
    if (state_ != NodeLifecycleState::stored) return false;
    state_ = NodeLifecycleState::loading;
    return true;
}

bool NodeLifecycle::completeLoad() noexcept
{
    if (state_ != NodeLifecycleState::loading) return false;
    state_ = NodeLifecycleState::resident;
    return true;
}

bool NodeLifecycle::failLoad(bool artifactExists) noexcept
{
    if (state_ != NodeLifecycleState::loading) return false;
    state_ = artifactExists
        ? NodeLifecycleState::corrupted
        : NodeLifecycleState::removed;
    return true;
}

bool NodeLifecycle::remove() noexcept
{
    if (state_ != NodeLifecycleState::stored
        && state_ != NodeLifecycleState::corrupted)
    {
        return false;
    }
    state_ = NodeLifecycleState::removed;
    return true;
}

} // namespace pi::storage
