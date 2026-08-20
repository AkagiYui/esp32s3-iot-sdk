import { afterEach, beforeEach, describe, expect, it, vi } from "vite-plus/test";
import { cleanup, fireEvent, render, waitFor } from "@solidjs/testing-library";
import { apiToken, authRequired, setApiToken, setAuthRequired } from "@/lib/auth";
import TokenGate from "./TokenGate";

const fetchMock = vi.fn();

describe("TokenGate", () => {
  beforeEach(() => {
    localStorage.clear();
    setApiToken("");
    setAuthRequired(false);
    fetchMock.mockReset();
    fetchMock.mockResolvedValue({
      ok: true,
      status: 200,
      statusText: "",
      json: () => Promise.resolve({}),
    } as Response);
    vi.stubGlobal("fetch", fetchMock);
  });

  afterEach(() => {
    cleanup();
    vi.unstubAllGlobals();
    localStorage.clear();
    setApiToken("");
    setAuthRequired(false);
  });

  it("未要求鉴权时不渲染任何东西", () => {
    const { queryByRole } = render(() => <TokenGate />);
    expect(queryByRole("dialog")).toBeNull();
  });

  it("要求鉴权时挡在最前面", () => {
    setAuthRequired(true);
    const { getByRole } = render(() => <TokenGate />);
    expect(getByRole("dialog")).toBeInTheDocument();
  });

  it("提交后保存令牌并解除拦截", async () => {
    setAuthRequired(true);
    const { container } = render(() => <TokenGate />);

    // password 类型的输入框没有 textbox role，只能直接查 DOM
    const field = container.querySelector("input");
    expect(field).toBeTruthy();

    fireEvent.input(field!, { target: { value: "  abc123  " } });
    fireEvent.submit(container.querySelector("form")!);

    await waitFor(() => {
      expect(apiToken()).toBe("abc123");
      expect(authRequired()).toBe(false);
    });
  });

  it("空输入不会提交", () => {
    setAuthRequired(true);
    const { container } = render(() => <TokenGate />);

    fireEvent.submit(container.querySelector("form")!);

    expect(apiToken()).toBe("");
    expect(authRequired()).toBe(true);
  });
});
