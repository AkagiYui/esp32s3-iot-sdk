<script module lang="ts">
  import type { RouteMeta } from "@/lib/route-manifest";
  import { Wifi as WifiIcon } from "lucide-svelte";

  export const routeMeta: RouteMeta = {
    label: "WiFi",
    icon: WifiIcon,
    order: 3,
  };
</script>

<script lang="ts">
  import WifiScanModal from "@/components/WifiScanModal.svelte";
  import { showConfirm, showToast } from "@/lib/feedback.svelte";
  import {
    GripVertical,
    LockKeyhole,
    Plus,
    Radio,
    RefreshCw,
    Save,
    Trash2,
    Wifi,
  } from "lucide-svelte";

  type WifiConfig = {
    id: string;
    ssid: string;
    password: string;
  };

  type WifiScanEntry = {
    ssid: string;
    rssi: number;
    authmode: string;
  };

  let configs = $state<WifiConfig[]>([]);
  let scanResults = $state<WifiScanEntry[]>([]);
  let loading = $state(true);
  let saving = $state(false);
  let scanning = $state(false);
  let editingId = $state<string | null>(null);
  let draggedId = $state<string | null>(null);
  let scanModalOpen = $state(false);
  let scanTargetId = $state<string | null>(null);

  function createId() {
    return `${Date.now()}-${Math.random().toString(36).slice(2, 8)}`;
  }

  function normalizeConfig(
    item: Partial<WifiConfig> & { ssid?: string; password?: string },
  ) {
    return {
      id: item.id ?? createId(),
      ssid: item.ssid ?? "",
      password: item.password ?? "",
    } satisfies WifiConfig;
  }

  async function readJson<T>(url: string, init?: RequestInit): Promise<T> {
    const response = await fetch(url, init);
    if (!response.ok) {
      const message = await response.text();
      throw new Error(message || `Request failed: ${response.status}`);
    }
    return response.json() as Promise<T>;
  }

  async function loadConfigs() {
    loading = true;
    try {
      const payload =
        await readJson<Array<{ ssid: string; password: string }>>(
          "/api/wifi-config",
        );
      configs = payload.map((item) => normalizeConfig(item));
    } catch (error) {
      console.error(error);
      showToast("加载 WiFi 配置失败", "error");
    } finally {
      loading = false;
    }
  }

  async function loadScanResults(force = false) {
    if (scanning || (!force && scanResults.length > 0)) {
      return;
    }

    scanning = true;
    try {
      const payload = await readJson<WifiScanEntry[]>("/api/wifi-scan");
      scanResults = [...payload].sort((left, right) => right.rssi - left.rssi);
    } catch (error) {
      console.error(error);
      showToast("扫描 WiFi 失败", "error");
    } finally {
      scanning = false;
    }
  }

  function toggleEditConfig(id: string) {
    if (editingId === id) {
      editingId = null;
      return;
    }

    editingId = id;
  }

  function stopEdit() {
    editingId = null;
  }

  function addConfig() {
    const next = normalizeConfig({ ssid: "", password: "" });
    configs = [...configs, next];
    editingId = next.id;
  }

  async function removeConfig(id: string) {
    const target = configs.find((item) => item.id === id);
    if (!target) {
      return;
    }

    const confirmed = await showConfirm(
      `删除 ${target.ssid || "未命名网络"}？`,
      "删除配置",
      true,
    );
    if (confirmed !== "ok") {
      return;
    }

    configs = configs.filter((item) => item.id !== id);
    if (editingId === id) {
      editingId = null;
    }
  }

  function updateConfig(id: string, field: "ssid" | "password", value: string) {
    configs = configs.map((item) =>
      item.id === id ? { ...item, [field]: value } : item,
    );
  }

  function moveConfig(fromId: string, toId: string) {
    if (fromId === toId) {
      return;
    }

    const next = [...configs];
    const fromIndex = next.findIndex((item) => item.id === fromId);
    const toIndex = next.findIndex((item) => item.id === toId);
    if (fromIndex < 0 || toIndex < 0) {
      return;
    }

    const [moved] = next.splice(fromIndex, 1);
    next.splice(toIndex, 0, moved);
    configs = next;
  }

  function applyScannedSsid(id: string, ssid: string) {
    updateConfig(id, "ssid", ssid);
    scanModalOpen = false;
    scanTargetId = null;
    queueMicrotask(() => {
      const passwordInput = document.querySelector<HTMLInputElement>(
        `[data-password-for="${id}"]`,
      );
      passwordInput?.focus();
    });
  }

  function openScanModal(id: string) {
    scanTargetId = id;
    scanModalOpen = true;
  }

  function closeScanModal() {
    scanModalOpen = false;
    scanTargetId = null;
  }

  async function saveConfigs() {
    const invalid = configs.find((item) => item.ssid.trim().length === 0);
    if (invalid) {
      showToast("SSID 不能为空", "warning");
      editingId = invalid.id;
      return;
    }

    saving = true;
    try {
      await readJson<{ message: string }>("/api/wifi-config", {
        method: "PUT",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify(
          configs.map((item) => ({
            ssid: item.ssid.trim(),
            password: item.password,
          })),
        ),
      });

      showToast("WiFi 配置已保存", "success");
      stopEdit();
      closeScanModal();
      await loadConfigs();
    } catch (error) {
      console.error(error);
      showToast("保存 WiFi 配置失败", "error");
    } finally {
      saving = false;
    }
  }

  $effect(() => {
    void loadConfigs();
  });

  function signalLevel(rssi: number) {
    if (rssi >= -55) return "极佳";
    if (rssi >= -67) return "良好";
    if (rssi >= -75) return "一般";
    return "较弱";
  }

  function spinningStyle(active: boolean) {
    return active ? "animation: spin 0.9s linear infinite;" : undefined;
  }
