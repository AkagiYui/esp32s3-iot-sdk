import { apiRequest, ApiError } from "./api";

export type OtaState = "idle" | "receiving" | "ready" | "failed";

export type OtaStatus = {
  state: OtaState;
  received: number;
  total: number;
  message: string;
  running_partition: string;
  boot_partition: string;
  awaiting_confirm: boolean;
  max_image_size: number;
};

export function fetchOtaStatus(): Promise<OtaStatus> {
  return apiRequest<OtaStatus>("/api/system/ota");
}

export function confirmFirmware(): Promise<OtaStatus> {
  return apiRequest<OtaStatus>("/api/system/ota/confirm", { method: "POST" });
}

/**
 * 上传固件。
 *
 * 这里用 XMLHttpRequest 而不是 fetch：几 MB 的固件在 WiFi 上要传十几秒，
 * 没有上传进度的话界面只能干等，而 fetch 至今没有可用的上传进度事件。
 */
export function uploadFirmware(
  file: File,
  onProgress: (loaded: number, total: number) => void,
  signal?: AbortSignal,
): Promise<OtaStatus> {
  return new Promise((resolve, reject) => {
    const request = new XMLHttpRequest();
    request.open("POST", "/api/system/ota");
    request.setRequestHeader("Content-Type", "application/octet-stream");
    request.timeout = 180000;

    request.upload.addEventListener("progress", (event) => {
      onProgress(event.loaded, event.lengthComputable ? event.total : file.size);
    });

    request.addEventListener("load", () => {
      let payload: unknown;
      try {
        payload = JSON.parse(request.responseText) as unknown;
      } catch {
        payload = undefined;
      }

      if (request.status >= 200 && request.status < 300) {
        resolve(payload as OtaStatus);
        return;
      }

      const body = payload as { error?: { code?: string; message?: string } } | undefined;
      reject(
        new ApiError(
          body?.error?.message ?? `上传失败 (${request.status})`,
          request.status,
          body?.error?.code ?? `http_${request.status}`,
        ),
      );
    });

    request.addEventListener("error", () => {
      reject(new ApiError("上传过程中与设备断开", 0, "offline"));
    });
    request.addEventListener("timeout", () => {
      reject(new ApiError("上传超时", 0, "timeout"));
    });
    request.addEventListener("abort", () => {
      reject(new ApiError("上传已取消", 0, "aborted"));
    });

    signal?.addEventListener("abort", () => request.abort(), { once: true });
    request.send(file);
  });
}

/** 固件文件的基本校验，避免把明显不对的文件推上设备。 */
export function validateFirmwareFile(file: File, maxImageSize: number): string | undefined {
  if (file.size === 0) {
    return "文件为空";
  }
  if (maxImageSize > 0 && file.size > maxImageSize) {
    return "固件超出 OTA 分区容量";
  }
  if (!file.name.endsWith(".bin")) {
    return "请选择 .bin 固件文件";
  }
  return undefined;
}
