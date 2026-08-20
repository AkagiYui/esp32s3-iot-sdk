import { afterEach, beforeEach, describe, expect, it, vi } from "vite-plus/test";
import {
  createEmptyConfig,
  fetchWifiConfigs,
  saveWifiConfigs,
  scanWifiNetworks,
  validateConfig,
  type WifiConfigEntry,
} from "./wifi";

const fetchMock = vi.fn();

function jsonResponse(payload: unknown) {
  return {
    ok: true,
    status: 200,
    statusText: "",
    json: () => Promise.resolve(payload),
  } as Response;
}

function entry(overrides: Partial<WifiConfigEntry> = {}): WifiConfigEntry {
  return { ...createEmptyConfig(), ssid: "home", password: "12345678", ...overrides };
}

describe("validateConfig", () => {
  it("SSID 不能为空", () => {
    expect(validateConfig(entry({ ssid: "   " }))).toBe("SSID 不能为空");
  });

  it("SSID 超过 32 字节被拒绝", () => {
    expect(validateConfig(entry({ ssid: "a".repeat(33) }))).toBe("SSID 超过 32 字节");
    expect(validateConfig(entry({ ssid: "a".repeat(32) }))).toBeUndefined();
  });

  it("按字节而不是字符判断长度", () => {
    // 11 个汉字 = 33 字节
    expect(validateConfig(entry({ ssid: "网".repeat(11) }))).toBe("SSID 超过 32 字节");
  });

  it("沿用已保存密码时不校验密码", () => {
    expect(validateConfig(entry({ password: null }))).toBeUndefined();
  });

  it("空密码视为开放网络", () => {
    expect(validateConfig(entry({ password: "" }))).toBeUndefined();
  });

  it("WPA 密码至少 8 位", () => {
    expect(validateConfig(entry({ password: "1234567" }))).toBe("WPA 密码至少 8 个字符");
    expect(validateConfig(entry({ password: "12345678" }))).toBeUndefined();
  });

  it("密码不能超过 63 字节", () => {
    expect(validateConfig(entry({ password: "a".repeat(64) }))).toBe("密码超过 63 字节");
  });
});

describe("设备接口", () => {
  beforeEach(() => {
    fetchMock.mockReset();
    vi.stubGlobal("fetch", fetchMock);
  });

  afterEach(() => {
    vi.unstubAllGlobals();
  });

  it("拉取配置时把 has_password 映射为本地状态，密码留空待沿用", async () => {
    fetchMock.mockResolvedValue(
      jsonResponse({
        items: [
          { ssid: "a", has_password: true },
          { ssid: "b", has_password: false },
        ],
        max_items: 16,
      }),
    );

    const list = await fetchWifiConfigs();

    expect(list.maxItems).toBe(16);
    expect(list.items).toHaveLength(2);
    expect(list.items[0]).toMatchObject({ ssid: "a", hasPassword: true, password: null });
    expect(list.items[1]).toMatchObject({ ssid: "b", hasPassword: false, password: null });
    expect(list.items[0]!.id).not.toBe(list.items[1]!.id);
  });

  it("保存时裁剪 SSID 空白，并把 null 密码原样送出表示沿用", async () => {
    fetchMock.mockResolvedValue(jsonResponse({ items: [], max_items: 16 }));

    await saveWifiConfigs([
      entry({ ssid: "  home  ", password: "secret12" }),
      entry({ ssid: "guest", password: null }),
    ]);

    const [url, init] = fetchMock.mock.calls[0] as [string, RequestInit];
    expect(url).toBe("/api/wifi/config");
    expect(init.method).toBe("PUT");
    expect(JSON.parse(init.body as string)).toEqual({
      items: [
        { ssid: "home", password: "secret12" },
        { ssid: "guest", password: null },
      ],
    });
  });

  it("扫描结果按设备返回的顺序透传", async () => {
    fetchMock.mockResolvedValue(
      jsonResponse({
        items: [
          { ssid: "near", rssi: -40, channel: 6, authmode: "WPA2-PSK", secured: true },
          { ssid: "far", rssi: -80, channel: 1, authmode: "OPEN", secured: false },
        ],
      }),
    );

    const results = await scanWifiNetworks();

    expect(fetchMock.mock.calls[0]?.[0]).toBe("/api/wifi/scan");
    expect(results.map((item) => item.ssid)).toEqual(["near", "far"]);
  });

  it("强制刷新时带上 force 参数", async () => {
    fetchMock.mockResolvedValue(jsonResponse({ items: [] }));

    await scanWifiNetworks(true);

    expect(fetchMock.mock.calls[0]?.[0]).toBe("/api/wifi/scan?force=1");
  });
});