</script>

<div class="page wifi-page">
  <div class="page-header">
    <h1>WiFi 配置</h1>
    <p class="subtitle">
      管理连接顺序，编辑账号密码，并在编辑时直接选择扫描到的网络
    </p>
  </div>

  <section class="toolbar">
    <div class="summary-pill">已保存 {configs.length} 项</div>
    <div class="toolbar-actions">
      <button class="primary-btn" onclick={addConfig}>
        <Plus size={16} />
        <span>新增</span>
      </button>
      <button
        class="save-btn"
        onclick={saveConfigs}
        disabled={saving || loading}
      >
        <Save size={16} />
        <span>{saving ? "保存中" : "保存"}</span>
      </button>
    </div>
  </section>

  {#if loading}
    <section class="empty-card">
      <p>正在加载 WiFi 配置…</p>
    </section>
  {:else if configs.length === 0}
    <section class="empty-card">
      <Wifi size={26} />
      <p>还没有任何 WiFi 配置，先新增一项。</p>
    </section>
  {:else}
    <section class="config-list">
      {#each configs as item, index (item.id)}
        <article
          class="config-card"
          class:editing={editingId === item.id}
          draggable="true"
          ondragstart={() => {
            draggedId = item.id;
          }}
          ondragover={(event) => {
            event.preventDefault();
          }}
          ondrop={(event) => {
            event.preventDefault();
            if (draggedId) {
              moveConfig(draggedId, item.id);
            }
            draggedId = null;
          }}
          ondragend={() => {
            draggedId = null;
          }}
        >
          <div class="card-top">
            <div class="order-block">
              <span class="drag-handle"><GripVertical size={18} /></span>
              <div>
                <p class="order-label">优先级 {index + 1}</p>
                <h3>{item.ssid || "未填写 SSID"}</h3>
              </div>
            </div>
            <div class="card-actions">
              <button
                class="chip-btn"
                onclick={() => toggleEditConfig(item.id)}
              >
                {editingId === item.id ? "收起" : "编辑"}
              </button>
              <button
                class="chip-btn danger"
                onclick={() => removeConfig(item.id)}
              >
                <Trash2 size={14} />
                <span>删除</span>
              </button>
            </div>
          </div>

          {#if editingId === item.id}
            <div class="editor-grid">
              <label class="field">
                <span>SSID</span>
                <div class="ssid-row">
                  <input
                    type="text"
                    value={item.ssid}
                    placeholder="输入网络名称"
                    autocomplete="off"
                    autocapitalize="off"
                    autocorrect="off"
                    spellcheck="false"
                    oninput={(event) =>
                      updateConfig(
                        item.id,
                        "ssid",
                        (event.currentTarget as HTMLInputElement).value,
                      )}
                  />
                  <button
                    class="ssid-picker-btn"
                    onclick={() => openScanModal(item.id)}
                  >
                    <Radio size={16} />
                    <span>附近 WiFi</span>
                  </button>
                </div>
              </label>

              <label class="field">
                <span>密码</span>
                <div class="password-wrap">
                  <LockKeyhole size={16} />
                  <input
                    data-password-for={item.id}
                    type="password"
                    value={item.password}
                    placeholder="输入密码，可为空"
                    autocomplete="new-password"
                    autocapitalize="off"
                    autocorrect="off"
                    spellcheck="false"
                    oninput={(event) =>
                      updateConfig(
                        item.id,
                        "password",
                        (event.currentTarget as HTMLInputElement).value,
                      )}
                  />
                </div>
              </label>
            </div>
          {:else}
            <p class="password-preview">
              密码：{item.password
                ? "•".repeat(Math.min(item.password.length, 12))
                : "开放网络 / 未填写"}
            </p>
          {/if}
        </article>
      {/each}
    </section>
  {/if}

  <WifiScanModal
    open={scanModalOpen}
    entries={scanResults}
    {scanning}
    onClose={closeScanModal}
    onRefresh={() => {
      void loadScanResults(true);
    }}
    onSelect={(ssid) => {
      if (scanTargetId) {
        applyScannedSsid(scanTargetId, ssid);
      }
    }}
    {signalLevel}
    {spinningStyle}
  />
</div>

<style>
  .wifi-page {
    gap: 18px;
  }

  .toolbar-actions,
  .toolbar,
  .card-top,
  .card-actions,
  .ssid-row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 10px;
  }

  .toolbar {
    align-items: stretch;
  }

  .toolbar-actions {
    justify-content: flex-end;
  }

  .summary-pill {
    display: inline-flex;
    align-items: center;
    padding: 0 14px;
    min-height: 44px;
    border-radius: 999px;
    background: rgba(43, 122, 120, 0.1);
    color: var(--accent-strong);
    font-weight: 700;
  }

  .primary-btn,
  .save-btn,
  .chip-btn {
    border: none;
    cursor: pointer;
    -webkit-tap-highlight-color: transparent;
  }

  .primary-btn,
  .save-btn {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    gap: 8px;
    min-height: 44px;
    padding: 0 14px;
    border-radius: 14px;
    font-weight: 600;
  }

  .primary-btn,
  .save-btn {
    background: var(--accent);
    color: white;
    box-shadow: var(--shadow-soft);
  }

  .config-list {
    display: flex;
    flex-direction: column;
    gap: 14px;
  }

  .config-card,
  .empty-card {
    padding: 18px;
    border: 1px solid var(--border);
    border-radius: 22px;
    background: var(--card-bg);
    box-shadow: var(--shadow-soft);
  }

  .config-card.editing {
    border-color: rgba(43, 122, 120, 0.32);
    box-shadow: 0 20px 44px rgba(43, 122, 120, 0.12);
  }

  .order-block {
    display: flex;
    align-items: center;
    gap: 12px;
    min-width: 0;
  }

  .drag-handle {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    width: 38px;
    height: 38px;
    border-radius: 12px;
    background: rgba(43, 122, 120, 0.1);
    color: var(--accent);
    flex-shrink: 0;
  }

  .order-label {
    margin-bottom: 4px;
    font-size: 12px;
    letter-spacing: 0.08em;
    text-transform: uppercase;
    color: var(--muted);
  }

  .order-block h3 {
    font-size: 19px;
    color: var(--text-h);
    word-break: break-all;
  }

  .chip-btn {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    min-height: 34px;
    padding: 0 12px;
    border-radius: 999px;
    background: rgba(43, 122, 120, 0.08);
    color: var(--accent-strong);
    font-size: 13px;
    font-weight: 700;
  }

  .chip-btn.danger {
    background: rgba(199, 77, 60, 0.12);
    color: var(--danger);
  }

  .password-preview {
    margin-top: 14px;
    color: var(--muted);
    font-size: 14px;
  }

  .editor-grid {
    display: grid;
    gap: 12px;
    margin-top: 16px;
  }

  .field {
    display: grid;
    gap: 8px;
  }

  .ssid-row {
    align-items: stretch;
  }

  .field span {
    font-size: 13px;
    font-weight: 700;
    color: var(--text-h);
  }

  .field input,
  .password-wrap {
    width: 100%;
    min-height: 46px;
    border: 1px solid var(--border);
    border-radius: 14px;
    background: rgba(255, 255, 255, 0.64);
  }

  .field input {
    padding: 0 14px;
    color: var(--text-h);
    outline: none;
  }

  .field input:focus,
  .password-wrap:focus-within {
    border-color: rgba(43, 122, 120, 0.42);
    box-shadow: 0 0 0 4px rgba(43, 122, 120, 0.12);
  }

  .password-wrap {
    display: flex;
    align-items: center;
    gap: 8px;
    padding: 0 14px;
    color: var(--muted);
  }

  .password-wrap input {
    min-height: 44px;
    border: none;
    background: transparent;
    padding: 0;
    box-shadow: none;
  }

  .ssid-picker-btn {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    gap: 8px;
    min-height: 46px;
    padding: 0 14px;
    border: 1px solid var(--border);
    border-radius: 14px;
    background: rgba(43, 122, 120, 0.08);
    color: var(--accent-strong);
    font-weight: 700;
    white-space: nowrap;
    cursor: pointer;
    -webkit-tap-highlight-color: transparent;
  }

  .empty-card {
    display: grid;
    justify-items: center;
    gap: 10px;
    color: var(--muted);
    text-align: center;
    padding: 32px 18px;
  }

  @keyframes spin {
    from {
      transform: rotate(0deg);
    }
    to {
      transform: rotate(360deg);
    }
  }

  @media (min-width: 900px) {
    .editor-grid {
      grid-template-columns: 1fr 1fr;
    }
  }
</style>
