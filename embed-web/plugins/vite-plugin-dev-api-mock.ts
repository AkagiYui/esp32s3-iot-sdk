import type { Connect, Plugin } from "vite";

/**
 * 仅在 `vp dev` 下生效的设备 API 模拟层。
 *
 * 真机上这些接口由 `main/api_handlers.c` 提供；本地开发时没有设备，
 * 用一份内存态数据让页面可以独立联调。构建产物完全不包含这段代码。
 *
 * 这里刻意保持和固件一致的语义：
 * - `/api/wifi/config` 不回传明文密码，只回 `has_password`
 * - `PUT` 时 `password: null` 表示沿用已保存的密码
 * - 未知的 `/api/**` 返回 404 JSON，而不是落到 index.html
 */

type StoredCredential = {
  ssid: string;
  password: string;
};

const startedAt = Date.now();

const scanResults = [
  { ssid: "Kenko-Lab", rssi: -41, channel: 6, authmode: "WPA2-PSK", secured: true },
  { ssid: "kenko32-ab12", rssi: -58, channel: 1, authmode: "OPEN", secured: false },
  { ssid: "Office-5G", rssi: -70, channel: 11, authmode: "WPA2/WPA3-PSK", secured: true },
  { ssid: "Neighbour", rssi: -83, channel: 3, authmode: "WPA/WPA2-PSK", secured: true },
];

function readBody(req: Connect.IncomingMessage): Promise<string> {
  return new Promise((resolve, reject) => {
    let body = "";
    req.on("data", (chunk) => {
      body += String(chunk);
    });
    req.on("end", () => resolve(body));
    req.on("error", reject);
  });
}

