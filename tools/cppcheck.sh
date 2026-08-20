#!/usr/bin/env bash
#
# 对固件代码跑 cppcheck，与 clang-tidy 互补：
# cppcheck 更擅长缓冲区、未初始化变量、可疑的有无符号比较，
# clang-tidy 更擅长跨函数的路径敏感分析。两者盲区不同，都跑一遍成本很低。
#
# 需要先构建过一次（依赖 build/compile_commands.json 里的宏定义与 include 路径），
# 并安装 cppcheck（brew install cppcheck / apt-get install cppcheck）。
#
# 只分析我们自己的代码：ESP-IDF 与托管组件的告警不是我们能修的，
# 混进来只会让报告没人看。
set -euo pipefail

cd "$(dirname "$0")/.."

if ! command -v cppcheck > /dev/null; then
    echo "cppcheck 未安装：brew install cppcheck 或 apt-get install -y cppcheck" >&2
    exit 1
fi

if [ ! -f build/compile_commands.json ]; then
    echo "build/compile_commands.json 不存在，请先运行 idf.py build" >&2
    exit 1
fi

cppcheck \
    --project=build/compile_commands.json \
    --file-filter='*/components/kenko_*' \
    --file-filter='*/main/*' \
    --enable=warning,style,performance,portability \
    --inline-suppr \
    --quiet \
    -j "$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)" \
    --error-exitcode=1 \
    `# ESP-IDF 的头文件不在 cppcheck 的系统路径里，缺头文件不是我们的问题` \
    --suppress=missingIncludeSystem \
    `# 组件对外导出的 API 在本项目里可能确实没被调用，这是库的常态` \
    --suppress=unusedFunction \
    `# esp_event / httpd 的回调签名是固定的，改成 const 就注册不上了` \
    --suppress=constParameterCallback

echo "cppcheck 干净。"
