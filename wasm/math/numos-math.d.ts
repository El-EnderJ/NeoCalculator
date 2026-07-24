export type NumosMathErrorCode =
  | "NOT_INITIALIZED" | "INVALID_REQUEST" | "SOURCE_TOO_LONG"
  | "DEPTH_LIMIT" | "NODE_LIMIT" | "INVALID_IDENTIFIER" | "PARSE_ERROR"
  | "EVALUATION_ERROR" | "UNSUPPORTED_RESULT" | "TIME_LIMIT"
  | "STALE_HANDLE" | "DISPOSED_HANDLE" | "INVALID_ANGLE_MODE"
  | "SHUTDOWN" | "INTERNAL_ERROR";

export type StructuredNodeKind =
  | "integer" | "decimal" | "rational" | "symbol" | "sum" | "negative"
  | "product" | "inverse" | "power" | "sqrt" | "root" | "function"
  | "pi" | "e" | "imaginary_unit" | "positive_infinity"
  | "negative_infinity" | "unsigned_infinity" | "equation" | "assignment"
  | "list" | "set" | "matrix" | "interval" | "piecewise" | "complex"
  | "unevaluated_call" | "undefined" | "opaque";

export type StructuredNode = {
  kind: StructuredNodeKind;
  value?: string;
  name?: string;
  children?: StructuredNode[];
  rows?: number;
  columns?: number;
  leftClosed?: boolean;
  rightClosed?: boolean;
  reason?: string;
  displayText?: string;
};

export type NumericValue =
  | { kind: "finite"; value: number }
  | { kind: "non_finite";
      value: "nan" | "positive_infinity" | "negative_infinity" };

export interface SolutionValue {
  variable: string;
  value: StructuredNode;
}

export interface SolutionSet {
  kind: "solution_set";
  setKind: "solutions" | "no_solution" | "all_values" | "unsupported";
  groups: Array<{ values: SolutionValue[] }>;
}

export interface NumosMathResult<T = StructuredNode> {
  schemaVersion: 1;
  ok: true;
  result: T;
  exact?: StructuredNode;
  approximate?: StructuredNode | null;
  displayText?: string;
  operationMs?: number;
  unevaluated?: boolean;
}

export interface RetainedExpression1D {
  evaluate(x: number): Promise<NumericValue>;
  evaluateMany(values: readonly number[] | ArrayLike<number>):
    Promise<NumericValue[]>;
  dispose(): Promise<unknown>;
}

export interface GridRequest {
  xMin: number; xMax: number; yMin: number; yMax: number;
  width: number; height: number;
}

export interface RetainedExpression2D {
  evaluate(x: number, y: number): Promise<NumericValue>;
  evaluateGrid(request: GridRequest): Promise<{
    width: number;
    height: number;
    order: "row-major-y-then-x";
    values: NumericValue[];
  }>;
  dispose(): Promise<unknown>;
}

export interface NumosMath {
  evaluate(expression: string, options?: object): Promise<NumosMathResult>;
  approximate(expression: string, options?: object): Promise<NumosMathResult>;
  simplify(expression: string, options?: object): Promise<NumosMathResult>;
  differentiate(expression: string, variable: string, options?: object):
    Promise<NumosMathResult>;
  integrate(expression: string, variable: string, options?: object):
    Promise<NumosMathResult>;
  solve(equations: string | string[], variables: string | string[],
    options?: { domain?: "real" | "complex" }):
    Promise<NumosMathResult<SolutionSet>>;
  compile1D(expression: string, variable: string, options?: object):
    Promise<RetainedExpression1D>;
  compile2D(expression: string, xVariable: string, yVariable: string,
    options?: object): Promise<RetainedExpression2D>;
  setAngleMode(mode: "radian" | "degree"): Promise<unknown>;
  getAngleMode(): Promise<{ mode: "radian" | "degree" }>;
  setVariable(name: string, expression: string): Promise<unknown>;
  removeVariable(name: string): Promise<unknown>;
  clearVariables(): Promise<unknown>;
  reset(): Promise<unknown>;
  diagnostics(): Promise<unknown>;
  cancel(): Promise<{ cancelled: true; stateLost: true }>;
  shutdown(): Promise<unknown>;
}

export function createNumosMath(options?: {
  manifestUrl?: string | URL;
}): Promise<NumosMath>;
