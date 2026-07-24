import { mkdir } from "node:fs/promises";
import { chromium } from "playwright";
import { startStaticServer } from "./test-server.mjs";

const root = new URL("../../out/wasm/dist/release/", import.meta.url).pathname;
const output = new URL("../../out/wasm-embed-01/screenshots/", import.meta.url).pathname;
await mkdir(output, { recursive: true });
const server = await startStaticServer(root, 4211);
const browser = await chromium.launch({
  headless: true,
  executablePath: chromium.executablePath(),
});
const context = await browser.newContext({
  viewport: { width: 1100, height: 820 },
  deviceScaleFactor: 1,
});

async function install(page, attributes = {}) {
  await page.goto(`${server.origin}/fixture.html`, { waitUntil: "domcontentloaded" });
  await page.waitForFunction(() => customElements.get("numos-emulator"));
  await page.evaluate((attrs) => {
    const emulator = document.createElement("numos-emulator");
    for (const [name, value] of Object.entries(attrs)) {
      if (value === true) emulator.setAttribute(name, "");
      else emulator.setAttribute(name, value);
    }
    document.body.append(emulator);
    window.emulator = emulator;
  }, attributes);
}

async function shot(page, name) {
  await page.locator("numos-emulator").screenshot({
    path: `${output}/${name}.png`,
  });
}

try {
  let page = await context.newPage();
  await install(page, { controls: "hidden" });
  await shot(page, "01-idle-start");
  await page.close();

  page = await context.newPage();
  const cdp = await context.newCDPSession(page);
  await cdp.send("Network.enable");
  await cdp.send("Network.emulateNetworkConditions", {
    offline: false,
    latency: 120,
    downloadThroughput: 96 * 1024,
    uploadThroughput: 96 * 1024,
    connectionType: "cellular3g",
  });
  await install(page, { controls: "hidden" });
  await page.locator("numos-emulator")
    .locator('[data-action="overlay-start"]').click();
  await page.waitForFunction(() => window.emulator.state === "downloading_wasm");
  await page.waitForFunction(() => {
    const text = window.emulator.shadowRoot.querySelector(".overlay p").textContent;
    return text.includes("KiB") || text.includes("MiB");
  });
  await new Promise((resolve) => setTimeout(resolve, 180));
  await shot(page, "02-download-progress");
  await page.evaluate(() => window.emulator.shutdown());
  await page.close();

  page = await context.newPage();
  await install(page, { controls: "hidden" });
  await page.evaluate(() => { window.startPromise = window.emulator.start(); });
  await page.waitForFunction(() => window.emulator.state === "booting");
  await shot(page, "03-boot");
  await page.evaluate(() => window.startPromise);
  await shot(page, "04-desktop-ready");

  await page.setViewportSize({ width: 390, height: 844 });
  await page.evaluate(() => { window.emulator.controls = "visible"; });
  await shot(page, "05-mobile-portrait-controls");
  await page.setViewportSize({ width: 844, height: 390 });
  await shot(page, "06-mobile-landscape-controls");

  await page.setViewportSize({ width: 1100, height: 820 });
  await page.locator("numos-emulator")
    .locator('[data-action="fullscreen"]').click();
  try {
    await page.waitForFunction(() => document.fullscreenElement, null,
      { timeout: 3000 });
  } catch {
    await page.evaluate(() => {
      document.querySelector("numos-emulator").style.minHeight = "100vh";
    });
  }
  await shot(page, "07-fullscreen-shell");
  if (await page.evaluate(() => Boolean(document.fullscreenElement))) {
    await page.keyboard.press("Escape");
  }
  await page.evaluate(() => window.emulator.shutdown());
  await page.close();

  page = await context.newPage();
  await install(page, {
    controls: "hidden",
    "persistence-test": "indexeddb-denied",
  });
  await page.evaluate(() => window.emulator.start());
  await shot(page, "08-ephemeral-warning");
  await page.evaluate(() => window.emulator.shutdown());
  await page.close();

  page = await context.newPage();
  await install(page, { manifest: "./missing-assets.json", controls: "hidden" });
  await page.evaluate(async () => {
    try { await window.emulator.start(); } catch {}
  });
  await shot(page, "09-recoverable-error");
  await page.close();
} finally {
  await context.close();
  await browser.close();
  await server.close();
}

console.log(`NumOS shell screenshots: ${output}`);
