import { createSignal, For } from "solid-js";
import { Dynamic } from "solid-js/web";
import { Monitor, Moon, Settings, Sun } from "lucide-solid";
import type { IconComponent, RouteMeta } from "@/lib/route-manifest";
import { setThemeMode, themeMode, type ThemeMode } from "@/lib/theme";
import { showAlert, showConfirm, showToast } from "@/lib/feedback";
import { cx } from "@/lib/cx";
import styles from "./settings.module.css";

export const routeMeta: RouteMeta = {
  label: "设置",
  icon: Settings,
  order: 2,
};

const themeOptions: { value: ThemeMode; label: string; icon: IconComponent }[] = [
  { value: "system", label: "跟随系统", icon: Monitor },
  { value: "light", label: "亮色", icon: Sun },
  { value: "dark", label: "暗色", icon: Moon },
];

export default function SettingsPage() {
  const [deviceName, setDeviceName] = createSignal("ESP32-S3-IoT");

  const checkUpdate = () => {
    void showAlert("OTA 升级功能待实现", "提示");
    showToast("当前已是最新版本", "info");
  };

  const rebootDevice = async () => {
    const result = await showConfirm("确定要重启设备吗？", "重启设备", true);
    if (result === "ok") {
      showToast("设备正在重启…", "success");
    } else {
      showToast("已取消重启", "warning");
    }
  };

  return (
    <div class="page">
      <div class={cx("page-header", styles.pageHeader)}>
        <h1>设置</h1>
        <p class={cx("subtitle", styles.subtitle)}>设备配置</p>
      </div>

      <div class={styles.settingsGroup}>
        <h2 class={styles.groupTitle}>外观</h2>
        <div class={styles.settingsList}>
          <div class={styles.settingItem}>
            <span class={styles.settingLabel}>主题模式</span>
            <div class={styles.themePicker}>
              <For each={themeOptions}>
                {(option) => (
                  <button
                    class={cx(styles.themeBtn, themeMode() === option.value && styles.active)}
                    onClick={() => setThemeMode(option.value)}
                    aria-label={option.label}
                    aria-pressed={themeMode() === option.value}
                  >
                    <Dynamic component={option.icon} size={16} />
                    <span>{option.label}</span>
                  </button>
                )}
              </For>
            </div>
          </div>
        </div>
      </div>

      <div class={styles.settingsGroup}>
        <h2 class={styles.groupTitle}>设备</h2>
        <div class={styles.settingsList}>
          <label class={styles.settingItem}>
            <span class={styles.settingLabel}>设备名称</span>
            <input
              type="text"
              value={deviceName()}
              onInput={(event) => setDeviceName(event.currentTarget.value)}
              class={styles.settingInput}
            />
          </label>
          <div class={styles.settingItem}>
            <span class={styles.settingLabel}>固件版本</span>
            <span class={styles.settingValue}>v1.0.0</span>
          </div>
          <div class={styles.settingItem}>
            <span class={styles.settingLabel}>SDK 版本</span>
            <span class={styles.settingValue}>ESP-IDF 5.x</span>
          </div>
        </div>
      </div>

      <div class={styles.settingsGroup}>
        <h2 class={styles.groupTitle}>网络</h2>
        <div class={styles.settingsList}>
          <div class={styles.settingItem}>
            <span class={styles.settingLabel}>IP 地址</span>
            <span class={styles.settingValue}>192.168.4.1</span>
          </div>
          <div class={styles.settingItem}>
            <span class={styles.settingLabel}>MAC 地址</span>
            <span class={styles.settingValue}>AA:BB:CC:DD:EE:FF</span>
          </div>
        </div>
      </div>

      <div class={styles.settingsGroup}>
        <h2 class={styles.groupTitle}>操作</h2>
        <div class={styles.settingsList}>
          <div class={cx(styles.settingItem, styles.action)}>
            <button class={styles.btn} onClick={checkUpdate}>
              检查更新
            </button>
          </div>
          <div class={cx(styles.settingItem, styles.action)}>
            <button class={cx(styles.btn, styles.danger)} onClick={() => void rebootDevice()}>
              重启设备
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}
