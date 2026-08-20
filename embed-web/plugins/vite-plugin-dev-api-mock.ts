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

  // 与固件一致：配网模式下不校验，接入局域网后所有接口都要登录换来的会话令牌。
  const PASSWORD_MIN_LENGTH = 8;
  let accessPassword = "kenko1234";
  let sessions: string[] = [];
  let provisioning = false;
  let connected = true;
  let otaState: "idle" | "receiving" | "ready" | "failed" = "idle";
  let coredumpPresent = true;

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
      password_configured: accessPassword.length > 0,
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
      coredump_present: false,
    },
    runtime: {
      uptime_ms: Date.now() - startedAt,
      heap: {
        internal: {
          free: 212_344,
          total: 341_000,
          min_free: 186_120,
          largest_free_block: 110_592,
        },
        psram: {
          free: 8_200_192,
          total: 8_388_608,
          min_free: 8_150_016,
          largest_free_block: 8_126_464,
        },
      },
      tasks: [
        { name: "IDLE0", priority: 0, stack_free: 1_136 },
        { name: "IDLE1", priority: 0, stack_free: 1_180 },
        { name: "app_state", priority: 6, stack_free: 2_048 },
        { name: "wifi_sta_loop", priority: 5, stack_free: 1_792 },
        { name: "httpd", priority: 5, stack_free: 3_456 },
        { name: "status_led", priority: 4, stack_free: 1_920 },
        { name: "button_monitor", priority: 5, stack_free: 2_240 },
        { name: "tiT", priority: 18, stack_free: 1_408 },
        { name: "wifi", priority: 23, stack_free: 1_664 },
      ],
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
    factory_available: true,
    max_image_size: 2 * 1024 * 1024,
  });

  return {
    name: "vite-plugin-dev-api-mock",
    apply: "serve",

    configureServer(server) {
      server.config.logger.info(`[dev-api-mock] 访问密码: ${accessPassword}`);

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

        const presentedToken = (() => {
          const header = req.headers.authorization ?? "";
          if (header.toLowerCase().startsWith("bearer ")) {
            return header.slice(7);
          }
          return (req.headers["x-session-token"] as string | undefined) ?? "";
        })();
        const authorized = provisioning || sessions.includes(presentedToken);
        const issueSession = () => {
          const token = Math.random().toString(16).slice(2).padEnd(32, "0").slice(0, 32);
          sessions = [...sessions.slice(-3), token];
          return { token, expires_in: 604800 };
        };

        // 登录与状态探测必须能在未鉴权时访问，否则无法自举
        if (path === "/auth/status" && method === "GET") {
          json({
            configured: accessPassword.length > 0,
            authenticated: authorized,
            provisioning,
            password_min_length: PASSWORD_MIN_LENGTH,
          });
          return;
        }

        if (path === "/auth/login" && method === "POST") {
          void readBody(req).then((body) => {
            try {
              const payload = JSON.parse(body) as { password?: string };
              if (!accessPassword) {
                fail(409, "password_not_set", "no access password has been set yet");
              } else if (payload.password === accessPassword) {
                json(issueSession());
              } else {
                fail(401, "invalid_password", "wrong access password");
              }
            } catch {
              fail(400, "invalid_json", "request body is not valid json");
            }
          });
          return;
        }

        if (!authorized) {
          res.setHeader("WWW-Authenticate", 'Bearer realm="kenko"');
          fail(401, "unauthorized", "log in with the device access password");
          return;
        }

        if (path === "/auth/logout" && method === "POST") {
          sessions = sessions.filter((token) => token !== presentedToken);
          json({ status: "logged_out" });
          return;
        }

        if (path === "/auth/password" && (method === "PUT" || method === "POST")) {
          void readBody(req).then((body) => {
            try {
              const payload = JSON.parse(body) as { password?: string; current_password?: string };
              const next = payload.password ?? "";
              if (next.length < PASSWORD_MIN_LENGTH) {
                fail(400, "weak_password", "password is too short");
                return;
              }
              if (!provisioning && accessPassword && payload.current_password !== accessPassword) {
                fail(401, "invalid_password", "the current password does not match");
                return;
              }
              accessPassword = next;
              sessions = [];
              server.config.logger.info(`[dev-api-mock] 访问密码已更新: ${accessPassword}`);
              json(issueSession());
            } catch {
              fail(400, "invalid_json", "request body is not valid json");
            }
          });
          return;
        }

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
          accessPassword = "";
          sessions = [];
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

        if (path === "/system/revert-to-factory" && method === "POST") {
          json({ status: "reverting", note: "rebooting into the factory image" }, 202);
          return;
        }

        if (path === "/system/coredump" && method === "GET") {
          json({ present: coredumpPresent });
          return;
        }

        if (path === "/system/coredump" && method === "DELETE") {
          coredumpPresent = false;
          json({ present: false });
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
          if (!accessPassword) {
            fail(409, "password_not_set", "set an access password before joining a network");
            return;
          }
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
