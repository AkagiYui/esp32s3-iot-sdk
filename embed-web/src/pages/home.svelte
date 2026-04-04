<script module lang="ts">
  import type { RouteMeta } from '../lib/route-manifest';

  export const routeMeta: RouteMeta = {
    label: '首页',
    icon: 'home',
    order: 0,
  };
</script>

<div class="page">
  <div class="page-header">
    <h1>首页</h1>
    <p class="subtitle">ESP32-S3 IoT 控制面板</p>
  </div>

  <div class="card-grid">
    <div class="card">
      <div class="card-icon">📶</div>
      <div class="card-body">
        <h3>WiFi 状态</h3>
        <p class="status online">已连接</p>
      </div>
    </div>

    <div class="card">
      <div class="card-icon">🌡️</div>
      <div class="card-body">
        <h3>温度</h3>
        <p class="value">24.5°C</p>
      </div>
    </div>

    <div class="card">
      <div class="card-icon">💧</div>
      <div class="card-body">
        <h3>湿度</h3>
        <p class="value">62%</p>
      </div>
    </div>

    <div class="card">
      <div class="card-icon">⏱️</div>
      <div class="card-body">
        <h3>运行时间</h3>
        <p class="value">3h 24m</p>
      </div>
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
  .card-grid {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 12px;
  }
  .card {
    background: var(--card-bg);
    border: 1px solid var(--border);
    border-radius: 12px;
    padding: 16px;
    display: flex;
    flex-direction: column;
    gap: 8px;
  }
  .card-icon {
    font-size: 28px;
  }
  .card-body h3 {
    margin: 0;
    font-size: 13px;
    color: var(--text);
    font-weight: 400;
  }
  .value {
    font-size: 22px;
    font-weight: 600;
    color: var(--text-h);
    margin: 2px 0 0;
  }
  .status {
    font-size: 14px;
    font-weight: 500;
    margin: 2px 0 0;
  }
  .status.online {
    color: #22c55e;
  }
</style>
