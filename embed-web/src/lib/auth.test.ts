import { afterEach, beforeEach, describe, expect, it } from "vite-plus/test";
import { apiToken, authRequired, setApiToken, setAuthRequired } from "./auth";

describe("令牌保管", () => {
  beforeEach(() => {
    localStorage.clear();
    setApiToken("");
    setAuthRequired(false);
  });

  afterEach(() => {
    localStorage.clear();
    setApiToken("");
  });

  it("写入后同时更新信号与 localStorage", () => {
    setApiToken("abc123");

    expect(apiToken()).toBe("abc123");
    expect(localStorage.getItem("kenko-api-token")).toBe("abc123");
  });

  it("裁掉首尾空白——用户从日志里复制常带换行", () => {
    setApiToken("  abc123\n");

    expect(apiToken()).toBe("abc123");
    expect(localStorage.getItem("kenko-api-token")).toBe("abc123");
  });

  it("写入空串等于清除", () => {
    setApiToken("abc123");
    setApiToken("");

    expect(apiToken()).toBe("");
    expect(localStorage.getItem("kenko-api-token")).toBeNull();
  });

  it("成功写入令牌会解除「需要令牌」状态", () => {
    setAuthRequired(true);
    setApiToken("abc123");

    expect(authRequired()).toBe(false);
  });
});
