<script lang="ts">
  import { getRoute } from "./lib/router.svelte";
  import BottomNav from "./lib/BottomNav.svelte";
  import Home from "./pages/Home.svelte";
  import Dashboard from "./pages/Dashboard.svelte";
  import Settings from "./pages/Settings.svelte";
</script>

<svelte:head>
  <title>ESP32-S3 控制台</title>
  <meta
    name="viewport"
    content="width=device-width, initial-scale=1, viewport-fit=cover"
  />
</svelte:head>

<div class="shell">
  <header class="topbar">
    <div>
      <p class="eyebrow">ESP32-S3 Embedded Web</p>
      <h1>掌上设备控制台</h1>
    </div>
    <div class="status-pill">在线</div>
  </header>

  <main class="app-content">
    {#if getRoute() === "/"}
      <Home />
    {:else if getRoute() === "/dashboard"}
      <Dashboard />
    {:else if getRoute() === "/settings"}
      <Settings />
    {/if}
  </main>

  <BottomNav />
</div>

<style>
  .shell {
    min-height: 100dvh;
    max-width: 480px;
    margin: 0 auto;
    padding: 16px 16px calc(92px + env(safe-area-inset-bottom));
    box-sizing: border-box;
  }

  .topbar {
    display: flex;
    align-items: flex-start;
    justify-content: space-between;
    gap: 12px;
    margin-bottom: 18px;
  }

  .eyebrow {
    margin: 0 0 6px;
    font-size: 11px;
    letter-spacing: 0.18em;
    text-transform: uppercase;
    color: var(--muted);
  }

  .topbar h1 {
    margin: 0;
    font-size: 28px;
    line-height: 1.05;
  }

  .status-pill {
    flex-shrink: 0;
    padding: 8px 12px;
    border-radius: 999px;
    background: rgba(43, 122, 120, 0.12);
    color: var(--accent);
    font-size: 13px;
    font-weight: 700;
  }

  .app-content {
    display: flex;
    flex-direction: column;
    gap: 16px;
  }
</style>
