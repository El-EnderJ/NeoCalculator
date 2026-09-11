// Independent browser check of the compiled C++ Steps view and cache lifecycle.
import assert from "node:assert/strict";
import { mkdir, readFile, writeFile } from "node:fs/promises";
import { createHash } from "node:crypto";
import { resolve } from "node:path";
import { spawn } from "node:child_process";
import { chromium } from "../tests/wasm/node_modules/playwright/index.mjs";

const snapshot=process.argv[2];
assert.ok(snapshot,"pass the explicitly fingerprinted source snapshot");
const cycles=Number(process.argv[3]||12);
assert.ok(Number.isInteger(cycles)&&cycles>=1&&cycles<=100,"cycles must be 1..100");
const manifest=JSON.parse(await readFile(resolve(snapshot,"out/wasm/dist/release/numos-assets.json"),"utf8"));
const rawHash=createHash("sha256").update(await readFile(resolve(snapshot,"out/wasm/release/numos-emulator.wasm"))).digest("hex");
assert.equal(manifest.assets.wasm.sha256,rawHash,"package must contain the freshly compiled WASM");
const out=resolve("out/tutor-engine-01/review-math/regressions/web-steps");
await mkdir(out,{recursive:true});
const server=spawn("python",["-m","http.server","8796","--bind","127.0.0.1","--directory",resolve(snapshot,"out/wasm/dist/release")],{stdio:"ignore",windowsHide:true});
const delay=ms=>new Promise(resolve=>setTimeout(resolve,ms));
let browser;
try {
  for(let attempt=0;attempt<100;++attempt){try{if((await fetch("http://127.0.0.1:8796/index.html")).ok)break;}catch{}await delay(50);}
  browser=await chromium.launch({headless:true});const page=await browser.newPage({viewport:{width:900,height:720}});
  const errors=[];page.on("pageerror",e=>errors.push(String(e)));
  await page.goto("http://127.0.0.1:8796/index.html?persistence=disabled");
  await page.waitForFunction(()=>window.numos?.isReady(),null,{timeout:30000});
  const canvas=page.locator("numos-emulator").locator("canvas");
  const box=await canvas.boundingBox();await canvas.click({position:{x:255*box.width/320,y:72*box.height/240},delay:40});
  await page.waitForFunction(()=>window.numos.diagnosticState().app==="Equations");
  await page.keyboard.press("Enter");await page.keyboard.press("Enter");await page.keyboard.type("2*x+4=0");
  await page.keyboard.press("Enter");await page.keyboard.press("ArrowDown");await page.keyboard.press("ArrowDown");await page.keyboard.press("Enter");
  await page.waitForFunction(()=>window.numos.diagnosticState().equations.x0Exact==="-2",null,{timeout:20000});
  const capture=async name=>{await delay(180);const url=await canvas.evaluate(c=>c.toDataURL("image/png"));await writeFile(resolve(out,name+".png"),Buffer.from(url.split(",")[1],"base64"));return url;};
  const press=async code=>{assert.equal(await page.evaluate(code=>window.numos.pressLogicalKey(code),code),true,`native ABI rejected key ${code}`);await delay(100);};
  const result=await capture("result");
  await press(72);const compact=await capture("compact");assert.ok(compact!==result,"Steps must change the visible framebuffer");
  await press(50);const detail=await capture("detail");assert.ok(compact!==detail,"EXE must expose the primitive detail view");
  await press(16);await capture("next-primitive");await press(70);
  await page.waitForFunction(()=>window.numos.diagnosticState().frameMs.samples===512,null,{timeout:20000});
  const before=await page.evaluate(()=>window.numos.diagnosticState());const heap=[];
  for(let i=0;i<cycles;++i){await press(72);await press(70);heap.push((await page.evaluate(()=>window.numos.diagnosticState())).usedHeapBytes);}
  const after=await page.evaluate(()=>window.numos.diagnosticState());
  assert.equal(after.equations.x0Exact,"-2");assert.equal(after.giac.structuredSolves,before.giac.structuredSolves);
  assert.equal(after.giac.generation,before.giac.generation);assert.ok(heap.at(-1)<=heap[0]+65536,JSON.stringify(heap));
  assert.equal(heap.every((value,index)=>index===0||value>heap[index-1]),false,JSON.stringify(heap));
  for(const code of [70,14,14,50,10,17,75,44,41,78,46,50,15,15,50])await press(code);
  await page.waitForFunction(()=>{const e=window.numos.diagnosticState().equations;return e.solutionCount===2&&["i","-i"].includes(e.x0Exact);},null,{timeout:20000});
  await press(72);await press(16);await capture("complex-signed");await press(15);await press(15);await capture("complex-scrolled");await press(70);
  for(const code of [70,14,14,50,10,17,78,37,43,50,15,15,50])await press(code);
  await page.waitForFunction(()=>window.numos.diagnosticState().equations.x0Exact==="-3",null,{timeout:20000});
  await press(72);await capture("isolated-negative");await press(70);
  await press(69);await page.waitForFunction(()=>window.numos.diagnosticState().app==="Menu");
  await delay(400);const home=await page.evaluate(()=>window.numos.diagnosticState());
  await delay(500);const homeStable=await page.evaluate(()=>window.numos.diagnosticState());
  const homeSamples=[home.usedHeapBytes,homeStable.usedHeapBytes];
  for(let i=0;i<3;++i){await delay(250);homeSamples.push((await page.evaluate(()=>window.numos.diagnosticState())).usedHeapBytes);}
  assert.equal(home.giac.activeContexts,1);assert.equal(errors.length,0,errors.join("\n"));
  assert.equal(home.frameMs.samples,512);assert.equal(homeStable.frameMs.samples,512);
  assert.ok(Math.max(...homeSamples)-Math.min(...homeSamples)<=128,"live heap must plateau after HOME: "+homeSamples);
  assert.equal(homeSamples.every((v,i)=>i===0||v>homeSamples[i-1]),false,"HOME heap must not accumulate");
  await writeFile(resolve(out,"result.json"),JSON.stringify({pass:true,wasmSha256:rawHash,logicalSize:[320,240],cycles,heap,before,after,home,homeStable,homeSamples,errors},null,2));
  console.log(JSON.stringify({pass:true,wasmSha256:rawHash,cycles,heap,homeSamples,artifacts:out}));
}finally{if(browser)await browser.close();server.kill();}
