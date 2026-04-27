#!/bin/bash
set -e
QT_PATH=$(qt6-cmake-config-path Qt6 2>/dev/null || echo "/usr/lib/x86_64-linux-gnu/cmake/Qt6")
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QT_PATH"
cmake --build build -j$(nproc)
