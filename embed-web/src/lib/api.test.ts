import { afterEach, beforeEach, describe, expect, it, vi } from "vite-plus/test";
import { ApiError, apiRequest, describeError } from "./api";
import { authRequired, setApiToken, setAuthRequired } from "./auth";

const fetchMock = vi.fn();

function response(payload: unknown, init: { ok?: boolean; status?: number } = {}) {
  const { ok = true, status = 200 } = init;
  return {
    ok,
    status,
    statusText: "",
    json: () => Promise.resolve(payload),
  } as Response;
}

describe("apiRequest", () => {
  beforeEach(() => {
    fetchMock.mockReset();
    vi.stubGlobal("fetch", fetchMock);
    setApiToken("");
    setAuthRequired(false);
  });

  afterEach(() => {
    vi.unstubAllGlobals();
    setApiToken("");
    setAuthRequired(false);
  });

  it("GET 请求不带 body 与 Content-Type", async () => {
    fetchMock.mockResolvedValue(response({ ok: true }));

    await apiRequest("/api/system/info");

    const [url, init] = fetchMock.mock.calls[0] as [string, RequestInit];
    expect(url).toBe("/api/system/info");
    expect(init.method).toBe("GET");
    expect(init.body).toBeUndefined();
    expect(init.headers).toEqual({});
  });

  it("有令牌时带上 Authorization 头", async () => {
    setApiToken("deadbeef");
    fetchMock.mockResolvedValue(response({}));

    await apiRequest("/api/system/info");

    const [, init] = fetchMock.mock.calls[0] as [string, RequestInit];
    expect(init.headers).toEqual({ Authorization: "Bearer deadbeef" });
  });

  it("带 body 且有令牌时两个头都在", async () => {
    setApiToken("deadbeef");
    fetchMock.mockResolvedValue(response({}));

    await apiRequest("/api/settings", { method: "PUT", body: { a: 1 } });

    const [, init] = fetchMock.mock.calls[0] as [string, RequestInit];
    expect(init.headers).toEqual({
      "Content-Type": "application/json",
      Authorization: "Bearer deadbeef",
    });
  });

  it("401 会切到需要令牌的状态，而不是当成普通错误", async () => {
    fetchMock.mockResolvedValue(response({}, { ok: false, status: 401 }));

    await expect(apiRequest("/api/system/info")).rejects.toMatchObject({
      status: 401,
      code: "unauthorized",
    });
    expect(authRequired()).toBe(true);
  });

  it("带 body 时序列化为 JSON 并设置 Content-Type", async () => {
    fetchMock.mockResolvedValue(response({}));

    await apiRequest("/api/settings", { method: "PUT", body: { device_name: "x" } });

    const [, init] = fetchMock.mock.calls[0] as [string, RequestInit];
    expect(init.method).toBe("PUT");
    expect(init.headers).toEqual({ "Content-Type": "application/json" });
    expect(JSON.parse(init.body as string)).toEqual({ device_name: "x" });
  });

  it("把设备的结构化错误体还原成 ApiError", async () => {
    fetchMock.mockResolvedValue(
      response(
        { error: { code: "invalid_ssid", message: "ssid is required" } },
        {
          ok: false,
          status: 400,
        },
      ),
    );

    await expect(apiRequest("/api/wifi/config")).rejects.toMatchObject({
      name: "ApiError",
      status: 400,
      code: "invalid_ssid",
      message: "ssid is required",
    });
  });

  it("错误体不是 JSON 时退回 HTTP 状态", async () => {
    fetchMock.mockResolvedValue({
      ok: false,
      status: 500,
      statusText: "Internal Server Error",
      json: () => Promise.reject(new Error("not json")),
    } as unknown as Response);

    await expect(apiRequest("/api/system/info")).rejects.toMatchObject({
      status: 500,
      code: "http_500",
    });
  });

  it("网络失败归类为 offline", async () => {
    fetchMock.mockRejectedValue(new TypeError("Failed to fetch"));

    await expect(apiRequest("/api/system/info")).rejects.toMatchObject({
      status: 0,
      code: "offline",
    });
  });

  it("超时归类为 timeout，并且是网络类错误", async () => {
    const abortError = new Error("aborted");
    abortError.name = "AbortError";
    fetchMock.mockRejectedValue(abortError);

    const error = await apiRequest("/api/system/info").catch((caught: unknown) => caught);
    expect(error).toBeInstanceOf(ApiError);
    expect((error as ApiError).code).toBe("timeout");
    expect((error as ApiError).isNetworkError).toBe(true);
  });

  it("204 响应不尝试解析 JSON", async () => {
    fetchMock.mockResolvedValue({
      ok: true,
      status: 204,
      statusText: "",
      json: () => Promise.reject(new Error("should not be called")),
    } as unknown as Response);

    await expect(apiRequest("/api/system/reboot", { method: "POST" })).resolves.toBeUndefined();
  });

  it("响应正文无法解析时报 invalid_response", async () => {
    fetchMock.mockResolvedValue({
      ok: true,
      status: 200,
      statusText: "",
      json: () => Promise.reject(new Error("boom")),
    } as unknown as Response);

    await expect(apiRequest("/api/system/info")).rejects.toMatchObject({
      code: "invalid_response",
    });
  });

  it("超时会真正中断请求", async () => {
    fetchMock.mockImplementation((_url: string, init: RequestInit) => {
      return new Promise((_resolve, reject) => {
        init.signal?.addEventListener("abort", () => {
          const error = new Error("aborted");
          error.name = "AbortError";
          reject(error);
        });
      });
    });

    await expect(apiRequest("/api/system/info", { timeoutMs: 5 })).rejects.toMatchObject({
      code: "timeout",
    });
  });
});

describe("describeError", () => {
  it("ApiError 直接用它的信息", () => {
    expect(describeError(new ApiError("设备无响应", 0, "timeout"))).toBe("设备无响应");
  });

  it("普通 Error 用 message", () => {
    expect(describeError(new Error("boom"))).toBe("boom");
  });

  it("其它值降级为未知错误", () => {
    expect(describeError("oops")).toBe("未知错误");
  });
});
