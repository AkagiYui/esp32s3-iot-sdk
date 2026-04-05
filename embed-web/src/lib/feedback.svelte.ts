/** Centralised feedback layer — dialog (alert / confirm) + toast */

// ─── Dialog ────────────────────────────────────────────────────

export interface DialogButton {
  label: string;
  /** 'accent' (default) | 'danger' | 'plain' */
  variant?: "accent" | "danger" | "plain";
  /** Value resolved when the button is pressed */
  value?: string;
}

export interface DialogOptions {
  title?: string;
  message: string;
  buttons?: DialogButton[];
}

interface DialogEntry extends DialogOptions {
  resolve: (value: string | undefined) => void;
}

let currentDialog = $state<DialogEntry | undefined>(undefined);

export function getDialog() {
  return currentDialog;
}

export function resolveDialog(value: string | undefined) {
  currentDialog?.resolve(value);
  currentDialog = undefined;
}

function openDialog(opts: DialogOptions): Promise<string | undefined> {
  return new Promise((resolve) => {
    currentDialog = { ...opts, resolve };
  });
}

/** Single‑button informational dialog (replaces `alert`) */
export function showAlert(message: string, title?: string) {
  return openDialog({
    title,
    message,
    buttons: [{ label: "确定", variant: "accent" }],
  });
}

/** Two‑button confirmation dialog (replaces `confirm`).
 *  Resolves `"ok"` or `undefined` (dismissed / cancelled). */
export function showConfirm(
  message: string,
  title?: string,
  danger = false,
) {
  return openDialog({
    title,
    message,
    buttons: [
      { label: "取消", variant: "plain", value: undefined },
      { label: "确定", variant: danger ? "danger" : "accent", value: "ok" },
    ],
  });
}

/** Fully custom dialog */
export function showDialog(opts: DialogOptions) {
  return openDialog(opts);
}

// ─── Toast ─────────────────────────────────────────────────────

export type ToastType = "success" | "warning" | "error" | "info";

export interface ToastEntry {
  id: number;
  message: string;
  type: ToastType;
  duration: number;
}

let nextId = 0;
let toasts = $state<ToastEntry[]>([]);

export function getToasts() {
  return toasts;
}

export function removeToast(id: number) {
  toasts = toasts.filter((t) => t.id !== id);
}

export function showToast(
  message: string,
  type: ToastType = "info",
  duration = 2500,
) {
  const id = nextId++;
  toasts = [...toasts, { id, message, type, duration }];
  setTimeout(() => removeToast(id), duration);
}
