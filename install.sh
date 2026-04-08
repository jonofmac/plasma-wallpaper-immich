#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
INSTALL_PREFIX="${INSTALL_PREFIX:-$HOME/.local}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

echo "==> Configuring"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"

echo "==> Building"
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo "==> Installing to $INSTALL_PREFIX"
cmake --install "$BUILD_DIR"

echo
echo "Installed plasma-wallpaper-immich."
echo "If it is not visible yet, restart plasmashell or log out and back in."
