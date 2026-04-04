# Svelte + TS + Vite

此模板旨在帮助您在 Vite 环境中开始使用 Svelte 和 TypeScript 进行开发。

## 推荐的 IDE 设置

[VS Code](https://code.visualstudio.com/) + [Svelte](https://marketplace.visualstudio.com/items?itemName=svelte.svelte-vscode)。

## 需要官方的 Svelte 框架吗？

查看 [SvelteKit](https://github.com/sveltejs/kit#readme)，它也是由 Vite 提供支持的。利用其无服务器优先的方法，您可以将应用部署到任何地方并适配各种平台，开箱即支持 TypeScript、SCSS 和 Less，以及易于添加的 mdsvex、GraphQL、PostCSS、Tailwind CSS 等支持。

## 技术考量

**为什么要使用这个而不是 SvelteKit？**

- 它自带路由解决方案，这可能对某些用户来说并不是最佳选择。
- 它首先是一个框架，只是恰好内部使用 Vite，而不是一个 Vite 应用。

该模板包含了尽可能少的内容，以便您开始使用 Vite + TypeScript + Svelte，同时考虑了 HMR（热模块替换）和智能提示的开发体验。它展示了与其他 `create-vite` 模板相当的功能，并且是初学者涉足 Vite + Svelte 项目的良好起点。

如果您以后需要 SvelteKit 提供的扩展功能和可扩展性，该模板的结构与 SvelteKit 相似，因此易于迁移。

**为什么要使用 `global.d.ts` 而不是 `jsconfig.json` 或 `tsconfig.json` 中的 `compilerOptions.types`？**

设置 `compilerOptions.types` 会屏蔽配置中未明确列出的所有其他类型。使用三斜线引用可以保持 TypeScript 接受整个工作区类型信息的默认设置，同时添加 `svelte` 和 `vite/client` 类型信息。

**为什么要包含 `.vscode/extensions.json`？**

其他模板通过 README 间接推荐扩展，但此文件允许 VS Code 在打开项目时提示用户安装推荐的扩展。

**为什么要启用 TS 模板中的 `allowJs`？**

虽然 `allowJs: false` 确实会阻止在项目中使用 `.js` 文件，但它不会阻止在 `.svelte` 文件中使用 JavaScript 语法。此外，它还会强制 `checkJs: false`，这会导致最坏的结果：既无法保证整个代码库都是 TypeScript，也使得现有 JavaScript 的类型检查变得更差。此外，也存在混合代码库有效的用例。

**为什么 HMR 没有保留我的本地组件状态？**

HMR 状态保留是有很多棘手问题的！由于其行为往往出人意料，它已经在 `svelte-hmr` 和 `@sveltejs/vite-plugin-svelte` 中被默认禁用。您可以在 [这里](https://github.com/rixo/svelte-hmr#svelte-hmr) 阅读详情。

如果您有在组件中需要保留的重要状态，请考虑创建一个外部 store，这样它不会被 HMR 替换。

```ts
// store.ts
// An extremely simple external store
import { writable } from 'svelte/store'
export default writable(0)
```