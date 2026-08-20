# ESP32-S3 IoT SDK

本项目为 ESP32-S3 提供一套可复用的基础固件：WiFi 联网与配网、状态指示、配置持久化、
内置 Web 控制台、OTA 升级与时间同步，供后续项目直接在其上开发。

> [!CAUTION]
> 该项目仅供学习和参考，不应用于生产环境。
> 设备接口没有任何鉴权，同一局域网内的任何人都能调用。

使用开发板：源地工作室(VCC-GND Studio) YD-ESP32-S3 Type-A-V1.5，板载模组 ESP32-S3-WROOM-1-N16R8。
带有 CH343P USB 转串口芯片，支持 USB 连接和编程，内置 SRAM 512KB，内置 ROM 384KB，外扩 PSRAM 8MB，外扩 Flash 16MB。
直通转出了 ESP32-S3 的 USB 接口，支持 USB OTG 功能。

板载一颗 WS2812B RGB LED，连接到 GPIO 47 或 GPIO 48。
板载一颗轻触开关(BOOT)，连接到 GPIO 0，默认上拉，按下时接地。

## SDK 功能

- [x] LittleFS 文件系统挂载（配置分区挂不上时自动格式化，前端分区缺失时退化为内置兜底页）
- [x] 使用 JSON 文件存储多组 WiFi 配置（原子写入，掉电不会留下半截文件）
- [x] 无 WiFi 配置时自动进入 AP 配网模式
- [x] SoftAP + DHCP + DNS captive portal
- [x] 按顺序循环尝试已保存的 WiFi 配置，一轮失败后退避重试
- [x] 已联网状态下启动 mDNS，支持 `.local` 域名访问并公告 `_http._tcp` 服务
- [x] BOOT 键长按进入配网（5s）或恢复出厂设置（10s）
- [x] WS2812 状态灯指示当前系统状态，亮度可调
- [x] 内置 Web 控制台：静态资源托管 + 完整 REST 接口
- [x] WiFi 配置的读写、扫描与即时连接
- [x] 设备设置持久化（设备名、时区、NTP 开关、状态灯亮度）
- [x] SNTP 时间同步与时区设置
- [x] OTA 升级：上传、镜像校验、双槽位轮换、失败自动回滚
- [x] 出厂基线固件（`factory` 分区），可一键回退，OTA 永不覆盖
- [x] 重启与恢复出厂设置
- [x] Coredump 落盘，Web 页面可查看与擦除，崩溃现场可事后分析
- [ ] 蓝牙配网或设备控制
- [ ] MQTT 通信

## 系统状态

| LED 状态 | 通用含义                                         |
| -------- | ------------------------------------------------ |
| 常亮     | 设备正常工作中，无需任何操作                     |
| 呼吸     | 设备忙碌中，正在尝试连接                         |
| 闪烁     | 需要注意：正在执行破坏性操作，或存在硬件故障     |

| LED 状态 | 含义                                     |
| -------- | ---------------------------------------- |
| 橙色常亮 | 设备已上电，尚未进入任何工作状态         |
| 蓝色呼吸 | 正在尝试连接 WiFi                        |
| 蓝色常亮 | 处于配网模式，等待用户操作               |
| 绿色常亮 | 已连接网络                               |
| 紫色闪烁 | 正在恢复出厂设置                         |
| 红色闪烁 | 配置分区不可用，设置无法持久化           |

### 配网模式

- [x] 设备进入 **APSTA** 模式，SSID 为设备名（默认 `kenko32-xxxx`）
- [x] AP 为开放网络，无密码
- [x] AP 地址固定为 `192.168.6.1`
- [x] 启动 DHCP 服务器，网关和 DNS 指向设备自身
- [x] 启动 DNS 服务器，把 A 查询解析到 `192.168.6.1`（AAAA 返回空 NOERROR，客户端会自行回退）
- [x] 各系统的联网探测地址会被重定向到配网页
- [x] 通过 Web 页面提交 WiFi SSID 和密码并立即连接
- [x] LED 显示蓝色常亮

> 用 APSTA 而不是纯 AP 是必须的：`esp_wifi_scan_start()` 需要 STA 接口在线，
> 而扫描周边热点恰恰是配网页面最需要的能力。

