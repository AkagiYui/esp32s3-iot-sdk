import { createSignal, Show } from "solid-js";
import { KeyRound } from "lucide-solid";
import { authRequired, setApiToken } from "@/lib/auth";
import { refreshSystemInfo } from "@/lib/device";
import Button from "./Button";
import styles from "./TokenGate.module.css";

/**
 * 令牌输入闸门。
 *
 * 设备联入局域网后接口需要令牌；换一台设备或清了浏览器数据的用户手上没有令牌，
 * 这里给出恢复途径：从已授权的浏览器里复制、看串口日志，或长按 BOOT 键 5 秒
 * 让设备回到配网模式重新领取。
 */
export default function TokenGate() {
  const [value, setValue] = createSignal("");
  const [submitting, setSubmitting] = createSignal(false);

  async function submit(event: Event) {
    event.preventDefault();
    if (!value().trim()) {
      return;
    }

    setSubmitting(true);
    setApiToken(value());
    try {
      await refreshSystemInfo();
    } finally {
      setSubmitting(false);
      setValue("");
    }
  }

  return (
    <Show when={authRequired()}>
      <div class={styles.overlay} role="dialog" aria-modal="true" aria-label="需要访问令牌">
        <form class={styles.panel} onSubmit={(event) => void submit(event)}>
          <span class={styles.icon}>
            <KeyRound size={22} />
          </span>
          <h2>需要访问令牌</h2>
          <p class={styles.hint}>
            设备已接入局域网，接口需要令牌才能访问。可以从已授权的浏览器里复制，
            或在串口日志中查看；都拿不到时长按 BOOT 键 5 秒回到配网模式重新领取。
          </p>
          <input
            class={styles.input}
            type="password"
            value={value()}
            placeholder="32 位十六进制令牌"
            autocomplete="off"
            autocapitalize="off"
            autocorrect="off"
            spellcheck={false}
            onInput={(event) => setValue(event.currentTarget.value)}
          />
          <Button
            type="submit"
            variant="primary"
            block
            loading={submitting()}
            disabled={!value().trim()}
          >
            解锁
          </Button>
        </form>
      </div>
    </Show>
  );
}
