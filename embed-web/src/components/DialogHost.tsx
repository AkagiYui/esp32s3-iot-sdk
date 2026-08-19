import { createEffect, createSignal, For, onCleanup, Show } from "solid-js";
import { createPresence } from "@/lib/presence";
import { currentDialog, DIALOG_EXIT_MS, resolveDialog, type DialogEntry } from "@/lib/feedback";
import { cx } from "@/lib/cx";
import styles from "./DialogHost.module.css";

export default function DialogHost() {
  // 出场动画期间对话框已经从 store 移除，这里保留最后一份内容继续渲染
  const [rendered, setRendered] = createSignal<DialogEntry | undefined>(undefined);

  createEffect(() => {
    const entry = currentDialog();
    if (entry) {
      setRendered(entry);
    }
  });

  const presence = createPresence(() => currentDialog() !== undefined, DIALOG_EXIT_MS);

  const dismiss = () => resolveDialog(undefined);

  const onKeyDown = (event: KeyboardEvent) => {
    if (event.key === "Escape" && currentDialog()) {
      dismiss();
    }
  };

  document.addEventListener("keydown", onKeyDown);
  onCleanup(() => document.removeEventListener("keydown", onKeyDown));

  return (
    <Show when={presence.mounted() && rendered()} keyed>
      {(dialog) => (
        <div
          class={cx(styles.overlay, presence.exiting() && styles.leaving)}
          onClick={dismiss}
          role="presentation"
        >
          <div
            class={cx(styles.dialog, presence.exiting() && styles.leaving)}
            onClick={(event) => event.stopPropagation()}
            role="alertdialog"
            aria-modal="true"
            aria-label={dialog.title ?? "提示"}
          >
            <Show when={dialog.title}>
              {(title) => <h2 class={styles.dialogTitle}>{title()}</h2>}
            </Show>
            <p class={styles.dialogMessage}>{dialog.message}</p>
            <div class={cx(styles.dialogActions, dialog.buttons?.length === 1 && styles.single)}>
              <For each={dialog.buttons ?? []}>
                {(button) => (
                  <button
                    class={cx(styles.dialogBtn, styles[button.variant ?? "accent"])}
                    onClick={() => resolveDialog(button.value)}
                  >
                    {button.label}
                  </button>
                )}
              </For>
            </div>
          </div>
        </div>
      )}
    </Show>
  );
}
