import { apiRequest } from "./api";

export type WifiStatus = {
  connected: boolean;
  connecting: boolean;
  ap_active: boolean;
  mode: string;
  ssid: string;
  rssi: number;
  channel: number;
  ip: string;
  netmask: string;
  gateway: string;
  ap_ip: string;
  ap_clients: number;
  state?: string;
};

export type WifiScanEntry = {
  ssid: string;
  rssi: number;
  channel: number;
  authmode: string;
  secured: boolean;
};

/**
 * 前端持有的一条 WiFi 配置。
 *
 * `password` 为 `null` 表示"沿用设备上已保存的那一份"——设备接口不会回传明文密码，
 * 前端也就没有原值可以回填。用户真正输入了新密码时它才是字符串。
 */
export type WifiConfigEntry = {
  id: string;
  ssid: string;
  hasPassword: boolean;
  password: string | null;
};

type WifiConfigResponse = {
  items: { ssid: string; has_password: boolean }[];
  max_items: number;
};

export type WifiConfigList = {
  items: WifiConfigEntry[];
  maxItems: number;
};

let nextLocalId = 0;

export function createConfigId(): string {
  nextLocalId += 1;
  return `wifi-${nextLocalId}`;
}

export function createEmptyConfig(): WifiConfigEntry {
  return { id: createConfigId(), ssid: "", hasPassword: false, password: "" };
}

export async function fetchWifiConfigs(): Promise<WifiConfigList> {
  const payload = await apiRequest<WifiConfigResponse>("/api/wifi/config");
  return {
    items: payload.items.map((item) => ({
      id: createConfigId(),
      ssid: item.ssid,
      hasPassword: item.has_password,
      password: null,
    })),
    maxItems: payload.max_items,
  };
}

export async function saveWifiConfigs(
  entries: readonly WifiConfigEntry[],
): Promise<WifiConfigList> {
  const payload = await apiRequest<WifiConfigResponse>("/api/wifi/config", {
    method: "PUT",
    body: {
      items: entries.map((entry) => ({
        ssid: entry.ssid.trim(),
        password: entry.password,
      })),
    },
  });

  return {
    items: payload.items.map((item) => ({
      id: createConfigId(),
      ssid: item.ssid,
      hasPassword: item.has_password,
      password: null,
    })),
    maxItems: payload.max_items,
  };
}

export async function scanWifiNetworks(force = false): Promise<WifiScanEntry[]> {
  const payload = await apiRequest<{ items: WifiScanEntry[] }>(
    force ? "/api/wifi/scan?force=1" : "/api/wifi/scan",
    { timeoutMs: 15000 },
  );
  return payload.items;
}

export function fetchWifiStatus(): Promise<WifiStatus> {
  return apiRequest<WifiStatus>("/api/wifi/status");
}

/** 让设备用已保存的配置去连路由器；配网热点会随之关闭。 */
export function applyWifiConfig(): Promise<{ status: string }> {
  return apiRequest("/api/wifi/connect", { method: "POST" });
}

/** 让设备重新进入配网模式。 */
export function enterProvisioning(): Promise<{ status: string }> {
  return apiRequest("/api/wifi/provision", { method: "POST" });
}

/** 校验一条配置，返回错误说明；通过时返回 undefined。 */
export function validateConfig(entry: WifiConfigEntry): string | undefined {
  const ssid = entry.ssid.trim();
  if (ssid.length === 0) {
    return "SSID 不能为空";
  }
  if (new TextEncoder().encode(ssid).length > 32) {
    return "SSID 超过 32 字节";
  }

  if (entry.password === null) {
    return undefined;
  }

  const length = new TextEncoder().encode(entry.password).length;
  if (length === 0) {
    return undefined;
  }
  if (length < 8) {
    return "WPA 密码至少 8 个字符";
  }
  if (length > 63) {
    return "密码超过 63 字节";
  }
  return undefined;
}
