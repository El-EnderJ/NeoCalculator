// End-to-end real key event timings. No renderer mutations or injected ASTs.
import assert from 'node:assert/strict';
import {mkdir,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {chromium,firefox,webkit} from 'playwright';
import {startStaticServer} from './test-server.mjs';
import {calculationDriver} from './calculation-driver.mjs';
const out=resolve(process.env.NUMOS_PARITY_EVENTS_OUT),root=resolve(process.env.NUMOS_WEB_ROOT);
const server=await startStaticServer(root,8916),records=[],traces=[];
const repeats=Number(process.env.NUMOS_PARITY_REPEATS||3);
await mkdir(out,{recursive:true});
try {for(const [browserName,type] of Object.entries({chromium,firefox,webkit})) {
 if(process.env.NUMOS_BROWSER&&process.env.NUMOS_BROWSER!==browserName)continue;
 const browser=await type.launch({headless:true});
 try {for(const surface of ['shell','component']) {
  if(process.env.NUMOS_SURFACE&&process.env.NUMOS_SURFACE!==surface)continue;
  const context=await browser.newContext({viewport:{width:1100,height:1100},deviceScaleFactor:1}),page=await context.newPage(),errors=[];
  page.on('pageerror',e=>errors.push(String(e)));const trace=[];
  const folder=resolve(out,browserName,surface);await mkdir(folder,{recursive:true});
  try {
   const bootStarted=performance.now();await page.goto(server.origin+(surface==='shell'?'/index.html':'/fixture.html'));
   if(surface==='component') {
    await page.waitForFunction(()=>customElements.get('numos-emulator'));
    await page.evaluate(()=>{const e=document.createElement('numos-emulator');e.setAttribute('controls','');document.body.append(e);});
    await page.locator('numos-emulator').locator('[data-action=overlay-start]').click();
   }
   await page.waitForFunction(()=>document.querySelector('numos-emulator')?.diagnosticState()?.ready,null,{timeout:60000});
   const bootMs=performance.now()-bootStarted,el=page.locator('numos-emulator');
   if(!await el.locator('[data-physical-id=r9c4]').isVisible())await el.locator('[data-action=controls]').click();
   const driver=calculationDriver(page,el,trace),keys=driver.keys;
   await keys('ENTER');assert.equal((await driver.state()).app,'Calculation');
   const expect=async expected=>page.waitForFunction(expected=>{const s=document.querySelector('numos-emulator').diagnosticState().toolbox;return Object.entries(expected).every(([k,v])=>s[k]===v);},expected);
   const pick=async(q,id)=>{await keys('TOOLBOX');await el.locator('canvas').focus();await page.keyboard.type(q);await expect({queryBytes:new TextEncoder().encode(q).length});await keys('DOWN');await expect({id});await keys('ENTER');await expect({open:false});};
   const measure=async(n,u)=>{await pick('measurement',750);await keys(n.split('').join(' '));await keys('RIGHT');await keys(u.split('').join(' '));await keys('RIGHT');};
   const workloads=[
    ['short-entry',()=>keys('AC'), '2 + 3'],
    ['short-evaluate',()=>keys('AC 2 + 3'), 'ENTER','5'],
    ['short-cursor',()=>keys('AC 2 + 3'), 'LEFT RIGHT'],
    ['short-delete',()=>keys('AC 2 + 3'), 'DEL 4'],
    ['fraction-template',()=>keys('AC 7'), '/'],
    ['fraction-evaluate',()=>keys('AC 7 / 3'), 'ENTER','7/3'],
    ['root-edit',()=>keys('AC SQRT'), '2 RIGHT + 3'],
    ['nested-script-edit',()=>keys('AC 5 ^ 5 ^ 6'), 'LEFT RIGHT'],
    ['long-cursor',()=>keys('AC '+Array(24).fill('2').join(' + ')), 'LEFT LEFT RIGHT RIGHT'],
    ['wide-result-pan',()=>keys('AC 2 ^ 2 0 0 ENTER'), 'RIGHT RIGHT RIGHT RIGHT LEFT LEFT LEFT LEFT'],
    ['result-format',()=>keys('AC 7 / 3 ENTER'), 'FORMAT FORMAT'],
    ['result-history',()=>keys('AC 0 . 1 ENTER'), 'UP DOWN'],
    ['format-menu',()=>keys('AC 7 / 3 ENTER'), 'SHIFT ALPHA FORMAT BACK'],
    ['measurement-edit',async()=>{await keys('AC');await measure('10','1');},'LEFT RIGHT'],
    ['measurement-evaluate',async()=>{await keys('AC');await measure('10','1');await pick('metre',32769);},'ENTER','10'],
    ['temperature-evaluate',async()=>{await keys('AC');await measure('20','0.5');await pick('Degree Celsius',32791);},'ENTER','5863/20'],
    ['reference-square',async()=>{await keys('AC');await pick('electron mass',16387);},'^ 2 RIGHT'],
    ['reference-evaluate',async()=>{await keys('AC');await pick('gravitational constant',16443);},'ENTER','66743/1000000000000000'],
   ];
   for(let repeat=0;repeat<repeats;++repeat)for(const [id,prepare,events,expected] of workloads) {
    await prepare();const mark=trace.length,before=await driver.state(),started=performance.now();await keys(events);const wallMs=performance.now()-started,after=await driver.state();
    const samples=trace.slice(mark),start=samples.find(x=>x.phase==='start'),processed=samples.find(x=>x.phase==='processed'),published=samples.find(x=>x.phase==='published');
    assert.ok(start&&processed&&published);assert.equal(after.app,'Calculation');assert.ok(after.render.transitionIdle&&!after.render.pending);
    if(expected!==undefined){assert.equal(after.calculation.status,'ok');assert.equal(after.calculation.exact,expected);}
    const png=await el.locator('canvas').evaluate(c=>({w:c.width,h:c.height,png:c.toDataURL().split(',')[1]}));assert.deepEqual([png.w,png.h],[320,240]);
    if(repeat===0)await writeFile(resolve(folder,id+'.png'),Buffer.from(png.png,'base64'));
    records.push({browser:browserName,surface,id,repeat,events,wallMs,processingMs:processed.at-start.at,presentationMs:published.at-processed.at,inputEdges:after.input.events-before.input.events,publications:after.render.published-before.render.published,frameMs:after.frameMs,heapBytes:after.heapBytes,usedHeapBytes:after.usedHeapBytes});
   }
   assert.deepEqual(errors,[]);traces.push({browser:browserName,surface,bootMs,events:trace});
   console.log('PASS',browserName,surface,workloads.length*repeats,'event timing samples');
  } finally {await context.close();}
 }}finally{await browser.close();}
}}finally {
 await writeFile(resolve(out,'results.json'),JSON.stringify(records,null,2));await writeFile(resolve(out,'events.json'),JSON.stringify(traces,null,2));await server.close();
}
const groups=new Map();for(const row of records){const key=[row.browser,row.surface,row.id].join('/');if(!groups.has(key))groups.set(key,[]);groups.get(key).push(row);}
const medians=[...groups].map(([id,rows])=>{const result={id,repetitions:rows.length};for(const metric of ['wallMs','processingMs','presentationMs','heapBytes','usedHeapBytes']){const values=rows.map(r=>r[metric]).filter(Number.isFinite).sort((a,b)=>a-b);result[metric]=values.length?values[Math.floor(values.length/2)]:null;}for(const metric of ['p50','p95','max']){const values=rows.map(r=>r.frameMs?.[metric]).filter(Number.isFinite).sort((a,b)=>a-b);result['cumulativeFrame'+metric.toUpperCase()+'Ms']=values.length?values[Math.floor(values.length/2)]:null;}return result;});
await writeFile(resolve(out,'summary.json'),JSON.stringify({passed:true,records:records.length,browsers:[...new Set(records.map(r=>r.browser))],surfaces:[...new Set(records.map(r=>r.surface))],scope:'Host browser end-to-end event processing and published presentation, includes Playwright delivery and two RAF composition barriers; not pure C++ renderer or ESP32 latency',medians},null,2));
