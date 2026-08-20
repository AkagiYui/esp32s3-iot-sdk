/**
 * 设备端 HTTP 接口的统一客户端。
 *
 * 设备是一块单核 MCU，随时可能因为切换 WiFi 模式、重启或 OTA 而失联，
 * 所以每个请求都必须有超时和结构化的错误，绝不能让界面无限转圈。
 */

import { sessionToken, setAuthRequired } from "./auth";

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
  /**
   * 收到 401 时的处理方式。
   *
   * 默认 "gate"：把界面切到登录闸门。登录接口本身必须用 "throw"，
   * 否则"密码错了"会被当成"你还没登录"，用户看到的提示牛头不对马嘴。
   */
  onUnauthorized?: "gate" | "throw";
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
  const {
    method = "GET",
    body,
    timeoutMs = API_TIMEOUT_MS,
    signal: external,
    onUnauthorized = "gate",
  } = options;
  const [signal, cleanup] = combineSignals(timeoutMs, external);

  const headers: Record<string, string> = {};
  if (body !== undefined) {
    headers["Content-Type"] = "application/json";
  }
  const token = sessionToken();
  if (token) {
    headers["Authorization"] = `Bearer ${token}`;
  }

  let response: Response;
  try {
    response = await fetch(path, {
      method,
      signal,
      headers,
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
    const error = await readError(response);
    // 会话缺失或过期：把界面切到登录闸门，而不是反复弹一堆请求失败。
    // 但错误本身要原样抛出，调用方才能显示设备给出的具体原因。
    if (response.status === 401 && onUnauthorized === "gate") {
      setAuthRequired(true);
    }
    throw error;
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

/**
 * 设备返回的错误信息是英文的（固件里塞中文既占 flash 又容易编码出错），
 * 这里按错误码翻成中文。没收录的码就退回设备给的原文，至少不会更糟。
 */
const ERROR_MESSAGES: Record<string, string> = {
  unauthorized: "需要登录",
  invalid_password: "密码不正确",
  current_password_required: "请填写当前密码",
  weak_password: "密码太短或包含不可见字符",
  password_not_set: "设备还没有设置访问密码",
  too_many_attempts: "尝试次数过多，请稍后再试",
  storage_unavailable: "配置分区不可用，无法保存",
  no_wifi_config: "还没有保存任何 WiFi 配置",
  invalid_ssid: "SSID 不合法",
  too_many_items: "WiFi 配置数量超出上限",
  scan_failed: "扫描失败，请稍后重试",
  body_too_large: "请求内容过大",
  invalid_json: "请求格式不正确",
  ota_busy: "已有一个升级正在进行",
  image_too_large: "固件超出 OTA 分区容量",
  image_rejected: "固件校验未通过",
  upload_interrupted: "上传过程中断开",
  no_factory_partition: "当前分区表没有出厂基线镜像",
  factory_image_invalid: "出厂基线镜像校验失败",
  persist_failed: "写入失败",
  no_memory: "设备内存不足",
  not_found: "接口不存在",
  method_not_allowed: "该接口不支持这个操作",
};

/** 把任意异常转成可以直接展示给用户的一句话。 */
export function describeError(error: unknown): string {
  if (error instanceof ApiError) {
    return ERROR_MESSAGES[error.code] ?? error.message;
  }
  if (error instanceof Error) {
    return error.message;
  }
  return "未知错误";
}
