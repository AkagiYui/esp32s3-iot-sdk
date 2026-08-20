import { Show } from "solid-js";
import { percentage } from "@/lib/format";
import { cx } from "@/lib/cx";
import styles from "./Meter.module.css";

type MeterProps = {
  label: string;
  used: number;
  total: number;
  detail?: string;
  /** 超过该百分比时进度条转为告警色。 */
  warnAt?: number;
};

/** 容量/占用类指标的进度条。 */
export default function Meter(props: MeterProps) {
  const percent = () => percentage(props.used, props.total);
  const warning = () => percent() >= (props.warnAt ?? 85);

  return (
    <div class={styles.meter}>
      <div class={styles.meterHead}>
        <span class={styles.label}>{props.label}</span>
        <span class={cx(styles.percent, warning() && styles.warning)}>{percent()}%</span>
      </div>
      <div
        class={styles.track}
        role="progressbar"
        aria-valuenow={percent()}
        aria-valuemin={0}
        aria-valuemax={100}
      >
        <div
          class={cx(styles.fill, warning() && styles.warning)}
          style={{ width: `${percent()}%` }}
        />
      </div>
      <Show when={props.detail}>{(detail) => <span class={styles.detail}>{detail()}</span>}</Show>
    </div>
  );
}
