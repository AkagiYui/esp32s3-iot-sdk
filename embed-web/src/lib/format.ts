/** 展示层的纯格式化函数，集中放在这里方便单测。 */

const UNITS = ["B", "KB", "MB", "GB"] as const;

/** 字节数转人类可读，保留一位小数（整数则不带小数）。 */
export function formatBytes(bytes: number): string {
  if (!Number.isFinite(bytes) || bytes < 0) {
    return "—";
  }

  let value = bytes;
  let unitIndex = 0;
  while (value >= 1024 && unitIndex < UNITS.length - 1) {
    value /= 1024;
    unitIndex += 1;
  }

  const rounded = Math.round(value * 10) / 10;
  const text = Number.isInteger(rounded) ? String(rounded) : rounded.toFixed(1);
  return `${text} ${UNITS[unitIndex]}`;
}

/** 毫秒转 "3d 4h 12m" 形式的运行时长。 */
export function formatUptime(milliseconds: number): string {
  if (!Number.isFinite(milliseconds) || milliseconds < 0) {
    return "—";
  }

  const totalSeconds = Math.floor(milliseconds / 1000);
  const days = Math.floor(totalSeconds / 86400);
  const hours = Math.floor((totalSeconds % 86400) / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);
  const seconds = totalSeconds % 60;

  if (days > 0) return `${days}d ${hours}h ${minutes}m`;
  if (hours > 0) return `${hours}h ${minutes}m`;
  if (minutes > 0) return `${minutes}m ${seconds}s`;
  return `${seconds}s`;
}

/** 百分比，输入非法或分母为 0 时返回 0。 */
export function percentage(used: number, total: number): number {
  if (!Number.isFinite(used) || !Number.isFinite(total) || total <= 0) {
    return 0;
  }
  return Math.min(100, Math.max(0, Math.round((used / total) * 100)));
}

/** 把 RSSI 翻译成人话。 */
export function signalLevel(rssi: number): string {
  if (rssi >= -55) return "极佳";
  if (rssi >= -67) return "良好";
  if (rssi >= -75) return "一般";
  return "较弱";
}

/** RSSI 映射到 0..4 格信号强度。 */
export function signalBars(rssi: number): number {
  if (!Number.isFinite(rssi) || rssi === 0) return 0;
  if (rssi >= -55) return 4;
  if (rssi >= -67) return 3;
  if (rssi >= -75) return 2;
  if (rssi >= -85) return 1;
  return 0;
}

/** 设备返回的 ISO 时间串转本地可读形式；空串或非法值原样降级。 */
export function formatDateTime(iso: string): string {
  if (!iso) {
    return "未同步";
  }

  const parsed = new Date(iso);
  if (Number.isNaN(parsed.getTime())) {
    return iso;
  }

  const pad = (value: number) => String(value).padStart(2, "0");
  return (
    `${parsed.getFullYear()}-${pad(parsed.getMonth() + 1)}-${pad(parsed.getDate())} ` +
    `${pad(parsed.getHours())}:${pad(parsed.getMinutes())}:${pad(parsed.getSeconds())}`
  );
}
