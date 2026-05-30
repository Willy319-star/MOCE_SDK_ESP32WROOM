#!/usr/bin/env bash
set -e

SDK_ROOT=$(cd "$(dirname "$0")/.." && pwd)
PROJECT_DIR=${1:-"$SDK_ROOT/examples/blink_minimal"}
PROJECT_DIR=$(cd "$PROJECT_DIR" && pwd)

if [ $# -gt 0 ]; then
    shift
fi

TARGET=""
BOARD=""
PORT=""

while [ $# -gt 0 ]; do
    case "$1" in
        --target)
            TARGET=${2:-}
            shift 2
            ;;
        --board)
            BOARD=${2:-}
            shift 2
            ;;
        --port|-p)
            PORT=${2:-}
            shift 2
            ;;
        esp32*)
            TARGET=$1
            shift
            ;;
        my_board_*)
            BOARD=$1
            shift
            ;;
        /dev/*|COM*|com*)
            PORT=$1
            shift
            ;;
        *)
            if [ -z "$PORT" ]; then
                PORT=$1
            elif [ -z "$BOARD" ]; then
                BOARD=$1
            else
                echo "Unknown argument: $1" >&2
                exit 2
            fi
            shift
            ;;
    esac
done

if [ -z "$TARGET" ] && [ -f "$PROJECT_DIR/sdkconfig" ]; then
    TARGET=$(sed -n 's/^CONFIG_IDF_TARGET="\([^"]*\)"/\1/p' "$PROJECT_DIR/sdkconfig" | head -n 1)
fi

TARGET=${TARGET:-esp32}
BOARD=${BOARD:-my_board_$TARGET}
PORT=${PORT:-/dev/ttyUSB0}

SDKCONFIG_DEFAULTS="$SDK_ROOT/boards/$BOARD/sdkconfig.defaults"
if [ -f "$PROJECT_DIR/sdkconfig.defaults" ]; then
    SDKCONFIG_DEFAULTS="$PROJECT_DIR/sdkconfig.defaults;$SDKCONFIG_DEFAULTS"
fi

source "$SDK_ROOT/env/export.sh"

cd "$PROJECT_DIR"

CURRENT_TARGET=""
if [ -f sdkconfig ]; then
    CURRENT_TARGET=$(sed -n 's/^CONFIG_IDF_TARGET="\([^"]*\)"/\1/p' sdkconfig | head -n 1)
fi

if [ "$CURRENT_TARGET" != "$TARGET" ]; then
    idf.py -DMOCE_BOARD="$BOARD" -DSDKCONFIG_DEFAULTS="$SDKCONFIG_DEFAULTS" set-target "$TARGET"
fi

idf.py -DMOCE_BOARD="$BOARD" -DSDKCONFIG_DEFAULTS="$SDKCONFIG_DEFAULTS" -p "$PORT" flash
