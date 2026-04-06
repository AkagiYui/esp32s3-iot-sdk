<script lang="ts">
  import { fade, scale } from "svelte/transition";
  import { cubicOut } from "svelte/easing";
  import { RefreshCw, Signal, Wifi, X } from "lucide-svelte";

  export type WifiScanEntry = {
    ssid: string;
    rssi: number;
    authmode: string;
  };

  let {
    open,
    entries,
    scanning,
    onClose,
    onRefresh,
    onSelect,
    signalLevel,
    spinningStyle,
  }: {
    open: boolean;
    entries: WifiScanEntry[];
    scanning: boolean;
    onClose: () => void;
    onRefresh: () => void;
    onSelect: (ssid: string) => void;
    signalLevel: (rssi: number) => string;
    spinningStyle: (active: boolean) => string | undefined;
  } = $props();
</script>

{#if open}
  <!-- svelte-ignore a11y_no_static_element_interactions -->
  <div
    class="overlay"
    transition:fade={{ duration: 180 }}
    onclick={onClose}
    onkeydown={(event) => {
      if (event.key === "Escape") {
        onClose();
      }
    }}
  >
    <!-- svelte-ignore a11y_click_events_have_key_events -->
    <!-- svelte-ignore a11y_interactive_supports_focus -->
    <div
      class="modal"
      transition:scale={{ start: 0.9, duration: 220, easing: cubicOut }}
      onclick={(event) => event.stopPropagation()}
      role="dialog"
      aria-modal="true"
      aria-label="选择附近的 WiFi"
    >
      <header class="modal-header">
        <div>
          <p class="eyebrow">Nearby Networks</p>
          <h2>选择附近的 WiFi</h2>
          <p class="subtitle">
            点击后会把 SSID 填入当前配置，并自动聚焦密码输入框
          </p>
        </div>
        <div class="header-actions">
          <button
            class="icon-btn"
            onclick={onRefresh}
            aria-label="扫描附近的 WiFi"
            disabled={scanning}
          >
            <RefreshCw size={18} style={spinningStyle(scanning)} />
          </button>
          <button class="icon-btn" onclick={onClose} aria-label="关闭">
            <X size={18} />
          </button>
        </div>
      </header>

      {#if entries.length === 0 && !scanning}
        <div class="empty-state">
          <Wifi size={24} />
          <p>还没有扫描结果，点击右上角扫描按钮开始搜索附近的 WiFi</p>
        </div>
      {:else}
        <div class="scan-list">
          {#each entries as network}
            <button class="scan-item" onclick={() => onSelect(network.ssid)}>
              <div class="scan-main">
                <div class="signal-badge">
                  <Signal size={14} />
                  <span>{network.rssi} dBm</span>
                </div>
                <div class="ssid-stack">
                  <strong>{network.ssid}</strong>
                  <span>{network.authmode}</span>
                </div>
              </div>
              <span class="quality">{signalLevel(network.rssi)}</span>
            </button>
          {/each}
        </div>
      {/if}
    </div>
  </div>
{/if}

<style>
  .overlay {
    position: fixed;
    inset: 0;
    z-index: 9100;
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 20px;
    background: rgba(16, 20, 18, 0.34);
    -webkit-backdrop-filter: blur(6px);
    backdrop-filter: blur(6px);
  }

  .modal {
    width: min(100%, 560px);
    max-height: min(80dvh, 720px);
    overflow: auto;
    padding: 20px;
    border: 1px solid var(--border);
    border-radius: 24px;
    background: var(--surface-strong);
    box-shadow: var(--shadow);
  }

  .modal-header,
  .scan-main {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
  }

  .header-actions {
    display: flex;
    align-items: center;
    gap: 8px;
    flex-shrink: 0;
  }

  .eyebrow {
    margin: 0 0 8px;
    font-size: 11px;
    letter-spacing: 0.18em;
    text-transform: uppercase;
    color: var(--accent);
  }

  .modal-header h2 {
    margin: 0 0 6px;
    font-size: 24px;
    color: var(--text-h);
  }

  .subtitle {
    color: var(--muted);
  }

  .icon-btn,
  .scan-item {
    border: none;
    cursor: pointer;
    -webkit-tap-highlight-color: transparent;
  }

  .icon-btn {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    width: 40px;
    height: 40px;
    border-radius: 12px;
    background: rgba(43, 122, 120, 0.08);
    color: var(--accent-strong);
    flex-shrink: 0;
  }

  .scan-list {
    display: flex;
    flex-direction: column;
    gap: 10px;
    margin-top: 18px;
  }

  .scan-item {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
    width: 100%;
    padding: 12px;
    border-radius: 16px;
    background: rgba(255, 255, 255, 0.72);
    text-align: left;
  }

  .signal-badge {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    min-width: 92px;
    color: var(--accent-strong);
    font-size: 12px;
    font-weight: 700;
  }

  .ssid-stack {
    display: grid;
    gap: 3px;
    min-width: 0;
  }

  .ssid-stack strong,
  .ssid-stack span {
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }

  .ssid-stack strong {
    color: var(--text-h);
    font-size: 15px;
  }

  .ssid-stack span,
  .quality,
  .empty-state {
    color: var(--muted);
    font-size: 13px;
  }

  .quality {
    flex-shrink: 0;
    font-weight: 700;
  }

  .empty-state {
    display: grid;
    justify-items: center;
    gap: 10px;
    padding: 32px 18px 12px;
    text-align: center;
  }
</style>
