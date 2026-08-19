import { afterEach, beforeEach, describe, expect, it, vi } from "vite-plus/test";
import {
  currentDialog,
  DIALOG_EXIT_MS,
  removeToast,
  resolveDialog,
  showAlert,
  showConfirm,
  showToast,
  toasts,
  TOAST_EXIT_MS,
} from "./feedback";

describe("dialog", () => {
  afterEach(() => {
    resolveDialog(undefined);
  });

  it("showAlert 只给一个确定按钮", async () => {
    const pending = showAlert("已完成", "提示");

    expect(currentDialog()?.title).toBe("提示");
    expect(currentDialog()?.buttons).toEqual([{ label: "确定", variant: "accent" }]);

    resolveDialog(undefined);
    await expect(pending).resolves.toBeUndefined();
    expect(currentDialog()).toBeUndefined();
  });

  it("showConfirm 的确定按钮解析为 ok", async () => {
    const pending = showConfirm("确定要重启吗？", "重启设备", true);

    const confirmButton = currentDialog()?.buttons?.at(-1);
    expect(confirmButton?.variant).toBe("danger");

    resolveDialog(confirmButton?.value);
    await expect(pending).resolves.toBe("ok");
  });

  it("新对话框顶替旧对话框时，旧 Promise 按取消结算", async () => {
    const first = showAlert("第一个");
    const second = showAlert("第二个");

    await expect(first).resolves.toBeUndefined();
    expect(currentDialog()?.message).toBe("第二个");

    resolveDialog(undefined);
    await expect(second).resolves.toBeUndefined();
  });
});

describe("toast", () => {
  beforeEach(() => {
    vi.useFakeTimers();
  });

  afterEach(() => {
    vi.runAllTimers();
    vi.useRealTimers();
  });

  it("到期后先播出场动画再移除", () => {
    showToast("已保存", "success", 1000);
    expect(toasts()).toHaveLength(1);
    expect(toasts()[0]).toMatchObject({ message: "已保存", type: "success", leaving: false });

    vi.advanceTimersByTime(1000);
    expect(toasts()[0]?.leaving).toBe(true);

    vi.advanceTimersByTime(TOAST_EXIT_MS);
    expect(toasts()).toHaveLength(0);
  });

  it("手动关闭时不会重复排队移除", () => {
    const id = showToast("手动关闭", "info", 5000);

    removeToast(id);
    removeToast(id);
    expect(toasts()[0]?.leaving).toBe(true);

    vi.advanceTimersByTime(TOAST_EXIT_MS);
    expect(toasts()).toHaveLength(0);

    // 原本的自动关闭定时器已经被取消，不会再动到列表
    vi.advanceTimersByTime(5000);
    expect(toasts()).toHaveLength(0);
  });

  it("多条 toast 各自独立计时", () => {
    showToast("先", "info", 500);
    showToast("后", "info", 2000);
    expect(toasts()).toHaveLength(2);

    vi.advanceTimersByTime(500 + TOAST_EXIT_MS);
    expect(toasts().map((toast) => toast.message)).toEqual(["后"]);

    vi.advanceTimersByTime(1500 + TOAST_EXIT_MS);
    expect(toasts()).toHaveLength(0);
  });
});

describe("动画时长常量", () => {
  it("与 CSS 中的时长保持同步", () => {
    expect(DIALOG_EXIT_MS).toBe(220);
    expect(TOAST_EXIT_MS).toBe(160);
  });
});
