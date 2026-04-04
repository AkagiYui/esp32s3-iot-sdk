export type ThemeMode = "system" | "light" | "dark";

const STORAGE_KEY = "theme-mode";
const mql = typeof window !== "undefined"
  ? window.matchMedia("(prefers-color-scheme: dark)")
  : null;

let mode = $state<ThemeMode>(loadMode());
let systemDark = $state(mql?.matches ?? false);
let resolved = $derived<"light" | "dark">(
  mode === "system" ? (systemDark ? "dark" : "light") : mode
);

function loadMode(): ThemeMode {
  try {
    const v = localStorage.getItem(STORAGE_KEY);
    if (v === "light" || v === "dark" || v === "system") return v;
  } catch { }
  return "system";
}

// Listen for system preference changes
mql?.addEventListener("change", (e) => {
  systemDark = e.matches;
});

// Apply theme attribute reactively
$effect.root(() => {
  $effect(() => {
    document.documentElement.setAttribute("data-theme", resolved);
  });
});

export function getThemeMode(): ThemeMode {
  return mode;
}

export function getResolvedTheme(): "light" | "dark" {
  return resolved;
}

export function setThemeMode(m: ThemeMode) {
  mode = m;
  try {
    localStorage.setItem(STORAGE_KEY, m);
  } catch { }
}
