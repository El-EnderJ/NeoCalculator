// Real app key events, pixel checks at the native 320x240 resolution.
import assert from 'node:assert/strict';
import {mkdir,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {chromium,firefox,webkit} from 'playwright';
import {startStaticServer} from './test-server.mjs';
import {NUMOS_LOGICAL_KEYS} from '../../wasm/numos-keypad.js';
const codes=Object.fromEntries(NUMOS_LOGICAL_KEYS.map(k=>[k.id,k.code]));
const alias={'^':'POW','/':'FRAC',sd:'FREE_EQ',neg:'NEGATE'};
const out=resolve(process.env.NUMOS_VIEWPORT_OUT),server=await startStaticServer(resolve(process.env.NUMOS_WEB_ROOT),8898);
const records=[];
try {for(const [name,type] of Object.entries({chromium,firefox,webkit})) {
 const browser=await type.launch({headless:true});
 try {for(const surface of ['shell','component']) {
  const context=await browser.newContext({viewport:{width:1100,height:1100},deviceScaleFactor:1});
  const page=await context.newPage(),errors=[];page.on('pageerror',e=>errors.push(String(e)));
  await page.goto(server.origin+(surface==='shell'?'/index.html':'/fixture.html'));
  if(surface==='component') {
   await page.waitForFunction(()=>customElements.get('numos-emulator'));
   await page.evaluate(()=>{const e=document.createElement('numos-emulator');e.setAttribute('controls','');document.body.append(e);});
   await page.locator('numos-emulator').locator('[data-action=overlay-start]').click();
  }
  await page.waitForFunction(()=>document.querySelector('numos-emulator')?.diagnosticState()?.ready,null,{timeout:60000});
  const el=page.locator('numos-emulator'),folder=resolve(out,name,surface);await mkdir(folder,{recursive:true});
  const keys=async text=>{
   const values=text.trim().split(/\s+/).map(k=>codes[/^\d$/.test(k)?'NUM_'+k:alias[k]||k]);assert.ok(values.every(Number.isInteger));
   await el.evaluate(async(e,values)=>{for(const v of values){if(!e.pressLogicalKey(v))throw Error('key rejected');await new Promise(requestAnimationFrame);}},values);
   await page.waitForTimeout(40);
  };
  const shot=async id=>{
   const data=await el.locator('canvas').evaluate(async c=>{
    if(c.width!==320||c.height!==240)throw Error('unexpected framebuffer size');
    const png=c.toDataURL(),im=new Image();im.src=png;await im.decode();
    const s=document.createElement('canvas');s.width=320;s.height=240;const ctx=s.getContext('2d');ctx.drawImage(im,0,0);
    const p=ctx.getImageData(0,0,320,240).data;let reds=0,ink=0,hash=2166136261;
    for(let y=25;y<234;y++)for(let x=6;x<314;x++) {
     const i=(y*320+x)*4,r=p[i],g=p[i+1],b=p[i+2];if(r>g+70&&r>b+70)reds++;
     if(y>=134){if(Math.max(r,g,b)<160)ink++;for(let j=0;j<3;j++)hash=Math.imul(hash^p[i+j],16777619);}
    }return {png:png.split(',')[1],reds,ink,hash};
   });
   await writeFile(resolve(folder,id+'.png'),Buffer.from(data.png,'base64'));delete data.png;return data;
  };
  // DEVICE-INPUT-PATH-01: a result scrolls with SHIFT + arrow; a plain arrow re-edits.
  await keys('ENTER');
  await keys('AC '+'SQRT '.repeat(14)+'2');const deep=await shot('deep-root');assert.equal(deep.reds,0);
  for(const [id,input] of [['exact','2 ^ 2 0 0 ENTER'],['periodic','1 / 9 7 ENTER SHIFT ALPHA FORMAT DOWN DOWN ENTER'],['extended','1 / 7 ENTER SHIFT ALPHA FORMAT DOWN DOWN DOWN ENTER']]) {
   await keys('AC '+input);const semantic=await el.evaluate(e=>e.diagnosticState().calculation),start=await shot(id+'-start');
   await keys('SHIFT RIGHT '.repeat(120));const end=await shot(id+'-end');assert.notEqual(end.hash,start.hash);assert.ok(end.ink>15);
   await keys('SHIFT RIGHT '.repeat(10));const edge=await shot(id+'-edge');assert.equal(edge.hash,end.hash);
   await keys('SHIFT LEFT '.repeat(130));const back=await shot(id+'-back');assert.equal(back.hash,start.hash);
   assert.deepEqual(await el.evaluate(e=>e.diagnosticState().calculation),semantic);
   records.push({browser:name,version:browser.version(),surface,id,start,end,edge,back,semantic});
  }
  assert.deepEqual(errors,[]);await context.close();console.log('PASS',name,surface,'deep roots and three bounded result modes');
 }}finally{await browser.close();}
}}finally{await server.close();await mkdir(out,{recursive:true});await writeFile(resolve(out,'results.json'),JSON.stringify(records,null,2));}
