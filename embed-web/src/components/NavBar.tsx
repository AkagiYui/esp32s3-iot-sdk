import { For } from "solid-js";
import { Dynamic } from "solid-js/web";
import { routeEntries } from "@/lib/route-manifest";
import { navigate, route } from "@/lib/router";
import { cx } from "@/lib/cx";
import styles from "./NavBar.module.css";

export default function NavBar() {
  return (
    <nav class={styles.navBar}>
      <For each={routeEntries}>
        {(tab) => (
          <button
            class={cx(styles.navItem, route() === tab.path && styles.active)}
            onClick={() => navigate(tab.path)}
            aria-label={tab.label}
            aria-current={route() === tab.path ? "page" : undefined}
          >
            <Dynamic component={tab.icon} size={22} />
            <span class={styles.navLabel}>{tab.label}</span>
          </button>
        )}
      </For>
    </nav>
  );
}
