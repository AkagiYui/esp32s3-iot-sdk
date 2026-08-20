# ESP32-S3 Embedded Web

这是一个面向 ESP32-S3 设备端的嵌入式 Web 控制界面项目。

项目运行目标不是通用 Web 站点，而是部署在设备侧静态资源服务器上的轻量控制台，因此在技术选型、打包体积、路由方式和运行模式上都需要遵循更严格的约束。

## 项目目标

- 为 ESP32-S3 提供一个轻量、稳定、可离线访问的设备端 Web UI
- 优先服务窄屏触屏设备，交互形态以底部导航、多页面切换、单手操作为主
- 页面需适配设备热点、本地局域网和直接 IP 访问场景
- 保持前端资源简单可控，便于嵌入 LittleFS 或其他设备文件系统中发布
- 后续页面可逐步接入设备状态、传感器数据、控制指令、OTA、日志等能力

## 强制技术约束

### 技术栈

- 使用 `SolidJS + TypeScript`
- 工具链统一由 `Vite+`（`vp`）管理：dev / build / lint / format / test / 包管理
- UI 必须按移动端优先设计
- 页面必须以 CSR 方式运行

### 路由约束

- 必须使用 `Hash Route`
- 不使用 `History Route`
- 路由切换必须能在设备端静态资源部署环境下稳定工作
- 页面刷新、直接访问、收藏链接后再次打开时，路由状态必须可恢复

### 渲染约束

- 必须是 `CSR`（Client-Side Rendering）
- 不能有任何 `SSR`（Server-Side Rendering）行为
- 不能依赖服务端渲染、服务端路由接管或服务端注水
- 所有页面内容都应在浏览器端完成初始化、切换和渲染

### 部署约束

- 产物应当可作为纯静态资源部署
- 不假设存在 Node.js 服务端
- 不假设存在 Nginx rewrite 或 fallback 规则
- 不要求设备 Web 服务器支持任意路径回退到入口文件

## 为什么要求 Hash Route

在 ESP32-S3 这类嵌入式设备中，前端通常由一个非常轻量的静态文件服务器提供。

`Hash Route` 的优点是：

- `#` 之后的内容不会发送到服务端
- 设备端只需要始终返回同一个入口页面即可
- 刷新 `/index.html#/dashboard` 不会因为服务端找不到 `/dashboard` 而 404
- 更适合热点配网页、设备本地控制页、局域网 IP 直连等部署方式

相比之下，`History Route` 往往要求服务端支持前端路由回退，这与设备侧静态资源服务的能力边界并不匹配。

## 为什么要求纯 CSR

这个项目不是面向搜索引擎抓取的内容站点，而是设备控制界面。

选择纯 CSR 的原因：

- 简化架构，避免引入 SSR 生命周期和双端行为差异
- 适合静态资源直接下发到设备
- 降低构建、部署和运行复杂度
- 更符合嵌入式 Web UI 的资源限制与维护方式

因此，本项目不引入任何 SSR 框架行为，也不接受半 SSR、预渲染依赖服务端逻辑、或混合渲染作为默认架构。

## 为什么选 SolidJS

- 编译期把 JSX 展开为直接的 DOM 操作，没有虚拟 DOM diff，运行时非常小
- 细粒度响应式，页面局部更新不会重建节点，输入框不会掉焦点
- 纯 CSR 友好，不带任何 SSR 心智负担
- 产物体积对设备 flash 友好：当前单文件产物 101 KB，brotli 后 29 KB

## 路由组织

路由由 `src/lib/route-manifest.ts` 在构建时通过 `import.meta.glob` 扫描 `src/pages/**/*.tsx` 生成，新增页面不需要改动集中式路由表。

映射规则：

- `src/pages/home.tsx` -> `#/`
- `src/pages/dashboard.tsx` -> `#/dashboard`
- `src/pages/settings.tsx` -> `#/settings`
- `src/pages/network/wifi.tsx` -> `#/network/wifi`
- `src/pages/device.logs.tsx` -> `#/device/logs`（文件名里的 `.` 也是一段路径）
- 以 `index` 结尾的段会被去掉：`src/pages/network/index.tsx` -> `#/network`

页面文件名使用小写开头，`*.test.tsx` 不会被当成页面。

## 页面文件约定

页面需要 `export default` 一个组件，并导出 `routeMeta` 提供导航元信息：

```tsx
import { Wifi } from "lucide-solid";
import type { RouteMeta } from "@/lib/route-manifest";

export const routeMeta: RouteMeta = {
  label: "网络",
  icon: Wifi,
  order: 30,
};

export default function NetworkPage() {
  return <div class="page">…</div>;
}
```

没有导出 `routeMeta` 时会按路径推导一个兜底的标题与图标，排序值为 `999`。

## 样式约定

样式分两层：

- **全局层** `src/app.css`：设计令牌（CSS 变量）、reset，以及三个跨页面复用的排版原语 `.page` / `.page-header` / `.subtitle`
- **组件层** 每个组件旁边的 `*.module.css`：CSS Modules，构建时类名会被压成短哈希

两层需要叠加时用 `cx()` 拼接，例如 `class={cx("page-header", styles.pageHeader)}`。
组件层的规则一定晚于 `app.css` 注入，因此同优先级下组件样式覆盖全局样式。

