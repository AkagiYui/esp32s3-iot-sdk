import { afterEach, describe, expect, it } from "vite-plus/test";
import { resolvedTheme, setThemeMode, themeMode } from "./theme";

describe("主题", () => {
  afterEach(() => {
    setThemeMode("system");
  });

  it("默认跟随系统，jsdom 下解析为亮色", () => {
    expect(themeMode()).toBe("system");
    expect(resolvedTheme()).toBe("light");
    expect(document.documentElement.getAttribute("data-theme")).toBe("light");
  });

  it("切换后立即写入 data-theme 并持久化", () => {
    setThemeMode("dark");

    expect(themeMode()).toBe("dark");
    expect(resolvedTheme()).toBe("dark");
    expect(document.documentElement.getAttribute("data-theme")).toBe("dark");
    expect(localStorage.getItem("theme-mode")).toBe("dark");
  });

  it("显式选择亮色时不再跟随系统", () => {
    setThemeMode("light");
    expect(resolvedTheme()).toBe("light");
    expect(localStorage.getItem("theme-mode")).toBe("light");
  });
});
