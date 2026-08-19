import { describe, expect, it } from "vite-plus/test";
import { HOME_PATH, parseHash } from "./router";

describe("parseHash", () => {
  it.each([
    ["", HOME_PATH],
    ["#", HOME_PATH],
    ["#/", HOME_PATH],
    ["#/dashboard", "/dashboard"],
    ["#/settings", "/settings"],
    ["#/wifi", "/wifi"],
  ])("%s -> %s", (hash, expected) => {
    expect(parseHash(hash)).toBe(expected);
  });

  it("未知路径回落到首页，刷新和收藏链接都能恢复", () => {
    expect(parseHash("#/does-not-exist")).toBe(HOME_PATH);
    expect(parseHash("#/wifi/extra")).toBe(HOME_PATH);
  });
});
