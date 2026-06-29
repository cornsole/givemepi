#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "==> Creating platform structure"

mkdir -p "${ROOT_DIR}/include/platform"
mkdir -p "${ROOT_DIR}/src/platform"
mkdir -p "${ROOT_DIR}/tests/platform"

touch "${ROOT_DIR}/include/platform/CPUInfo.hpp"
touch "${ROOT_DIR}/include/platform/CPUFeatures.hpp"

touch "${ROOT_DIR}/src/platform/CPUInfo.cpp"
touch "${ROOT_DIR}/src/platform/CPUFeatures.cpp"

touch "${ROOT_DIR}/tests/platform/CPUFeaturesTest.cpp"

echo "==> Platform structure created"

echo
echo "Created:"
echo "  include/platform/"
echo "    CPUInfo.hpp"
echo "    CPUFeatures.hpp"
echo
echo "  src/platform/"
echo "    CPUInfo.cpp"
echo "    CPUFeatures.cpp"
echo
echo "  tests/platform/"
echo "    CPUFeaturesTest.cpp"