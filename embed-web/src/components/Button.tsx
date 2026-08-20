import { Show, splitProps, type JSX } from "solid-js";
import { Dynamic } from "solid-js/web";
import { LoaderCircle } from "lucide-solid";
import type { IconComponent } from "@/lib/route-manifest";
import { cx } from "@/lib/cx";
import styles from "./Button.module.css";

type ButtonProps = JSX.ButtonHTMLAttributes<HTMLButtonElement> & {
  variant?: "primary" | "secondary" | "danger" | "ghost";
  size?: "md" | "sm";
  icon?: IconComponent;
  loading?: boolean;
  block?: boolean;
};

/** 统一的按钮：带图标位、加载态，加载时自动禁用避免重复提交。 */
export default function Button(props: ButtonProps) {
  const [local, rest] = splitProps(props, [
    "variant",
    "size",
    "icon",
    "loading",
    "block",
    "class",
    "children",
    "disabled",
  ]);

  return (
    <button
      {...rest}
      class={cx(
        styles.button,
        styles[local.variant ?? "secondary"],
        local.size === "sm" && styles.sm,
        local.block && styles.block,
        local.class,
      )}
      disabled={local.disabled || local.loading}
    >
      <Show
        when={local.loading}
        fallback={
          <Show when={local.icon}>
            {(icon) => <Dynamic component={icon()} size={local.size === "sm" ? 14 : 16} />}
          </Show>
        }
      >
        <LoaderCircle size={local.size === "sm" ? 14 : 16} class={styles.spinner} />
      </Show>
      <Show when={local.children}>{(children) => <span>{children()}</span>}</Show>
    </button>
  );
}
