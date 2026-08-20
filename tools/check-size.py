#!/usr/bin/env python3
"""固件体积回归门禁。

固件在 2MB 的 OTA 槽位里跑，一次不经意的改动（多链一个组件、开一项 Kconfig）
就可能吃掉几十上百 KB，而这种增长在 code review 里几乎看不出来。这个脚本把
体积钉在一条基线上：涨得超过容差就失败，需要显式跑 --update 更新基线并提交。

用法：
    idf.py build
    python tools/check-size.py            # 校验
    python tools/check-size.py --update   # 接受当前体积为新基线
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD_DIR = ROOT / "build"
BASELINE_PATH = ROOT / "tools" / "size-baseline.json"

# 相对基线允许的增长；超过就必须显式更新基线
DEFAULT_TOLERANCE = 0.03
# 占 OTA 槽位的比例上限，给未来的功能和 OTA 本身留余量
DEFAULT_PARTITION_BUDGET = 0.75

UNIT_MULTIPLIERS = {"K": 1024, "M": 1024 * 1024}


def parse_size(text: str) -> int:
    """解析 partitions.csv 里的大小，支持 0x 十六进制与 K/M 后缀。"""
    text = text.strip()
    if text.lower().startswith("0x"):
        return int(text, 16)
    if text and text[-1].upper() in UNIT_MULTIPLIERS:
        return int(text[:-1]) * UNIT_MULTIPLIERS[text[-1].upper()]
    return int(text)


def smallest_app_partition() -> int:
    """分区表里最小的 app 分区大小——固件必须能装进任何一个槽位。"""
    sizes = []
    for line in (ROOT / "partitions.csv").read_text(encoding="utf-8").splitlines():
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        fields = [field.strip() for field in line.split(",")]
        if len(fields) >= 5 and fields[1] == "app":
            sizes.append(parse_size(fields[4]))
    if not sizes:
        raise SystemExit("partitions.csv 里没有 app 分区")
    return min(sizes)


def app_binary() -> Path:
    description = BUILD_DIR / "project_description.json"
    if not description.is_file():
        raise SystemExit("build/project_description.json 不存在，请先运行 idf.py build")
    project = json.loads(description.read_text(encoding="utf-8"))["project_name"]
    binary = BUILD_DIR / f"{project}.bin"
    if not binary.is_file():
        raise SystemExit(f"{binary} 不存在，请先运行 idf.py build")
    return binary


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--update", action="store_true", help="把当前体积写成新基线")
    parser.add_argument("--tolerance", type=float, default=DEFAULT_TOLERANCE)
    parser.add_argument("--partition-budget", type=float, default=DEFAULT_PARTITION_BUDGET)
    args = parser.parse_args()

    binary = app_binary()
    current = binary.stat().st_size
    partition = smallest_app_partition()

    if args.update:
        BASELINE_PATH.write_text(
            json.dumps({"app_bin_bytes": current}, indent=2) + "\n", encoding="utf-8"
        )
        print(f"基线已更新为 {current:,} B ({current / 1024:.1f} KB)")
        return 0

    if not BASELINE_PATH.is_file():
        raise SystemExit(f"{BASELINE_PATH} 不存在，先跑一次 --update")

    baseline = json.loads(BASELINE_PATH.read_text(encoding="utf-8"))["app_bin_bytes"]
    delta = current - baseline
    ratio = delta / baseline if baseline else 0.0
    usage = current / partition

    print(f"固件      {current:>9,} B ({current / 1024:8.1f} KB)")
    print(f"基线      {baseline:>9,} B ({baseline / 1024:8.1f} KB)")
    print(f"变化      {delta:>+9,} B ({ratio:+.2%})")
    print(f"槽位占用  {usage:.1%}  (槽位 {partition / 1024 / 1024:.0f} MB)")

    failed = False
    if ratio > args.tolerance:
        print(
            f"\n体积增长 {ratio:.2%} 超过容差 {args.tolerance:.0%}。"
            f"\n确认这是必要的增长后，跑 `python tools/check-size.py --update` 更新基线并提交。",
            file=sys.stderr,
        )
        failed = True

    if usage > args.partition_budget:
        print(
            f"\n固件已占用 OTA 槽位的 {usage:.1%}，超过 {args.partition_budget:.0%} 的预算。"
            f"\n需要缩减固件，或在 partitions.csv 里重新分配空间。",
            file=sys.stderr,
        )
        failed = True

    if failed:
        return 1

    print("\n体积在预算内。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
