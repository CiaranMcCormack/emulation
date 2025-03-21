import { useRef, useEffect, useCallback, useState } from 'react'

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

/**
 * useScript
 *
 * Dynamically loads an external JavaScript file by injecting a <script> tag into the document body,
 * then cleans it up automatically when the component unmounts or when the `src` changes.
 *
 * WHY use it?
 * - Lazy‑loads large scripts (e.g., Emscripten-generated WASM glue) only when needed, reducing initial bundle size.
 * - Ensures scripts aren’t loaded multiple times if the component re-renders.
 * - Automatically removes the script when navigating away, preventing global namespace collisions and freeing memory.
 * - Provides a simple “loaded” flag so you can safely call script‑exported functions only after initialization.
 *
 * WHEN to use it?
 * - In route-based pages (e.g., /chip8 or /atari2600) where each page needs a different WASM/JS runtime.
 * - When you need to load third‑party SDKs or plugins on demand (analytics, payment SDKs, emulators, etc.).
 * - Whenever you want full control over when and how external scripts are inserted into the DOM.
 *
 * HOW it works:
 * 1️⃣ On mount (or when `src` changes), creates a <script> element pointing at the URL you pass in.
 * 2️⃣ Sets the script’s async flag so it doesn’t block rendering.
 * 3️⃣ Defines a global `Module` object (used by Emscripten) with `print` and `printErr` handlers mapped to console.log/error.
 * 4️⃣ Appends the script to document.body — the browser then fetches & executes it.
 * 5️⃣ When the script’s onload event fires, updates local state to indicate “loaded = true.”
 * 6️⃣ On unmount (or when `src` changes), removes the <script> tag and resets the loaded flag.
 *
 * IMPORTANT:
 * - This hook assumes the script itself will register any global exports (e.g., window.Module for Emscripten).
 * - Because it removes the script on cleanup, any globals it created will persist until overwritten by subsequent loads.
 *
 * @param src URL or path to the JavaScript file you want to load.
 * @returns `loaded` boolean — becomes true once the <script> has finished loading and executing.
 */
export function useScript(src: string) {
  const [loaded, setLoaded] = useState(false);

  useEffect(() => {
    (window as any).Module = { print: console.log, printErr: console.error };
    const script = document.createElement('script');
    script.src = src;
    script.async = true;
    script.onload = () => setLoaded(true);
    document.body.appendChild(script);
    return () => {
      document.body.removeChild(script);
      setLoaded(false);
    };
  }, [src]);

  return loaded;
}