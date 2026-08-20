import { For, onCleanup, Show } from "solid-js";
import { RefreshCw, Signal, Wifi, X } from "lucide-solid";
import { createPresence } from "@/lib/presence";
import { signalLevel } from "@/lib/format";
import type { WifiScanEntry } from "@/lib/wifi";
import { cx } from "@/lib/cx";
import styles from "./WifiScanModal.module.css";

/** 出场动画时长，需与 WifiScanModal.module.css 保持一致。 */
const EXIT_MS = 220;

type WifiScanModalProps = {
  open: boolean;
  entries: readonly WifiScanEntry[];
  scanning: boolean;
  onClose: () => void;
  onRefresh: () => void;
  onSelect: (ssid: string) => void;
};

export default function WifiScanModal(props: WifiScanModalProps) {
  const presence = createPresence(() => props.open, EXIT_MS);

  const onKeyDown = (event: KeyboardEvent) => {
    if (event.key === "Escape" && props.open) {
      props.onClose();
    }
  };

  document.addEventListener("keydown", onKeyDown);
  onCleanup(() => document.removeEventListener("keydown", onKeyDown));

  return (
    <Show when={presence.mounted()}>
      <div
        class={cx(styles.overlay, presence.exiting() && styles.leaving)}
        onClick={() => props.onClose()}
        role="presentation"
      >
        <div
          class={cx(styles.modal, presence.exiting() && styles.leaving)}
          onClick={(event) => event.stopPropagation()}
          role="dialog"
          aria-modal="true"
          aria-label="选择附近的 WiFi"
        >
          <header class={styles.modalHeader}>
            <div>
              <p class={styles.eyebrow}>Nearby Networks</p>
              <h2>选择附近的 WiFi</h2>
              <p class={styles.subtitle}>点击后会把 SSID 填入当前配置，并自动聚焦密码输入框</p>
            </div>
            <div class={styles.headerActions}>
              <button
                class={styles.iconBtn}
                onClick={() => props.onRefresh()}
                aria-label="扫描附近的 WiFi"
                disabled={props.scanning}
              >
                <RefreshCw size={18} class={cx(props.scanning && styles.spinning)} />
              </button>
              <button class={styles.iconBtn} onClick={() => props.onClose()} aria-label="关闭">
                <X size={18} />
              </button>
            </div>
          </header>

          <Show
            when={props.entries.length > 0 || props.scanning}
            fallback={
              <div class={styles.emptyState}>
                <Wifi size={24} />
                <p>还没有扫描结果，点击右上角扫描按钮开始搜索附近的 WiFi</p>
              </div>
            }
          >
            <div class={styles.scanList}>
              <For each={props.entries}>
                {(network) => (
                  <button class={styles.scanItem} onClick={() => props.onSelect(network.ssid)}>
                    <div class={styles.scanMain}>
                      <div class={styles.signalBadge}>
                        <Signal size={14} />
                        <span>{network.rssi} dBm</span>
                      </div>
                      <div class={styles.ssidStack}>
                        <strong>{network.ssid}</strong>
                        <span>
                          {network.authmode} · CH{network.channel}
                        </span>
                      </div>
                    </div>
                    <span class={styles.quality}>{signalLevel(network.rssi)}</span>
                  </button>
                )}
              </For>
            </div>
          </Show>
        </div>
      </div>
    </Show>
  );
}
