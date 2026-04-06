# WS2812 Smoke Test

这是一个独立于主固件的最小 ESP-IDF 子项目，只负责驱动板载 WS2812B 点灯，用于排查硬件连线、GPIO 选择、RMT 驱动和烧录流程是否正常。

默认使用 GPIO 48，与主工程保持一致。

## 构建 merged bin

```bash
cd tools/ws2812-smoke-test
./build-merged.sh
```

生成产物：

- `build/ws2812_smoke_test.bin`
- `build/ws2812_smoke_test-merged.bin`

## 从 0x0 直接烧录

```bash
esptool.py --chip esp32s3 write_flash 0x0 build/ws2812_smoke_test-merged.bin
```

## 修改测试 GPIO

如果怀疑板载灯不在 GPIO 48，可以在配置阶段覆盖：

```bash
cmake -S . -B build -DTEST_WS2812_GPIO=47
cmake --build build --target merged_bin
```

固件上电后会按 红 -> 绿 -> 蓝 -> 白 -> 灭 循环，每步 1 秒。