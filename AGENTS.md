# 面向自动化助手的项目说明

## 环境准备

固件侧需要 ESP-IDF **v6.0.2**。先激活环境（路径按本机安装位置替换）：

```bash
. "$HOME/.espressif/tools/activate_idf_v6.0.2.sh"   # 或 . "$IDF_PATH/export.sh"
```

前端侧由 [Vite+](https://viteplus.dev/)（`vp`）统一管理，`vp` 会自行准备 Node.js 与包管理器：

```bash
curl -fsSL https://vite.plus | bash
```

## 常用命令

| 目的 | 命令 |
|------|------|
| 编译固件 | `idf.py build` |
| 产出可直接烧录的完整镜像 | `cmake --build build --target merged_bin` |
| 固件 host 端单测 | `cmake -S test/host -B build-host && cmake --build build-host && ctest --test-dir build-host` |
| 前端依赖安装 | `vp -C embed-web install` |
| 前端 lint + 类型检查 | `vp -C embed-web check` |
| 前端单测 | `vp -C embed-web test` |
| 前端开发服务器（自带设备 API 模拟层） | `vp -C embed-web dev` |
| C 代码格式校验 | `clang-format --dry-run --Werror components/*/*.c components/*/include/*.h main/*.c main/*.h` |

## 约定

- `sdkconfig` 是构建产物，不要提交；配置真源是 `sdkconfig.defaults`，改动后需要 `idf.py fullclean`。
- 功能实现全部在 `components/kenko_*` 下，`main/` 只保留启动序列与状态机。组件之间通过 `kenko_core` 的事件基通信，不允许组件反向依赖 `main/`。
- 与 ESP-IDF 解耦的纯逻辑模块（`kenko_board/led_color.c`、`kenko_wifi/dns_message.c`、`kenko_web/http_utils.c`）必须保持可在 host 上编译，改动时同步更新 `test/host/`。
- 可调参数一律走 Kconfig（`CONFIG_KENKO_*`），不要在源码里写死。
- 提交信息使用 Conventional Commits。
