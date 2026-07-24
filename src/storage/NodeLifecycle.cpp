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
