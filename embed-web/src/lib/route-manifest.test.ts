import { describe, expect, it } from "vite-plus/test";
import {
  defaultLabelFromPath,
  routeEntries,
  routeMap,
  routePathFromFilePath,
} from "./route-manifest";

describe("routePathFromFilePath", () => {
  it.each([
    ["../pages/home.tsx", "/"],
    ["../pages/dashboard.tsx", "/dashboard"],
    ["../pages/network/wifi.tsx", "/network/wifi"],
    ["../pages/device.logs.tsx", "/device/logs"],
    ["../pages/network/index.tsx", "/network"],
    ["../pages/Settings.tsx", "/settings"],
  ])("%s -> %s", (filePath, expected) => {
    expect(routePathFromFilePath(filePath)).toBe(expected);
  });
});

describe("defaultLabelFromPath", () => {
  it("根路由回退为首页", () => {
    expect(defaultLabelFromPath("/")).toBe("首页");
  });

  it("多段路径拼成可读标题", () => {
    expect(defaultLabelFromPath("/network/wifi")).toBe("Network / Wifi");
  });
});

describe("页面清单", () => {
  it("扫描出全部页面并按 order 排序", () => {
    expect(routeEntries.map((entry) => entry.path)).toEqual([
      "/",
      "/dashboard",
      "/wifi",
      "/firmware",
      "/settings",
    ]);
    expect(routeEntries.map((entry) => entry.label)).toEqual([
      "首页",
      "仪表盘",
      "WiFi",
      "固件",
      "设置",
    ]);
  });

  it("每个页面都提供组件和图标", () => {
    for (const entry of routeEntries) {
      expect(typeof entry.component).toBe("function");
      expect(typeof entry.icon).toBe("function");
      expect(routeMap.get(entry.path)).toBe(entry);
    }
  });
});
