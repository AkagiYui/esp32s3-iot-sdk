import { ErrorBoundary, onCleanup, onMount, Show } from "solid-js";
import "./lib/theme";
import ConnectionBanner from "./components/ConnectionBanner";
import DialogHost from "./components/DialogHost";
import NavBar from "./components/NavBar";
import RouteView from "./components/RouteView";
import ToastHost from "./components/ToastHost";
import LoginGate from "./components/LoginGate";
import { DEVICE_STATE_LABELS, subscribeDevice, systemInfo } from "./lib/device";
import { refreshAuthStatus } from "./lib/session";
import { cx } from "./lib/cx";
import styles from "./App.module.css";

function stateTone(state: string | undefined): string | undefined {
  if (state === "online") return styles.online;
  if (state === "provisioning") return styles.provisioning;
  if (state === "offline") return styles.offline;
  return undefined;
}

export default function App() {
  onMount(() => {
    // 全局只订阅一次设备状态轮询，页面从中读取，避免每个页面各拉一份。
    onCleanup(subscribeDevice());
  });

  // 鉴权状态是未鉴权就能读的，界面靠它决定该显示"设置密码"还是"登录"。
  onMount(() => {
    void refreshAuthStatus();
  });

  const deviceName = () => systemInfo()?.device.name ?? "掌上设备控制台";
  const deviceState = () => systemInfo()?.device.state;

  return (
    <>
      <div class={styles.shell}>
        <header class={styles.topbar}>
          <div class={styles.titleBlock}>
            <p class={styles.eyebrow}>ESP32-S3 Embedded Web</p>
            <h1>{deviceName()}</h1>
          </div>
          <div class={cx(styles.statusPill, stateTone(deviceState()))}>
            <Show when={deviceState()} fallback="连接中">
              {(state) => DEVICE_STATE_LABELS[state()] ?? state()}
            </Show>
          </div>
        </header>

        <ConnectionBanner />

        <ErrorBoundary
          fallback={(error, reset) => (
            <section class={styles.crash}>
              <h2>页面出错了</h2>
              <p>{error instanceof Error ? error.message : String(error)}</p>
              <button onClick={reset}>重新加载</button>
            </section>
          )}
        >
          <RouteView />
        </ErrorBoundary>

        <NavBar />
      </div>

      <LoginGate />
      <DialogHost />
      <ToastHost />
    </>
  );
}
