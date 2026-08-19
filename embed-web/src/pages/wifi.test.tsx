import { afterEach, beforeEach, describe, expect, it, vi } from "vite-plus/test";
import { cleanup, render, waitFor } from "@solidjs/testing-library";
import WifiPage from "./wifi";

const fetchMock = vi.fn();

function jsonResponse(payload: unknown) {
  return {
    ok: true,
    status: 200,
    json: () => Promise.resolve(payload),
    text: () => Promise.resolve(JSON.stringify(payload)),
  } as Response;
}

describe("WiFi 配置页", () => {
  beforeEach(() => {
    fetchMock.mockReset();
    fetchMock.mockImplementation((url: string, init?: RequestInit) => {
      if (url === "/api/wifi-config" && (!init || init.method === undefined)) {
        return Promise.resolve(
          jsonResponse([
            { ssid: "Kenko-Lab", password: "12345678" },
            { ssid: "Guest", password: "" },
          ]),
        );
      }
      if (url === "/api/wifi-config") {
        return Promise.resolve(jsonResponse({ message: "ok" }));
      }
      if (url === "/api/wifi-scan") {
        return Promise.resolve(
          jsonResponse([{ ssid: "Office-5G", rssi: -50, authmode: "WPA2_PSK" }]),
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

  it("挂载后拉取并按优先级渲染已保存的网络", async () => {
    const { findByText, getByText } = render(() => <WifiPage />);

    await findByText("Kenko-Lab");
    expect(getByText("Guest")).toBeInTheDocument();
    expect(getByText("已保存 2 项")).toBeInTheDocument();
    // 未展开时密码以圆点掩码显示
    expect(getByText("密码：••••••••")).toBeInTheDocument();
    expect(getByText("密码：开放网络 / 未填写")).toBeInTheDocument();
  });

  it("编辑 SSID 时输入框不会被重建，焦点保持不变", async () => {
    const { findAllByText, container } = render(() => <WifiPage />);

    (await findAllByText("编辑"))[0]!.click();

    const ssidInput = await waitFor(() => {
      const input = container.querySelector<HTMLInputElement>('input[type="text"]');
      expect(input).not.toBeNull();
      return input!;
    });

    ssidInput.focus();
    ssidInput.value = "Kenko-Lab-2";
    ssidInput.dispatchEvent(new Event("input", { bubbles: true }));

    await waitFor(() => {
      const current = container.querySelector<HTMLInputElement>('input[type="text"]');
      // 同一个 DOM 节点、值已更新、焦点没丢
      expect(current).toBe(ssidInput);
      expect(current!.value).toBe("Kenko-Lab-2");
      expect(document.activeElement).toBe(ssidInput);
    });
  });

  it("保存时提交去掉前端 id 的配置数组", async () => {
    const { findByText } = render(() => <WifiPage />);

    await findByText("Kenko-Lab");
    (await findByText("保存")).click();

    await waitFor(() => {
      const put = fetchMock.mock.calls.find(
        (call) => (call[1] as RequestInit | undefined)?.method === "PUT",
      ) as [string, RequestInit] | undefined;
      expect(put).toBeDefined();
      expect(typeof put![1].body).toBe("string");
      expect(JSON.parse(put![1].body as string)).toEqual([
        { ssid: "Kenko-Lab", password: "12345678" },
        { ssid: "Guest", password: "" },
      ]);
    });
  });

  it("SSID 为空时拒绝保存并展开该项", async () => {
    const { findByText, container } = render(() => <WifiPage />);

    await findByText("Kenko-Lab");
    (await findByText("新增")).click();
    (await findByText("保存")).click();

    await waitFor(() => {
      expect(
        fetchMock.mock.calls.some((call) => (call[1] as RequestInit | undefined)?.method === "PUT"),
      ).toBe(false);
      expect(container.querySelector('input[type="text"]')).not.toBeNull();
    });
  });
});
