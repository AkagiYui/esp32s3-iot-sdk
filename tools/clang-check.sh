#!/usr/bin/env bash
#
# 对固件代码跑 clang-tidy，只在我们自己的代码有告警时失败。
#
# 需要先激活 ESP-IDF 环境（`. $IDF_PATH/export.sh`），并且已经构建过一次
# （clang-tidy 依赖 build/compile_commands.json）。
#
# 检查集合从 `-*` 起步、只挑高信噪比的项，而不是打开全量后再逐条压制：
# ESP-IDF 的默认集合在本项目上产生 2000+ 条告警，其中绝大多数来自 ESP_LOGx /
# ESP_RETURN_ON_ERROR 这类宏展开和纯风格偏好，会把真正的问题淹掉。
#
# 明确排除的两项：
#   bugprone-branch-clone        ESP_LOGx 展开成条件链，每条日志都会命中
#   bugprone-easily-swappable-*  对 (ssid, password) 这种同类型相邻参数误报率极高
#
# 注意：检查集合只能从命令行传，写在 .clang-tidy 里会被 idf.py clang-check
# 自己传的 -checks 覆盖掉。
set -euo pipefail

CHECKS='-*,clang-analyzer-*,bugprone-*,performance-*,-bugprone-branch-clone,-bugprone-easily-swappable-parameters'

cd "$(dirname "$0")/.."

if [ ! -f build/compile_commands.json ]; then
    echo "build/compile_commands.json 不存在，请先运行 idf.py build" >&2
    exit 1
fi

# pyclang 会在 PATH 里找可执行的 idf.py；交互式 shell 里它通常只是个函数。
export PATH="${IDF_PATH:?IDF_PATH is not set, source export.sh first}/tools:$PATH"

# 纯逻辑模块不包含任何 ESP-IDF 头文件，clang 找不到 newlib 的 <string.h>，
# 把工具链自己的 sysroot 显式喂给它。
SYSROOT="$(xtensa-esp32s3-elf-gcc -print-sysroot)"

idf.py clang-check \
    --run-clang-tidy-options "-checks=${CHECKS} -extra-arg=-isystem${SYSROOT}/include"

echo
echo "===== 我们自己代码的诊断 ====="
if grep -E '/(components/kenko_[a-z_]+|main)/' warnings.txt | grep -E 'warning:|error:'; then
    echo
    echo "clang-tidy 在固件代码里发现问题，完整报告见 warnings.txt" >&2
    exit 1
fi

echo "干净。（托管组件与 ESP-IDF 自身的告警不计入，见 warnings.txt）"
