import { createRoot, createSignal } from "solid-js";

/**
 * 接口访问令牌的本地保管。
 *
 * 设备接入局域网后，所有 `/api/**` 都要求令牌；配网模式下不要求
 * （那时用户手上还没有令牌，无密码热点本身就是信任边界）。
 *
 * 这个模块刻意不依赖 `api.ts`，否则会和它形成循环引用。
 */

const STORAGE_KEY = "kenko-api-token";

function load(): string {
  try {
    return localStorage.getItem(STORAGE_KEY) ?? "";
  } catch {
    // 隐私模式下 localStorage 可能不可用，退化为仅本次会话有效
    return "";
  }
}

const auth = createRoot(() => {
  const [apiToken, setToken] = createSignal(load());
  const [authRequired, setAuthRequired] = createSignal(false);

  function setApiToken(token: string): void {
    const trimmed = token.trim();
    setToken(trimmed);
    setAuthRequired(false);

    try {
      if (trimmed) {
        localStorage.setItem(STORAGE_KEY, trimmed);
      } else {
        localStorage.removeItem(STORAGE_KEY);
      }
    } catch {
      // 存不下就只在本次会话生效
    }
  }

  return { apiToken, setApiToken, authRequired, setAuthRequired };
});

export const { apiToken, setApiToken, authRequired, setAuthRequired } = auth;
