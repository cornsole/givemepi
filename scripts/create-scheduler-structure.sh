#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

mkdir -p \
    "$PROJECT_ROOT/include/scheduler" \
    "$PROJECT_ROOT/src/scheduler" \
    "$PROJECT_ROOT/tests/scheduler"

touch \
    "$PROJECT_ROOT/include/scheduler/ThreadPool.hpp" \
    "$PROJECT_ROOT/src/scheduler/ThreadPool.cpp" \
    "$PROJECT_ROOT/include/scheduler/Task.hpp" \
    "$PROJECT_ROOT/src/scheduler/Task.cpp" \
    "$PROJECT_ROOT/include/scheduler/Worker.hpp" \
    "$PROJECT_ROOT/src/scheduler/Worker.cpp" \
    "$PROJECT_ROOT/include/scheduler/Scheduler.hpp" \
    "$PROJECT_ROOT/src/scheduler/Scheduler.cpp" \
    "$PROJECT_ROOT/include/scheduler/IQueue.hpp" \
    "$PROJECT_ROOT/include/scheduler/ReferenceQueue.hpp" \
    "$PROJECT_ROOT/src/scheduler/ReferenceQueue.cpp" \
    "$PROJECT_ROOT/include/scheduler/LockFreeQueue.hpp" \
    "$PROJECT_ROOT/src/scheduler/LockFreeQueue.cpp" \
    "$PROJECT_ROOT/tests/scheduler/SchedulerTest.cpp"

echo "Scheduler structure created successfully."