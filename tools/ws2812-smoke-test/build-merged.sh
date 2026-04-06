#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

rm -rf "${BUILD_DIR}"

bash -lc "
set -e
set +u
source /root/.espressif/tools/activate_idf_v5.5.3.sh
set -u
cmake -G Ninja -S '${SCRIPT_DIR}' -B '${BUILD_DIR}'
cmake --build '${BUILD_DIR}'
cmake --build '${BUILD_DIR}' --target merged_bin
"

printf '\nMerged firmware: %s\n' "${SCRIPT_DIR}/build/ws2812_smoke_test-merged.bin"