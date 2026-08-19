import { describe, expect, it, vi, beforeEach, afterEach } from "vite-plus/test";
import {
  fetchWifiConfigs,
  normalizeConfig,
  saveWifiConfigs,
  scanWifiNetworks,
  signalLevel,
} from "./wifi";

describe("signalLevel", () => {
  it.each([
    [-30, "极佳"],
    [-55, "极佳"],
    [-56, "良好"],
    [-67, "良好"],
    [-68, "一般"],
    [-75, "一般"],
    [-76, "较弱"],
    [-95, "较弱"],
  ])("把 %i dBm 描述为 %s", (rssi, expected) => {
    expect(signalLevel(rssi)).toBe(expected);
  });
});

describe("normalizeConfig", () => {
  it("补齐缺失字段并生成 id", () => {
    const config = normalizeConfig({});
    expect(config.ssid).toBe("");
    expect(config.password).toBe("");
    expect(config.id).toMatch(/^\d+-[a-z0-9]+$/);
  });

  it("保留已有 id", () => {
    expect(normalizeConfig({ id: "fixed", ssid: "a", password: "b" })).toEqual({
      id: "fixed",
      ssid: "a",
      password: "b",
    });
  });

  it("为每一项生成互不相同的 id", () => {
    const ids = new Set(Array.from({ length: 50 }, () => normalizeConfig({}).id));
    expect(ids.size).toBe(50);
  });
});

describe("设备接口", () => {
  const fetchMock = vi.fn();

  beforeEach(() => {
    fetchMock.mockReset();
    vi.stubGlobal("fetch", fetchMock);
  });

  afterEach(() => {
    vi.unstubAllGlobals();
  });

  function jsonResponse(payload: unknown, ok = true, status = 200) {
    return {
      ok,
      status,
      json: () => Promise.resolve(payload),
      text: () => Promise.resolve(JSON.stringify(payload)),
    } as Response;
  }

  it("拉取配置时补齐前端 id", async () => {
    fetchMock.mockResolvedValue(jsonResponse([{ ssid: "a", password: "b" }]));

    const configs = await fetchWifiConfigs();

    expect(fetchMock).toHaveBeenCalledWith("/api/wifi-config", undefined);
    expect(configs).toHaveLength(1);
    expect(configs[0]).toMatchObject({ ssid: "a", password: "b" });
    expect(configs[0]!.id).toBeTruthy();
  });

  it("保存时去掉前端 id 并裁剪 SSID 两端空白", async () => {
    fetchMock.mockResolvedValue(jsonResponse({ message: "ok" }));

    await saveWifiConfigs([{ id: "x", ssid: "  home  ", password: "pw" }]);

    const [, init] = fetchMock.mock.calls[0] as [string, RequestInit];
    expect(init.method).toBe("PUT");
    expect(typeof init.body).toBe("string");
    expect(JSON.parse(init.body as string)).toEqual([{ ssid: "home", password: "pw" }]);
  });

  it("扫描结果按信号从强到弱排序", async () => {
    fetchMock.mockResolvedValue(
      jsonResponse([
        { ssid: "far", rssi: -80, authmode: "OPEN" },
        { ssid: "near", rssi: -40, authmode: "WPA2_PSK" },
        { ssid: "mid", rssi: -60, authmode: "WPA2_PSK" },
      ]),
    );

    const results = await scanWifiNetworks();

    expect(results.map((entry) => entry.ssid)).toEqual(["near", "mid", "far"]);
  });

  it("HTTP 失败时抛出响应正文", async () => {
    fetchMock.mockResolvedValue({
      ok: false,
      status: 500,
      text: () => Promise.resolve("boom"),
      json: () => Promise.resolve(null),
    } as Response);

    await expect(fetchWifiConfigs()).rejects.toThrow("boom");
  });
});
