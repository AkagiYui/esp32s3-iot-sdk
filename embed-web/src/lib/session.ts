import { createRoot, createSignal } from "solid-js";
import { apiRequest } from "./api";
import { sessionToken, setAuthRequired, setSessionToken } from "./auth";

export type AuthStatus = {
  /** 用户是否已经设过访问密码。没设过时设备不允许离开配网模式。 */
  configured: boolean;
  authenticated: boolean;
  provisioning: boolean;
  password_min_length: number;
};

type SessionResponse = {
  token: string;
  expires_in: number;
};

const store = createRoot(() => {
  const [authStatus, setAuthStatus] = createSignal<AuthStatus | undefined>(undefined);
  return { authStatus, setAuthStatus };
});

export const { authStatus } = store;

/** 未鉴权即可访问，前端靠它决定该显示「设置密码」还是「登录」。 */
export async function refreshAuthStatus(): Promise<AuthStatus | undefined> {
  try {
    const status = await apiRequest<AuthStatus>("/api/auth/status");
    store.setAuthStatus(status);
    // 设备说我们已经通过鉴权，就没必要再挡着
    if (status.authenticated) {
      setAuthRequired(false);
    }
    return status;
  } catch {
    return undefined;
  }
}

export async function login(password: string): Promise<void> {
  const session = await apiRequest<SessionResponse>("/api/auth/login", {
    method: "POST",
    body: { password },
    // PBKDF2 在设备上要算几百毫秒，给足余量
    timeoutMs: 15000,
    // 密码错了要如实告诉用户，不能被当成"你还没登录"
    onUnauthorized: "throw",
  });
  setSessionToken(session.token);
  await refreshAuthStatus();
}

export async function logout(): Promise<void> {
  try {
    await apiRequest("/api/auth/logout", { method: "POST" });
  } finally {
    setSessionToken("");
    setAuthRequired(true);
    await refreshAuthStatus();
  }
}

/**
 * 设置或修改访问密码。
 *
 * 配网模式下不需要旧密码——那时用户是物理接近的，这也正是忘记密码后的找回途径。
 * 成功后设备会踢掉其它所有会话并签发一个新的。
 */
export async function setPassword(next: string, current?: string): Promise<void> {
  const session = await apiRequest<SessionResponse>("/api/auth/password", {
    method: "PUT",
    body:
      current === undefined ? { password: next } : { password: next, current_password: current },
    timeoutMs: 15000,
  });
  setSessionToken(session.token);
  await refreshAuthStatus();
}

export function hasSession(): boolean {
  return sessionToken().length > 0;
}
