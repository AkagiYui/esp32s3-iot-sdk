import { Show, type JSX } from "solid-js";
import { cx } from "@/lib/cx";
import styles from "./InfoRow.module.css";

type InfoRowProps = {
  label: string;
  value?: JSX.Element;
  hint?: string;
  mono?: boolean;
  tone?: "default" | "accent" | "danger" | "muted";
  children?: JSX.Element;
};

/** 「标签 — 取值」一行，信息展示页面里出现频率最高的排版单元。 */
export default function InfoRow(props: InfoRowProps) {
  return (
    <div class={styles.row}>
      <div class={styles.labelBlock}>
        <span class={styles.label}>{props.label}</span>
        <Show when={props.hint}>{(hint) => <span class={styles.hint}>{hint()}</span>}</Show>
      </div>
      <Show
        when={props.children}
        fallback={
          <span
            class={cx(
              styles.value,
              props.mono && styles.mono,
              props.tone && props.tone !== "default" && styles[props.tone],
            )}
          >
            {props.value ?? "—"}
          </span>
        }
      >
        {(children) => <div class={styles.slot}>{children()}</div>}
      </Show>
    </div>
  );
}
