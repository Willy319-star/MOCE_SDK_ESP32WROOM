#!/usr/bin/env bash
set -e

SDK_ROOT=$(cd "$(dirname "$0")/.." && pwd)
PROJECT_DIR=${1:-"$SDK_ROOT/examples/blink_minimal"}
PROJECT_DIR=$(cd "$PROJECT_DIR" && pwd)
TARGET=${2:-esp32}
BOARD=${3:-my_board_$TARGET}

SDKCONFIG_DEFAULTS="$SDK_ROOT/boards/$BOARD/sdkconfig.defaults"
if [ -f "$PROJECT_DIR/sdkconfig.defaults" ]; then
    SDKCONFIG_DEFAULTS="$PROJECT_DIR/sdkconfig.defaults;$SDKCONFIG_DEFAULTS"
fi

source "$SDK_ROOT/env/export.sh"

cd "$PROJECT_DIR"

BUILD_DIR="$PROJECT_DIR/build"
if [ -d "$BUILD_DIR" ] && { [ ! -f "$BUILD_DIR/CMakeCache.txt" ] || [ ! -f "$BUILD_DIR/build.ninja" ]; }; then
    BACKUP_DIR="$PROJECT_DIR/build.invalid.$(date +%Y%m%d_%H%M%S)"
    echo "Build directory exists but is not a valid CMake build directory."
    echo "Moving it aside: $BUILD_DIR -> $BACKUP_DIR"
    mv "$BUILD_DIR" "$BACKUP_DIR"
fi

idf.py -DMOCE_BOARD="$BOARD" -DSDKCONFIG_DEFAULTS="$SDKCONFIG_DEFAULTS" set-target "$TARGET"
idf.py -DMOCE_BOARD="$BOARD" -DSDKCONFIG_DEFAULTS="$SDKCONFIG_DEFAULTS" build