Solid 没有内置过渡指令，出入场动画统一由 `src/lib/presence.ts` 的 `createPresence`
配合纯 CSS `@keyframes` 实现：元素在隐藏后会多挂载一段时间，播完动画再卸载。
JS 里的时长常量与对应 `*.module.css` 中的 `animation-duration` 必须保持一致。

## 目录结构

```text
src/
  lib/
    api.ts              # 带超时与结构化错误的 fetch 客户端
    device.ts           # 设备状态轮询（全局单例，带失败退避）
    settings.ts         # 设备设置读写与差异计算
    wifi.ts             # WiFi 配置 / 扫描 / 连接
    ota.ts              # 固件上传（XHR，带进度）与回滚确认
    format.ts           # 字节 / 时长 / 信号强度等展示层格式化
    cx.ts               # class 拼接
    feedback.ts         # dialog / toast 状态
    presence.ts         # 出场动画的挂载保持原语
    route-manifest.ts   # 由 pages/ 扫描生成的路由清单
    router.ts           # hash 路由信号
    theme.ts            # 主题模式与 data-theme
  components/
    Button.tsx          # 统一按钮（图标位 / 加载态）
    Card.tsx            # 分组容器
    InfoRow.tsx         # 「标签 — 取值」行
    Meter.tsx           # 占用类指标进度条
    ConnectionBanner.tsx# 设备失联横幅
    DialogHost.tsx      # 全局对话框
    NavBar.tsx          # 底部 / 侧边导航
    RouteView.tsx       # 页面容器与切换动画
    ToastHost.tsx       # 全局轻提示
    WifiScanModal.tsx   # 附近 WiFi 选择弹窗
  pages/
    home.tsx            # 概览与快捷操作
    dashboard.tsx       # 运行指标与硬件信息
    wifi.tsx            # WiFi 配置管理
    firmware.tsx        # OTA 升级与回滚
    settings.tsx        # 设备设置与危险操作
  test/
    setup.ts
  App.tsx
  app.css
  main.tsx
```

## 与设备接口的约定

页面消费的全部是 `main/api_handlers.c` 提供的真实接口，没有任何占位假数据：

| 接口 | 用途 |
|------|------|
| `GET /api/system/info` | 设备、芯片、固件、运行时、时间、网络、文件系统的完整快照 |
| `POST /api/system/reboot` | 重启设备 |
| `POST /api/system/factory-reset` | 清空配置并重启 |
| `GET/POST /api/system/ota` | 查询升级状态 / 上传固件 |
| `POST /api/system/ota/confirm` | 确认当前镜像，取消回滚 |
| `GET/PUT /api/settings` | 设备名、时区、NTP 开关、状态灯亮度 |
| `GET /api/wifi/status` | 当前连接状态 |
| `GET /api/wifi/scan` | 扫描附近热点（`?force=1` 跳过设备端缓存） |
| `GET/PUT /api/wifi/config` | 读写多组 WiFi 配置 |
| `POST /api/wifi/connect` | 用已保存的配置连接，关闭配网热点 |
| `POST /api/wifi/provision` | 重新进入配网模式 |

两条必须遵守的约定：

- **设备永远不会回传明文 WiFi 密码**，`GET /api/wifi/config` 只给 `has_password`。
  因此前端把"未改动的密码"表示为 `null`，`PUT` 时原样送回，由设备沿用已保存的那一份。
- **所有错误都是 `{"error":{"code","message"}}`**，`lib/api.ts` 会把它还原成 `ApiError`，
  页面直接展示 `message`，不需要各自拼错误文案。

设备随时可能因为重启、切换 WiFi 模式或 OTA 而失联，所以每个请求都有超时，
`lib/device.ts` 的轮询在失败后指数退避，界面顶部会出现失联横幅而不是无声地停在旧数据上。

## 开发命令

前提：本机已安装 Vite+（`curl -fsSL https://vite.plus | bash`）。`vp` 会按
`.node-version` 自行准备 Node.js，按 `packageManager` 自行准备包管理器，不需要额外装 Node 或 pnpm。

```bash
vp install
```

```bash
vp dev
```

```bash
vp check
```

```bash
vp test
```

```bash
vp build
```

`vp dev` 下会启用一个仅开发期生效的设备 API 模拟层（`plugins/vite-plugin-dev-api-mock.ts`），
`/api/wifi-config`、`/api/wifi-scan`、`/api/device-info` 都有内存态假数据，
所以不接硬件也能完整联调。构建产物不包含这段代码。

固件侧的 `cmake --build build --target merged_bin` 会自动调用 `vp build` 生成 `dist/`，
再打包进 `web` 分区镜像，不需要手动先构建前端。

## 构建产物

`vite-plugin-singlefile` 会把 JS/CSS 全部内联进 `index.html`，
`plugins/vite-plugin-precompress.ts` 再额外生成 `.gz` / `.br` / `.zst`，
设备端 `main/web_server.c` 会按请求的 `Accept-Encoding` 选择最合适的一份直接下发。

## 后续演进建议

- 增加传感器读数、日志查看等设备页
- 为固件上传补充断点续传或分片重试
- 若设备接入公网，需要给接口加上鉴权（当前接口对同网段完全开放）
