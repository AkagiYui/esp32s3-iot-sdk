import { afterEach, describe, expect, it } from "vite-plus/test";
import { cleanup, render, waitFor } from "@solidjs/testing-library";
import DialogHost from "./DialogHost";
import { resolveDialog, showAlert, showConfirm } from "@/lib/feedback";

describe("DialogHost", () => {
  afterEach(() => {
    resolveDialog(undefined);
    cleanup();
  });

  it("没有待处理对话框时什么也不渲染", () => {
    const { queryByRole } = render(() => <DialogHost />);
    expect(queryByRole("alertdialog")).toBeNull();
  });

  it("渲染标题、正文和按钮", async () => {
    const { findByRole, getByText } = render(() => <DialogHost />);

    void showAlert("OTA 升级功能待实现", "提示");

    const dialog = await findByRole("alertdialog");
    expect(dialog).toHaveAttribute("aria-label", "提示");
    expect(getByText("OTA 升级功能待实现")).toBeInTheDocument();
    expect(getByText("确定")).toBeInTheDocument();
  });

  it("点击按钮把按钮的 value 交给调用方", async () => {
    const { findByText } = render(() => <DialogHost />);

    const pending = showConfirm("确定要重启设备吗？", "重启设备", true);
    (await findByText("确定")).click();

    await expect(pending).resolves.toBe("ok");
  });

  it("按 Esc 视为取消", async () => {
    const { findByRole } = render(() => <DialogHost />);

    const pending = showConfirm("确定要重启设备吗？", "重启设备");
    await findByRole("alertdialog");

    document.dispatchEvent(new KeyboardEvent("keydown", { key: "Escape", bubbles: true }));

    await expect(pending).resolves.toBeUndefined();
  });

  it("出场动画结束后才从 DOM 移除", async () => {
    const { findByRole, queryByRole } = render(() => <DialogHost />);

    void showAlert("再见");
    await findByRole("alertdialog");

    resolveDialog(undefined);
    expect(queryByRole("alertdialog")).not.toBeNull();

    await waitFor(() => expect(queryByRole("alertdialog")).toBeNull(), { timeout: 1000 });
  });
});
