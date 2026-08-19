import { createSignal } from "solid-js";
import { ChartColumn, LayoutDashboard, Lightbulb, Radio, Sun } from "lucide-solid";
import type { RouteMeta } from "@/lib/route-manifest";
import { cx } from "@/lib/cx";
import styles from "./dashboard.module.css";

export const routeMeta: RouteMeta = {
  label: "仪表盘",
  icon: LayoutDashboard,
  order: 1,
};

export default function DashboardPage() {
  const [ledOn, setLedOn] = createSignal(false);
  const [brightness, setBrightness] = createSignal(75);

  return (
    <div class="page">
      <div class={cx("page-header", styles.pageHeader)}>
        <h1>仪表盘</h1>
        <p class={cx("subtitle", styles.subtitle)}>设备控制与监控</p>
      </div>

      <div class={styles.controlList}>
        <div class={styles.controlItem}>
          <div class={styles.controlInfo}>
            <span class={styles.controlIcon}>
              <Lightbulb size={24} />
            </span>
            <div>
              <h3>LED 灯</h3>
              <p>{ledOn() ? "已开启" : "已关闭"}</p>
            </div>
          </div>
          <button
            class={cx(styles.toggle, ledOn() && styles.active)}
            aria-label="切换 LED 灯"
            aria-pressed={ledOn()}
            onClick={() => setLedOn(!ledOn())}
          >
            <span class={styles.toggleKnob} />
          </button>
        </div>

        <div class={styles.controlItem}>
          <div class={styles.controlInfo}>
            <span class={styles.controlIcon}>
              <Sun size={24} />
            </span>
            <div>
              <h3>亮度</h3>
              <p>{brightness()}%</p>
            </div>
          </div>
          <input
            type="range"
            min="0"
            max="100"
            aria-label="亮度"
            value={brightness()}
            onInput={(event) => setBrightness(event.currentTarget.valueAsNumber)}
            class={styles.slider}
          />
        </div>

        <div class={styles.controlItem}>
          <div class={styles.controlInfo}>
            <span class={styles.controlIcon}>
              <ChartColumn size={24} />
            </span>
            <div>
              <h3>内存使用</h3>
              <p>124 KB / 320 KB</p>
            </div>
          </div>
          <div class={styles.progressBar}>
            <div class={styles.progressFill} style={{ width: "39%" }} />
          </div>
        </div>

        <div class={styles.controlItem}>
          <div class={styles.controlInfo}>
            <span class={styles.controlIcon}>
              <Radio size={24} />
            </span>
            <div>
              <h3>RSSI 信号</h3>
              <p>-42 dBm</p>
            </div>
          </div>
          <span class={cx(styles.badge, styles.good)}>良好</span>
        </div>
      </div>
    </div>
  );
}
