const routes = ['/', '/dashboard', '/settings'] as const;
export type Route = (typeof routes)[number];

let current = $state<Route>(parseHash());

function parseHash(): Route {
  const hash = window.location.hash.slice(1) || '/';
  return routes.includes(hash as Route) ? (hash as Route) : '/';
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
