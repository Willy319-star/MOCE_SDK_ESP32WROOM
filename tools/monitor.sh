#!/usr/bin/env bash
set -e

SDK_ROOT=$(cd "$(dirname "$0")/.." && pwd)
PROJECT_DIR=${1:-"$SDK_ROOT/examples/blink_minimal"}
PORT=${2:-/dev/ttyUSB0}
DURATION=${3:-0}

if [ -t 0 ]; then
    source "$SDK_ROOT/env/export.sh"
    cd "$PROJECT_DIR"
    idf.py -p "$PORT" monitor
else
    source "$SDK_ROOT/env/export.sh" >/dev/null 2>&1
    python "$SDK_ROOT/tools/monitor_capture.py" "$PROJECT_DIR" "$PORT" --duration "$DURATION"
fi
