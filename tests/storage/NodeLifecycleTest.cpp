#include "storage/NodeLifecycle.hpp"

#include <cassert>
#include <iostream>

int main()
{
    using pi::storage::NodeLifecycle;
    using pi::storage::NodeLifecycleState;
    using pi::storage::toString;

    NodeLifecycle lifecycle;
    assert(lifecycle.state() == NodeLifecycleState::resident);
    assert(toString(lifecycle.state()) == "resident");
    assert(!lifecycle.completeSpill());
    assert(lifecycle.beginSpill());
    assert(!lifecycle.beginSpill());
    assert(lifecycle.failSpill());
    assert(lifecycle.state() == NodeLifecycleState::resident);

    assert(lifecycle.beginSpill());
    assert(lifecycle.completeSpill());
    assert(lifecycle.state() == NodeLifecycleState::stored);
    assert(!lifecycle.completeLoad());
    assert(lifecycle.beginLoad());
    assert(lifecycle.completeLoad());
    assert(lifecycle.state() == NodeLifecycleState::resident);

    assert(lifecycle.beginSpill());
    assert(lifecycle.completeSpill());
    assert(lifecycle.beginLoad());
    assert(lifecycle.failLoad(true));
    assert(lifecycle.state() == NodeLifecycleState::corrupted);
    assert(!lifecycle.beginLoad());
    assert(lifecycle.remove());
    assert(lifecycle.state() == NodeLifecycleState::removed);
    assert(!lifecycle.remove());

    NodeLifecycle missing;
    assert(missing.beginSpill());
    assert(missing.completeSpill());
    assert(missing.beginLoad());
    assert(missing.failLoad(false));
    assert(missing.state() == NodeLifecycleState::removed);

    NodeLifecycle invalid(NodeLifecycleState::removed);
    assert(!invalid.beginSpill());
    assert(!invalid.beginLoad());
    assert(toString(NodeLifecycleState::loading) == "loading");

    std::cout << "NodeLifecycle OK\n";
    return 0;
}
