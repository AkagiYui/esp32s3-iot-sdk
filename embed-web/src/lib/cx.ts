/**
 * 拼接 class 名，过滤掉假值。
 *
 * 项目同时使用两层样式：`app.css` 里的全局排版原语（`page` / `page-header` / `subtitle`）
 * 和每个组件自己的 CSS Module，`cx` 让两者的组合保持可读。
 */
export function cx(...values: Array<string | false | null | undefined>): string {
  return values.filter(Boolean).join(" ");
}
