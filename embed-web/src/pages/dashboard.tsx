import { For, Show } from "solid-js";
import {
  ChartColumn,
  HardDrive,
  LayoutDashboard,
  MemoryStick,
  Microchip,
  Radio,
} from "lucide-solid";
import Card from "@/components/Card";
import InfoRow from "@/components/InfoRow";
import Meter from "@/components/Meter";
import { systemInfo } from "@/lib/device";
import { formatBytes, signalBars, signalLevel } from "@/lib/format";
import type { RouteMeta } from "@/lib/route-manifest";
import styles from "./dashboard.module.css";

export const routeMeta: RouteMeta = {
  label: "仪表盘",
  icon: LayoutDashboard,
  order: 1,
};

const PARTITION_LABELS: Record<string, string> = {
  storage: "配置分区",
  web: "前端资源分区",
};

export default function DashboardPage() {
  const info = systemInfo;

  const heapUsed = () => {
    const runtime = info()?.runtime;
    if (!runtime) return 0;
    return Math.max(0, runtime.total_heap - runtime.free_heap);
  };

  return (
    <div class="page">
      <div class="page-header">
        <h1>仪表盘</h1>
        <p class="subtitle">运行指标与硬件信息</p>
      </div>

      <Show
        when={info()}
        fallback={
          <Card>
            <p class={styles.placeholder}>正在读取运行指标…</p>
          </Card>
        }
      >
        {(data) => (
          <>
            <Card icon={MemoryStick} title="内存">
              <Meter
                label="内部 RAM 占用"
                used={heapUsed()}
                total={data().runtime.total_heap}
                detail={`${formatBytes(heapUsed())} / ${formatBytes(data().runtime.total_heap)}`}
              />
              <InfoRow label="当前可用" value={formatBytes(data().runtime.free_heap)} />
              <InfoRow
                label="历史最低可用"
                value={formatBytes(data().runtime.min_free_heap)}
                hint="启动至今的水位线"
              />
            </Card>

            <Card icon={HardDrive} title="存储">
              <For each={Object.entries(data().filesystem)}>
                {([label, usage]) => (
                  <Show
                    when={usage.mounted}
                    fallback={
                      <InfoRow
                        label={PARTITION_LABELS[label] ?? label}
                        value="未挂载"
                        tone="danger"
                      />
                    }
                  >
                    <Meter
                      label={PARTITION_LABELS[label] ?? label}
                      used={usage.used}
                      total={usage.total}
                      detail={`${formatBytes(usage.used)} / ${formatBytes(usage.total)}`}
                    />
                  </Show>
                )}
              </For>
            </Card>

            <Card icon={Radio} title="无线">
              <Show
                when={data().wifi.connected}
                fallback={<InfoRow label="STA 连接" value="未连接" tone="muted" />}
              >
                <div class={styles.signalRow}>
                  <div class={styles.signalBars} aria-hidden="true">
                    <For each={[1, 2, 3, 4]}>
                      {(bar) => (
                        <span
                          class={styles.bar}
                          classList={{ [styles.barActive!]: signalBars(data().wifi.rssi) >= bar }}
                          style={{ height: `${6 + bar * 5}px` }}
                        />
                      )}
                    </For>
                  </div>
                  <div class={styles.signalText}>
                    <strong>{data().wifi.rssi} dBm</strong>
                    <span>{signalLevel(data().wifi.rssi)}</span>
                  </div>
                </div>
                <InfoRow label="SSID" value={data().wifi.ssid} />
                <InfoRow label="信道" value={data().wifi.channel} />
                <InfoRow label="网关" value={data().wifi.gateway} mono />
                <InfoRow label="子网掩码" value={data().wifi.netmask} mono />
              </Show>
              <InfoRow label="工作模式" value={data().wifi.mode.toUpperCase()} mono />
              <Show when={data().wifi.ap_active}>
                <InfoRow label="热点接入数" value={`${data().wifi.ap_clients} 台`} />
              </Show>
            </Card>

            <Card icon={Microchip} title="硬件">
              <InfoRow
                label="芯片"
                value={`${data().chip.model} ${data().chip.revision}`}
                hint={`${data().chip.cores} 核`}
              />
              <InfoRow label="Flash" value={formatBytes(data().chip.flash_size)} />
              <InfoRow
                label="PSRAM"
                value={data().chip.psram_size > 0 ? formatBytes(data().chip.psram_size) : "未启用"}
                tone={data().chip.psram_size > 0 ? "default" : "muted"}
              />
            </Card>

            <Card icon={ChartColumn} title="固件">
              <InfoRow label="版本号" value={`#${data().firmware.version}`} />
              <InfoRow label="提交" value={data().firmware.name} mono />
              <InfoRow label="构建时间" value={data().firmware.build_time} mono />
              <InfoRow label="ESP-IDF" value={data().firmware.idf_version} mono />
              <InfoRow label="运行分区" value={data().firmware.running_partition} mono />
            </Card>
          </>
        )}
      </Show>
    </div>
  );
}
