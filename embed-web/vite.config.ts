import { resolve } from "node:path";
import { defineConfig, lazyPlugins } from "vite-plus";
import solid from "vite-plugin-solid";
import { viteSingleFile } from "vite-plugin-singlefile";
import { precompress } from "./plugins/vite-plugin-precompress";
import { viteBundleSize } from "./plugins/vite-plugin-bundle-size";
import { devApiMock } from "./plugins/vite-plugin-dev-api-mock";

const isProduction = process.env.NODE_ENV === "production";

// Vite+ keeps the whole toolchain (dev / build / lint / fmt / test) in this single file.
// https://viteplus.dev/config/
export default defineConfig({
  plugins: lazyPlugins(() => [
    solid(),
    devApiMock(),
    viteSingleFile(),
    precompress(),
    viteBundleSize(),
  ]),

  resolve: {
    alias: {
      "@": resolve(__dirname, "src"),
    },
  },

  css: {
    modules: {
      // 产物要塞进设备 flash，生产构建下用最短的哈希类名；开发时保留可读名称。
      generateScopedName: isProduction ? "[hash:base64:5]" : "[name]__[local]",
    },
  },

  build: {
    target: "es2022",
    // 单文件产物已内联，chunk 体积告警没有意义
    chunkSizeWarningLimit: 4096,
  },

  test: {
    environment: "jsdom",
    include: ["src/**/*.test.{ts,tsx}"],
    setupFiles: ["./src/test/setup.ts"],
    // Solid 必须在测试里走 browser/development 条件，否则响应式不生效
    server: { deps: { inline: [/solid-js/, /@solidjs\/testing-library/] } },
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
    overrides: [
      {
        files: ["**/*.test.ts", "**/*.test.tsx"],
        rules: {
          // testing-library 的 render() 返回值就是这样解构使用的
          "typescript/unbound-method": "off",
        },
      },
    ],
    options: {
      typeAware: true,
      typeCheck: true,
    },
  },

  staged: {
    "*.{js,ts,tsx,css,json}": "vp check --fix",
  },
});
