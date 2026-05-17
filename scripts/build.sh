#!/bin/bash
set -euo pipefail

cmake -B build -DCMAKE_BUILD_TYPE=Debug 2>&1 || { echo "CMake configure failed"; exit 1; }

cmake --build build --parallel "$(nproc)" 2>&1 || { echo "Build failed"; exit 1; }

ctest --test-dir build --output-on-failure