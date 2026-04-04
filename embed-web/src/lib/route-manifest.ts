import type { Component } from 'svelte';

type PageModule = {
  default: Component;
  routeMeta?: RouteMeta;
};

export type RouteIcon = 'home' | 'dashboard' | 'settings';

export type RouteMeta = {
  icon: RouteIcon;
  label: string;
  order: number;
};

export type RouteEntry = {
  component: Component;
  filePath: string;
  icon: RouteIcon;
  label: string;
  order: number;
  path: string;
};

const pageModules = import.meta.glob<PageModule>('../pages/**/*.svelte', {
  eager: true,
});

function stripPagesPrefix(filePath: string): string {
  return filePath.replace(/^\.\.\/pages\//, '').replace(/\.svelte$/, '');
}

function routeKeyFromFilePath(filePath: string): string {
  return stripPagesPrefix(filePath)
    .split('/')
    .map((segment) => segment.trim())
    .filter(Boolean)
    .join('/');
}

function routePathFromFilePath(filePath: string): string {
  const routeKey = routeKeyFromFilePath(filePath);
  const normalized = routeKey
    .split('/')
    .flatMap((segment) => segment.split('.'))
    .map((segment) => segment.trim().toLowerCase())
    .filter(Boolean);

  if (normalized.length === 1 && normalized[0] === 'home') {
    return '/';
  }

  if (normalized.at(-1) === 'index') {
    normalized.pop();
  }

  return normalized.length === 0 ? '/' : `/${normalized.join('/')}`;
}

function defaultLabelFromPath(path: string): string {
  if (path === '/') {
    return '首页';
  }

  return path
    .slice(1)
    .split('/')
    .map((segment) => segment.charAt(0).toUpperCase() + segment.slice(1))
    .join(' / ');
}

function defaultMeta(path: string): RouteMeta {
  return {
    icon: 'home',
    label: defaultLabelFromPath(path),
    order: 999,
  };
}

export const routeEntries: RouteEntry[] = Object.entries(pageModules)
  .map(([filePath, module]) => {
    const path = routePathFromFilePath(filePath);
    const meta = module.routeMeta ?? defaultMeta(path);

    return {
      component: module.default,
      filePath,
      icon: meta.icon,
      label: meta.label,
      order: meta.order,
      path,
    } satisfies RouteEntry;
  })
  .sort((left, right) => left.order - right.order || left.path.localeCompare(right.path));

export const routeMap = new Map(routeEntries.map((entry) => [entry.path, entry]));
export const routePaths = routeEntries.map((entry) => entry.path);
