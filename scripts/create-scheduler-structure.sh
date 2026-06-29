#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "==> Creating scheduler structure"

mkdir -p "${ROOT_DIR}/include/scheduler"
mkdir -p "${ROOT_DIR}/src/scheduler"
mkdir -p "${ROOT_DIR}/tests/scheduler"


touch "${ROOT_DIR}/include/scheduler/Task.hpp"
touch "${ROOT_DIR}/include/scheduler/LockFreeQueue.hpp"
touch "${ROOT_DIR}/include/scheduler/Worker.hpp"
touch "${ROOT_DIR}/include/scheduler/Scheduler.hpp"


touch "${ROOT_DIR}/src/scheduler/Task.cpp"
touch "${ROOT_DIR}/src/scheduler/LockFreeQueue.cpp"
touch "${ROOT_DIR}/src/scheduler/Worker.cpp"
touch "${ROOT_DIR}/src/scheduler/Scheduler.cpp"


touch "${ROOT_DIR}/tests/scheduler/SchedulerTest.cpp"


echo "==> Scheduler structure created"

echo
echo "Created:"
echo
echo "include/scheduler/"
echo "  Task.hpp"
echo "  LockFreeQueue.hpp"
echo "  Worker.hpp"
echo "  Scheduler.hpp"
echo
echo "src/scheduler/"
echo "  Task.cpp"
echo "  LockFreeQueue.cpp"
echo "  Worker.cpp"
echo "  Scheduler.cpp"
echo
echo "tests/scheduler/"
echo "  SchedulerTest.cpp"