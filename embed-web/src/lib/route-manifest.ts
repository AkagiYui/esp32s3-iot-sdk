import type { Component, JSX } from "solid-js";
import { House, type LucideProps } from "lucide-solid";

/** lucide 图标组件，用于导航栏与页面标题区。 */
export type IconComponent = (props: LucideProps) => JSX.Element;

/** 页面模块通过 `export const routeMeta` 声明自己的导航元信息。 */
export type RouteMeta = {
  icon: IconComponent;
  label: string;
  order: number;
};

export type RouteEntry = RouteMeta & {
  component: Component;
  filePath: string;
  path: string;
};

type PageModule = {
  default: Component;
  routeMeta?: RouteMeta;
};

const pageModules = import.meta.glob<PageModule>(["../pages/**/*.tsx", "!**/*.test.tsx"], {
  eager: true,
});

function stripPagesPrefix(filePath: string): string {
  return filePath.replace(/^\.\.\/pages\//, "").replace(/\.tsx$/, "");
}

/**
 * 由页面文件路径推导 hash 路由：
 * `home.tsx` -> `/`，`network/wifi.tsx` -> `/network/wifi`，`device.logs.tsx` -> `/device/logs`。
 */
export function routePathFromFilePath(filePath: string): string {
  const segments = stripPagesPrefix(filePath)
    .split("/")
    .flatMap((segment) => segment.split("."))
    .map((segment) => segment.trim().toLowerCase())
    .filter(Boolean);

  if (segments.length === 1 && segments[0] === "home") {
    return "/";
  }

  if (segments.at(-1) === "index") {
    segments.pop();
  }

  return segments.length === 0 ? "/" : `/${segments.join("/")}`;
}

/** 页面没有导出 `routeMeta` 时的兜底标题。 */
export function defaultLabelFromPath(path: string): string {
  if (path === "/") {
    return "首页";
  }

  return path
    .slice(1)
    .split("/")
    .map((segment) => segment.charAt(0).toUpperCase() + segment.slice(1))
    .join(" / ");
}

function defaultMeta(path: string): RouteMeta {
  return {
    icon: House,
    label: defaultLabelFromPath(path),
    order: 999,
  };
}

export const routeEntries: RouteEntry[] = Object.entries(pageModules)
  .map(([filePath, module]) => {
    const path = routePathFromFilePath(filePath);
    const meta = module.routeMeta ?? defaultMeta(path);

    return {
      ...meta,
      component: module.default,
      filePath,
      path,
    } satisfies RouteEntry;
  })
  .sort((left, right) => left.order - right.order || left.path.localeCompare(right.path));

export const routeMap = new Map(routeEntries.map((entry) => [entry.path, entry]));
export const routePaths = routeEntries.map((entry) => entry.path);
