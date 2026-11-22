// Product controls, SDL text and pointer only. Diagnostics are read-only.
import assert from 'node:assert/strict';
import {mkdir,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {chromium,firefox,webkit} from 'playwright';
import {startStaticServer} from './test-server.mjs';
import {calculationDriver} from './calculation-driver.mjs';
const out=resolve(process.env.NUMOS_QUANTITY_OUT||'out/calc-units-01/web/quantities');
const server=await startStaticServer(resolve(process.env.NUMOS_WEB_ROOT),8902);
const records=[],trace=[];
try {for(const [name,type] of Object.entries({chromium,firefox,webkit})) {
 if(process.env.NUMOS_BROWSER&&process.env.NUMOS_BROWSER!==name)continue;
 const browser=await type.launch({headless:true});
 try {for(const surface of ['shell','component']) {
  const context=await browser.newContext({viewport:{width:1100,height:1100}}),page=await context.newPage(),errors=[];
  page.on('pageerror',e=>errors.push(String(e)));
  const folder=resolve(out,name,surface);await mkdir(folder,{recursive:true});
  const boot=async()=>{
   await page.goto(server.origin+(surface==='shell'?'/index.html':'/fixture.html'));
   if(surface==='component') {
    await page.waitForFunction(()=>customElements.get('numos-emulator'));
    await page.evaluate(()=>{const e=document.createElement('numos-emulator');e.setAttribute('controls','');document.body.append(e);});
    await page.locator('numos-emulator').locator('[data-action=overlay-start]').click();
   }
   await page.waitForFunction(()=>document.querySelector('numos-emulator')?.diagnosticState()?.ready,null,{timeout:60000});
   const el=page.locator('numos-emulator');
   if(!await el.locator('[data-physical-id=r9c4]').isVisible())await el.locator('[data-action=controls]').click();
   return el;
  };
  let el=await boot(),driver=calculationDriver(page,el,trace),keys=driver.keys;
  const shot=async id=>{const data=await el.locator('canvas').evaluate(c=>c.toDataURL().split(',')[1]);await writeFile(resolve(folder,id+'.png'),Buffer.from(data,'base64'));};
  const expect=async expected=>{
   try {await page.waitForFunction(expected=>{const s=document.querySelector('numos-emulator').diagnosticState().toolbox;return Object.entries(expected).every(([k,v])=>s[k]===v);},expected,{timeout:10000});}
   catch(error){await shot('failure');const state=await driver.state();records.push({browser:name,surface,expected,state,errors});console.error(JSON.stringify({expected,toolbox:state.toolbox}));throw error;}
  };
  // Launch and editing have separate transition/publication boundaries.
  await keys('ENTER');await keys('2 ^ 3');const before=await driver.state();
  await keys('TOOLBOX');await expect({open:true,group:0});await shot('root');
  await driver.clickCanvas(8,45,'dismiss-popup');await expect({open:false});
  await keys('TOOLBOX');await expect({open:true,group:0,id:103});
  await keys('UP RIGHT RIGHT RIGHT ENTER');await expect({queryFocus:true});
  await el.locator('canvas').focus();await page.keyboard.type('raiz');
  await expect({queryBytes:4,count:1});await driver.settled('SDL-search');
  await keys('DOWN');await expect({id:111});await shot('search');
  assert.deepEqual((await driver.state()).giac,before.giac,'search evaluated mathematics');
  await keys('FORMAT');await shot('options');await keys('ENTER BACK BACK');await expect({open:false,favorites:1});
  await keys('ENTER');assert.equal((await driver.state()).calculation.exact,'8','cancel changed exponent');
  await keys('AC TOOLBOX UP RIGHT ENTER');await expect({group:1,id:111});await shot('favorites');
  const prior=(await driver.state()).giac.structuredEvaluations;
  await driver.clickCanvas(150,111,'insert-favorite');await expect({open:false});
  assert.equal((await driver.state()).giac.structuredEvaluations,prior,'click leaked evaluation');
  await keys('3 RIGHT 8 ENTER');assert.equal((await driver.state()).calculation.exact,'2');await shot('inserted');
  await keys('AC TOOLBOX UP RIGHT RIGHT RIGHT ENTER');await el.locator('canvas').focus();await page.keyboard.type('Logarithm');
  await expect({queryBytes:9});await keys('DOWN RIGHT');await expect({group:100,id:120,variant:1});
  await keys('UP ENTER 2 RIGHT 8 ENTER');assert.equal((await driver.state()).calculation.exact,'3');
  await keys('AC TOOLBOX UP RIGHT');await shot('tabs-focus');await keys('BACK');
  await keys('TOOLBOX');await el.locator('canvas').focus();await page.keyboard.type('zzzz');
  await keys('DOWN');await expect({count:0});await shot('empty-search');await keys('BACK BACK');
  await keys('TOOLBOX DOWN DOWN DOWN DOWN DOWN DOWN DOWN DOWN ENTER');await expect({group:18,count:3});await shot('variables');
  await keys('DOWN ENTER');await expect({group:20,count:24,id:300});await shot('greek');
  await keys('DOWN DOWN DOWN RIGHT');await expect({group:1303,id:303,variant:1});await shot('delta-case');
  await keys('ENTER ^ 2 ENTER');assert.equal((await driver.state()).calculation.exact,'Δ^2');await shot('delta-result');
  await keys('AC TOOLBOX');await el.locator('canvas').focus();await page.keyboard.type('Infinity');
  await keys('DOWN');await expect({id:410,variant:0});await keys('RIGHT');await expect({group:101,count:3,id:410,variant:1});await shot('infinity-signs');
  await keys('DOWN ENTER ^ 2 ENTER');assert.equal((await driver.state()).calculation.exact,'+infinity');
  await keys('AC TOOLBOX');await el.locator('canvas').focus();await page.keyboard.type('Both infinities');
  await keys('DOWN ENTER ENTER');assert.equal((await driver.state()).calculation.exact,'[-infinity, +infinity]');await shot('infinity-both');
  // Units use the real modal/provider and the existing observable publication boundary.
  const unitSearch=async(query,id,variant=0)=>{
   await keys('AC TOOLBOX');await el.locator('canvas').focus();
   for(const character of query) {
    if(character.codePointAt(0)<128)await page.keyboard.type(character);
    else {
     // Canvas is not contenteditable: IME insertText does not emit keypress.
     // Deliver the Unicode keyboard event SDL actually consumes, through DOM,
     // without an expression/AST/text export or access to runtime memory.
     const code=character.codePointAt(0);
     await el.locator('canvas').dispatchEvent('keypress',{key:character,charCode:code,keyCode:code,which:code,bubbles:true,cancelable:true});
    }
   }
   await expect({queryBytes:new TextEncoder().encode(query).length});await keys('DOWN');await expect({id,variant});
  };
  await keys('AC 2 TOOLBOX DOWN DOWN DOWN DOWN DOWN DOWN DOWN DOWN DOWN DOWN ENTER');
  await expect({group:200});await shot('units');await keys('ENTER RIGHT');
  await expect({group:32769,id:32769,variant:0,selection:12,count:25});await shot('metre-centred');
  const beforeUnits=(await driver.state()).giac;
  await keys('UP UP UP');await expect({variant:10});await keys('DOWN DOWN DOWN DOWN DOWN DOWN DOWN');await expect({variant:16});
  await keys('BACK');await expect({group:201,id:32769,variant:0});await keys('ENTER');await expect({open:false});
  assert.deepEqual((await driver.state()).giac,beforeUnits,'unit navigation/insertion called Giac');
  await keys('ENTER');assert.equal((await driver.state()).calculation.status,'ok');await shot('unit-unavailable');
  assert.ok((await driver.state()).giac.structuredEvaluations>beforeUnits.structuredEvaluations,'quantity coefficient did not reach exact engine');
  await keys('UP ENTER');assert.equal((await driver.state()).calculation.status,'ok','history lost typed unit');
  for(const [q,id,variant] of [['milímetro',32769,15],['millimeter',32769,15],['µs',32771,16],['μs',32771,16],['mA',32772,15],['MA',32772,9],['MHz',32778,9],['mHz',32778,15],['Pa',32780,0],['pA',32772,18],['kΩ',32786,10],['µF',32785,16],['cm²',32869,14],['cm³',32870,14]]) {
   await unitSearch(q,id,variant);await keys('ENTER ENTER');assert.equal((await driver.state()).calculation.status,'ok',q);
  }
  await unitSearch('mm',32769,15);await keys('FORMAT ENTER BACK BACK');await expect({open:false,favorites:2});
  await unitSearch('km/h',32879);await keys('FORMAT ENTER BACK BACK');await expect({open:false,favorites:3});
  await keys('AC 2 + 2 ENTER');assert.equal((await driver.state()).calculation.exact,'4');
  // The existing public persistence controller flushes IDBFS; FS and memory
  // stay private. Real reload in the same browser context proves durability.
  const flushed=await el.evaluate(e=>e.flushPersistence());
  records.push({browser:name,version:browser.version(),surface,phase:'before-reload',flushed,state:await driver.state()});
  await shot('log-result');el=await boot();driver=calculationDriver(page,el,trace);keys=driver.keys;
  await keys('ENTER');await keys('TOOLBOX UP RIGHT ENTER');await expect({group:1,id:111,favorites:3});await shot('persisted-after-reload');
  await keys('DOWN RIGHT');await expect({group:32769,id:32769,variant:15,selection:15});await shot('persisted-mm-prefix');
  await keys('BACK DOWN');await expect({id:32879,variant:0});await keys('ENTER ENTER');assert.equal((await driver.state()).calculation.status,'ok');
  await keys('AC TOOLBOX UP RIGHT ENTER');await expect({id:111});
  await keys('FORMAT DOWN DOWN DOWN ENTER');await shot('help');await keys('BACK BACK BACK');
  // New presses immediately after closing must survive; shortcuts retain
  // their existing meaning outside the modal.
  await keys('2 + 3 ENTER FORMAT');assert.equal((await driver.state()).calculation.status,'ok');
  // Same modal in target mode: coefficient and display unit publish together.
  const quantityPick=async(query,id,variant=0)=>{
   await keys('TOOLBOX');await el.locator('canvas').focus();await page.keyboard.type(query);
   await expect({queryBytes:new TextEncoder().encode(query).length});await keys('DOWN');
   await expect({id,variant});await keys('ENTER');await expect({open:false});
  };
  const output=async()=>{await keys('SHIFT ALPHA FORMAT ENTER');};
  const allUnits=async()=>keys('DOWN '.repeat(10)+'UP UP ENTER');
  const target=async(query,id,variant=0,component=-1)=>{
   await output();
   if(component<0)await allUnits();
   else {await keys('DOWN '.repeat(10)+'UP ENTER');await keys('DOWN '.repeat(component)+'ENTER');}
   await expect({open:true,group:200});await el.locator('canvas').focus();await page.keyboard.type(query);
   await expect({queryBytes:new TextEncoder().encode(query).length});await keys('DOWN');await expect({id,variant});
   await keys('ENTER');await expect({open:false});
  };
  const answer=async()=>{const before=(await driver.state()).render.published;await el.locator('canvas').focus();await page.keyboard.press('a');await page.waitForFunction(n=>document.querySelector('numos-emulator').diagnosticState().render.published>n,before);await driver.settled('SDL-Ans');};
  const exact=async(value)=>{const c=(await driver.state()).calculation;assert.equal(c.status,'ok');assert.equal(c.exact,value);};
  await keys('AC 2');await quantityPick('metre',32769);await keys('+ 3 0');await quantityPick('cm',32769,14);
  await keys('ENTER');await exact('23/10');await shot('quantity-sum');
  await target('cm',32769,14);await exact('230');await shot('quantity-sum-cm');
  await keys('FORMAT FORMAT');await exact('230');
  await output();await allUnits();await el.locator('canvas').focus();await page.keyboard.type('km');
  await keys('DOWN RIGHT DOWN BACK BACK BACK');await expect({open:false});await exact('230');
  await keys('AC');await answer();await keys('/ 2');await quantityPick('Second',32771);await keys('ENTER');await exact('23/20');await shot('quantity-ans');
  await keys('AC 6');await quantityPick('km',32769,10);await keys('/ 3 0 0');await quantityPick('Second',32771);
  await keys('RIGHT ENTER');await exact('20');
  const beforeQuick=(await driver.state()).giac;
  await output();await shot('quick-speed');
  assert.deepEqual((await driver.state()).giac,beforeQuick,'quick choices evaluated mathematics');
  await driver.clickCanvas(152,142,'quick-kmh');await exact('72');await shot('quick-speed-kmh');
  await output();await keys('DOWN ENTER');await exact('20');
  await target('km',32769,10,0);await exact('1/50');
  await target('hour',32816,0,1);await exact('72');await shot('quantity-components-kmh');
  await keys('AC 2');await quantityPick('metre',32769);await keys('+ 3');await quantityPick('Second',32771);
  await keys('ENTER');assert.equal((await driver.state()).calculation.status,'quantity_error');await shot('quantity-error');
  await keys('AC');await answer();await keys('ENTER');await exact('20');await shot('quantity-recovery');
  await keys('AC TOOLBOX');await el.locator('canvas').focus();await page.keyboard.type('Mach number');await keys('DOWN');await expect({id:16497});
  await keys('ENTER ENTER');assert.equal((await driver.state()).calculation.status,'units_unavailable');await shot('quantity-context');
  await keys('AC 1');await quantityPick('metre',32769);
  for(const [query,id,prefix] of [['Second',32771,0],['kg',32770,10],['Ampere',32772,0],['delta k',32829,0],['mole',32774,0],['candela',32775,0]]) {
   await keys('* 1');await quantityPick(query,id,prefix);
  }
  await keys('ENTER');await exact('1');await output();await shot('quick-wide-current');
  await keys('BACK');await exact('1');
  await keys('AC 2 + 2 ENTER');await exact('4');
  assert.deepEqual(errors,[]);records.push({browser:name,surface,passed:true});
  await context.close();
 }}finally{await browser.close();}
}}finally{
 await mkdir(out,{recursive:true});await writeFile(resolve(out,'results.json'),JSON.stringify(records,null,2));await writeFile(resolve(out,'events.json'),JSON.stringify(trace,null,2));await server.close();
}
console.log('PASS Units + Toolbox: all requested browser surfaces, events and real IDBFS reload');
