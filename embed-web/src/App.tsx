import "./lib/theme";
import DialogHost from "./components/DialogHost";
import NavBar from "./components/NavBar";
import RouteView from "./components/RouteView";
import ToastHost from "./components/ToastHost";
import styles from "./App.module.css";

export default function App() {
  return (
    <>
      <div class={styles.shell}>
        <header class={styles.topbar}>
          <div class={styles.titleBlock}>
            <p class={styles.eyebrow}>ESP32-S3 Embedded Web</p>
            <h1>掌上设备控制台</h1>
          </div>
          <div class={styles.statusPill}>在线</div>
        </header>

        <RouteView />

        <NavBar />
      </div>

      <DialogHost />
      <ToastHost />
    </>
  );
}
