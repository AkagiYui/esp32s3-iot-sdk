import { resolve } from "node:path";
import { defineConfig, lazyPlugins } from "vite-plus";
import { svelte } from "@sveltejs/vite-plugin-svelte";
import { viteSingleFile } from "vite-plugin-singlefile";
import { precompress } from "./plugins/vite-plugin-precompress";
import { viteBundleSize } from "./plugins/vite-plugin-bundle-size";

// Vite+ keeps the whole toolchain (dev/build/lint/fmt/test) in this single file.
// https://viteplus.dev/config/
export default defineConfig({
  plugins: lazyPlugins(() => [svelte(), viteSingleFile(), precompress(), viteBundleSize()]),

  resolve: {
    alias: {
      "@": resolve(__dirname, "src"),
    },
  },

  fmt: {
    ignorePatterns: ["dist/**"],
  },

  lint: {
    ignorePatterns: ["dist/**"],
    jsPlugins: [{ name: "vite-plus", specifier: "vite-plus/oxlint-plugin" }],
    rules: {
      "vite-plus/prefer-vite-plus-imports": "error",
    },
    options: {
      typeAware: true,
      typeCheck: true,
    },
  },

  staged: {
    "*.{js,ts,svelte,css,json}": "vp check --fix",
  },
});
