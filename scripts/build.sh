#!/usr/bin/env bash
set -euo pipefail

export PKG_CONFIG_PATH="${PKG_CONFIG_PATH:-/usr/local/freeswitch/lib/pkgconfig}"
cmake -S . -B build -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-RelWithDebInfo}"
cmake --build build --parallel
ctest --test-dir build --output-on-failure

