import { createSignal, Show } from "solid-js";
import { KeyRound, TriangleAlert } from "lucide-solid";
import { describeError } from "@/lib/api";
import { authRequired } from "@/lib/auth";
import { authStatus, login } from "@/lib/session";
import Button from "./Button";
import styles from "./LoginGate.module.css";

/**
 * 登录闸门。
 *
 * 设备联入局域网后接口需要登录。密码是用户自己在配网时设的，所以换手机、换电脑、
 * 清了浏览器数据都只要重新输一遍即可——这正是当初用随机令牌做不到的事。
 * 忘了密码的找回途径：长按 BOOT 键 5 秒回到配网模式，在那里重设一个新的。
 */
export default function LoginGate() {
  const [password, setPassword] = createSignal("");
  const [error, setError] = createSignal("");
  const [submitting, setSubmitting] = createSignal(false);

  const notConfigured = () => authStatus()?.configured === false;

  async function submit(event: Event) {
    event.preventDefault();
    if (!password()) {
      return;
    }

    setSubmitting(true);
    setError("");
    try {
      await login(password());
      setPassword("");
    } catch (caught) {
      setError(describeError(caught));
    } finally {
      setSubmitting(false);
    }
  }

  return (
    <Show when={authRequired()}>
      <div class={styles.overlay} role="dialog" aria-modal="true" aria-label="需要登录">
        <Show
          when={!notConfigured()}
          fallback={
            <div class={styles.panel}>
              <span class={styles.icon}>
                <TriangleAlert size={22} />
              </span>
              <h2>设备尚未设置访问密码</h2>
              <p class={styles.hint}>
                这台设备还没有走完配网流程。长按 BOOT 键 5 秒让它回到配网模式，
                连上它的热点后设置一个访问密码。
              </p>
            </div>
          }
        >
          <form class={styles.panel} onSubmit={(event) => void submit(event)}>
            <span class={styles.icon}>
              <KeyRound size={22} />
            </span>
            <h2>请输入访问密码</h2>
            <p class={styles.hint}>
              配网时设置的那个密码。忘记了的话，长按 BOOT 键 5 秒回到配网模式，
              在那里可以直接重设，不需要旧密码。
            </p>
            <input
              class={styles.input}
              type="password"
              value={password()}
              placeholder="访问密码"
              autocomplete="current-password"
              onInput={(event) => setPassword(event.currentTarget.value)}
            />
            <Show when={error()}>{(message) => <p class={styles.error}>{message()}</p>}</Show>
            <Button
              type="submit"
              variant="primary"
              block
              loading={submitting()}
              disabled={!password()}
            >
              登录
            </Button>
          </form>
        </Show>
      </div>
    </Show>
  );
}
