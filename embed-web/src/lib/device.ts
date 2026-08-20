import { createRoot, createSignal } from "solid-js";
import { apiRequest, describeError } from "./api";
import type { WifiStatus } from "./wifi";

export type FilesystemUsage = {
  mounted: boolean;
  total: number;
  used: number;
};

export type HeapStats = {
  free: number;
  total: number;
  min_free: number;
  /** 最大连续可分配块，比剩余总量更能说明碎片化程度。 */
  largest_free_block: number;
};

export type TaskStats = {
  name: string;
  priority: number;
  /** 该任务栈的历史最小剩余字节数。 */
  stack_free: number;
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
    coredump_present: boolean;
  };
  runtime: {
    uptime_ms: number;
    heap: {
      internal: HeapStats;
      /** 仅在设备启用了 PSRAM 时出现。 */
      psram?: HeapStats;
    };
    tasks: TaskStats[];
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

/** 取当前的接口访问令牌（配网模式下无需鉴权即可读取）。 */
export function fetchApiToken(): Promise<{ token: string }> {
  return apiRequest<{ token: string }>("/api/system/token");
}

/** 重新生成令牌，旧令牌立即失效。 */
export function rotateApiToken(): Promise<{ token: string }> {
  return apiRequest<{ token: string }>("/api/system/token", { method: "POST" });
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
  let paused = false;

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
    if (paused || subscribers === 0) {
      return;
    }
    timer = setTimeout(() => {
      void refresh().finally(() => {
        if (subscribers > 0) {
          schedule();
        }
      });
    }, interval);
  }

  /**
   * 暂停轮询。
   *
   * 设备端的 HTTP 服务是单任务顺序处理请求的，OTA 上传会独占它十几秒，
   * 期间任何轮询都只会排队然后超时，把"失联"横幅误弹出来。
   */
  function setPaused(next: boolean): void {
    paused = next;
    if (paused) {
      clearTimeout(timer);
      timer = undefined;
      return;
    }
    if (subscribers > 0) {
      interval = POLL_INTERVAL_MS;
      void refresh().finally(schedule);
    }
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

  return { systemInfo, lastError, reachable, loading, refresh, subscribe, setPaused };
});

export const {
  systemInfo,
  lastError: deviceError,
  reachable: deviceReachable,
  loading: deviceLoading,
  refresh: refreshSystemInfo,
  subscribe: subscribeDevice,
  setPaused: setDevicePollingPaused,
} = monitor;
