import { createSignal } from "solid-js";
import { routeMap, type RouteEntry } from "./route-manifest";

export const HOME_PATH = "/";

/**
 * 解析 `location.hash`。未知路径回落到首页，
 * 保证刷新、直接访问、收藏链接都能恢复出合法路由。
 */
export function parseHash(hash: string): string {
  const path = hash.replace(/^#/, "") || HOME_PATH;
  return routeMap.has(path) ? path : HOME_PATH;
}

const [route, setRoute] = createSignal(parseHash(window.location.hash));

window.addEventListener("hashchange", () => {
  setRoute(parseHash(window.location.hash));
});

export { route };

export function navigate(path: string): void {
  window.location.hash = path;
}

export function currentRouteEntry(): RouteEntry | undefined {
  return routeMap.get(route()) ?? routeMap.get(HOME_PATH);
}