### 设备内置 Web 服务

静态资源：

- [x] 托管 `web` 分区中的静态文件
- [x] 支持 `GET` 和 `HEAD`，其它方法返回 `405`
- [x] 按 `Accept-Encoding` 的 q 值与压缩率协商，优先下发 `.br` / `.zst` / `.gz`
- [x] `ETag` + `304 Not Modified`，静态资源带 `Cache-Control`
- [x] 路径做百分号解码并拒绝 `..`、反斜杠与非法转义
- [x] 单页应用回落到 `index.html`，回落路径同样走编码协商

REST 接口：

| 接口 | 说明 |
|------|------|
| `GET /api/system/info` | 设备、芯片、固件、运行时、时间、网络、文件系统的完整快照 |
| `POST /api/system/reboot` | 重启设备 |
| `POST /api/system/factory-reset` | 清空全部配置并重启进入配网 |
| `GET /api/system/ota` | 升级状态与 OTA 分区容量 |
| `POST /api/system/ota` | 上传固件（请求体为原始 `.bin`） |
| `POST /api/system/ota/confirm` | 确认当前镜像可用，取消回滚 |
| `POST /api/system/revert-to-factory` | 下次启动切回出厂基线固件 |
| `GET/DELETE /api/system/coredump` | 查询 / 擦除设备上保存的崩溃现场 |
| `GET/PUT /api/settings` | 设备名、时区、NTP 开关、状态灯亮度 |
| `GET /api/wifi/status` | 当前连接状态 |
| `GET /api/wifi/scan` | 扫描附近热点（`?force=1` 跳过设备端缓存） |
| `GET/PUT /api/wifi/config` | 读写多组 WiFi 配置 |
| `POST /api/wifi/connect` | 用已保存的配置连接，关闭配网热点 |
| `POST /api/wifi/provision` | 重新进入配网模式 |

错误一律返回 `{"error":{"code":"...","message":"..."}}`；未知的 `/api` 路径返回 `404` 而不是首页。

> `GET /api/wifi/config` **不会回传明文密码**，只给出 `has_password`。
> `PUT` 时把某条的 `password` 置为 `null` 即表示沿用设备上已保存的那一份。

## 分区布局

16MB Flash，一份出厂基线 + 两个 OTA 槽位：

| 分区 | 类型 | 偏移 | 大小 | 用途 |
|------|------|------|------|------|
| `nvs` | data/nvs | `0x9000` | 24K | WiFi 驱动等系统数据 |
| `otadata` | data/ota | `0xF000` | 8K | 记录下次从哪个 app 分区启动 |
| `phy_init` | data/phy | `0x11000` | 4K | 射频校准数据 |
| `coredump` | data/coredump | `0x12000` | 64K | 崩溃现场 |
| `factory` | app | `0x30000` | 2M | 出厂基线固件，永远不会被 OTA 覆盖 |
| `ota_0` | app | `0x230000` | 2M | OTA 槽位 A |
| `ota_1` | app | `0x430000` | 2M | OTA 槽位 B |
| `web` | data/littlefs | `0x630000` | 4M | 前端构建产物 |
| `storage` | data/littlefs | `0xA30000` | 5952K | WiFi 配置与设备设置 |

当前固件 924KB，2M 的槽位还有 56% 余量。`storage` 用尽 flash 尾部剩余空间，
所以它的大小不是整数 M。

### 三种"恢复"的区别

这三件事名字接近但互不相同，别搞混：

| 操作 | 触发方式 | 动固件？ | 动用户配置？ |
|------|----------|----------|--------------|
| **恢复出厂设置** | BOOT 键长按 10s / `POST /api/system/factory-reset` | 否 | 是，清空并格式化 `storage` |
| **OTA 自动回滚** | 新固件启动后未被确认，下次重启自动发生 | 是，退回上一个槽位 | 否 |
| **回退到出厂固件** | `POST /api/system/revert-to-factory` | 是，切回 `factory` | 否 |

`factory` 分区的意义在于：`esp_ota_get_next_update_partition()` 只在 `ota_*` 之间轮转，
所以它是一份**永远回得去的基线**。自动回滚只能退回上一个 OTA 版本，
两个槽位都被刷坏时就只剩它了。

