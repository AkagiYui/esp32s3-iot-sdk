import { Show, type JSX } from "solid-js";
import { Dynamic } from "solid-js/web";
import type { IconComponent } from "@/lib/route-manifest";
import { cx } from "@/lib/cx";
import styles from "./Card.module.css";

type CardProps = {
  title?: string;
  subtitle?: string;
  icon?: IconComponent;
  actions?: JSX.Element;
  class?: string;
  children: JSX.Element;
};

/** 页面里所有分组容器的统一外观，避免每个页面各写一套卡片样式。 */
export default function Card(props: CardProps) {
  return (
    <section class={cx(styles.card, props.class)}>
      <Show when={props.title || props.actions}>
        <header class={styles.header}>
          <div class={styles.titleBlock}>
            <Show when={props.icon}>
              {(icon) => (
                <span class={styles.icon}>
                  <Dynamic component={icon()} size={18} />
                </span>
              )}
            </Show>
            <div>
              <Show when={props.title}>{(title) => <h2>{title()}</h2>}</Show>
              <Show when={props.subtitle}>
                {(subtitle) => <p class={styles.subtitle}>{subtitle()}</p>}
              </Show>
            </div>
          </div>
          <Show when={props.actions}>
            {(actions) => <div class={styles.actions}>{actions()}</div>}
          </Show>
        </header>
      </Show>
      {props.children}
    </section>
  );
}
