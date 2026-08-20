import { createRoot, createSignal } from "solid-js";
import { apiRequest, describeError } from "./api";
import type { WifiStatus } from "./wifi";

export type FilesystemUsage = {
  mounted: boolean;
  total: number;
  used: number;
};

export type SystemInfo = {
  device: {
    name: string;
    default_name: string;
    mdns_hostname: string;
    mac: string;
    state: string;
    provisioning: boolean;
  };
  chip: {
    model: string;
    revision: string;
    cores: number;
    flash_size: number;
    psram_size: number;
  };
  firmware: {
    version: number;
    name: string;
    build_time: string;
    idf_version: string;
    running_partition: string;
    awaiting_confirm: boolean;
  };
  runtime: {
    uptime_ms: number;
    free_heap: number;
    min_free_heap: number;
    total_heap: number;
  };
  time: {
    synced: boolean;
    epoch: number;
    local: string;
    timezone: string;
  };
  wifi: WifiStatus;
  filesystem: Record<string, FilesystemUsage>;
};

export const DEVICE_STATE_LABELS: Record<string, string> = {
  boot: "启动中",
  provisioning: "配网模式",
  connecting: "正在连接",
  online: "已联网",
  offline: "已掉线",
};

/** 轮询间隔：正常 5 秒；连续失败后指数退避，避免设备重启时疯狂重试。 */
const POLL_INTERVAL_MS = 5000;
const POLL_MAX_INTERVAL_MS = 30000;

export function fetchSystemInfo(): Promise<SystemInfo> {
  return apiRequest<SystemInfo>("/api/system/info");
}

export function rebootDevice(): Promise<{ status: string }> {
  return apiRequest("/api/system/reboot", { method: "POST" });
}

export function factoryResetDevice(): Promise<{ status: string }> {
  return apiRequest("/api/system/factory-reset", { method: "POST" });
}

const monitor = createRoot(() => {
  const [systemInfo, setSystemInfo] = createSignal<SystemInfo | undefined>(undefined);
  const [lastError, setLastError] = createSignal<string | undefined>(undefined);
  const [reachable, setReachable] = createSignal(true);
  const [loading, setLoading] = createSignal(false);

  let timer: ReturnType<typeof setTimeout> | undefined;
  let interval = POLL_INTERVAL_MS;
  let subscribers = 0;

  async function refresh(): Promise<SystemInfo | undefined> {
    setLoading(true);
    try {
      const info = await fetchSystemInfo();
      setSystemInfo(info);
      setLastError(undefined);
      setReachable(true);
      interval = POLL_INTERVAL_MS;
      return info;
    } catch (error) {
      setLastError(describeError(error));
      setReachable(false);
      interval = Math.min(interval * 2, POLL_MAX_INTERVAL_MS);
      return undefined;
    } finally {
      setLoading(false);
    }
  }

  function schedule(): void {
    clearTimeout(timer);
    timer = setTimeout(() => {
      void refresh().finally(() => {
        if (subscribers > 0) {
          schedule();
        }
      });
    }, interval);
  }

  /**
   * 订阅设备状态轮询。多个页面同时订阅只会有一条轮询链，
   * 最后一个订阅者离开时停止，避免后台空转。
   */
  function subscribe(): () => void {
    subscribers += 1;
    if (subscribers === 1) {
      void refresh().finally(() => {
        if (subscribers > 0) {
          schedule();
        }
      });
    }

    return () => {
      subscribers -= 1;
      if (subscribers === 0) {
        clearTimeout(timer);
        timer = undefined;
      }
    };
  }

  return { systemInfo, lastError, reachable, loading, refresh, subscribe };
});

export const {
  systemInfo,
  lastError: deviceError,
  reachable: deviceReachable,
  loading: deviceLoading,
  refresh: refreshSystemInfo,
  subscribe: subscribeDevice,
} = monitor;
