import { apiRequest } from "./api";

export type DeviceSettings = {
  device_name: string;
  timezone: string;
  ntp_enabled: boolean;
  led_brightness: number;
};

/** 常用时区的 POSIX TZ 串。设置页也允许直接手填其它值。 */
export const TIMEZONE_PRESETS: { label: string; value: string }[] = [
  { label: "中国标准时间 (UTC+8)", value: "CST-8" },
  { label: "日本标准时间 (UTC+9)", value: "JST-9" },
  { label: "协调世界时 (UTC)", value: "UTC0" },
  { label: "中欧时间 (UTC+1/+2)", value: "CET-1CEST,M3.5.0,M10.5.0/3" },
  { label: "美国东部时间 (UTC-5/-4)", value: "EST5EDT,M3.2.0,M11.1.0" },
  { label: "美国太平洋时间 (UTC-8/-7)", value: "PST8PDT,M3.2.0,M11.1.0" },
];

export function fetchSettings(): Promise<DeviceSettings> {
  return apiRequest<DeviceSettings>("/api/settings");
}

/** 部分更新：只发送真正改动过的字段。 */
export function saveSettings(patch: Partial<DeviceSettings>): Promise<DeviceSettings> {
  return apiRequest<DeviceSettings>("/api/settings", { method: "PUT", body: patch });
}

/** 计算 `next` 相对 `base` 的差异，没有改动时返回 undefined。 */
export function diffSettings(
  base: DeviceSettings | undefined,
  next: DeviceSettings,
): Partial<DeviceSettings> | undefined {
  if (!base) {
    return next;
  }

  const patch: Partial<DeviceSettings> = {};
  if (base.device_name !== next.device_name) patch.device_name = next.device_name;
  if (base.timezone !== next.timezone) patch.timezone = next.timezone;
  if (base.ntp_enabled !== next.ntp_enabled) patch.ntp_enabled = next.ntp_enabled;
  if (base.led_brightness !== next.led_brightness) patch.led_brightness = next.led_brightness;

  return Object.keys(patch).length > 0 ? patch : undefined;
}

/** 与固件 settings_store_validate() 保持一致的前端校验。 */
export function validateSettings(settings: DeviceSettings): string | undefined {
  const nameLength = new TextEncoder().encode(settings.device_name).length;
  if (nameLength === 0) {
    return "设备名称不能为空";
  }
  if (nameLength > 32) {
    return "设备名称超过 32 字节";
  }
  if (settings.timezone.trim().length === 0) {
    return "时区不能为空";
  }
  if (settings.led_brightness < 0 || settings.led_brightness > 100) {
    return "亮度必须在 0..100 之间";
  }
  return undefined;
}
