import { createSignal, For, onCleanup, onMount, Show } from "solid-js";
import { createStore, produce } from "solid-js/store";
import {
  GripVertical,
  LockKeyhole,
  Plus,
  Radio,
  Save,
  Trash2,
  Wifi as WifiIcon,
} from "lucide-solid";
import WifiScanModal from "@/components/WifiScanModal";
import { showConfirm, showToast } from "@/lib/feedback";
import type { RouteMeta } from "@/lib/route-manifest";
import {
  fetchWifiConfigs,
  normalizeConfig,
  saveWifiConfigs,
  scanWifiNetworks,
  type WifiConfig,
  type WifiScanEntry,
} from "@/lib/wifi";
import { cx } from "@/lib/cx";
import styles from "./wifi.module.css";

export const routeMeta: RouteMeta = {
  label: "WiFi",
  icon: WifiIcon,
  order: 3,
};

export default function WifiPage() {
  // 用 store 就地更新单个配置项，输入时不会重建 DOM，也就不会丢焦点
  const [configs, setConfigs] = createStore<WifiConfig[]>([]);
  const [scanResults, setScanResults] = createSignal<WifiScanEntry[]>([]);
  const [loading, setLoading] = createSignal(true);
  const [saving, setSaving] = createSignal(false);
  const [scanning, setScanning] = createSignal(false);
  const [editingId, setEditingId] = createSignal<string | null>(null);
  const [draggedId, setDraggedId] = createSignal<string | null>(null);
  const [scanModalOpen, setScanModalOpen] = createSignal(false);
  const [scanTargetId, setScanTargetId] = createSignal<string | null>(null);

  const passwordInputs = new Map<string, HTMLInputElement>();

  async function loadConfigs() {
    setLoading(true);
    try {
      setConfigs(await fetchWifiConfigs());
    } catch (error) {
      console.error(error);
      showToast("加载 WiFi 配置失败", "error");
    } finally {
      setLoading(false);
    }
  }

  async function loadScanResults(force = false) {
    if (scanning() || (!force && scanResults().length > 0)) {
      return;
    }

    setScanning(true);
    try {
      setScanResults(await scanWifiNetworks());
    } catch (error) {
      console.error(error);
      showToast("扫描 WiFi 失败", "error");
    } finally {
      setScanning(false);
    }
  }

  function toggleEditConfig(id: string) {
    setEditingId(editingId() === id ? null : id);
  }

  function addConfig() {
    const next = normalizeConfig({ ssid: "", password: "" });
    setConfigs(
      produce((list) => {
        list.push(next);
      }),
    );
    setEditingId(next.id);
  }

  async function removeConfig(id: string) {
    const target = configs.find((item) => item.id === id);
    if (!target) {
      return;
    }

    const confirmed = await showConfirm(`删除 ${target.ssid || "未命名网络"}？`, "删除配置", true);
    if (confirmed !== "ok") {
      return;
    }

    setConfigs(
      produce((list) => {
        const index = list.findIndex((item) => item.id === id);
        if (index >= 0) {
          list.splice(index, 1);
        }
      }),
    );

    if (editingId() === id) {
      setEditingId(null);
    }
  }

  function updateConfig(id: string, field: "ssid" | "password", value: string) {
    const index = configs.findIndex((item) => item.id === id);
    if (index >= 0) {
      setConfigs(index, field, value);
    }
  }

  function moveConfig(fromId: string, toId: string) {
    if (fromId === toId) {
      return;
    }

    setConfigs(
      produce((list) => {
        const fromIndex = list.findIndex((item) => item.id === fromId);
        const toIndex = list.findIndex((item) => item.id === toId);
        if (fromIndex < 0 || toIndex < 0) {
          return;
        }
        const [moved] = list.splice(fromIndex, 1);
        if (moved) {
          list.splice(toIndex, 0, moved);
        }
      }),
    );
  }

  function applyScannedSsid(id: string, ssid: string) {
    updateConfig(id, "ssid", ssid);
    closeScanModal();
    passwordInputs.get(id)?.focus();
  }

  function openScanModal(id: string) {
    setScanTargetId(id);
    setScanModalOpen(true);
  }

  function closeScanModal() {
    setScanModalOpen(false);
    setScanTargetId(null);
  }

  async function saveConfigs() {
    const invalid = configs.find((item) => item.ssid.trim().length === 0);
    if (invalid) {
      showToast("SSID 不能为空", "warning");
      setEditingId(invalid.id);
      return;
    }

    setSaving(true);
    try {
      await saveWifiConfigs(configs);
      showToast("WiFi 配置已保存", "success");
      setEditingId(null);
      closeScanModal();
      await loadConfigs();
    } catch (error) {
      console.error(error);
      showToast("保存 WiFi 配置失败", "error");
    } finally {
      setSaving(false);
    }
  }

  onMount(() => {
    void loadConfigs();
  });

  return (
    <div class={cx("page", styles.wifiPage)}>
      <div class="page-header">
        <h1>WiFi 配置</h1>
        <p class="subtitle">管理连接顺序，编辑账号密码，并在编辑时直接选择扫描到的网络</p>
      </div>

      <section class={styles.toolbar}>
        <div class={styles.summaryPill}>已保存 {configs.length} 项</div>
        <div class={styles.toolbarActions}>
          <button class={styles.primaryBtn} onClick={addConfig}>
            <Plus size={16} />
            <span>新增</span>
          </button>
          <button
            class={styles.saveBtn}
            onClick={() => void saveConfigs()}
            disabled={saving() || loading()}
          >
            <Save size={16} />
            <span>{saving() ? "保存中" : "保存"}</span>
          </button>
        </div>
      </section>

      <Show
        when={!loading()}
        fallback={
          <section class={styles.emptyCard}>
            <p>正在加载 WiFi 配置…</p>
          </section>
        }
      >
        <Show
          when={configs.length > 0}
          fallback={
            <section class={styles.emptyCard}>
              <WifiIcon size={26} />
              <p>还没有任何 WiFi 配置，先新增一项。</p>
            </section>
          }
        >
          <section class={styles.configList}>
            <For each={configs}>
              {(item, index) => (
                <article
                  class={cx(styles.configCard, editingId() === item.id && styles.editing)}
                  draggable="true"
                  onDragStart={() => setDraggedId(item.id)}
                  onDragOver={(event) => event.preventDefault()}
                  onDrop={(event) => {
                    event.preventDefault();
                    const dragged = draggedId();
                    if (dragged) {
                      moveConfig(dragged, item.id);
                    }
                    setDraggedId(null);
                  }}
                  onDragEnd={() => setDraggedId(null)}
                >
                  <div class={styles.cardTop}>
                    <div class={styles.orderBlock}>
                      <span class={styles.dragHandle}>
                        <GripVertical size={18} />
                      </span>
                      <div>
                        <p class={styles.orderLabel}>优先级 {index() + 1}</p>
                        <h3>{item.ssid || "未填写 SSID"}</h3>
                      </div>
                    </div>
                    <div class={styles.cardActions}>
                      <button class={styles.chipBtn} onClick={() => toggleEditConfig(item.id)}>
                        {editingId() === item.id ? "收起" : "编辑"}
                      </button>
                      <button
                        class={cx(styles.chipBtn, styles.danger)}
                        onClick={() => void removeConfig(item.id)}
                      >
                        <Trash2 size={14} />
                        <span>删除</span>
                      </button>
                    </div>
                  </div>

                  <Show
                    when={editingId() === item.id}
                    fallback={
                      <p class={styles.passwordPreview}>
                        密码：
                        {item.password
                          ? "•".repeat(Math.min(item.password.length, 12))
                          : "开放网络 / 未填写"}
                      </p>
                    }
                  >
                    <div class={styles.editorGrid}>
                      <label class={styles.field}>
                        <span>SSID</span>
                        <div class={styles.ssidRow}>
                          <input
                            type="text"
                            value={item.ssid}
                            placeholder="输入网络名称"
                            autocomplete="off"
                            autocapitalize="off"
                            autocorrect="off"
                            spellcheck={false}
                            onInput={(event) =>
                              updateConfig(item.id, "ssid", event.currentTarget.value)
                            }
                          />
                          <button
                            class={styles.ssidPickerBtn}
                            onClick={() => openScanModal(item.id)}
                          >
                            <Radio size={16} />
                            <span>附近 WiFi</span>
                          </button>
                        </div>
                      </label>

                      <label class={styles.field}>
                        <span>密码</span>
                        <div class={styles.passwordWrap}>
                          <LockKeyhole size={16} />
                          <input
                            ref={(element) => {
                              passwordInputs.set(item.id, element);
                              onCleanup(() => passwordInputs.delete(item.id));
                            }}
                            type="password"
                            value={item.password}
                            placeholder="输入密码，可为空"
                            autocomplete="new-password"
                            autocapitalize="off"
                            autocorrect="off"
                            spellcheck={false}
                            onInput={(event) =>
                              updateConfig(item.id, "password", event.currentTarget.value)
                            }
                          />
                        </div>
                      </label>
                    </div>
                  </Show>
                </article>
              )}
            </For>
          </section>
        </Show>
      </Show>

      <WifiScanModal
        open={scanModalOpen()}
        entries={scanResults()}
        scanning={scanning()}
        onClose={closeScanModal}
        onRefresh={() => void loadScanResults(true)}
        onSelect={(ssid) => {
          const target = scanTargetId();
          if (target) {
            applyScannedSsid(target, ssid);
          }
        }}
      />
    </div>
  );
}
