<script lang="ts">
  import "./lib/theme.svelte";
  import { getRoute, getCurrentRouteEntry } from "./lib/router.svelte";
  import NavBar from "./components/NavBar.svelte";
  import DialogHost from "./components/DialogHost.svelte";
  import ToastHost from "./components/ToastHost.svelte";
  import { scale } from "svelte/transition";
  import { cubicOut, cubicIn } from "svelte/easing";

  const fallbackMessage = "页面加载失败";
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
    {#key getRoute()}
      <div
        class="page-wrapper"
        in:scale={{
          start: 0.85,
          duration: 280,
          delay: 200,
          opacity: 0,
          easing: cubicOut,
        }}
        out:scale={{ start: 0.85, duration: 200, opacity: 0, easing: cubicIn }}
      >
        {#if getCurrentRouteEntry()}
          <svelte:component this={getCurrentRouteEntry()?.component} />
        {:else}
          <section class="route-error">{fallbackMessage}</section>
        {/if}
      </div>
    {/key}
  </main>

  <NavBar />
</div>

<DialogHost />
<ToastHost />

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

  .topbar > div:first-child {
    min-width: 0;
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
    display: grid;
  }

  .page-wrapper {
    grid-area: 1 / 1;
    display: flex;
    flex-direction: column;
    gap: 16px;
  }

  .route-error {
    padding: 24px 16px;
    border: 1px solid var(--border);
    border-radius: 20px;
    background: var(--card-bg);
    color: var(--muted);
    text-align: center;
  }

  @media (min-width: 1100px) {
    .shell {
      max-width: none;
      margin: 0;
      padding: 28px 32px 32px 172px;
      display: flex;
      flex-direction: column;
      align-items: center;
    }

    .topbar {
      width: min(100%, 960px);
      align-items: end;
      gap: 24px;
      margin-bottom: 28px;
      padding: 0 0 18px;
      border-bottom: 1px solid var(--border);
    }

    .eyebrow {
      margin-bottom: 10px;
      font-size: 12px;
      letter-spacing: 0.24em;
    }

    .topbar h1 {
      font-size: clamp(32px, 3vw, 42px);
      line-height: 0.98;
      max-width: 12ch;
    }

    .status-pill {
      padding: 10px 16px;
      font-size: 14px;
      box-shadow: var(--shadow-soft);
    }

    .app-content {
      width: min(100%, 960px);
    }
  }
</style>
