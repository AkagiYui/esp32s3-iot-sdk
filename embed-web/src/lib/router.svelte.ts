import { routeMap, routePaths } from './route-manifest';

export type Route = (typeof routePaths)[number];

let current = $state<Route>(parseHash());

function parseHash(): Route {
  const hash = window.location.hash.slice(1) || '/';
  return routeMap.has(hash) ? (hash as Route) : '/';
}

window.addEventListener('hashchange', () => {
  current = parseHash();
});

export function navigate(path: Route) {
  window.location.hash = path;
}

export function getRoute(): Route {
  return current;
}

export function getCurrentRouteEntry() {
  return routeMap.get(current) ?? routeMap.get('/');
}
