import { routeMap, routePaths } from './route-manifest';

export type Route = (typeof routePaths)[number];

const isBrowser = typeof window !== 'undefined';

function parseHash(): Route {
  if (!isBrowser) return '/';
  const hash = window.location.hash.slice(1) || '/';
  return routeMap.has(hash) ? (hash as Route) : '/';
}

let current = $state<Route>(parseHash());

if (isBrowser) {
  window.addEventListener('hashchange', () => {
    current = parseHash();
  });
}

export function navigate(path: Route) {
  window.location.hash = path;
}

/** @internal Used by the prerender script to set the route for SSR */
export function __setSSRRoute(route: Route) {
  current = route;
}

export function getRoute(): Route {
  return current;
}

export function getCurrentRouteEntry() {
  return routeMap.get(current) ?? routeMap.get('/');
}
