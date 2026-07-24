import { createNumosMathDirect } from "./numos-math-direct.js";

let math = null;
let nextHandle = 1;
const handles = new Map();

function staleHandle() {
  const error = new Error("unknown retained Worker handle");
  error.code = "STALE_HANDLE";
  return error;
}

async function dispatch(method, args) {
  if (method === "initialize") {
    math = await createNumosMathDirect({ manifestUrl: args[0] });
    return { ready: true };
  }
  if (!math) {
    const error = new Error("worker is not initialized");
    error.code = "NOT_INITIALIZED";
    throw error;
  }
  if (method === "shutdown") {
    handles.clear();
    const result = await math.shutdown();
    math = null;
    return result;
  }
  if (method === "compileHandle1D" || method === "compileHandle2D") {
    const handle = method === "compileHandle1D"
      ? await math.compile1D(...args) : await math.compile2D(...args);
    const id = `worker-handle-${nextHandle++}`;
    handles.set(id, handle);
    return id;
  }
  if (method.startsWith("handle.")) {
    const handle = handles.get(args[0]);
    if (!handle) throw staleHandle();
    switch (method) {
      case "handle.evaluate1D": return handle.evaluate(args[1]);
      case "handle.evaluateMany1D": return handle.evaluateMany(args[1]);
      case "handle.evaluate2D": return handle.evaluate(args[1], args[2]);
      case "handle.evaluateGrid2D": return handle.evaluateGrid(args[1]);
      case "handle.dispose": return handle.dispose();
      default: throw staleHandle();
    }
  }
  return math[method](...args);
}

self.onmessage = async ({ data }) => {
  const { requestId, method, args = [] } = data || {};
  try {
    const result = await dispatch(method, args);
    self.postMessage({ requestId, ok: true, result });
  } catch (cause) {
    self.postMessage({
      requestId,
      ok: false,
      error: {
        code: cause?.code || "INTERNAL_ERROR",
        message: String(cause?.message || cause).slice(0, 512),
      },
    });
  }
};
