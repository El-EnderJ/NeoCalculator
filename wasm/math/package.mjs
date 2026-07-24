#!/usr/bin/env node
import { mkdir, readFile, rm, stat, writeFile } from "node:fs/promises";
import { createHash } from "node:crypto";
import { brotliCompressSync, constants, gzipSync } from "node:zlib";
import { execFileSync } from "node:child_process";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";

const repo = dirname(dirname(dirname(fileURLToPath(import.meta.url))));
const requested = process.argv.find((value) =>
  value === "Release" || value === "Debug") || "Release";
const variant = requested.toLowerCase();
const rawDir = join(repo, "out", "wasm-math", variant);
const distDir = join(repo, "out", "wasm-math", "dist", variant);
const sourceDir = join(repo, "wasm", "math");
const sha256 = (data) => createHash("sha256").update(data).digest("hex");
const compression = (data) => ({
  gzipBytes: gzipSync(data, { level: 9 }).byteLength,
  brotliBytes: brotliCompressSync(data, {
    params: {
      [constants.BROTLI_PARAM_QUALITY]: 11,
      [constants.BROTLI_PARAM_MODE]: constants.BROTLI_MODE_GENERIC,
    },
  }).byteLength,
});

async function asset(logicalName, name, data, mime) {
  const digest = sha256(data);
  const dot = name.lastIndexOf(".");
  const fileName = `${name.slice(0, dot)}.${digest.slice(0, 12)}${name.slice(dot)}`;
  await writeFile(join(distDir, fileName), data);
  return {
    logicalName, url: `./${fileName}`, bytes: data.byteLength,
    sha256: digest, mime, ...compression(data),
  };
}

await rm(distDir, { recursive: true, force: true });
await mkdir(distDir, { recursive: true });
const assets = {};
assets.wasm = await asset("wasm", "numos-math.wasm",
  await readFile(join(rawDir, "numos-math-runtime.wasm")), "application/wasm");
assets.runtime = await asset("runtime", "numos-math-runtime.js",
  await readFile(join(rawDir, "numos-math-runtime.js")), "text/javascript");
assets.direct = await asset("direct", "numos-math-direct.js",
  await readFile(join(sourceDir, "numos-math-direct.js")), "text/javascript");
let worker = await readFile(join(sourceDir, "numos-math-worker.js"), "utf8");
worker = worker.replace("./numos-math-direct.js", assets.direct.url);
assets.worker = await asset("worker", "numos-math-worker.js",
  Buffer.from(worker), "text/javascript");
assets.client = await asset("client", "numos-math-client.js",
  await readFile(join(sourceDir, "numos-math-client.js")), "text/javascript");
assets.types = await asset("types", "numos-math.d.ts",
  await readFile(join(sourceDir, "numos-math.d.ts")), "text/plain");

const gitRevision = execFileSync(
  "git", ["rev-parse", "--short=12", "HEAD"],
  { cwd: repo, encoding: "utf8" }).trim();
const dirty = execFileSync(
  "git", ["status", "--porcelain", "--untracked-files=no"],
  { cwd: repo, encoding: "utf8" }).trim().length > 0;
const emscriptenVersion = (
  await readFile(join(repo, "wasm", "emscripten.version"), "utf8")).trim();
const manifest = {
  schemaVersion: 1,
  build: {
    identity: `${gitRevision}-${variant}${dirty ? "-dirty" : ""}`,
    gitRevision,
    configuration: requested,
    emscriptenVersion,
    wasmExceptions: "native",
    pthreads: false,
    asyncify: false,
    productionMode: "worker",
  },
  structuredResultSchemaVersion: 1,
  assets,
};
const manifestData = Buffer.from(`${JSON.stringify(manifest, null, 2)}\n`);
await writeFile(join(distDir, "numos-math-assets.json"), manifestData);
const report = {
  output: relative(repo, distDir),
  manifestBytes: manifestData.byteLength,
  manifestGzipBytes: compression(manifestData).gzipBytes,
  manifestBrotliBytes: compression(manifestData).brotliBytes,
  assets,
};
await writeFile(join(distDir, "package-report.json"),
  `${JSON.stringify(report, null, 2)}\n`);
await writeFile(join(distDir, "fixture.html"),
  "<!doctype html><meta charset=\"utf-8\"><title>NumOS Math fixture</title>\n");
for (const entry of Object.values(assets)) {
  const path = join(distDir, entry.url.slice(2));
  const data = await readFile(path);
  if ((await stat(path)).size !== entry.bytes || sha256(data) !== entry.sha256)
    throw new Error(`invalid ${entry.logicalName}`);
}
console.log(JSON.stringify(report, null, 2));
