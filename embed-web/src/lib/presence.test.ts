import { afterEach, beforeEach, describe, expect, it, vi } from "vite-plus/test";
import { createRoot, createSignal, type Setter } from "solid-js";
import { createPresence, type Presence } from "./presence";

/**
 * Solid 的 createEffect 在当前更新周期结束后才执行，
 * 所以断言必须放在 createRoot 的同步函数体之外。
 */
function setup(initial: boolean, exitMs: number) {
  let presence!: Presence;
  let setShow!: Setter<boolean>;
  let dispose!: () => void;

  createRoot((disposeRoot) => {
    dispose = disposeRoot;
    const [show, set] = createSignal(initial);
    setShow = set;
    presence = createPresence(show, exitMs);
  });

  return { presence, setShow, dispose };
}

describe("createPresence", () => {
  beforeEach(() => {
    vi.useFakeTimers();
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it("初始可见时立即挂载", () => {
    const { presence, dispose } = setup(true, 200);

    expect(presence.mounted()).toBe(true);
    expect(presence.exiting()).toBe(false);
    dispose();
  });

  it("初始不可见时不挂载", () => {
    const { presence, dispose } = setup(false, 200);

    expect(presence.mounted()).toBe(false);
    dispose();
  });

  it("显示时立即挂载", () => {
    const { presence, setShow, dispose } = setup(false, 200);

    setShow(true);
    expect(presence.mounted()).toBe(true);
    expect(presence.exiting()).toBe(false);
    dispose();
  });

  it("隐藏后先保持挂载播动画，超时才卸载", () => {
    const { presence, setShow, dispose } = setup(true, 200);

    setShow(false);
    expect(presence.mounted()).toBe(true);
    expect(presence.exiting()).toBe(true);

    vi.advanceTimersByTime(199);
    expect(presence.mounted()).toBe(true);

    vi.advanceTimersByTime(1);
    expect(presence.mounted()).toBe(false);
    expect(presence.exiting()).toBe(false);
    dispose();
  });

  it("出场途中再次显示会取消卸载", () => {
    const { presence, setShow, dispose } = setup(true, 200);

    setShow(false);
    vi.advanceTimersByTime(100);
    setShow(true);

    expect(presence.exiting()).toBe(false);

    vi.advanceTimersByTime(500);
    expect(presence.mounted()).toBe(true);
    dispose();
  });

  it("dispose 后不再有定时器残留", () => {
    const { presence, setShow, dispose } = setup(true, 200);

    setShow(false);
    dispose();

    vi.advanceTimersByTime(500);
    expect(presence.mounted()).toBe(true);
  });
});
