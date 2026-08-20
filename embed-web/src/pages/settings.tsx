import { createSignal, For, onMount, Show } from "solid-js";
import { createStore, reconcile } from "solid-js/store";
import { Dynamic } from "solid-js/web";
import {
  Clock,
  Lightbulb,
  Monitor,
  Moon,
  Palette,
  RotateCcw,
  Save,
  Settings,
  Sun,
  TriangleAlert,
} from "lucide-solid";
import Button from "@/components/Button";
import Card from "@/components/Card";
import InfoRow from "@/components/InfoRow";
import { describeError } from "@/lib/api";
import { factoryResetDevice, rebootDevice, refreshSystemInfo, systemInfo } from "@/lib/device";
import { showConfirm, showToast } from "@/lib/feedback";
import {
  diffSettings,
  fetchSettings,
  saveSettings,
  TIMEZONE_PRESETS,
  validateSettings,
  type DeviceSettings,
} from "@/lib/settings";
import { setThemeMode, themeMode, type ThemeMode } from "@/lib/theme";
import type { IconComponent, RouteMeta } from "@/lib/route-manifest";
import { cx } from "@/lib/cx";
import styles from "./settings.module.css";

export const routeMeta: RouteMeta = {
  label: "设置",
  icon: Settings,
  order: 4,
};

const THEME_OPTIONS: { value: ThemeMode; label: string; icon: IconComponent }[] = [
  { value: "system", label: "跟随系统", icon: Monitor },
  { value: "light", label: "亮色", icon: Sun },
  { value: "dark", label: "暗色", icon: Moon },
];

const EMPTY_SETTINGS: DeviceSettings = {
  device_name: "",
  timezone: "",
  ntp_enabled: true,
  led_brightness: 100,
};

