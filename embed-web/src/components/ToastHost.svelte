<script lang="ts">
  import {
    getToasts,
    removeToast,
    type ToastType,
  } from "@/lib/feedback.svelte";
  import { fly, fade } from "svelte/transition";
  import { cubicOut } from "svelte/easing";
  import { CircleCheck, TriangleAlert, CircleX, Info } from "lucide-svelte";

  const iconMap: Record<ToastType, typeof Info> = {
    success: CircleCheck,
    warning: TriangleAlert,
    error: CircleX,
    info: Info,
  };
</script>

{#if getToasts().length}
  <div class="toast-host" aria-live="polite">
    {#each getToasts() as toast (toast.id)}
      <button
        class="toast {toast.type}"
        in:fly={{ y: -32, duration: 240, easing: cubicOut }}
        out:fade={{ duration: 160 }}
        onclick={() => removeToast(toast.id)}
      >
        <svelte:component this={iconMap[toast.type]} size={18} />
        <span>{toast.message}</span>
      </button>
    {/each}
  </div>
{/if}

<style>
  .toast-host {
    position: fixed;
    top: 0;
    left: 0;
    right: 0;
    z-index: 9500;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 8px;
    padding: 16px 16px 0;
    pointer-events: none;
  }

  .toast {
    pointer-events: auto;
    display: flex;
    align-items: center;
    gap: 8px;
    padding: 10px 18px;
    border-radius: 12px;
    border: 1px solid var(--border);
    background: var(--surface-strong);
    box-shadow: var(--shadow-soft);
    font-size: 14px;
    font-weight: 500;
    color: var(--text-h);
    cursor: pointer;
    -webkit-tap-highlight-color: transparent;
    max-width: 360px;
    width: max-content;
    text-align: left;
    font-family: inherit;
    line-height: 1.4;
  }

  .toast.success {
    border-color: rgba(34, 160, 90, 0.25);
    color: #1a8a50;
  }
  :global([data-theme="dark"]) .toast.success {
    color: #3ec97a;
  }

  .toast.warning {
    border-color: rgba(210, 160, 30, 0.25);
    color: #a07800;
  }
  :global([data-theme="dark"]) .toast.warning {
    color: #e0b840;
  }

  .toast.error {
    border-color: rgba(220, 60, 50, 0.25);
    color: var(--danger);
  }

  .toast.info {
    border-color: var(--border);
    color: var(--accent);
  }
</style>
