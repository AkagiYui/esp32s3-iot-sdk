import { requestJson } from "./api";

export type WifiConfig = {
  /** 仅前端使用的稳定标识，用于列表排序与就地编辑 */
  id: string;
  ssid: string;
  password: string;
};

export type WifiScanEntry = {
  ssid: string;
  rssi: number;
  authmode: string;
};

type WifiConfigPayload = {
  ssid: string;
  password: string;
};

export function createConfigId(): string {
  return `${Date.now()}-${Math.random().toString(36).slice(2, 8)}`;
}

export function normalizeConfig(item: Partial<WifiConfig>): WifiConfig {
  return {
    id: item.id ?? createConfigId(),
    ssid: item.ssid ?? "",
    password: item.password ?? "",
  };
}

/** 把 RSSI 翻译成人话。 */
export function signalLevel(rssi: number): string {
  if (rssi >= -55) return "极佳";
  if (rssi >= -67) return "良好";
  if (rssi >= -75) return "一般";
  return "较弱";
}

export async function fetchWifiConfigs(): Promise<WifiConfig[]> {
  const payload = await requestJson<WifiConfigPayload[]>("/api/wifi-config");
  return payload.map((item) => normalizeConfig(item));
}

export async function saveWifiConfigs(configs: readonly WifiConfig[]): Promise<void> {
  await requestJson<{ message: string }>("/api/wifi-config", {
    method: "PUT",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(
      configs.map<WifiConfigPayload>((item) => ({
        ssid: item.ssid.trim(),
        password: item.password,
      })),
    ),
  });
}

/** 扫描结果按信号强度从强到弱排序。 */
export async function scanWifiNetworks(): Promise<WifiScanEntry[]> {
  const payload = await requestJson<WifiScanEntry[]>("/api/wifi-scan");
  return [...payload].sort((left, right) => right.rssi - left.rssi);
}
