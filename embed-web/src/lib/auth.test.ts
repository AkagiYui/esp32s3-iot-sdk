import { afterEach, beforeEach, describe, expect, it } from "vite-plus/test";
import { authRequired, sessionToken, setAuthRequired, setSessionToken } from "./auth";

describe("会话令牌保管", () => {
  beforeEach(() => {
    localStorage.clear();
    setSessionToken("");
    setAuthRequired(false);
  });

  afterEach(() => {
    localStorage.clear();
    setSessionToken("");
  });

  it("写入后同时更新信号与 localStorage", () => {
    setSessionToken("abc123");

    expect(sessionToken()).toBe("abc123");
    expect(localStorage.getItem("kenko-session-token")).toBe("abc123");
  });

  it("裁掉首尾空白——用户从日志里复制常带换行", () => {
    setSessionToken("  abc123\n");

    expect(sessionToken()).toBe("abc123");
    expect(localStorage.getItem("kenko-session-token")).toBe("abc123");
  });

  it("写入空串等于清除", () => {
    setSessionToken("abc123");
    setSessionToken("");

    expect(sessionToken()).toBe("");
    expect(localStorage.getItem("kenko-session-token")).toBeNull();
  });

  it("写入会话令牌会解除登录拦截", () => {
    setAuthRequired(true);
    setSessionToken("abc123");

    expect(authRequired()).toBe(false);
  });
});