> ESP-IDF 还有 bootloader 级的 GPIO 触发出厂重置（`CONFIG_BOOTLOADER_FACTORY_RESET`），
> 但这块板子用不了：唯一的按键接在 GPIO 0，而 GPIO 0 复位时拉低是 ROM 下载模式的
> strapping，二级 bootloader 根本轮不到执行。所以回退只做成了应用层的接口。

## 构建

固件侧需要 ESP-IDF v6.0.2；设备内置 Web 前端（`embed-web/`）由
[Vite+](https://viteplus.dev/) 统一管理，安装后 `vp` 会自行准备 Node.js 和包管理器：

```bash
curl -fsSL https://vite.plus | bash
```

首次拉取仓库后先安装前端依赖：

```bash
vp -C embed-web install
```

之后一条命令即可产出可直接烧录的完整镜像（会自动触发 `vp build` 生成前端产物，
再打包为 `web` 分区镜像并与 bootloader、分区表、固件合并）：

```bash
cmake --build build --target merged_bin
```

烧录：

```bash
esptool --chip esp32s3 --flash-mode dio --flash-size 16MB --flash-freq 80m write-flash 0x0 build/kenko-iot-sdk-merged.bin
```

前端可以脱离硬件单独开发，`vp dev` 自带设备 API 的模拟层：

```bash
vp -C embed-web dev
```

详见 [embed-web/README.md](embed-web/README.md)。

### 配置真源

`sdkconfig` 是构建产物，**不纳入版本控制**。配置的真源是 `sdkconfig.defaults`，
改动后需要 `idf.py fullclean` 或删掉 `build/` 与 `sdkconfig` 再构建。

其中几项值得注意：

- `CONFIG_SPIRAM=y` + `CONFIG_SPIRAM_MODE_OCT=y`：启用模组自带的 8MB Octal PSRAM。
  换用非 R8 型号的模组时必须关掉，否则无法启动。
- `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`：新固件启动后必须被确认，
  否则下次重启自动回退。固件在进入配网或联网状态时会自动确认。
- `CONFIG_ESPTOOLPY_FLASHMODE_DIO`：保持 DIO。S3 模组上 QIO 的收益有限，
  而个别 flash 颗粒在 80MHz QIO 下并不稳定。

## 测试

固件里与 ESP-IDF 解耦的纯逻辑模块（HSV 转换、DNS 报文构造、HTTP 路径与编码协商）
可以直接在开发机上编译运行，不需要硬件也不需要 QEMU：

```bash
cmake -S test/host -B build-host && cmake --build build-host && ctest --test-dir build-host
```

前端：

```bash
vp -C embed-web check && vp -C embed-web test
```

## 目录结构

```text
main/                 # 固件源码
  app_main.c          # 启动序列
  app_state.c         # 状态机（boot / provisioning / connecting / online / offline）
  wifi_manager.c      # STA 连接循环、SoftAP、异步扫描
  wifi_config_store.c # 多组 WiFi 配置的持久化
  settings_store.c    # 设备设置的持久化
  web_server.c        # 静态资源服务
  api_handlers.c      # REST 接口
  ota_service.c       # OTA 写入、校验与回滚
  time_sync.c         # SNTP 与时区
  dns_captive.c       # captive portal 的 DNS 服务
  status_led.c        # WS2812 状态灯
  button_monitor.c    # BOOT 键长按
  storage_fs.c        # LittleFS 挂载
  device_info.c       # 启动时采集的硬件与固件身份
  json_file.c         # 原子的 JSON 文件读写
  http_utils.c        # 路径消毒、MIME、编码协商（纯逻辑，可 host 单测）
  dns_message.c       # DNS 应答构造（纯逻辑，可 host 单测）
  led_color.c         # HSV/RGB 与呼吸曲线（纯逻辑，可 host 单测）
embed-web/            # 设备内置 Web 前端（SolidJS + Vite+）
test/host/            # 固件纯逻辑模块的 host 端单测
tools/                # 独立的硬件验证子项目（WS2812 点灯 / 跑马灯）
```
