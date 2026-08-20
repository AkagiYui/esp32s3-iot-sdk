import { afterEach, describe, expect, it } from "vite-plus/test";
import { cleanup, render, waitFor } from "@solidjs/testing-library";
import RouteView from "./RouteView";

describe("RouteView", () => {
  afterEach(async () => {
    window.location.hash = "";
    await waitFor(() => expect(window.location.hash).toBe(""));
    cleanup();
  });

  it("首屏渲染当前路由且不播入场动画", async () => {
    const { container, findByText } = render(() => <RouteView />);

    await findByText("设备当前状态一览");
    const layers = container.querySelectorAll("main > div");
    expect(layers).toHaveLength(1);
    expect(layers[0]!.className).not.toMatch(/enter|leave/);
  });

  it("切换路由时新旧页面共存，旧页面播完动画才卸载", async () => {
    const { container, findByText } = render(() => <RouteView />);
    await findByText("设备当前状态一览");

    const firstLayer = container.querySelector("main > div");
    window.location.hash = "#/dashboard";

    // 过渡期间两层并存：旧页面 leave，新页面 enter
    await waitFor(() => {
      const layers = container.querySelectorAll("main > div");
      expect(layers).toHaveLength(2);
      expect(layers[0]!.className).toMatch(/leave/);
      expect(layers[1]!.className).toMatch(/enter/);
    });

    // 离场的仍然是原来那个 DOM 节点——没有被重建，页面状态不会重置
    expect(container.querySelector("main > div")).toBe(firstLayer);

    await waitFor(() => expect(container.querySelectorAll("main > div")).toHaveLength(1), {
      timeout: 1000,
    });
    await findByText("运行指标与硬件信息");
  });
});
