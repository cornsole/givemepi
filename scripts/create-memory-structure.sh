#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "==> Creating memory structure"

mkdir -p "${ROOT_DIR}/include/memory"
mkdir -p "${ROOT_DIR}/src/memory"
mkdir -p "${ROOT_DIR}/tests/memory"

touch "${ROOT_DIR}/include/memory/Arena.hpp"
touch "${ROOT_DIR}/include/memory/MemoryPool.hpp"
touch "${ROOT_DIR}/include/memory/Alignment.hpp"
touch "${ROOT_DIR}/include/memory/ScratchBuffer.hpp"

touch "${ROOT_DIR}/src/memory/Arena.cpp"
touch "${ROOT_DIR}/src/memory/MemoryPool.cpp"
touch "${ROOT_DIR}/src/memory/Alignment.cpp"
touch "${ROOT_DIR}/src/memory/ScratchBuffer.cpp"

touch "${ROOT_DIR}/tests/memory/MemoryTest.cpp"

echo "==> Memory structure created"

echo
echo "Created:"
echo
echo "include/memory/"
echo "  Arena.hpp"
echo "  MemoryPool.hpp"
echo "  Alignment.hpp"
echo "  ScratchBuffer.hpp"
echo
echo "src/memory/"
echo "  Arena.cpp"
echo "  MemoryPool.cpp"
echo "  Alignment.cpp"
echo "  ScratchBuffer.cpp"
echo
echo "tests/memory/"
echo "  MemoryTest.cpp"