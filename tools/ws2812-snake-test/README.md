# WS2812 Snake Test

这是一个独立于主固件的 ESP-IDF 子项目，在 WS2812B 灯带/灯板上跑一条彩虹跑马灯（"贪吃蛇"），用于验证长灯带的 RMT DMA 驱动、供电和刷新率是否正常。

与 [ws2812-smoke-test](../ws2812-smoke-test) 的区别：smoke-test 只做单色点灯，验证接线和 GPIO；本项目连续刷新 256 颗灯，压测 DMA 传输与电源。

默认参数（在 `main/app_main.c` 顶部的「可调参数」区修改）：

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `SNAKE_GPIO` | 18 | 灯带数据脚，注意与主工程的 GPIO 48 不同 |
| `SNAKE_LED_COUNT` | 256 | 灯珠总数 |
| `SNAKE_LENGTH` | 40 | 蛇身长度（亮灯个数） |
| `SNAKE_SPEED_MS` | 40 | 每帧间隔毫秒 |
| `HUE_STEP` | 3 | 每帧色相变化步长 |

## 构建 merged bin

```bash
cd tools/ws2812-snake-test
./build-merged.sh
```

生成产物：

- `build/ws2812_snake_test.bin`
- `build/ws2812_snake_test-merged.bin`

## 从 0x0 直接烧录

```bash
esptool.py --chip esp32s3 write_flash 0x0 build/ws2812_snake_test-merged.bin
```

固件上电后蛇身会绕灯带循环前进，头部最亮、尾部平方衰减，色相逐帧渐变。
