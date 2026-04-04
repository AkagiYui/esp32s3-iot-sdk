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
</style>
