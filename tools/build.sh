#!/usr/bin/env bash
set -e

SDK_ROOT=$(cd "$(dirname "$0")/.." && pwd)
PROJECT_DIR=${1:-"$SDK_ROOT/examples/blink_minimal"}
TARGET=${2:-esp32}

source "$SDK_ROOT/env/export.sh"

cd "$PROJECT_DIR"

idf.py set-target "$TARGET"
idf.py build