import { createSignal, For, onCleanup, onMount, Show } from "solid-js";
import { createStore, produce, reconcile } from "solid-js/store";
import {
  GripVertical,
  LockKeyhole,
  Plus,
  Radio,
  RefreshCw,
  Save,
  Trash2,
  Wifi as WifiIcon,
} from "lucide-solid";
import Button from "@/components/Button";
import Card from "@/components/Card";
import InfoRow from "@/components/InfoRow";
import WifiScanModal from "@/components/WifiScanModal";
import { describeError } from "@/lib/api";
import { refreshSystemInfo, systemInfo } from "@/lib/device";
import { showConfirm, showToast } from "@/lib/feedback";
import { signalLevel } from "@/lib/format";
import {
  applyWifiConfig,
  createEmptyConfig,
  fetchWifiConfigs,
  saveWifiConfigs,
  scanWifiNetworks,
  validateConfig,
  type WifiConfigEntry,
  type WifiScanEntry,
} from "@/lib/wifi";
import type { RouteMeta } from "@/lib/route-manifest";
import { cx } from "@/lib/cx";
import styles from "./wifi.module.css";

export const routeMeta: RouteMeta = {
  label: "WiFi",
  icon: WifiIcon,
  order: 2,
};

export default function WifiPage() {
  // 用 store 就地更新单个配置项，输入时不会重建 DOM，也就不会丢焦点
  const [configs, setConfigs] = createStore<WifiConfigEntry[]>([]);
  const [maxItems, setMaxItems] = createSignal(8);
  const [scanResults, setScanResults] = createSignal<WifiScanEntry[]>([]);
  const [loading, setLoading] = createSignal(true);
  const [saving, setSaving] = createSignal(false);
  const [connecting, setConnecting] = createSignal(false);
  const [scanning, setScanning] = createSignal(false);
  const [editingId, setEditingId] = createSignal<string | null>(null);
  const [draggedId, setDraggedId] = createSignal<string | null>(null);
  const [scanTargetId, setScanTargetId] = createSignal<string | null>(null);

  const passwordInputs = new Map<string, HTMLInputElement>();
  const wifi = () => systemInfo()?.wifi;

  async function load() {
    setLoading(true);
    try {
      const list = await fetchWifiConfigs();
      setConfigs(reconcile(list.items as WifiConfigEntry[]));
      setMaxItems(list.maxItems);
    } catch (error) {
      showToast(describeError(error), "error");
    } finally {
      setLoading(false);
    }
  }

  async function scan(force = false) {
    if (scanning()) {
      return;
    }
    setScanning(true);
    try {
      setScanResults(await scanWifiNetworks(force));
    } catch (error) {
      showToast(describeError(error), "error");
    } finally {
      setScanning(false);
    }
  }

  function addConfig() {
    if (configs.length >= maxItems()) {
      showToast(`最多只能保存 ${maxItems()} 组配置`, "warning");
      return;
    }

    const next = createEmptyConfig();
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
    if ((await showConfirm(`删除 ${target.ssid || "未命名网络"}？`, "删除配置", true)) !== "ok") {
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

  function updateEntry<K extends keyof WifiConfigEntry>(
    id: string,
    field: K,
    value: WifiConfigEntry[K],
  ) {
    const index = configs.findIndex((item) => item.id === id);
    if (index >= 0) {
      setConfigs(index, field, value);
    }
  }

  /** 切换"修改密码"：关闭时回到 null，也就是沿用设备上已保存的那一份。 */
  function togglePasswordEdit(entry: WifiConfigEntry) {
    updateEntry(entry.id, "password", entry.password === null ? "" : null);
    if (entry.password === null) {
      queueMicrotask(() => passwordInputs.get(entry.id)?.focus());
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

  async function save() {
    for (const entry of configs) {
      const problem = validateConfig(entry);
      if (problem) {
        showToast(problem, "warning");
        setEditingId(entry.id);
        return;
      }
    }

    setSaving(true);
    try {
      const list = await saveWifiConfigs(configs);
      setConfigs(reconcile(list.items as WifiConfigEntry[]));
      setMaxItems(list.maxItems);
      setEditingId(null);
      showToast("WiFi 配置已保存", "success");
    } catch (error) {
      showToast(describeError(error), "error");
    } finally {
      setSaving(false);
    }
  }

  async function connect() {
    const confirmed = await showConfirm(
      "设备会用已保存的配置去连接路由器，配网热点将关闭，本页面可能因此失联。",
      "立即连接",
    );
    if (confirmed !== "ok") {
      return;
    }

    setConnecting(true);
    try {
      await applyWifiConfig();
      showToast("设备正在连接…", "info");
      void refreshSystemInfo();
    } catch (error) {
      showToast(describeError(error), "error");
    } finally {
      setConnecting(false);
    }
  }

  onMount(() => void load());

  return (
    <div class="page">
      <div class="page-header">
        <h1>WiFi 配置</h1>
        <p class="subtitle">按列表顺序依次尝试连接</p>
      </div>

      <Card icon={WifiIcon} title="当前连接">
        <Show
          when={wifi()?.connected}
          fallback={
            <InfoRow
              label="状态"
              value={wifi()?.connecting ? "正在连接…" : wifi()?.ap_active ? "配网模式" : "未连接"}
              tone="muted"
            />
          }
        >
          <InfoRow label="SSID" value={wifi()?.ssid} tone="accent" />
          <InfoRow
            label="信号"
            value={`${wifi()?.rssi} dBm · ${signalLevel(wifi()?.rssi ?? -100)}`}
          />
          <InfoRow label="IP 地址" value={wifi()?.ip} mono />
        </Show>
        <Button
          variant="secondary"
          icon={WifiIcon}
          block
          loading={connecting()}
          disabled={configs.length === 0}
          onClick={() => void connect()}
        >
          用已保存的配置连接
        </Button>
      </Card>

      <Card
        title="已保存的网络"
        subtitle={`${configs.length} / ${maxItems()}`}
        actions={
          <>
            <Button variant="ghost" size="sm" icon={Plus} onClick={addConfig}>
              新增
            </Button>
            <Button
              variant="primary"
              size="sm"
              icon={Save}
              loading={saving()}
              disabled={loading()}
              onClick={() => void save()}
            >
              保存
            </Button>
          </>
        }
      >
        <Show when={!loading()} fallback={<p class={styles.placeholder}>正在加载 WiFi 配置…</p>}>
          <Show
            when={configs.length > 0}
            fallback={
              <div class={styles.emptyState}>
                <WifiIcon size={24} />
                <p>还没有任何 WiFi 配置，点击「新增」开始。</p>
              </div>
            }
          >
            <div class={styles.configList}>
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
                        <Button
                          variant="ghost"
                          size="sm"
                          onClick={() => setEditingId(editingId() === item.id ? null : item.id)}
                        >
                          {editingId() === item.id ? "收起" : "编辑"}
                        </Button>
                        <Button
                          variant="ghost"
                          size="sm"
                          icon={Trash2}
                          onClick={() => void removeConfig(item.id)}
                          aria-label="删除配置"
                        />
                      </div>
                    </div>

                    <Show
                      when={editingId() === item.id}
                      fallback={
                        <p class={styles.passwordPreview}>
                          {item.hasPassword ? "已保存密码" : "开放网络 / 无密码"}
                          <Show when={item.password !== null}> · 密码将被更新</Show>
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
                                updateEntry(item.id, "ssid", event.currentTarget.value)
                              }
                            />
                            <Button
                              variant="secondary"
                              size="sm"
                              icon={Radio}
                              onClick={() => {
                                setScanTargetId(item.id);
                                void scan(scanResults().length === 0);
                              }}
                            >
                              附近
                            </Button>
                          </div>
                        </label>

                        <div class={styles.field}>
                          <span>密码</span>
                          <Show
                            when={item.password !== null}
                            fallback={
                              <button
                                class={styles.keepPassword}
                                onClick={() => togglePasswordEdit(item)}
                              >
                                <LockKeyhole size={15} />
                                <span>
                                  {item.hasPassword ? "沿用已保存的密码" : "未设置密码"}，点击修改
                                </span>
                              </button>
                            }
                          >
                            <div class={styles.passwordWrap}>
                              <LockKeyhole size={16} />
                              <input
                                ref={(element) => {
                                  passwordInputs.set(item.id, element);
                                  onCleanup(() => passwordInputs.delete(item.id));
                                }}
                                type="password"
                                value={item.password ?? ""}
                                placeholder="留空表示开放网络"
                                autocomplete="new-password"
                                autocapitalize="off"
                                autocorrect="off"
                                spellcheck={false}
                                onInput={(event) =>
                                  updateEntry(item.id, "password", event.currentTarget.value)
                                }
                              />
                              <Show when={item.hasPassword}>
                                <button
                                  class={styles.cancelEdit}
                                  onClick={() => togglePasswordEdit(item)}
                                  aria-label="放弃修改密码"
                                >
                                  撤销
                                </button>
                              </Show>
                            </div>
                          </Show>
                        </div>
                      </div>
                    </Show>
                  </article>
                )}
              </For>
            </div>
          </Show>
        </Show>
      </Card>

      <WifiScanModal
        open={scanTargetId() !== null}
        entries={scanResults()}
        scanning={scanning()}
        onClose={() => setScanTargetId(null)}
        onRefresh={() => void scan(true)}
        onSelect={(ssid) => {
          const target = scanTargetId();
          if (target) {
            updateEntry(target, "ssid", ssid);
            setScanTargetId(null);
            passwordInputs.get(target)?.focus();
          }
        }}
      />

      <Show when={scanning() && scanTargetId() === null}>
        <p class={styles.placeholder}>
          <RefreshCw size={14} /> 正在扫描…
        </p>
      </Show>
    </div>
  );
}
