#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

THIRD_PARTY_DIR="${ROOT_DIR}/third_party"
TOMLPP_DIR="${THIRD_PARTY_DIR}/tomlplusplus"

REPOSITORY="https://github.com/marzer/tomlplusplus.git"
BRANCH="master"

echo "==> Installing toml++"

mkdir -p "${THIRD_PARTY_DIR}"

if [ -d "${TOMLPP_DIR}/.git" ]; then
    echo "==> toml++ already exists."
    echo "==> Updating..."

    git -C "${TOMLPP_DIR}" fetch origin
    git -C "${TOMLPP_DIR}" checkout "${BRANCH}"
    git -C "${TOMLPP_DIR}" pull --ff-only
else
    echo "==> Cloning toml++..."

    git clone \
        --depth 1 \
        --branch "${BRANCH}" \
        "${REPOSITORY}" \
        "${TOMLPP_DIR}"
fi

echo
echo "==> toml++ installed successfully."
echo "Location:"
echo "  ${TOMLPP_DIR}"