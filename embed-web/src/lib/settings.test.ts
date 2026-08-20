import { describe, expect, it } from "vite-plus/test";
import { diffSettings, validateSettings, type DeviceSettings } from "./settings";

const base: DeviceSettings = {
  device_name: "kenko32-ab12",
  timezone: "CST-8",
  ntp_enabled: true,
  led_brightness: 100,
};

describe("diffSettings", () => {
  it("没有基线时整份视为改动", () => {
    expect(diffSettings(undefined, base)).toEqual(base);
  });

  it("没有改动时返回 undefined", () => {
    expect(diffSettings(base, { ...base })).toBeUndefined();
  });

  it("只输出真正变化的字段", () => {
    expect(diffSettings(base, { ...base, led_brightness: 40 })).toEqual({ led_brightness: 40 });
  });

  it("false 与 0 这类假值也算改动", () => {
    expect(diffSettings(base, { ...base, ntp_enabled: false })).toEqual({ ntp_enabled: false });
    expect(diffSettings(base, { ...base, led_brightness: 0 })).toEqual({ led_brightness: 0 });
  });
});

describe("validateSettings", () => {
  it("接受合法设置", () => {
    expect(validateSettings(base)).toBeUndefined();
  });

  it("设备名不能为空", () => {
    expect(validateSettings({ ...base, device_name: "" })).toBe("设备名称不能为空");
  });

  it("设备名按字节限制在 32 以内", () => {
    expect(validateSettings({ ...base, device_name: "a".repeat(32) })).toBeUndefined();
    expect(validateSettings({ ...base, device_name: "a".repeat(33) })).toBe("设备名称超过 32 字节");
    expect(validateSettings({ ...base, device_name: "设".repeat(11) })).toBe(
      "设备名称超过 32 字节",
    );
  });

  it("时区不能为空", () => {
    expect(validateSettings({ ...base, timezone: "  " })).toBe("时区不能为空");
  });

  it("亮度必须在 0..100", () => {
    expect(validateSettings({ ...base, led_brightness: 0 })).toBeUndefined();
    expect(validateSettings({ ...base, led_brightness: 101 })).toBe("亮度必须在 0..100 之间");
    expect(validateSettings({ ...base, led_brightness: -1 })).toBe("亮度必须在 0..100 之间");
  });
});
