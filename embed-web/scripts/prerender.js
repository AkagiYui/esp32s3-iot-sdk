/**
 * Post-build SSG prerender script.
 *
 * 1. Uses Vite's SSR module loader to compile & import page components
 * 2. Renders each SSG-marked route with Svelte's server-side `render()`
 * 3. Injects the pre-rendered HTML into dist/index.html as <template> elements
 * 4. Adds a tiny inline bootstrap script that displays the matching template
 *    BEFORE any JS bundle loads — giving the user an instant page.
 * 5. When Svelte loads it calls `hydrate()` to reuse the existing DOM.
 */

import { createServer } from 'vite';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(__dirname, '..');

async function prerender() {
  // Spin up a Vite dev server in middleware mode (no HTTP listener)
  // so we can use ssrLoadModule to compile Svelte components for SSR.
  const vite = await createServer({
    root,
    server: { middlewareMode: true },
    appType: 'custom',
    logLevel: 'warn',
  });

  try {
    // Load render through ssrLoadModule so it shares the same svelte
    // internal state as the SSR-compiled components.
    const { render } = await vite.ssrLoadModule('svelte/server');

    // Load the router (so we can set the active route for each render)
    const router = await vite.ssrLoadModule('/src/lib/router.svelte.ts');
    const { routeEntries } = await vite.ssrLoadModule('/src/lib/route-manifest.ts');
    const { default: App } = await vite.ssrLoadModule('/src/App.svelte');

    const ssgRoutes = routeEntries.filter((e) => e.ssg);

    if (ssgRoutes.length === 0) {
      console.log('SSG: no routes marked with ssg — skipping.');
      return;
    }

    // Read the built index.html
    const distIndex = path.resolve(root, 'dist/index.html');
    let html = fs.readFileSync(distIndex, 'utf-8');

    // Render each SSG route
    const templates = [];
    for (const route of ssgRoutes) {
      router.__setSSRRoute(route.path);
      const result = render(App);
      templates.push(
        `<template data-ssg-route="${route.path}">${result.html}</template>`
      );
      console.log(`SSG: rendered ${route.path}`);
    }

    // Inline bootstrap script — runs synchronously before module scripts.
    // It checks the hash, finds the matching <template>, and clones its
    // content into #app so the user sees the page immediately.
    const bootstrap = [
      '<script>',
      '!function(){',
      '  var h=location.hash.slice(1)||"/";',
      '  var t=document.querySelector(\'template[data-ssg-route="\'+h+\'"]\');',
      '  if(t){',
      '    var a=document.getElementById("app");',
      '    if(a){a.appendChild(t.content.cloneNode(!0));a.dataset.ssgHydrate=""}',
      '  }',
      '}()',
      '</script>',
    ].join('');

    // Inject templates + bootstrap before </body>
    const injection = '\n' + templates.join('\n') + '\n' + bootstrap + '\n';
    html = html.replace('</body>', injection + '</body>');

    fs.writeFileSync(distIndex, html);
    console.log(`SSG: pre-rendered ${ssgRoutes.length} route(s) ✔`);
  } finally {
    await vite.close();
  }
}

prerender().catch((err) => {
  console.error('SSG prerender failed:', err);
  process.exit(1);
});
