import { afterEach, beforeEach, describe, expect, it, vi } from "vite-plus/test";
import { cleanup, fireEvent, render, waitFor } from "@solidjs/testing-library";
import { authRequired, sessionToken, setAuthRequired, setSessionToken } from "@/lib/auth";
import { refreshAuthStatus } from "@/lib/session";
import LoginGate from "./LoginGate";

const fetchMock = vi.fn();

function jsonResponse(payload: unknown, status = 200) {
  return {
    ok: status >= 200 && status < 300,
    status,
    statusText: "",
    json: () => Promise.resolve(payload),
  } as Response;
}

/** 默认的设备回应：已设过密码、当前未登录。 */
function withStatus(configured: boolean) {
  fetchMock.mockImplementation((url: string, init?: RequestInit) => {
    if (url === "/api/auth/status") {
      return Promise.resolve(
        jsonResponse({
          configured,
          authenticated: false,
          provisioning: false,
          password_min_length: 8,
        }),
      );
    }
    if (url === "/api/auth/login") {
      const body = JSON.parse((init?.body as string) ?? "{}") as { password?: string };
      if (body.password === "kenko1234") {
        return Promise.resolve(jsonResponse({ token: "session-abc", expires_in: 604800 }));
      }
      return Promise.resolve(
        jsonResponse(
          { error: { code: "invalid_password", message: "wrong access password" } },
          401,
        ),
      );
    }
    return Promise.reject(new Error(`unexpected request: ${url}`));
  });
}

describe("LoginGate", () => {
  beforeEach(() => {
    localStorage.clear();
    setSessionToken("");
    setAuthRequired(false);
    fetchMock.mockReset();
    vi.stubGlobal("fetch", fetchMock);
  });

  afterEach(() => {
    cleanup();
    vi.unstubAllGlobals();
    localStorage.clear();
    setSessionToken("");
    setAuthRequired(false);
  });

  it("未要求登录时不渲染任何东西", () => {
    withStatus(true);
    const { queryByRole } = render(() => <LoginGate />);
    expect(queryByRole("dialog")).toBeNull();
  });

  it("要求登录时挡在最前面", () => {
    withStatus(true);
    setAuthRequired(true);
    const { getByRole } = render(() => <LoginGate />);
    expect(getByRole("dialog")).toBeInTheDocument();
  });

  it("密码正确时保存会话并放行", async () => {
    withStatus(true);
    await refreshAuthStatus();
    setAuthRequired(true);

    const { container } = render(() => <LoginGate />);
    // password 类型的输入框没有 textbox role，只能直接查 DOM
    const field = container.querySelector("input");
    fireEvent.input(field!, { target: { value: "kenko1234" } });
    fireEvent.submit(container.querySelector("form")!);

    await waitFor(() => {
      expect(sessionToken()).toBe("session-abc");
      expect(authRequired()).toBe(false);
    });
  });

  it("密码错误时留在闸门内并给出原因", async () => {
    withStatus(true);
    await refreshAuthStatus();
    setAuthRequired(true);

    const { container, findByText } = render(() => <LoginGate />);
    const field = container.querySelector("input");
    fireEvent.input(field!, { target: { value: "wrong-one" } });
    fireEvent.submit(container.querySelector("form")!);

    expect(await findByText("密码不正确")).toBeInTheDocument();
    expect(sessionToken()).toBe("");
    expect(authRequired()).toBe(true);
  });

  it("设备还没设过密码时给出找回途径，而不是让人干瞪眼", async () => {
    withStatus(false);
    await refreshAuthStatus();
    setAuthRequired(true);

    const { findByText, container } = render(() => <LoginGate />);

    expect(await findByText("设备尚未设置访问密码")).toBeInTheDocument();
    // 没有可提交的表单，避免用户在这里白试
    expect(container.querySelector("form")).toBeNull();
  });
});
