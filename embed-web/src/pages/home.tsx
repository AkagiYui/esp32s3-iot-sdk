import { Clock, Droplets, House, Thermometer, Wifi } from "lucide-solid";
import type { RouteMeta } from "@/lib/route-manifest";
import { cx } from "@/lib/cx";
import styles from "./home.module.css";

export const routeMeta: RouteMeta = {
  label: "首页",
  icon: House,
  order: 0,
};

export default function HomePage() {
  return (
    <div class="page">
      <div class={cx("page-header", styles.pageHeader)}>
        <h1>首页</h1>
        <p class={cx("subtitle", styles.subtitle)}>ESP32-S3 IoT 控制面板</p>
      </div>

      <div class={styles.cardGrid}>
        <div class={styles.card}>
          <div class={styles.cardIcon}>
            <Wifi size={28} />
          </div>
          <div>
            <h3>WiFi 状态</h3>
            <p class={cx(styles.status, styles.online)}>已连接</p>
          </div>
        </div>

        <div class={styles.card}>
          <div class={styles.cardIcon}>
            <Thermometer size={28} />
          </div>
          <div>
            <h3>温度</h3>
            <p class={styles.value}>24.5°C</p>
          </div>
        </div>

        <div class={styles.card}>
          <div class={styles.cardIcon}>
            <Droplets size={28} />
          </div>
          <div>
            <h3>湿度</h3>
            <p class={styles.value}>62%</p>
          </div>
        </div>

        <div class={styles.card}>
          <div class={styles.cardIcon}>
            <Clock size={28} />
          </div>
          <div>
            <h3>运行时间</h3>
            <p class={styles.value}>3h 24m</p>
          </div>
        </div>
      </div>
    </div>
  );
}
