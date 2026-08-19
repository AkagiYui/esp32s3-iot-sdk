import { createEffect, createSignal, on, onCleanup, type Accessor } from "solid-js";

export type Presence = {
  /** 元素是否应该留在 DOM 中（出场动画期间仍为 true）。 */
  mounted: Accessor<boolean>;
  /** 是否正在播放出场动画。 */
  exiting: Accessor<boolean>;
};

/**
 * 让元素在 `show()` 变为 false 后继续挂载 `exitMs`，用于播放纯 CSS 出场动画。
 *
 * Solid 没有内置的 transition 指令，这个原语替代了 Svelte 的 `transition:` 行为。
 */
export function createPresence(show: Accessor<boolean>, exitMs: number): Presence {
  const [mounted, setMounted] = createSignal(show());
  const [exiting, setExiting] = createSignal(false);

  let timer: ReturnType<typeof setTimeout> | undefined;

  createEffect(
    on(
      show,
      (visible) => {
        clearTimeout(timer);

        if (visible) {
          setExiting(false);
          setMounted(true);
          return;
        }

        if (!mounted()) {
          return;
        }

        setExiting(true);
        timer = setTimeout(() => {
          setMounted(false);
          setExiting(false);
        }, exitMs);
      },
      { defer: true },
    ),
  );

  onCleanup(() => clearTimeout(timer));

  return { mounted, exiting };
}
