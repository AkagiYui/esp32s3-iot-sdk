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

- 使用 `Svelte + TypeScript + Vite`
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

## 路由组织建议

当前项目允许使用轻量路由实现，但长期建议采用“基于文件的路由生成”而不是手工维护路由表。

推荐目标如下：

- 页面文件放在 `src/pages/` 下
- 路由根据文件结构自动生成
- 路由元信息尽量从文件名或约定导出中推断
- 页面新增时尽量不需要手写集中式路由定义

例如：

- `src/pages/home.svelte` -> `#/`
- `src/pages/dashboard.svelte` -> `#/dashboard`
- `src/pages/settings.svelte` -> `#/settings`
- `src/pages/network/wifi.svelte` -> `#/network/wifi`
- `src/pages/device.logs.svelte` -> `#/device/logs`

## 是否可以改成基于文件的路由生成

可以，而且这更符合项目后续扩展方向。

在当前技术约束下，推荐做法不是引入带 SSR 心智负担的完整框架，而是继续保持 Vite + Svelte 的纯前端结构，并利用构建工具能力生成路由：

- 使用 `import.meta.glob` 扫描 `src/pages/**/*.svelte`
- 通过文件名、子目录和 `.` 分段自动推导 hash 路径
- 页面通过模块导出 `routeMeta` 提供标题、图标和排序
- 生成页面映射表与导航元数据
- 仍然保持纯 CSR
- 仍然使用 hash route
- 不引入任何 SSR 行为

也就是说：

- 可以改成“基于文件的路由生成”
- 但仍然必须是 `Hash Route + CSR`
- 不应为了文件路由而切换到带 SSR 默认能力的架构

## 目录建议

```text
src/
  lib/
    router.svelte.ts
    navigation.ts
  pages/
    home.svelte
    dashboard.svelte
    settings.svelte
  App.svelte
  app.css
  main.ts
```

后续如果切换为文件路由生成，可进一步演进为：

```text
src/
  lib/
    router.svelte.ts
    route-manifest.ts
  pages/
    home.svelte
    dashboard.svelte
    settings.svelte
    network/
      wifi.svelte
    device.logs.svelte
```

其中 `route-manifest.ts` 可以在运行时或构建时由 `import.meta.glob` 生成页面映射。

## 页面文件约定

- 页面文件名使用小写开头
- `home.svelte` 映射为根路由 `#/`
- 子目录会映射为多段路由，例如 `pages/network/wifi.svelte` -> `#/network/wifi`
- 文件名中的 `.` 会作为额外分段，例如 `pages/device.logs.svelte` -> `#/device/logs`
- 页面需在模块脚本中导出 `routeMeta`

示例：

```svelte
<script module lang="ts">
  import type { RouteMeta } from '../lib/route-manifest';

  export const routeMeta: RouteMeta = {
    label: '网络',
    icon: 'settings',
    order: 30,
  };
</script>
```

## 开发原则

- 优先保证部署可靠性，而不是追求 Web 框架特性完整性
- 优先保证移动端单手操作体验
- 优先保证代码可读性、可裁剪性和低依赖
- 优先保证设备端静态部署兼容性
- 新增页面和功能时，不得破坏 `Hash Route + CSR Only` 的基本约束

## 开发命令

```bash
npm install
npm run dev
npm run build
npm run check
```

## 后续演进建议

- 将当前手写路由表演进为基于文件扫描的路由清单生成
- 为页面定义统一的标题、图标、导航顺序元数据
- 将模拟数据替换为真实设备接口
- 增加网络状态、传感器状态、日志、OTA 等设备页
- 在保持纯 CSR 的前提下补充页面切换动画与错误态处理