export default function SettingsPage() {
  const [draft, setDraft] = createStore<DeviceSettings>({ ...EMPTY_SETTINGS });
  const [saved, setSaved] = createSignal<DeviceSettings | undefined>(undefined);
  const [loading, setLoading] = createSignal(true);
  const [saving, setSaving] = createSignal(false);

  const dirty = () => diffSettings(saved(), draft) !== undefined;

  async function load() {
    setLoading(true);
    try {
      const settings = await fetchSettings();
      setSaved(settings);
      // reconcile 就地更新，输入框不会因为整体替换而丢焦点
      setDraft(reconcile(settings));
    } catch (error) {
      showToast(describeError(error), "error");
    } finally {
      setLoading(false);
    }
  }

  async function submit() {
    const problem = validateSettings(draft);
    if (problem) {
      showToast(problem, "warning");
      return;
    }

    const patch = diffSettings(saved(), draft);
    if (!patch) {
      showToast("没有需要保存的改动", "info");
      return;
    }

    setSaving(true);
    try {
      const settings = await saveSettings(patch);
      setSaved(settings);
      setDraft(reconcile(settings));
      showToast("设置已保存", "success");
      void refreshSystemInfo();
    } catch (error) {
      showToast(describeError(error), "error");
    } finally {
      setSaving(false);
    }
  }

  function revert() {
    const settings = saved();
    if (settings) {
      setDraft(reconcile(settings));
    }
  }

  async function reboot() {
    if ((await showConfirm("设备会立即重启，页面将短暂失联。", "重启设备", true)) !== "ok") {
      return;
    }
    try {
      await rebootDevice();
      showToast("设备正在重启…", "info");
    } catch (error) {
      showToast(describeError(error), "error");
    }
  }

  async function factoryReset() {
    const confirmed = await showConfirm(
      "将清空所有 WiFi 配置与设备设置，并重启进入配网模式。此操作不可撤销。",
      "恢复出厂设置",
      true,
    );
    if (confirmed !== "ok") {
      return;
    }
    try {
      await factoryResetDevice();
      showToast("设备正在恢复出厂设置…", "warning");
    } catch (error) {
      showToast(describeError(error), "error");
    }
  }

  onMount(() => void load());

  return (
    <div class="page">
      <div class="page-header">
        <h1>设置</h1>
        <p class="subtitle">设备配置与外观</p>
      </div>

      <Card icon={Palette} title="外观" subtitle="仅影响当前浏览器，不写入设备">
        <div class={styles.themePicker}>
          <For each={THEME_OPTIONS}>
            {(option) => (
              <button
                class={cx(styles.themeBtn, themeMode() === option.value && styles.active)}
                onClick={() => setThemeMode(option.value)}
                aria-pressed={themeMode() === option.value}
              >
                <Dynamic component={option.icon} size={16} />
                <span>{option.label}</span>
              </button>
            )}
          </For>
        </div>
      </Card>

      <Card
        icon={Settings}
        title="设备"
        subtitle={loading() ? "读取中…" : undefined}
        actions={
          <Show when={dirty()}>
            <Button variant="ghost" size="sm" icon={RotateCcw} onClick={revert}>
              撤销
            </Button>
          </Show>
        }
      >
        <InfoRow label="设备名称" hint="配网热点名 / mDNS 实例名">
          <input
            class={styles.input}
            type="text"
            value={draft.device_name}
            maxlength={32}
            disabled={loading()}
            onInput={(event) => setDraft("device_name", event.currentTarget.value)}
          />
        </InfoRow>

        <InfoRow label="状态灯亮度" hint={`${draft.led_brightness}%`}>
          <input
            class={styles.slider}
            type="range"
            min="0"
            max="100"
            step="5"
            value={draft.led_brightness}
            disabled={loading()}
            aria-label="状态灯亮度"
            onInput={(event) => setDraft("led_brightness", event.currentTarget.valueAsNumber)}
          />
        </InfoRow>

        <InfoRow label="默认名称" value={systemInfo()?.device.default_name} mono tone="muted" />
      </Card>

      <Card icon={Clock} title="时间">
        <InfoRow label="自动校时" hint="联网后通过 NTP 同步">
          <button
            class={cx(styles.toggle, draft.ntp_enabled && styles.on)}
            role="switch"
            aria-checked={draft.ntp_enabled}
            aria-label="自动校时"
            disabled={loading()}
            onClick={() => setDraft("ntp_enabled", !draft.ntp_enabled)}
          >
            <span class={styles.knob} />
          </button>
        </InfoRow>

        <InfoRow label="时区" hint="POSIX TZ 串">
          <select
            class={styles.select}
            value={draft.timezone}
            disabled={loading()}
            onChange={(event) => setDraft("timezone", event.currentTarget.value)}
          >
            <For each={TIMEZONE_PRESETS}>
              {(preset) => <option value={preset.value}>{preset.label}</option>}
            </For>
            <Show when={!TIMEZONE_PRESETS.some((preset) => preset.value === draft.timezone)}>
              <option value={draft.timezone}>{draft.timezone || "自定义"}</option>
            </Show>
          </select>
        </InfoRow>

        <InfoRow
          label="同步状态"
          value={systemInfo()?.time.synced ? "已同步" : "未同步"}
          tone={systemInfo()?.time.synced ? "accent" : "muted"}
        />
      </Card>

      <Button
        variant="primary"
        icon={Save}
        block
        loading={saving()}
        disabled={loading() || !dirty()}
        onClick={() => void submit()}
      >
        {dirty() ? "保存设置" : "已是最新"}
      </Button>

      <Card icon={TriangleAlert} title="危险操作" subtitle="以下操作会中断设备当前工作">
        <div class={styles.dangerActions}>
          <Button variant="secondary" icon={Lightbulb} block onClick={() => void reboot()}>
            重启设备
          </Button>
          <Button variant="danger" icon={TriangleAlert} block onClick={() => void factoryReset()}>
            恢复出厂设置
          </Button>
        </div>
      </Card>
    </div>
  );
}
