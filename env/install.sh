#!/usr/bin/env bash
set -e

SDK_ROOT=$(cd "$(dirname "$0")/.." && pwd)
IDF_PATH="$SDK_ROOT/third_party/esp-idf"

echo "[INFO] SDK_ROOT = $SDK_ROOT"
echo "[INFO] IDF_PATH  = $IDF_PATH"

git submodule update --init --recursive

cd "$IDF_PATH"
./install.sh