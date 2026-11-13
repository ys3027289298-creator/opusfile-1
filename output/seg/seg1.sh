#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
banner "Segment 1/4: Build via the repository CMake entry"
export PKG_CONFIG_PATH="$HERE/deps/usr/lib/x86_64-linux-gnu/pkgconfig"
pause
echo '$ cmake -S . -B output/build -DOP_DISABLE_HTTP=ON -DOP_DISABLE_DOCS=ON'
pause
cmake -S "$ROOT" -B "$HERE/build" -DCMAKE_BUILD_TYPE=Debug \
  -DOP_DISABLE_HTTP=ON -DOP_DISABLE_DOCS=ON 2>&1 | tail -4
pause
echo '$ cmake --build output/build -j'
pause
cmake --build "$HERE/build" -j"$(nproc)" 2>&1 | tail -6
pause
echo "Build artifacts:"; ls -1 "$HERE/build"/libopusfile.a "$HERE/build"/opusfile_example
pause
