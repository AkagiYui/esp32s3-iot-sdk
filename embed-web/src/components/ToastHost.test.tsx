import { afterEach, describe, expect, it } from "vite-plus/test";
import { cleanup, render, waitFor } from "@solidjs/testing-library";
import ToastHost from "./ToastHost";
import { showToast, toasts } from "@/lib/feedback";

describe("ToastHost", () => {
  afterEach(async () => {
    await waitFor(() => expect(toasts()).toHaveLength(0), { timeout: 2000 });
    cleanup();
  });

  it("没有 toast 时不渲染容器", () => {
    const { container } = render(() => <ToastHost />);
    expect(container.querySelector("[aria-live]")).toBeNull();
  });

  it("渲染消息并附带类型图标", async () => {
    const { findByText, container } = render(() => <ToastHost />);

    showToast("WiFi 配置已保存", "success", 300);

    const toast = (await findByText("WiFi 配置已保存")).closest("button");
    expect(toast?.querySelector("svg")).not.toBeNull();
    expect(container.querySelector("[aria-live]")).toHaveAttribute("aria-live", "polite");
  });

  it("点击可以提前关闭", async () => {
    const { findByText, queryByText } = render(() => <ToastHost />);

    showToast("点我关闭", "info", 5000);
    (await findByText("点我关闭")).closest("button")!.click();

    await waitFor(() => expect(queryByText("点我关闭")).toBeNull(), { timeout: 1000 });
  });

  it("到期后自动消失", async () => {
    const { findByText, queryByText } = render(() => <ToastHost />);

    showToast("短暂提示", "warning", 100);
    await findByText("短暂提示");

    await waitFor(() => expect(queryByText("短暂提示")).toBeNull(), { timeout: 1000 });
  });
});
