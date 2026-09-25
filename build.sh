#!/usr/bin/env bash

set -eu

test -x "$(which cmake)" || { echo "Error! No cmake found!" >&1; exit 1; }
test -x "$(which cpack)" || { echo "Error! No cpack found!" >&1; exit 1; }

SRC_DIR="$(dirname $0)"
BUILD_DIR="__build"

echo "Sources: $SRC_DIR"
echo "Build dir: $BUILD_DIR"

mkdir -p "$BUILD_DIR"
cmake -S "$SRC_DIR" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" --parallel $(nproc) \
  --target all \
  --target package
