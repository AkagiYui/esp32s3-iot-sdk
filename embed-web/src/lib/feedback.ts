/** 统一的反馈层：对话框（alert / confirm）+ 轻提示（toast）。 */

import { createSignal } from "solid-js";
import { createStore, produce } from "solid-js/store";

// ─── Dialog ────────────────────────────────────────────────────

export interface DialogButton {
  label: string;
  /** 'accent'（默认）| 'danger' | 'plain' */
  variant?: "accent" | "danger" | "plain";
  /** 按下该按钮时 Promise 解析出的值 */
  value?: string;
}

export interface DialogOptions {
  title?: string;
  message: string;
  buttons?: DialogButton[];
}

export interface DialogEntry extends DialogOptions {
  resolve: (value: string | undefined) => void;
}

/** 出场动画时长，需与 DialogHost.module.css 保持一致。 */
export const DIALOG_EXIT_MS = 220;

const [currentDialog, setCurrentDialog] = createSignal<DialogEntry | undefined>(undefined);

export { currentDialog };

export function resolveDialog(value: string | undefined): void {
  const entry = currentDialog();
  setCurrentDialog(undefined);
  entry?.resolve(value);
}

function openDialog(options: DialogOptions): Promise<string | undefined> {
  // 同时只显示一个对话框；被顶替的那个按“取消”处理，避免 Promise 永远悬着
  resolveDialog(undefined);

  return new Promise((resolve) => {
    setCurrentDialog({ ...options, resolve });
  });
}

/** 单按钮提示框（替代 `alert`）。 */
export function showAlert(message: string, title?: string): Promise<string | undefined> {
  return openDialog({
    title,
    message,
    buttons: [{ label: "确定", variant: "accent" }],
  });
}

/**
 * 双按钮确认框（替代 `confirm`）。
 * 解析为 `"ok"` 或 `undefined`（取消 / 关闭）。
 */
export function showConfirm(
  message: string,
  title?: string,
  danger = false,
): Promise<string | undefined> {
  return openDialog({
    title,
    message,
    buttons: [
      { label: "取消", variant: "plain", value: undefined },
      { label: "确定", variant: danger ? "danger" : "accent", value: "ok" },
    ],
  });
}

/** 完全自定义的对话框。 */
export function showDialog(options: DialogOptions): Promise<string | undefined> {
  return openDialog(options);
}

// ─── Toast ─────────────────────────────────────────────────────

export type ToastType = "success" | "warning" | "error" | "info";

export interface ToastEntry {
  id: number;
  message: string;
  type: ToastType;
  duration: number;
  /** 正在播放出场动画，DOM 还在但即将移除。 */
  leaving: boolean;
}

/** 出场动画时长，需与 ToastHost.module.css 保持一致。 */
export const TOAST_EXIT_MS = 160;

let nextId = 0;
const timers = new Map<number, ReturnType<typeof setTimeout>>();

// 用 store 而不是 signal：`leaving` 就地变更，`<For>` 才不会重建 DOM、打断入场动画
const [toastList, setToastList] = createStore<ToastEntry[]>([]);

export function toasts(): readonly ToastEntry[] {
  return toastList;
}

export function showToast(message: string, type: ToastType = "info", duration = 2500): number {
  const id = nextId++;

  setToastList(
    produce((list) => {
      list.push({ id, message, type, duration, leaving: false });
    }),
  );

  timers.set(
    id,
    setTimeout(() => removeToast(id), duration),
  );

  return id;
}

export function removeToast(id: number): void {
  const index = toastList.findIndex((toast) => toast.id === id);
  if (index < 0 || toastList[index]?.leaving) {
    return;
  }

  clearTimeout(timers.get(id));
  setToastList(index, "leaving", true);

  timers.set(
    id,
    setTimeout(() => {
      timers.delete(id);
      setToastList(
        produce((list) => {
          const current = list.findIndex((toast) => toast.id === id);
          if (current >= 0) {
            list.splice(current, 1);
          }
        }),
      );
    }, TOAST_EXIT_MS),
  );
}
