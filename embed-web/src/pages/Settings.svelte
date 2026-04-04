<script lang="ts">
  let deviceName = $state("ESP32-S3-IoT");
</script>

<div class="page">
  <div class="page-header">
    <h1>设置</h1>
    <p class="subtitle">设备配置</p>
  </div>

  <div class="settings-group">
    <h2 class="group-title">设备</h2>
    <div class="settings-list">
      <label class="setting-item">
        <span class="setting-label">设备名称</span>
        <input type="text" bind:value={deviceName} class="setting-input" />
      </label>
      <div class="setting-item">
        <span class="setting-label">固件版本</span>
        <span class="setting-value">v1.0.0</span>
      </div>
      <div class="setting-item">
        <span class="setting-label">SDK 版本</span>
        <span class="setting-value">ESP-IDF 5.x</span>
      </div>
    </div>
  </div>

  <div class="settings-group">
    <h2 class="group-title">网络</h2>
    <div class="settings-list">
      <div class="setting-item">
        <span class="setting-label">IP 地址</span>
        <span class="setting-value">192.168.4.1</span>
      </div>
      <div class="setting-item">
        <span class="setting-label">MAC 地址</span>
        <span class="setting-value">AA:BB:CC:DD:EE:FF</span>
      </div>
    </div>
  </div>

  <div class="settings-group">
    <h2 class="group-title">操作</h2>
    <div class="settings-list">
      <div class="setting-item action">
        <button class="btn" onclick={() => alert("OTA 升级功能待实现")}
          >检查更新</button
        >
      </div>
      <div class="setting-item action">
        <button class="btn danger" onclick={() => confirm("确定要重启设备吗？")}
          >重启设备</button
        >
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
  .settings-group {
    margin-bottom: 24px;
  }
  .group-title {
    font-size: 13px;
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 0.5px;
    color: var(--text);
    margin: 0 0 8px;
    padding: 0 4px;
  }
  .settings-list {
    display: flex;
    flex-direction: column;
    gap: 1px;
    background: var(--border);
    border: 1px solid var(--border);
    border-radius: 12px;
    overflow: hidden;
  }
  .setting-item {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 14px 16px;
    background: var(--card-bg);
    min-height: 24px;
  }
  .setting-label {
    font-size: 15px;
    color: var(--text-h);
  }
  .setting-value {
    font-size: 14px;
    color: var(--text);
    font-family: var(--mono);
  }
  .setting-input {
    text-align: right;
    border: none;
    background: transparent;
    color: var(--accent);
    font-size: 14px;
    width: 140px;
    outline: none;
    font-family: var(--mono);
  }
  .setting-item.action {
    justify-content: center;
    padding: 8px 16px;
  }
  .btn {
    width: 100%;
    padding: 10px;
    border: none;
    border-radius: 8px;
    background: var(--accent);
    color: #fff;
    font-size: 15px;
    font-weight: 500;
    cursor: pointer;
    -webkit-tap-highlight-color: transparent;
  }
  .btn:active {
    opacity: 0.85;
  }
  .btn.danger {
    background: #ef4444;
  }
</style>
