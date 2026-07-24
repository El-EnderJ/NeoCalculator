#!/usr/bin/env node
import {
  mkdir, readFile, readdir, rm, stat, writeFile,
} from "node:fs/promises";
import { createHash } from "node:crypto";
import { brotliCompressSync, constants, gzipSync } from "node:zlib";
import { execFileSync } from "node:child_process";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";

const repo = dirname(dirname(fileURLToPath(import.meta.url)));
const requested = process.argv.find((argument) =>
  argument === "Release" || argument === "Debug") || "Release";
const variant = requested.toLowerCase();
const validateOnly = process.argv.includes("--validate");
const rawDir = join(repo, "out", "wasm", variant);
const distDir = join(repo, "out", "wasm", "dist", variant);
const sourceDir = join(repo, "wasm");

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

async function assetRecord(logicalName, fileName, data, mime) {
  const digest = sha256(data);
  const extension = fileName.slice(fileName.lastIndexOf("."));
  const stem = fileName.slice(0, -extension.length);
  const hashedName = `${stem}.${digest.slice(0, 12)}${extension}`;
  await writeFile(join(distDir, hashedName), data);
  return {
    logicalName,
    url: `./${hashedName}`,
    bytes: data.byteLength,
    sha256: digest,
    mime,
    ...compression(data),
  };
}

async function buildPackage() {
  await rm(distDir, { recursive: true, force: true });
  await mkdir(distDir, { recursive: true });

  const wasmData = await readFile(join(rawDir, "numos-emulator.wasm"));
  const runtimeData = await readFile(join(rawDir, "numos-emulator.js"));
  const cssData = await readFile(join(sourceDir, "numos-component.css"));
  const persistenceData = await readFile(join(sourceDir, "numos-persistence.js"));
  const keypadData = await readFile(join(sourceDir, "numos-keypad.js"));

  const assets = {};
  assets.wasm = await assetRecord(
    "wasm", "numos-emulator.wasm", wasmData, "application/wasm");
  assets.runtime = await assetRecord(
    "runtime", "numos-runtime.js", runtimeData, "text/javascript");
  assets.componentCss = await assetRecord(
    "componentCss", "numos-component.css", cssData, "text/css");
  assets.persistence = await assetRecord(
    "persistence", "numos-persistence.js", persistenceData, "text/javascript");
  assets.keypad = await assetRecord(
    "keypad", "numos-keypad.js", keypadData, "text/javascript");

  let componentText = await readFile(
    join(sourceDir, "numos-emulator-element.js"), "utf8");
  componentText = componentText
    .replace('"__NUMOS_INLINE_CSS__"', JSON.stringify(cssData.toString("utf8")))
    .replace("./numos-keypad.js", assets.keypad.url)
    .replace("./numos-persistence.js", assets.persistence.url);
  assets.component = await assetRecord(
    "component", "numos-component.js", Buffer.from(componentText),
    "text/javascript");

  let shellText = await readFile(join(sourceDir, "numos-shell.js"), "utf8");
  shellText = shellText.replace(
    "./numos-emulator-element.js", assets.component.url);
  assets.shell = await assetRecord(
    "shell", "numos-shell.js", Buffer.from(shellText), "text/javascript");

  const gitRevision = execFileSync("git", ["rev-parse", "--short=12", "HEAD"], {
    cwd: repo, encoding: "utf8",
  }).trim();
  const dirty = execFileSync(
    "git", ["status", "--porcelain", "--untracked-files=no"], {
      cwd: repo, encoding: "utf8",
    }).trim().length > 0;
  const emscriptenVersion = (
    await readFile(join(sourceDir, "emscripten.version"), "utf8")).trim();
  const buildIdentity = `${gitRevision}-${variant}${dirty ? "-dirty" : ""}`;
  const manifest = {
    schemaVersion: 1,
    build: {
      identity: buildIdentity,
      gitRevision,
      configuration: requested,
      emscriptenVersion,
      logicalDisplay: { width: 320, height: 240 },
      wasmExceptions: "native",
      pthreads: false,
      asyncify: false,
    },
    assets,
  };
  const manifestData = Buffer.from(`${JSON.stringify(manifest, null, 2)}\n`);
  await writeFile(join(distDir, "numos-assets.json"), manifestData);

  let indexText = await readFile(join(sourceDir, "index.html"), "utf8");
  indexText = indexText.replace("./numos-shell.js", assets.shell.url);
  await writeFile(join(distDir, "index.html"), indexText);
  const fixture = `<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<link rel="icon" href="data:,">
<title>NumOS component fixture</title></head><body>
<script type="module" src="${assets.component.url}"></script>
</body></html>
`;
  await writeFile(join(distDir, "fixture.html"), fixture);

  const summary = {
    output: relative(repo, distDir),
    manifestBytes: manifestData.byteLength,
    manifestGzipBytes: compression(manifestData).gzipBytes,
    manifestBrotliBytes: compression(manifestData).brotliBytes,
    assets,
  };
  await writeFile(join(distDir, "package-report.json"),
    `${JSON.stringify(summary, null, 2)}\n`);
  return summary;
}

async function validatePackage() {
  const manifestPath = join(distDir, "numos-assets.json");
  const manifest = JSON.parse(await readFile(manifestPath, "utf8"));
  if (manifest.schemaVersion !== 1) throw new Error("manifest schemaVersion must be 1");
  for (const required of ["wasm", "runtime", "component", "componentCss",
                           "persistence", "keypad", "shell"]) {
    const entry = manifest.assets?.[required];
    if (!entry) throw new Error(`missing manifest asset: ${required}`);
    if (!entry.url.startsWith("./") || entry.url.includes("..")) {
      throw new Error(`asset URL is not subpath-safe: ${entry.url}`);
    }
    const path = join(distDir, entry.url.slice(2));
    const data = await readFile(path);
    if ((await stat(path)).size !== entry.bytes) {
      throw new Error(`size mismatch: ${required}`);
    }
    if (sha256(data) !== entry.sha256) {
      throw new Error(`sha256 mismatch: ${required}`);
    }
    const expectedNameHash = entry.sha256.slice(0, 12);
    if (!entry.url.includes(`.${expectedNameHash}.`)) {
      throw new Error(`filename is not content addressed: ${required}`);
    }
  }
  const wasm = await readFile(join(distDir, manifest.assets.wasm.url.slice(2)));
  if (wasm.subarray(0, 4).toString("hex") !== "0061736d") {
    throw new Error("Wasm magic is invalid");
  }
  const files = await readdir(distDir);
  if (!files.includes("index.html") || !files.includes("fixture.html")) {
    throw new Error("distribution HTML entry points are missing");
  }
  return {
    valid: true,
    output: relative(repo, distDir),
    assets: Object.keys(manifest.assets).length,
  };
}

if (!validateOnly) {
  console.log(JSON.stringify(await buildPackage(), null, 2));
}
console.log(JSON.stringify(await validatePackage(), null, 2));
