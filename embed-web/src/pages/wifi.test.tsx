import { afterEach, beforeEach, describe, expect, it, vi } from "vite-plus/test";
import { cleanup, render, waitFor } from "@solidjs/testing-library";
import { toasts } from "@/lib/feedback";
import WifiPage from "./wifi";

const fetchMock = vi.fn();

function jsonResponse(payload: unknown) {
  return {
    ok: true,
    status: 200,
    statusText: "",
    json: () => Promise.resolve(payload),
  } as Response;
}

const initialConfig = {
  items: [
    { ssid: "Kenko-Lab", has_password: true },
    { ssid: "Guest", has_password: false },
  ],
  max_items: 16,
};

describe("WiFi 配置页", () => {
  beforeEach(() => {
    fetchMock.mockReset();
    fetchMock.mockImplementation((url: string, init?: RequestInit) => {
      if (url === "/api/wifi/config" && init?.method === "PUT") {
        return Promise.resolve(jsonResponse(initialConfig));
      }
      if (url === "/api/wifi/config") {
        return Promise.resolve(jsonResponse(initialConfig));
      }
      if (url.startsWith("/api/wifi/scan")) {
        return Promise.resolve(
          jsonResponse({
            items: [
              { ssid: "Office-5G", rssi: -50, channel: 6, authmode: "WPA2-PSK", secured: true },
            ],
          }),
        );
      }
      return Promise.reject(new Error(`unexpected request: ${url}`));
    });
    vi.stubGlobal("fetch", fetchMock);
  });

  afterEach(() => {
    cleanup();
    vi.unstubAllGlobals();
  });

  it("渲染已保存的网络及其优先级", async () => {
    const { findByText, getByText } = render(() => <WifiPage />);

    await findByText("Kenko-Lab");
    expect(getByText("优先级 1")).toBeInTheDocument();
    expect(getByText("优先级 2")).toBeInTheDocument();
  });

  it("只展示是否已设密码，不展示密码本身", async () => {
    const { findByText, getByText } = render(() => <WifiPage />);

    await findByText("Kenko-Lab");
    expect(getByText("已保存密码")).toBeInTheDocument();
    expect(getByText("开放网络 / 无密码")).toBeInTheDocument();
  });

  it("未修改密码时保存会把 password 送成 null，让设备沿用旧值", async () => {
    const { findByText, getAllByText } = render(() => <WifiPage />);

    await findByText("Kenko-Lab");
    getAllByText("保存")[0]!.click();

    await waitFor(() => {
      const put = fetchMock.mock.calls.find(
        (call) => (call[1] as RequestInit | undefined)?.method === "PUT",
      );
      expect(put).toBeTruthy();
      expect(JSON.parse((put![1] as RequestInit).body as string)).toEqual({
        items: [
          { ssid: "Kenko-Lab", password: null },
          { ssid: "Guest", password: null },
        ],
      });
    });
  });

  it("加载失败时弹出错误提示，而不是无声失败", async () => {
    fetchMock.mockImplementation(() => Promise.reject(new TypeError("Failed to fetch")));

    const { findByText } = render(() => <WifiPage />);

    // toast 由全局的 ToastHost 渲染，这里直接断言反馈层收到了消息
    await waitFor(() => {
      expect(
        toasts().some((toast) => toast.message === "无法连接到设备" && toast.type === "error"),
      ).toBe(true);
    });
    expect(await findByText("还没有任何 WiFi 配置，点击「新增」开始。")).toBeInTheDocument();
  });
});
