#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

rm -rf "${BUILD_DIR}"

BUILD_CMDS="
cmake -G Ninja -S '${SCRIPT_DIR}' -B '${BUILD_DIR}'
cmake --build '${BUILD_DIR}'
cmake --build '${BUILD_DIR}' --target merged_bin
"

if [[ -n "${IDF_PATH:-}" ]]; then
    # 已激活 ESP-IDF，直接构建
    bash -c "set -e; ${BUILD_CMDS}"
else
    ACTIVATE="${IDF_ACTIVATE:-${HOME}/.espressif/tools/activate_idf_v6.0.2.sh}"
    if [[ ! -f "${ACTIVATE}" ]]; then
        echo "找不到 ESP-IDF 激活脚本: ${ACTIVATE}" >&2
        echo "请先激活 ESP-IDF，或用 IDF_ACTIVATE=/path/to/activate_idf_vX.Y.Z.sh 指定" >&2
        exit 1
    fi
    # 激活脚本只在 $0 为 shell 名时才认为自己是被 source 的，故用 bash -c 包一层
    bash -c "
set -e
set +u
source '${ACTIVATE}' > /dev/null
set -u
${BUILD_CMDS}
"
fi

printf '\nMerged firmware: %s\n' "${BUILD_DIR}/ws2812_snake_test-merged.bin"
