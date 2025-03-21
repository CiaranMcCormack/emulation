import { useRef, useEffect, useCallback } from 'react'

/**
 * useEvent
 *
 * Returns a stable callback reference whose implementation always sees the latest props/state,
 * preventing “stale closure” bugs in long‑running loops, event handlers, or other callbacks that
 * should not change identity on every render.
 *
 * WHY use it?
 * - React’s useCallback(fn, []) returns a function that never changes, but closes over the initial
 *   values of variables — leading to stale data if those values update later.
 * - Directly defining a function inside a render loop causes the loop to capture outdated state,
 *   or forces you to rebuild the loop on every state change (bad for performance).
 *
 * WHEN to use it?
 * - In animation/render loops (requestAnimationFrame) where you want a single stable function pointer
 *   but still access up-to-date state each frame.
 * - In persistent event listeners (e.g., window.addEventListener) that shouldn’t be removed and
 *   re‑added on every render.
 * - In any situation where you need both a constant callback identity and the freshest closure values.
 *
 * HOW it works:
 * - Internally stores the latest callback in a ref, updating it on every render.
 * - Exposes a single memoized function that always invokes the current ref value.
 *
 * @param fn A callback function that may refer to React state or props.
 * @returns A stable function that calls the latest version of fn.
 */
export function useEvent<Fn extends (...args: any[]) => any>(fn: Fn): Fn {
  const ref = useRef(fn)
  useEffect(() => { ref.current = fn }, [fn])
  return useCallback(((...args: any[]) => ref.current(...args)) as Fn, [])
}