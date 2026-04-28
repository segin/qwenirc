#!/bin/bash
set -e

usage() {
  echo "Usage: $0 [-h] [-d]"
  echo "  -d    Build with Debug variant (default: Release)"
  echo "  -h    Show this help"
  exit 0
}

DEBUG=0

while getopts "dh" opt; do
  case "$opt" in
    d) DEBUG=1 ;;
    h) usage ;;
    *) usage ;;
  esac
done

BUILD_TYPE="Release"
if [ "$DEBUG" -eq 1 ]; then
  BUILD_TYPE="Debug"
fi

QT_PATH=""
if command -v pkg-config &>/dev/null; then
  QT_DIR="$(pkg-config --variable=cmake_dir Qt6)"
  if [ -n "$QT_DIR" ]; then
    QT_PATH="$QT_DIR"
  fi
fi

cmake -B build \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_PREFIX_PATH="${QT_PATH}"

cmake --build build -j$(nproc)
