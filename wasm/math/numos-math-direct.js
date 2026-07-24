function asError(response) {
  const error = new Error(response?.error?.message || "NumOS Math request failed");
  error.name = "NumosMathError";
  error.code = response?.error?.code || "INTERNAL_ERROR";
  error.details = structuredClone(response?.error || {});
  return error;
}

function check(response) {
  if (!response?.ok) throw asError(response);
  return response;
}

function invalidRequest(message) {
  const error = new Error(message);
  error.code = "INVALID_REQUEST";
  return error;
}

function validateOptions(options) {
  if (options === undefined) return {};
  if (!options || typeof options !== "object" || Array.isArray(options)) {
    throw invalidRequest("options must be an object");
  }
  return options;
}

export async function createNumosMathDirect({
  manifestUrl = "./numos-math-assets.json",
} = {}) {
  const manifestAbsolute = new URL(manifestUrl, import.meta.url);
  const manifestResponse = await fetch(manifestAbsolute);
  if (!manifestResponse.ok) throw new Error("Could not load NumOS Math manifest");
  const manifest = await manifestResponse.json();
  const runtimeUrl = new URL(manifest.assets.runtime.url, manifestAbsolute);
  const wasmUrl = new URL(manifest.assets.wasm.url, manifestAbsolute);
  const { default: createModule } = await import(runtimeUrl.href);
  const module = await createModule({
    locateFile: (name) => name.endsWith(".wasm") ? wasmUrl.href : name,
    print: () => {},
    printErr: () => {},
  });
  const call = (name, argumentTypes = [], args = []) => {
    const raw = module.ccall(name, "string", argumentTypes, args);
    try {
      return JSON.parse(raw);
    } catch (cause) {
      throw new Error(`${name} returned invalid JSON: ${cause.message}; ` +
        raw.slice(0, 768));
    }
  };
  check(call("numos_math_create"));
  check(call("numos_math_initialize"));
  let shutDown = false;
  const invoke = (name, types = [], args = []) => {
    if (shutDown) throw asError({
      error: { code: "SHUTDOWN", message: "session has been shut down" },
    });
    return check(call(name, types, args));
  };

  const make1D = (id) => Object.freeze({
    evaluate: async (x) =>
      invoke("numos_math_evaluate_1d", ["string", "number"], [id, x]).value,
    evaluateMany: async (values) => {
      if (!Array.isArray(values) && !ArrayBuffer.isView(values)) {
        throw invalidRequest("values must be an array or typed array");
      }
      const copy = Float64Array.from(values);
      const pointer = module._malloc(copy.byteLength || 8);
      try {
        module.HEAPF64.set(copy, pointer / Float64Array.BYTES_PER_ELEMENT);
        return invoke("numos_math_evaluate_many_1d",
          ["string", "number", "number"], [id, pointer, copy.length]).values;
      } finally {
        module._free(pointer);
      }
    },
    dispose: async () =>
      invoke("numos_math_dispose_handle", ["string"], [id]),
  });
  const make2D = (id) => Object.freeze({
    evaluate: async (x, y) =>
      invoke("numos_math_evaluate_2d",
        ["string", "string", "string"], [id, String(x), String(y)]).value,
    evaluateGrid: async ({ xMin, xMax, yMin, yMax, width, height }) =>
      invoke("numos_math_evaluate_grid_2d",
        ["string", "string", "string", "string", "string", "number", "number"],
        [id, String(xMin), String(xMax), String(yMin), String(yMax),
         width, height]),
    dispose: async () =>
      invoke("numos_math_dispose_handle", ["string"], [id]),
  });

  return Object.freeze({
    evaluate: async (expression, options) => {
      validateOptions(options);
      return invoke("numos_math_evaluate",
        ["string", "number"], [expression, 0]);
    },
    approximate: async (expression, options) => {
      validateOptions(options);
      return invoke("numos_math_evaluate",
        ["string", "number"], [expression, 1]);
    },
    simplify: async (expression, options) => {
      validateOptions(options);
      return invoke("numos_math_simplify", ["string"], [expression]);
    },
    differentiate: async (expression, variable, options) => {
      validateOptions(options);
      return invoke("numos_math_calculus",
        ["string", "string", "number"], [expression, variable, 0]);
    },
    integrate: async (expression, variable, options) => {
      validateOptions(options);
      return invoke("numos_math_calculus",
        ["string", "string", "number"], [expression, variable, 1]);
    },
    solve: async (equationOrSystem, variables, options) => {
      const settings = validateOptions(options);
      const equations = Array.isArray(equationOrSystem)
        ? equationOrSystem : [equationOrSystem];
      const names = Array.isArray(variables) ? variables : [variables];
      const sides = equations.map((equation) => {
        if (typeof equation === "string") {
          const position = equation.indexOf("=");
          if (position < 0 || equation.indexOf("=", position + 1) >= 0) {
            throw invalidRequest("each equation must contain exactly one =");
          }
          return [equation.slice(0, position), equation.slice(position + 1)];
        }
        if (equation && typeof equation.lhs === "string" &&
            typeof equation.rhs === "string") return [equation.lhs, equation.rhs];
        throw invalidRequest("invalid equation");
      });
      return invoke("numos_math_solve",
        ["string", "string", "string", "number"],
        [sides.map(([lhs]) => lhs).join("\n"),
         sides.map(([, rhs]) => rhs).join("\n"), names.join(","),
         settings.domain === "complex" ? 1 : 0]);
    },
    compile1D: async (expression, variable, options) => {
      validateOptions(options);
      return make1D(invoke("numos_math_compile_1d",
        ["string", "string"], [expression, variable]).handle);
    },
    compile2D: async (expression, xVariable, yVariable, options) => {
      validateOptions(options);
      return make2D(invoke("numos_math_compile_2d",
        ["string", "string", "string"],
        [expression, xVariable, yVariable]).handle);
    },
    setAngleMode: async (mode) =>
      invoke("numos_math_set_angle_mode", ["string"], [mode]),
    getAngleMode: async () => invoke("numos_math_get_angle_mode"),
    setVariable: async (name, expression) =>
      invoke("numos_math_set_variable",
        ["string", "string"], [name, expression]),
    removeVariable: async (name) =>
      invoke("numos_math_remove_variable", ["string"], [name]),
    clearVariables: async () => invoke("numos_math_clear_variables"),
    reset: async () => invoke("numos_math_reset"),
    diagnostics: async () => invoke("numos_math_diagnostics"),
    shutdown: async () => {
      const result = invoke("numos_math_shutdown");
      shutDown = true;
      return result;
    },
  });
}
