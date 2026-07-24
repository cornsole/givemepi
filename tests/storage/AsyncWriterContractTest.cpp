#include "storage/AsyncWriter.hpp"

#include <cassert>
#include <iostream>

int main()
{
    using pi::storage::AsyncWriteLifecycle;
    using pi::storage::AsyncWriteState;
    using pi::storage::toString;

    AsyncWriteLifecycle stored(7);
    assert(stored.requestId() == 7);
    assert(stored.state() == AsyncWriteState::queued);
    assert(!stored.hasDurableCopy());
    assert(stored.beginWriting());
    assert(!stored.cancel());
    assert(stored.completeStored());
    assert(stored.hasDurableCopy());
    assert(!stored.fail("too late"));

    AsyncWriteLifecycle failed(8);
    assert(failed.fail("fsync failed"));
    assert(failed.state() == AsyncWriteState::failed);
    assert(failed.error() == "fsync failed");
    assert(!failed.hasDurableCopy());
    assert(!failed.completeStored());

    AsyncWriteLifecycle cancelled(9);
    assert(cancelled.cancel());
    assert(cancelled.state() == AsyncWriteState::cancelled);
    assert(!cancelled.beginWriting());

    AsyncWriteLifecycle activeFailure(10);
    assert(activeFailure.beginWriting());
    assert(activeFailure.fail("atomic rename failed"));
    assert(activeFailure.state() == AsyncWriteState::failed);
    assert(toString(activeFailure.state()) == "failed");

    std::cout << "AsyncWriterContract OK\n";
    return 0;
}
