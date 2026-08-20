import { describe, expect, it } from "vite-plus/test";
import {
  formatBytes,
  formatDateTime,
  formatUptime,
  percentage,
  signalBars,
  signalLevel,
} from "./format";

describe("formatBytes", () => {
  it.each([
    [0, "0 B"],
    [512, "512 B"],
    [1024, "1 KB"],
    [1536, "1.5 KB"],
    [1024 * 1024, "1 MB"],
    [16 * 1024 * 1024, "16 MB"],
    [3 * 1024 * 1024 * 1024, "3 GB"],
  ])("%i -> %s", (bytes, expected) => {
    expect(formatBytes(bytes)).toBe(expected);
  });

  it("对非法输入降级显示", () => {
    expect(formatBytes(Number.NaN)).toBe("—");
    expect(formatBytes(-1)).toBe("—");
  });
});

describe("formatUptime", () => {
  it.each([
    [0, "0s"],
    [45_000, "45s"],
    [90_000, "1m 30s"],
    [3_600_000, "1h 0m"],
    [90_061_000, "1d 1h 1m"],
  ])("%i ms -> %s", (ms, expected) => {
    expect(formatUptime(ms)).toBe(expected);
  });

  it("对非法输入降级显示", () => {
    expect(formatUptime(-5)).toBe("—");
  });
});

describe("percentage", () => {
  it("正常计算并四舍五入", () => {
    expect(percentage(50, 200)).toBe(25);
    expect(percentage(1, 3)).toBe(33);
  });

  it("分母为 0 或非法输入时返回 0", () => {
    expect(percentage(10, 0)).toBe(0);
    expect(percentage(Number.NaN, 100)).toBe(0);
  });

  it("结果被夹在 0..100", () => {
    expect(percentage(300, 100)).toBe(100);
    expect(percentage(-10, 100)).toBe(0);
  });
});

describe("signalLevel", () => {
  it.each([
    [-30, "极佳"],
    [-55, "极佳"],
    [-56, "良好"],
    [-67, "良好"],
    [-68, "一般"],
    [-75, "一般"],
    [-76, "较弱"],
  ])("把 %i dBm 描述为 %s", (rssi, expected) => {
    expect(signalLevel(rssi)).toBe(expected);
  });
});

describe("signalBars", () => {
  it.each([
    [-40, 4],
    [-60, 3],
    [-70, 2],
    [-80, 1],
    [-95, 0],
  ])("%i dBm -> %i 格", (rssi, expected) => {
    expect(signalBars(rssi)).toBe(expected);
  });

  it("未连接时（rssi 为 0）没有信号格", () => {
    expect(signalBars(0)).toBe(0);
  });
});

describe("formatDateTime", () => {
  it("空串表示尚未同步", () => {
    expect(formatDateTime("")).toBe("未同步");
  });

  it("无法解析时原样返回", () => {
    expect(formatDateTime("not-a-date")).toBe("not-a-date");
  });

  it("格式化为本地时间", () => {
    const iso = new Date(2026, 7, 20, 9, 5, 3).toISOString();
    expect(formatDateTime(iso)).toBe("2026-08-20 09:05:03");
  });
});
