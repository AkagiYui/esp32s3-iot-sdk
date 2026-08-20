import { createSignal, onCleanup, onMount, Show } from "solid-js";
import {
  Bug,
  CircleCheck,
  HardDriveUpload,
  RefreshCw,
  RotateCw,
  Trash2,
  TriangleAlert,
  Upload,
} from "lucide-solid";
import Button from "@/components/Button";
import Card from "@/components/Card";
import InfoRow from "@/components/InfoRow";
import Meter from "@/components/Meter";
import { describeError } from "@/lib/api";
import { rebootDevice, refreshSystemInfo, systemInfo } from "@/lib/device";
import { showConfirm, showToast } from "@/lib/feedback";
import {
  confirmFirmware,
  eraseCoredump,
  fetchCoredump,
  fetchOtaStatus,
  uploadFirmware,
  validateFirmwareFile,
  type OtaStatus,
} from "@/lib/ota";
import { formatBytes } from "@/lib/format";
import type { RouteMeta } from "@/lib/route-manifest";
import styles from "./firmware.module.css";

export const routeMeta: RouteMeta = {
  label: "固件",
  icon: HardDriveUpload,
  order: 3,
};

export default function FirmwarePage() {
  const [status, setStatus] = createSignal<OtaStatus | undefined>(undefined);
  const [file, setFile] = createSignal<File | undefined>(undefined);
  const [uploading, setUploading] = createSignal(false);
  const [sent, setSent] = createSignal(0);
  const [refreshing, setRefreshing] = createSignal(false);
  const [coredump, setCoredump] = createSignal(false);
  const [erasing, setErasing] = createSignal(false);

  let controller: AbortController | undefined;

  async function loadStatus() {
    setRefreshing(true);
    try {
      const [ota, dump] = await Promise.all([fetchOtaStatus(), fetchCoredump()]);
      setStatus(ota);
      setCoredump(dump.present);
    } catch (error) {
      showToast(describeError(error), "error");
    } finally {
      setRefreshing(false);
    }
  }

  async function dropCoredump() {
    if ((await showConfirm("崩溃现场将被永久删除。", "擦除崩溃现场", true)) !== "ok") {
      return;
    }

    setErasing(true);
    try {
      setCoredump((await eraseCoredump()).present);
      showToast("崩溃现场已擦除", "success");
    } catch (error) {
      showToast(describeError(error), "error");
    } finally {
      setErasing(false);
    }
  }

  function pickFile(event: Event) {
    const input = event.currentTarget as HTMLInputElement;
    const selected = input.files?.[0];
    if (!selected) {
      return;
    }

    const problem = validateFirmwareFile(selected, status()?.max_image_size ?? 0);
    if (problem) {
      showToast(problem, "warning");
      input.value = "";
      return;
    }

    setFile(selected);
    setSent(0);
  }

  async function upload() {
    const selected = file();
    if (!selected) {
      return;
    }

    const confirmed = await showConfirm(
      `将向设备写入 ${selected.name}（${formatBytes(selected.size)}）。升级期间请不要断电。`,
      "开始升级",
    );
    if (confirmed !== "ok") {
      return;
    }

    controller = new AbortController();
    setUploading(true);
    setSent(0);
    try {
      const result = await uploadFirmware(selected, (loaded) => setSent(loaded), controller.signal);
      setStatus(result);
      showToast("固件已写入，重启后生效", "success");
    } catch (error) {
      showToast(describeError(error), "error");
      void loadStatus();
    } finally {
      setUploading(false);
      controller = undefined;
    }
  }

  async function reboot() {
    try {
      await rebootDevice();
      showToast("设备正在重启，请稍后刷新页面", "info");
    } catch (error) {
      showToast(describeError(error), "error");
    }
  }

  async function confirm() {
    try {
      setStatus(await confirmFirmware());
      showToast("已确认当前固件，不会再回滚", "success");
      void refreshSystemInfo();
    } catch (error) {
      showToast(describeError(error), "error");
    }
  }

  onMount(() => void loadStatus());
  onCleanup(() => controller?.abort());

  return (
    <div class="page">
      <div class="page-header">
        <h1>固件</h1>
        <p class="subtitle">上传新固件并管理回滚</p>
      </div>

      <Show when={status()?.awaiting_confirm}>
        <Card
          icon={TriangleAlert}
          title="当前固件待确认"
          subtitle="不确认的话，下次重启会自动回滚到上一个版本"
        >
          <Button variant="primary" icon={CircleCheck} block onClick={() => void confirm()}>
            确认当前固件可用
          </Button>
        </Card>
      </Show>

      <Card
        title="当前固件"
        actions={
          <Button
            variant="ghost"
            size="sm"
            icon={RefreshCw}
            loading={refreshing()}
            onClick={() => void loadStatus()}
            aria-label="刷新升级状态"
          />
        }
      >
        <InfoRow label="版本号" value={`#${systemInfo()?.firmware.version ?? "—"}`} />
        <InfoRow label="提交" value={systemInfo()?.firmware.name} mono />
        <InfoRow label="构建时间" value={systemInfo()?.firmware.build_time} mono />
        <InfoRow label="运行分区" value={status()?.running_partition} mono />
        <InfoRow
          label="下次启动分区"
          value={status()?.boot_partition}
          mono
          tone={
            status() && status()!.boot_partition !== status()!.running_partition
              ? "accent"
              : "default"
          }
        />
        <InfoRow label="OTA 分区容量" value={formatBytes(status()?.max_image_size ?? 0)} />
      </Card>

      <Card
        icon={Bug}
        title="崩溃现场"
        subtitle={
          coredump()
            ? "设备上存有一份 coredump，可用 espcoredump.py 读出后分析"
            : "上次启动以来没有记录到崩溃"
        }
      >
        <InfoRow
          label="状态"
          value={coredump() ? "存在" : "无"}
          tone={coredump() ? "danger" : "muted"}
        />
        <Show when={coredump()}>
          <Button
            variant="secondary"
            icon={Trash2}
            block
            loading={erasing()}
            onClick={() => void dropCoredump()}
          >
            擦除崩溃现场
          </Button>
        </Show>
      </Card>

      <Card icon={Upload} title="上传固件" subtitle="选择 idf.py build 产出的应用镜像（.bin）">
        <input
          class={styles.fileInput}
          type="file"
          accept=".bin"
          disabled={uploading()}
          onChange={pickFile}
        />

        <Show when={file()}>
          {(selected) => (
            <InfoRow label={selected().name} value={formatBytes(selected().size)} mono />
          )}
        </Show>

        <Show when={uploading() || sent() > 0}>
          <Meter
            label="上传进度"
            used={sent()}
            total={file()?.size ?? 1}
            warnAt={101}
            detail={`${formatBytes(sent())} / ${formatBytes(file()?.size ?? 0)}`}
          />
        </Show>

        <Show
          when={!uploading()}
          fallback={
            <Button variant="danger" block onClick={() => controller?.abort()}>
              取消上传
            </Button>
          }
        >
          <Button
            variant="primary"
            icon={Upload}
            block
            disabled={!file()}
            onClick={() => void upload()}
          >
            开始升级
          </Button>
        </Show>

        <Show when={status()?.state === "ready"}>
          <Button variant="secondary" icon={RotateCw} block onClick={() => void reboot()}>
            重启以应用新固件
          </Button>
        </Show>

        <Show when={status()?.state === "failed"}>
          <p class={styles.failure}>{status()?.message}</p>
        </Show>
      </Card>
    </div>
  );
}
