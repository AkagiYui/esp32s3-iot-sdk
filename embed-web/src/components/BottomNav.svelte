<script lang="ts">
  import { routeEntries } from "@/lib/route-manifest";
  import { getRoute, navigate, type Route } from "@/lib/router.svelte";
</script>

<nav class="bottom-nav">
  {#each routeEntries as tab}
    <button
      class="nav-item"
      class:active={getRoute() === tab.path}
      onclick={() => navigate(tab.path as Route)}
      aria-label={tab.label}
    >
      <svelte:component this={tab.icon} size={22} />
      <span class="nav-label">{tab.label}</span>
    </button>
  {/each}
</nav>

<style>
  .bottom-nav {
    position: fixed;
    bottom: 0;
    left: 0;
    right: 0;
    display: flex;
    justify-content: space-around;
    align-items: center;
    background: var(--card-bg);
    border-top: 1px solid var(--border);
    padding: 6px 0 calc(6px + env(safe-area-inset-bottom));
    z-index: 100;
    -webkit-backdrop-filter: blur(12px);
    backdrop-filter: blur(12px);
  }
  .nav-item {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 2px;
    border: none;
    background: none;
    cursor: pointer;
    padding: 6px 16px;
    color: var(--text);
    -webkit-tap-highlight-color: transparent;
    transition: color 0.15s;
    min-width: 64px;
  }
  .nav-item:active {
    opacity: 0.7;
  }
  .nav-item.active {
    color: var(--accent);
  }
  .nav-label {
    font-size: 11px;
    font-weight: 500;
    line-height: 1;
  }

  @media (min-width: 1100px) {
    .bottom-nav {
      top: 24px;
      bottom: 24px;
      left: 24px;
      right: auto;
      width: 124px;
      display: flex;
      flex-direction: column;
      justify-content: flex-start;
      align-items: stretch;
      gap: 8px;
      padding: 14px 10px;
      border-top: none;
      border-right: 1px solid var(--border);
      border-radius: 24px;
    }

    .nav-item {
      flex-direction: column;
      justify-content: center;
      gap: 8px;
      width: 100%;
      min-width: 0;
      min-height: 72px;
      padding: 12px 10px;
      border-radius: 18px;
    }

    .nav-item.active {
      background: rgba(43, 122, 120, 0.1);
    }

    .nav-label {
      font-size: 12px;
      line-height: 1.2;
      text-align: center;
    }
  }
</style>
