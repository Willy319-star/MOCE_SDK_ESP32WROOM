#!/usr/bin/env bash

SDK_ROOT=$(cd "$(dirname "$0")/.." && pwd)
export MY_SDK_ROOT="$SDK_ROOT"
export IDF_PATH="$SDK_ROOT/third_party/esp-idf"

. "$IDF_PATH/export.sh"