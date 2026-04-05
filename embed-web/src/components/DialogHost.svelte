<script lang="ts">
  import { getDialog, resolveDialog } from "@/lib/feedback.svelte";
  import { scale, fade } from "svelte/transition";
  import { cubicOut } from "svelte/easing";
</script>

{#if getDialog()}
  <!-- svelte-ignore a11y_no_static_element_interactions -->
  <div
    class="overlay"
    transition:fade={{ duration: 180 }}
    onkeydown={(e) => {
      if (e.key === "Escape") resolveDialog(undefined);
    }}
    onclick={() => resolveDialog(undefined)}
  >
    <!-- svelte-ignore a11y_click_events_have_key_events -->
    <!-- svelte-ignore a11y_interactive_supports_focus -->
    <div
      class="dialog"
      transition:scale={{ start: 0.88, duration: 220, easing: cubicOut }}
      onclick={(e) => e.stopPropagation()}
      role="alertdialog"
      aria-modal="true"
      aria-label={getDialog()?.title ?? "提示"}
    >
      {#if getDialog()?.title}
        <h2 class="dialog-title">{getDialog()!.title}</h2>
      {/if}
      <p class="dialog-message">{getDialog()!.message}</p>
      <div
        class="dialog-actions"
        class:single={getDialog()!.buttons?.length === 1}
      >
        {#each getDialog()!.buttons ?? [] as btn}
          <button
            class="dialog-btn {btn.variant ?? 'accent'}"
            onclick={() => resolveDialog(btn.value)}>{btn.label}</button
          >
        {/each}
      </div>
    </div>
  </div>
{/if}

<style>
  .overlay {
    position: fixed;
    inset: 0;
    z-index: 9000;
    display: flex;
    align-items: center;
    justify-content: center;
    background: rgba(0, 0, 0, 0.35);
    -webkit-backdrop-filter: blur(4px);
    backdrop-filter: blur(4px);
    padding: 24px;
  }

  .dialog {
    width: 100%;
    max-width: 340px;
    background: var(--surface-strong);
    border: 1px solid var(--border);
    border-radius: 18px;
    padding: 24px 22px 18px;
    box-shadow: var(--shadow);
    display: flex;
    flex-direction: column;
    gap: 12px;
  }

  .dialog-title {
    font-size: 17px;
    font-weight: 700;
    color: var(--text-h);
    margin: 0;
    line-height: 1.3;
  }

  .dialog-message {
    font-size: 14px;
    color: var(--text);
    line-height: 1.55;
    margin: 0;
    word-break: break-word;
  }

  .dialog-actions {
    display: flex;
    gap: 10px;
    margin-top: 6px;
  }

  .dialog-actions.single {
    justify-content: flex-end;
  }

  .dialog-btn {
    flex: 1;
    padding: 10px 0;
    border: none;
    border-radius: 10px;
    font-size: 15px;
    font-weight: 600;
    cursor: pointer;
    transition: opacity 0.12s;
    -webkit-tap-highlight-color: transparent;
  }

  .dialog-btn:active {
    opacity: 0.8;
  }

  .dialog-btn.accent {
    background: var(--accent);
    color: #fff;
  }

  .dialog-btn.danger {
    background: var(--danger);
    color: #fff;
  }

  .dialog-btn.plain {
    background: var(--border);
    color: var(--text-h);
  }

  .dialog-actions.single .dialog-btn {
    flex: none;
    min-width: 88px;
    padding: 10px 24px;
  }
</style>
