import { afterEach, describe, expect, it } from "vite-plus/test";
import { cleanup, render, waitFor } from "@solidjs/testing-library";
import NavBar from "./NavBar";
import { routeEntries } from "@/lib/route-manifest";

describe("NavBar", () => {
  afterEach(() => {
    cleanup();
    window.location.hash = "";
  });

  it("按清单顺序渲染全部页面入口", () => {
    const { getAllByRole } = render(() => <NavBar />);

    const labels = getAllByRole("button").map((button) => button.textContent);
    expect(labels).toEqual(routeEntries.map((entry) => entry.label));
  });

  it("点击后写入 hash 并把该项标记为当前页", async () => {
    const { getByLabelText } = render(() => <NavBar />);

    getByLabelText("设置").click();

    await waitFor(() => {
      expect(window.location.hash).toBe("#/settings");
      expect(getByLabelText("设置")).toHaveAttribute("aria-current", "page");
      expect(getByLabelText("首页")).not.toHaveAttribute("aria-current");
    });
  });
});
