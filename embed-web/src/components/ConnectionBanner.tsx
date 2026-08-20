import { Show } from "solid-js";
import { RefreshCw, WifiOff } from "lucide-solid";
import { deviceError, deviceLoading, deviceReachable, refreshSystemInfo } from "@/lib/device";
import Button from "./Button";
import styles from "./ConnectionBanner.module.css";

/**
 * 设备失联时的横幅。
 *
 * 设备重启、切换 WiFi 模式、OTA 都会让页面短暂断连，
 * 明确告诉用户"现在连不上"比让数据默默停留在旧值要好得多。
 */
export default function ConnectionBanner() {
  return (
    <Show when={!deviceReachable()}>
      <div class={styles.banner} role="status" aria-live="polite">
        <WifiOff size={18} />
        <div class={styles.text}>
          <strong>与设备失去连接</strong>
          <span>{deviceError() ?? "正在自动重试…"}</span>
        </div>
        <Button
          variant="ghost"
          size="sm"
          icon={RefreshCw}
          loading={deviceLoading()}
          onClick={() => void refreshSystemInfo()}
        >
          重试
        </Button>
      </div>
    </Show>
  );
}
