#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "[verify-text-layout] checking golden fixtures..."
bun scripts/generate-text-layout-goldens.mjs --check

BUILD_DIR="${RAYM3_BUILD_DIR:-$ROOT/build-test}"
if [[ ! -d "$BUILD_DIR" ]]; then
  cmake -B "$BUILD_DIR" \
    -DRAYM3_BUILD_TESTS=ON \
    -DRAYM3_BUILD_EXAMPLES=OFF \
    -DRAYM3_USE_YOGA=OFF
fi

cmake --build "$BUILD_DIR" --target text_layout_test -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"
"$BUILD_DIR/text_layout_test"

echo "[verify-text-layout] PASS"
