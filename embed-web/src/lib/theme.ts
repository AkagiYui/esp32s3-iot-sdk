import { createMemo, createRenderEffect, createRoot, createSignal } from "solid-js";

export type ThemeMode = "system" | "light" | "dark";
export type ResolvedTheme = "light" | "dark";

const STORAGE_KEY = "theme-mode";

function loadMode(): ThemeMode {
  try {
    const value = localStorage.getItem(STORAGE_KEY);
    if (value === "light" || value === "dark" || value === "system") {
      return value;
    }
  } catch {
    // 隐私模式下 localStorage 可能不可用，直接退回默认值
  }
  return "system";
}

const theme = createRoot(() => {
  const media = window.matchMedia("(prefers-color-scheme: dark)");

  const [themeMode, setMode] = createSignal<ThemeMode>(loadMode());
  const [systemDark, setSystemDark] = createSignal(media.matches);

  media.addEventListener("change", (event) => {
    setSystemDark(event.matches);
  });

  const resolvedTheme = createMemo<ResolvedTheme>(() => {
    const mode = themeMode();
    return mode === "system" ? (systemDark() ? "dark" : "light") : mode;
  });

  // createRenderEffect 在创建时同步执行，首帧就能带上正确的 data-theme，避免闪白
  createRenderEffect(() => {
    document.documentElement.setAttribute("data-theme", resolvedTheme());
  });

  function setThemeMode(mode: ThemeMode): void {
    setMode(mode);
    try {
      localStorage.setItem(STORAGE_KEY, mode);
    } catch {
      // 存不下就只在本次会话生效
    }
  }

  return { themeMode, resolvedTheme, setThemeMode };
});

export const { themeMode, resolvedTheme, setThemeMode } = theme;