export function devApiMock(): Plugin {
  let credentials: StoredCredential[] = [
    { ssid: "Kenko-Lab", password: "kenko-lab-pass" },
    { ssid: "Guest-Open", password: "" },
  ];

  let settings = {
    device_name: "kenko32-ab12",
    timezone: "CST-8",
    ntp_enabled: true,
    led_brightness: 100,
  };

  let provisioning = false;
  let connected = true;
  let otaState: "idle" | "receiving" | "ready" | "failed" = "idle";

  const wifiStatus = () => ({
    connected,
    connecting: false,
    ap_active: provisioning,
    mode: provisioning ? "apsta" : "sta",
    ssid: connected ? "Kenko-Lab" : "",
    rssi: connected ? -47 : 0,
    channel: connected ? 6 : 0,
    ip: connected ? "192.168.1.42" : "0.0.0.0",
    netmask: connected ? "255.255.255.0" : "0.0.0.0",
    gateway: connected ? "192.168.1.1" : "0.0.0.0",
    ap_ip: "192.168.6.1",
    ap_clients: provisioning ? 1 : 0,
  });

  const systemInfo = () => ({
    device: {
      name: settings.device_name,
      default_name: "kenko32-ab12",
      mdns_hostname: "kenko32-ab12",
      mac: "a0:b7:65:ab:12:34",
      state: provisioning ? "provisioning" : connected ? "online" : "connecting",
      provisioning,
    },
    chip: {
      model: "ESP32-S3",
      revision: "v0.2",
      cores: 2,
      flash_size: 16 * 1024 * 1024,
      psram_size: 8 * 1024 * 1024,
    },
    firmware: {
      version: 42,
      name: "dev0000",
      build_time: "2026-08-20T10:00:00+08:00",
      idf_version: "v6.0.2",
      running_partition: "ota_0",
      awaiting_confirm: false,
    },
    runtime: {
      uptime_ms: Date.now() - startedAt,
      free_heap: 212_344,
      min_free_heap: 186_120,
      total_heap: 341_000,
    },
    time: {
      synced: true,
      epoch: Math.floor(Date.now() / 1000),
      local: new Date().toISOString(),
      timezone: settings.timezone,
    },
    wifi: wifiStatus(),
    filesystem: {
      storage: { mounted: true, total: 6_094_848, used: 40_960 },
      web: { mounted: true, total: 4_194_304, used: 196_608 },
    },
  });

  const otaStatus = () => ({
    state: otaState,
    received: 0,
    total: 0,
    message: otaState === "ready" ? "update staged, reboot to apply" : "",
    running_partition: "ota_0",
    boot_partition: otaState === "ready" ? "ota_1" : "ota_0",
    awaiting_confirm: false,
    max_image_size: 3 * 1024 * 1024,
  });

  return {
    name: "vite-plugin-dev-api-mock",
    apply: "serve",

    configureServer(server) {
      server.middlewares.use("/api", (req, res, next) => {
        const path = req.url?.split("?")[0] ?? "";
        const method = req.method ?? "GET";

        const json = (payload: unknown, status = 200) => {
          res.statusCode = status;
          res.setHeader("Content-Type", "application/json");
          res.setHeader("Cache-Control", "no-store");
          res.end(JSON.stringify(payload));
        };
        const fail = (status: number, code: string, message: string) =>
          json({ error: { code, message } }, status);

        if (path === "/system/info" && method === "GET") {
          json(systemInfo());
          return;
        }

        if (path === "/system/reboot" && method === "POST") {
          json({ status: "rebooting" }, 202);
          return;
        }

        if (path === "/system/factory-reset" && method === "POST") {
          credentials = [];
          provisioning = true;
          connected = false;
          json({ status: "resetting" }, 202);
          return;
        }

        if (path === "/system/ota" && method === "GET") {
          json(otaStatus());
          return;
        }

        if (path === "/system/ota" && method === "POST") {
          // 模拟一次耗时上传，让进度条有东西可展示
          setTimeout(() => {
            otaState = "ready";
            json(otaStatus());
          }, 800);
          return;
        }

        if (path === "/system/ota/confirm" && method === "POST") {
          json(otaStatus());
          return;
        }

        if (path === "/settings" && method === "GET") {
          json(settings);
          return;
        }

        if (path === "/settings" && method === "PUT") {
          void readBody(req).then((body) => {
            try {
              const patch = JSON.parse(body) as Partial<typeof settings>;
              settings = { ...settings, ...patch };
              json(settings);
            } catch {
              fail(400, "invalid_json", "request body is not valid json");
            }
          });
          return;
        }

        if (path === "/wifi/status" && method === "GET") {
          json({ ...wifiStatus(), state: provisioning ? "provisioning" : "online" });
          return;
        }

        if (path === "/wifi/scan" && method === "GET") {
          setTimeout(() => json({ items: scanResults }), 600);
          return;
        }

        if (path === "/wifi/config" && method === "GET") {
          json({
            items: credentials.map((item) => ({
              ssid: item.ssid,
              has_password: item.password.length > 0,
            })),
            max_items: 16,
          });
          return;
        }

        if (path === "/wifi/config" && method === "PUT") {
          void readBody(req).then((body) => {
            try {
              const payload = JSON.parse(body) as {
                items: { ssid: string; password: string | null }[];
              };
              credentials = payload.items.map((item) => ({
                ssid: item.ssid,
                password:
                  item.password ??
                  credentials.find((existing) => existing.ssid === item.ssid)?.password ??
                  "",
              }));
              json({
                items: credentials.map((item) => ({
                  ssid: item.ssid,
                  has_password: item.password.length > 0,
                })),
                max_items: 16,
              });
            } catch {
              fail(400, "invalid_json", "request body is not valid json");
            }
          });
          return;
        }

        if (path === "/wifi/connect" && method === "POST") {
          provisioning = false;
          connected = true;
          json({ status: "connecting" }, 202);
          return;
        }

        if (path === "/wifi/provision" && method === "POST") {
          provisioning = true;
          connected = false;
          json({ status: "provisioning" }, 202);
          return;
        }

        if (path.startsWith("/")) {
          fail(404, "not_found", "unknown api endpoint");
          return;
        }

        next();
      });
    },
  };
}
