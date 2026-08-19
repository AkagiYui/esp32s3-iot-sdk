import type { Connect, Plugin } from "vite";

/**
 * 仅在 `vp dev` 下生效的设备 API 模拟层。
 *
 * 真机上这些接口由 `main/web_server.c` 提供；本地开发时没有设备，
 * 用一份内存态数据让页面可以独立联调。构建产物完全不包含这段代码。
 */

type WifiConfigPayload = {
  ssid: string;
  password: string;
};

const scanResults = [
  { ssid: "Kenko-Lab", rssi: -41, authmode: "WPA2_PSK" },
  { ssid: "ESP32-S3-Setup", rssi: -58, authmode: "OPEN" },
  { ssid: "Office-5G", rssi: -70, authmode: "WPA2_WPA3_PSK" },
  { ssid: "Neighbour", rssi: -83, authmode: "WPA_WPA2_PSK" },
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
  let wifiConfigs: WifiConfigPayload[] = [
    { ssid: "Kenko-Lab", password: "12345678" },
    { ssid: "ESP32-S3-Setup", password: "" },
  ];

  return {
    name: "vite-plugin-dev-api-mock",
    apply: "serve",

    configureServer(server) {
      server.middlewares.use("/api", (req, res, next) => {
        const url = req.url?.split("?")[0] ?? "";
        const json = (payload: unknown, status = 200) => {
          res.statusCode = status;
          res.setHeader("Content-Type", "application/json");
          res.end(JSON.stringify(payload));
        };

        if (url === "/wifi-config" && req.method === "GET") {
          json(wifiConfigs);
          return;
        }

        if (url === "/wifi-config" && req.method === "PUT") {
          void readBody(req).then((body) => {
            try {
              wifiConfigs = JSON.parse(body) as WifiConfigPayload[];
              json({ message: "ok" });
            } catch {
              json({ message: "invalid payload" }, 400);
            }
          });
          return;
        }

        if (url === "/wifi-scan" && req.method === "GET") {
          setTimeout(() => json(scanResults), 600);
          return;
        }

        if (url === "/device-info" && req.method === "GET") {
          json({ firmware: "dev", idf: "v6.0.2" });
          return;
        }

        next();
      });
    },
  };
}
