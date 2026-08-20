/**
 * 设备端 HTTP 接口的统一客户端。
 *
 * 设备是一块单核 MCU，随时可能因为切换 WiFi 模式、重启或 OTA 而失联，
 * 所以每个请求都必须有超时和结构化的错误，绝不能让界面无限转圈。
 */

export const API_TIMEOUT_MS = 8000;

type ApiErrorBody = {
  error?: {
    code?: string;
    message?: string;
  };
};

export class ApiError extends Error {
  readonly status: number;
  readonly code: string;

  constructor(message: string, status: number, code: string) {
    super(message);
    this.name = "ApiError";
    this.status = status;
    this.code = code;
  }

  /** 请求根本没发出去或超时——通常意味着设备正在重启或已换网。 */
  get isNetworkError(): boolean {
    return this.status === 0;
  }
}

export type RequestOptions = {
  method?: "GET" | "POST" | "PUT" | "DELETE";
  body?: unknown;
  timeoutMs?: number;
  signal?: AbortSignal;
};

function combineSignals(timeoutMs: number, external?: AbortSignal): [AbortSignal, () => void] {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(new Error("timeout")), timeoutMs);

  const onAbort = () => controller.abort(external?.reason);
  external?.addEventListener("abort", onAbort, { once: true });

  return [
    controller.signal,
    () => {
      clearTimeout(timer);
      external?.removeEventListener("abort", onAbort);
    },
  ];
}

async function readError(response: Response): Promise<ApiError> {
  let code = `http_${response.status}`;
  let message = `${response.status} ${response.statusText}`.trim();

  try {
    const payload = (await response.json()) as ApiErrorBody;
    if (payload.error?.code) {
      code = payload.error.code;
    }
    if (payload.error?.message) {
      message = payload.error.message;
    }
  } catch {
    // 设备在极端情况下可能返回非 JSON 的错误页，保留 HTTP 状态即可
  }

  return new ApiError(message, response.status, code);
}

export async function apiRequest<T>(path: string, options: RequestOptions = {}): Promise<T> {
  const { method = "GET", body, timeoutMs = API_TIMEOUT_MS, signal: external } = options;
  const [signal, cleanup] = combineSignals(timeoutMs, external);

  let response: Response;
  try {
    response = await fetch(path, {
      method,
      signal,
      headers: body === undefined ? undefined : { "Content-Type": "application/json" },
      body: body === undefined ? undefined : JSON.stringify(body),
    });
  } catch (error) {
    cleanup();
    if (external?.aborted) {
      throw new ApiError("请求已取消", 0, "aborted");
    }
    const timedOut = error instanceof Error && error.name === "AbortError";
    throw new ApiError(
      timedOut ? "设备无响应" : "无法连接到设备",
      0,
      timedOut ? "timeout" : "offline",
    );
  }
  cleanup();

  if (!response.ok) {
    throw await readError(response);
  }

  if (response.status === 204) {
    return undefined as T;
  }

  try {
    return (await response.json()) as T;
  } catch {
    throw new ApiError("设备返回了无法解析的内容", response.status, "invalid_response");
  }
}

/** 把任意异常转成可以直接展示给用户的一句话。 */
export function describeError(error: unknown): string {
  if (error instanceof ApiError) {
    return error.message;
  }
  if (error instanceof Error) {
    return error.message;
  }
  return "未知错误";
}
