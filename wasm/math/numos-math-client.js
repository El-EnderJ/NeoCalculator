function makeError(value) {
  const error = new Error(value?.message || "NumOS Math request failed");
  error.name = "NumosMathError";
  error.code = value?.code || "INTERNAL_ERROR";
  return error;
}

export async function createNumosMath({
  manifestUrl = "./numos-math-assets.json",
} = {}) {
  const manifestAbsolute = new URL(manifestUrl, import.meta.url);
  const manifestResponse = await fetch(manifestAbsolute);
  if (!manifestResponse.ok) throw new Error("Could not load NumOS Math manifest");
  const manifest = await manifestResponse.json();
  const workerUrl = new URL(manifest.assets.worker.url, manifestAbsolute);
  let worker = new Worker(workerUrl, { type: "module", name: "numos-math" });
  let nextRequestId = 1;
  let shutDown = false;
  const pending = new Map();

  const rejectAll = (code, message) => {
    for (const { reject } of pending.values())
      reject(makeError({ code, message }));
    pending.clear();
  };
  const bind = () => {
    worker.onmessage = ({ data }) => {
      const request = pending.get(data.requestId);
      if (!request) return;
      pending.delete(data.requestId);
      data.ok ? request.resolve(data.result)
              : request.reject(makeError(data.error));
    };
    worker.onerror = (event) =>
      rejectAll("INTERNAL_ERROR", event.message || "NumOS Math Worker failed");
  };
  bind();
  const request = (method, args = []) => {
    if (shutDown) return Promise.reject(makeError({
      code: "SHUTDOWN", message: "session has been shut down",
    }));
    const requestId = nextRequestId++;
    return new Promise((resolve, reject) => {
      pending.set(requestId, { resolve, reject });
      worker.postMessage({ requestId, method, args: structuredClone(args) });
    });
  };
  await request("initialize", [manifestAbsolute.href]);

  const make1D = (id) => Object.freeze({
    evaluate: (x) => request("handle.evaluate1D", [id, x]),
    evaluateMany: (values) =>
      request("handle.evaluateMany1D", [id, Array.from(values)]),
    dispose: () => request("handle.dispose", [id]),
  });
  const make2D = (id) => Object.freeze({
    evaluate: (x, y) => request("handle.evaluate2D", [id, x, y]),
    evaluateGrid: (bounds) =>
      request("handle.evaluateGrid2D", [id, structuredClone(bounds)]),
    dispose: () => request("handle.dispose", [id]),
  });

  return Object.freeze({
    evaluate: (expression, options) =>
      request("evaluate", [expression, options]),
    approximate: (expression, options) =>
      request("approximate", [expression, options]),
    simplify: (expression, options) =>
      request("simplify", [expression, options]),
    differentiate: (expression, variable, options) =>
      request("differentiate", [expression, variable, options]),
    integrate: (expression, variable, options) =>
      request("integrate", [expression, variable, options]),
    solve: (equations, variables, options) =>
      request("solve", [equations, variables, options]),
    compile1D: async (expression, variable, options) =>
      make1D(await request("compileHandle1D",
        [expression, variable, options])),
    compile2D: async (expression, xVariable, yVariable, options) =>
      make2D(await request("compileHandle2D",
        [expression, xVariable, yVariable, options])),
    setAngleMode: (mode) => request("setAngleMode", [mode]),
    getAngleMode: () => request("getAngleMode"),
    setVariable: (name, expression) =>
      request("setVariable", [name, expression]),
    removeVariable: (name) => request("removeVariable", [name]),
    clearVariables: () => request("clearVariables"),
    reset: () => request("reset"),
    diagnostics: () => request("diagnostics"),
    cancel: async () => {
      worker.terminate();
      rejectAll("TIME_LIMIT", "Worker terminated for cancellation");
      worker = new Worker(workerUrl, { type: "module", name: "numos-math" });
      bind();
      await request("initialize", [manifestAbsolute.href]);
      return { cancelled: true, stateLost: true };
    },
    shutdown: async () => {
      try {
        return await request("shutdown");
      } finally {
        shutDown = true;
        worker.terminate();
        rejectAll("SHUTDOWN", "session has been shut down");
      }
    },
  });
}
