import { Show } from "solid-js";
import { Activity, Cpu, House, RefreshCw, Router, Wifi } from "lucide-solid";
import Button from "@/components/Button";
import Card from "@/components/Card";
import InfoRow from "@/components/InfoRow";
import { showToast } from "@/lib/feedback";
import { describeError } from "@/lib/api";
import { DEVICE_STATE_LABELS, deviceLoading, refreshSystemInfo, systemInfo } from "@/lib/device";
import { formatBytes, formatDateTime, formatUptime, signalLevel } from "@/lib/format";
import { applyWifiConfig, enterProvisioning } from "@/lib/wifi";
import type { RouteMeta } from "@/lib/route-manifest";
import styles from "./home.module.css";

export const routeMeta: RouteMeta = {
  label: "首页",
  icon: House,
  order: 0,
};

export default function HomePage() {
  const info = systemInfo;
  const wifi = () => info()?.wifi;

  async function switchToProvisioning() {
    try {
      await enterProvisioning();
      showToast("设备正在切换到配网模式", "info");
    } catch (error) {
      showToast(describeError(error), "error");
    }
  }

  async function reconnect() {
    try {
      await applyWifiConfig();
      showToast("设备正在重新连接，配网热点将关闭", "info");
    } catch (error) {
      showToast(describeError(error), "error");
    }
  }

  return (
    <div class="page">
      <div class="page-header">
        <h1>首页</h1>
        <p class="subtitle">设备当前状态一览</p>
      </div>

      <Show
        when={info()}
        fallback={
          <Card>
            <p class={styles.placeholder}>正在读取设备状态…</p>
          </Card>
        }
      >
        {(data) => (
          <>
            <Card
              icon={Activity}
              title="运行状态"
              actions={
                <Button
                  variant="ghost"
                  size="sm"
                  icon={RefreshCw}
                  loading={deviceLoading()}
                  onClick={() => void refreshSystemInfo()}
                  aria-label="刷新设备状态"
                />
              }
            >
              <InfoRow
                label="设备状态"
                value={DEVICE_STATE_LABELS[data().device.state] ?? data().device.state}
                tone={data().device.state === "online" ? "accent" : "default"}
              />
              <InfoRow label="运行时长" value={formatUptime(data().runtime.uptime_ms)} />
              <InfoRow
                label="设备时间"
                value={formatDateTime(data().time.local)}
                hint={data().time.synced ? data().time.timezone : "尚未与 NTP 同步"}
              />
            </Card>

            <Card icon={Wifi} title="网络">
              <Show
                when={wifi()?.connected}
                fallback={
                  <>
                    <InfoRow
                      label="连接状态"
                      value={wifi()?.connecting ? "正在连接…" : "未连接"}
                      tone="muted"
                    />
                    <Show when={wifi()?.ap_active}>
                      <InfoRow label="配网热点" value={data().device.name} tone="accent" />
                      <InfoRow label="热点地址" value={wifi()?.ap_ip} mono />
                      <InfoRow label="已接入设备" value={`${wifi()?.ap_clients ?? 0} 台`} />
                    </Show>
                  </>
                }
              >
                <InfoRow label="已连接" value={wifi()?.ssid} tone="accent" />
                <InfoRow
                  label="信号强度"
                  value={`${wifi()?.rssi} dBm · ${signalLevel(wifi()?.rssi ?? -100)}`}
                />
                <InfoRow label="IP 地址" value={wifi()?.ip} mono />
                <InfoRow label="mDNS" value={`${data().device.mdns_hostname}.local`} mono />
              </Show>
            </Card>

            <Card icon={Cpu} title="设备">
              <InfoRow label="设备名称" value={data().device.name} />
              <InfoRow label="MAC 地址" value={data().device.mac} mono />
              <InfoRow
                label="固件版本"
                value={`#${data().firmware.version} · ${data().firmware.name}`}
                mono
              />
              <InfoRow
                label="可用内存"
                value={formatBytes(data().runtime.heap.internal.free)}
                hint={`内部 RAM，历史最低 ${formatBytes(data().runtime.heap.internal.min_free)}`}
              />
            </Card>

            <div class={styles.actions}>
              <Show
                when={data().device.provisioning}
                fallback={
                  <Button
                    variant="secondary"
                    icon={Router}
                    block
                    onClick={() => void switchToProvisioning()}
                  >
                    进入配网模式
                  </Button>
                }
              >
                <Button variant="primary" icon={Wifi} block onClick={() => void reconnect()}>
                  用已保存的配置连接
                </Button>
              </Show>
            </div>
          </>
        )}
      </Show>
    </div>
  );
}
