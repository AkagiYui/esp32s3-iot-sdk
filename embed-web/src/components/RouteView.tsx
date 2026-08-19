import { createEffect, For, on, onCleanup, Show } from "solid-js";
import { createStore, produce } from "solid-js/store";
import { Dynamic } from "solid-js/web";
import { routeMap } from "@/lib/route-manifest";
import { route } from "@/lib/router";
import { cx } from "@/lib/cx";
import styles from "./RouteView.module.css";

/** 出场动画时长，需与 RouteView.module.css 里 .leave 的 animation-duration 保持一致。 */
const PAGE_LEAVE_MS = 200;

const FALLBACK_MESSAGE = "页面加载失败";

type LayerState = "initial" | "enter" | "leave";

type Layer = {
  id: number;
  path: string;
  state: LayerState;
};

/**
 * 页面容器：新旧页面在同一个 grid 单元格里交叠，旧页面播完出场动画后才卸载。
 *
 * 之所以用 store 逐字段更新而不是替换数组元素，是为了让 `<For>` 复用已有 DOM——
 * 离场中的页面必须保持原实例，否则会重新挂载并重新拉取数据。
 */
export default function RouteView() {
  let nextId = 1;

  const [layers, setLayers] = createStore<Layer[]>([
    // 首屏不播入场动画，和之前 mount({ intro: false }) 的表现一致
    { id: 0, path: route(), state: "initial" },
  ]);

  const timers = new Set<ReturnType<typeof setTimeout>>();

  createEffect(
    on(
      route,
      (path) => {
        const leavingIds = layers.map((layer) => layer.id);

        setLayers(
          produce((list) => {
            for (const layer of list) {
              layer.state = "leave";
            }
            list.push({ id: nextId++, path, state: "enter" });
          }),
        );

        const timer = setTimeout(() => {
          timers.delete(timer);
          setLayers(
            produce((list) => {
              for (let index = list.length - 1; index >= 0; index--) {
                if (leavingIds.includes(list[index]!.id)) {
                  list.splice(index, 1);
                }
              }
            }),
          );
        }, PAGE_LEAVE_MS);

        timers.add(timer);
      },
      { defer: true },
    ),
  );

  onCleanup(() => {
    for (const timer of timers) {
      clearTimeout(timer);
    }
    timers.clear();
  });

  return (
    <main class={styles.appContent}>
      <For each={layers}>
        {(layer) => (
          <div
            class={cx(
              styles.pageWrapper,
              layer.state === "enter" && styles.enter,
              layer.state === "leave" && styles.leave,
            )}
          >
            <Show
              when={routeMap.get(layer.path)}
              keyed
              fallback={<section class={styles.routeError}>{FALLBACK_MESSAGE}</section>}
            >
              {(entry) => <Dynamic component={entry.component} />}
            </Show>
          </div>
        )}
      </For>
    </main>
  );
}
