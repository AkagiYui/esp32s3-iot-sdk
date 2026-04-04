<script module lang="ts">
  import type { RouteMeta } from "@/lib/route-manifest";
  import { LayoutDashboard } from "lucide-svelte";

  export const routeMeta: RouteMeta = {
    label: "仪表盘",
    icon: LayoutDashboard,
    order: 1,
  };
</script>

<script lang="ts">
  import { Lightbulb, Sun, BarChart3, Radio } from "lucide-svelte";

  let ledOn = $state(false);
  let brightness = $state(75);
</script>

<div class="page">
  <div class="page-header">
    <h1>仪表盘</h1>
    <p class="subtitle">设备控制与监控</p>
  </div>

  <div class="control-list">
    <div class="control-item">
      <div class="control-info">
        <span class="control-icon"><Lightbulb size={24} /></span>
        <div>
          <h3>LED 灯</h3>
          <p>{ledOn ? "已开启" : "已关闭"}</p>
        </div>
      </div>
      <button
        class="toggle"
        class:active={ledOn}
        aria-label="切换 LED 灯"
        onclick={() => (ledOn = !ledOn)}
      >
        <span class="toggle-knob"></span>
      </button>
    </div>

    <div class="control-item">
      <div class="control-info">
        <span class="control-icon"><Sun size={24} /></span>
        <div>
          <h3>亮度</h3>
          <p>{brightness}%</p>
        </div>
      </div>
      <input
        type="range"
        min="0"
        max="100"
        bind:value={brightness}
        class="slider"
      />
    </div>

    <div class="control-item">
      <div class="control-info">
        <span class="control-icon"><BarChart3 size={24} /></span>
        <div>
          <h3>内存使用</h3>
          <p>124 KB / 320 KB</p>
        </div>
      </div>
      <div class="progress-bar">
        <div class="progress-fill" style="width: 39%"></div>
      </div>
    </div>

    <div class="control-item">
      <div class="control-info">
        <span class="control-icon"><Radio size={24} /></span>
        <div>
          <h3>RSSI 信号</h3>
          <p>-42 dBm</p>
        </div>
      </div>
      <span class="badge good">良好</span>
    </div>
  </div>
</div>

<style>
  .page-header {
    margin-bottom: 24px;
  }
  .page-header h1 {
    font-size: 28px;
    margin: 0 0 4px;
  }
  .subtitle {
    color: var(--text);
    font-size: 14px;
  }
  .control-list {
    display: flex;
    flex-direction: column;
    gap: 1px;
    background: var(--border);
    border: 1px solid var(--border);
    border-radius: 12px;
    overflow: hidden;
  }
  .control-item {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 16px;
    background: var(--card-bg);
  }
  .control-info {
    display: flex;
    align-items: center;
    gap: 12px;
  }
  .control-icon {
    color: var(--accent);
    line-height: 0;
  }
  .control-info h3 {
    margin: 0;
    font-size: 15px;
    font-weight: 500;
    color: var(--text-h);
  }
  .control-info p {
    margin: 2px 0 0;
    font-size: 13px;
    color: var(--text);
  }
  .toggle {
    position: relative;
    width: 50px;
    height: 28px;
    border-radius: 14px;
    border: none;
    background: var(--border);
    cursor: pointer;
    transition: background 0.2s;
    flex-shrink: 0;
  }
  .toggle.active {
    background: var(--accent);
  }
  .toggle-knob {
    position: absolute;
    top: 3px;
    left: 3px;
    width: 22px;
    height: 22px;
    border-radius: 50%;
    background: #fff;
    transition: transform 0.2s;
    box-shadow: 0 1px 3px rgba(0, 0, 0, 0.2);
  }
  .toggle.active .toggle-knob {
    transform: translateX(22px);
  }
  .slider {
    width: 100px;
    flex-shrink: 0;
    accent-color: var(--accent);
  }
  .progress-bar {
    width: 80px;
    height: 6px;
    background: var(--border);
    border-radius: 3px;
    overflow: hidden;
    flex-shrink: 0;
  }
  .progress-fill {
    height: 100%;
    background: var(--accent);
    border-radius: 3px;
  }
  .badge {
    font-size: 12px;
    font-weight: 500;
    padding: 4px 10px;
    border-radius: 20px;
    flex-shrink: 0;
  }
  .badge.good {
    background: rgba(34, 197, 94, 0.15);
    color: #22c55e;
  }
</style>
