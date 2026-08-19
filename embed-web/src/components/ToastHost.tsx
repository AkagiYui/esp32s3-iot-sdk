import { For, Show } from "solid-js";
import { Dynamic } from "solid-js/web";
import { CircleCheck, CircleX, Info, TriangleAlert } from "lucide-solid";
import { removeToast, toasts, type ToastType } from "@/lib/feedback";
import type { IconComponent } from "@/lib/route-manifest";
import { cx } from "@/lib/cx";
import styles from "./ToastHost.module.css";

const iconMap: Record<ToastType, IconComponent> = {
  success: CircleCheck,
  warning: TriangleAlert,
  error: CircleX,
  info: Info,
};

export default function ToastHost() {
  return (
    <Show when={toasts().length > 0}>
      <div class={styles.toastHost} aria-live="polite">
        <For each={toasts()}>
          {(toast) => (
            <button
              class={cx(styles.toast, styles[toast.type], toast.leaving && styles.leaving)}
              onClick={() => removeToast(toast.id)}
            >
              <Dynamic component={iconMap[toast.type]} size={18} />
              <span>{toast.message}</span>
            </button>
          )}
        </For>
      </div>
    </Show>
  );
}
