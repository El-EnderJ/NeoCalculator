// Real product controls; observers never mutate the mathematical state.
import assert from 'node:assert/strict';
import {mkdir,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {chromium,firefox,webkit} from 'playwright';
import {startStaticServer} from './test-server.mjs';
import {calculationDriver} from './calculation-driver.mjs';
const out=resolve(process.env.NUMOS_SCIENTIFIC_OUT||'out/scientific-quantities-01/E-final/web');
const server=await startStaticServer(resolve(process.env.NUMOS_WEB_ROOT),8908);
const records=[],trace=[];
try {for(const [name,type] of Object.entries({chromium,firefox,webkit})) {
 if(process.env.NUMOS_BROWSER&&process.env.NUMOS_BROWSER!==name)continue;
 const browser=await type.launch({headless:true});
 try {for(const surface of ['shell','component']) {
  if(process.env.NUMOS_SURFACE&&process.env.NUMOS_SURFACE!==surface)continue;
  const context=await browser.newContext({viewport:{width:1100,height:1100}}),page=await context.newPage(),errors=[];
  page.on('pageerror',e=>errors.push(String(e)));
  const folder=resolve(out,name,surface);await mkdir(folder,{recursive:true});
  const watchdog=setTimeout(async()=>{
   await writeFile(resolve(folder,'watchdog.json'),JSON.stringify({errors,lastEvents:trace.slice(-12)},null,2));
   await browser.close();
  },240000);
  watchdog.unref();
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
  const expect=async expected=>page.waitForFunction(expected=>{const s=document.querySelector('numos-emulator').diagnosticState().toolbox;return Object.entries(expected).every(([k,v])=>s[k]===v);},expected,{timeout:10000});
  const query=async(q,id)=>{await el.locator('canvas').focus();await page.keyboard.type(q);await expect({queryBytes:new TextEncoder().encode(q).length});await keys('DOWN');await expect({id});};
  const pick=async(q,id)=>{await keys('TOOLBOX');await query(q,id);await keys('ENTER');await expect({open:false});};
  const measure=async(n,u)=>{await pick('measurement',750);await keys(n.split('').join(' '));await keys('RIGHT');await keys(u.split('').join(' '));await keys('RIGHT');};
  const result=async(exact,u,count,display=u)=>{
   const s=await driver.state();assert.equal(s.calculation.status,'ok');assert.equal(s.calculation.exact,exact);
   assert.equal(s.uncertainty.sources,count);assert.ok(Math.abs(s.uncertainty.standard-u)<=Math.max(1e-100,Math.abs(u)*6e-6));
   assert.ok(Math.abs(s.uncertainty.display-display)<=Math.max(1e-100,Math.abs(display)*6e-6));
  };
  const target=async(q,id)=>{await keys('SHIFT ALPHA FORMAT ENTER');await keys('DOWN '.repeat(10)+'UP UP ENTER');await expect({open:true,group:200});await query(q,id);await keys('ENTER');await expect({open:false});};
  const openApp=async(id,name)=>{
   if((await driver.state()).app!=='Menu')await keys('MODE');
   for(let i=0;i<24&&(await driver.state()).menuFocus!==id;i++)await keys((await driver.state()).menuFocus<id?'RIGHT':'LEFT');
   assert.equal((await driver.state()).menuFocus,id);await keys('ENTER');
   await page.waitForFunction(name=>document.querySelector('numos-emulator').diagnosticState().app===name,name);
  };
  const angle=async(mode)=>{
   await openApp(10,'Settings');
   if((await driver.state()).storage.angleMode!==mode)await keys('ENTER');
   assert.equal((await driver.state()).storage.angleMode,mode);await openApp(0,'Calculation');
  };
  try {
   await keys('ENTER');await driver.settled('launch');
   await measure('10','1');await pick('metre',32769);await keys('ENTER STO 1');await result('10',1,1);await shot('measurement');
   await keys('AC ALPHA 1 + ALPHA 1 ENTER');await result('20',2,1);
   await keys('AC ALPHA 1 - ALPHA 1 ENTER');await result('0',0,0);await shot('same-source');
   await keys('AC');await measure('10','1');await keys('-');await measure('10','1');await keys('ENTER');await result('0',Math.SQRT2,2);await shot('independent');
   await keys('AC');await measure('20','0.5');await pick('Degree Celsius',32791);await keys('ENTER');await result('5863/20',.5,1);
   await target('Degree Fahrenheit',32828);await result('68',.5,1,.9);await shot('temperature');
   for(let mode=0;mode<4;mode++) {
    await keys('SHIFT ALPHA FORMAT DOWN DOWN DOWN ENTER');await shot('uncertainty-menu');
    await keys('DOWN '.repeat(mode)+'ENTER');await result('68',.5,1,.9);await shot('format-'+mode);
   }
   await keys('AC');await pick('gravitational constant',16443);await keys('ENTER');await result('66743/1000000000000000',1.5e-15,1);await shot('G');
   await keys('AC');await pick('Bohr radius',16386);await keys('*');await pick('electron mass',16387);await keys('ENTER');
   const correlated=(await driver.state()).uncertainty;assert.equal(correlated.sources,2);assert.ok(Math.abs(correlated.standard/7.347417886856706e-51-1)<6e-6);await shot('correlation');
   await keys('AC');await measure('0','1');await keys('^ 2 RIGHT ENTER');assert.equal((await driver.state()).calculation.status,'quantity_error');await shot('nonlinear');
   await keys('AC 2 + 2 ENTER');assert.equal((await driver.state()).calculation.exact,'4');
   // A non-cardinal explicit radian must keep its nominal and sensitivity even
   // when Settings is DEG. A second measured factor probes that nominal through
   // an independent derivative, instead of trusting a displayed text string.
   await angle('deg');await keys('AC SIN');await measure('1','0.01');await pick('Radian',32776);await keys('RIGHT ENTER STO 1');
   await result('sin(1)',Math.cos(1)*.01,1);await shot('explicit-radian-deg');
   await keys('AC ALPHA 1 *');await measure('3','0.01');await keys('ENTER');
   const expectedProduct=Math.hypot(3*Math.cos(1)*.01,Math.sin(1)*.01);
   const angleProduct=(await driver.state()).uncertainty;
   assert.ok(Math.abs(angleProduct.standard/expectedProduct-1)<6e-6);
   await keys('AC SIN');await measure('1','0.01');await pick('Degree',32825);await keys('RIGHT ENTER');
   await result('sin(pi/180)',Math.cos(Math.PI/180)*Math.PI/180*.01,1);await shot('explicit-degree-deg');
   await angle('rad');await keys('AC ALPHA 1 ENTER');await result('sin(1)',Math.cos(1)*.01,1);
   await angle('deg');await keys('AC SIN');await measure('30','0.1');await keys('RIGHT ENTER STO 1');
   const degreeU=Math.cos(Math.PI/6)*Math.PI/180*.1;
   await result('1/2',degreeU,1);await shot('dimensionless-deg');
   await angle('rad');await keys('AC ALPHA 1 ENTER');await result('1/2',degreeU,1);
   await keys('AC TOOLBOX');await query('gravitational constant',16443);await keys('FORMAT ENTER BACK BACK');assert.ok(await el.evaluate(e=>e.flushPersistence()));
   el=await boot();driver=calculationDriver(page,el,trace);keys=driver.keys;await keys('ENTER');await driver.settled('reload');
   await keys('TOOLBOX UP RIGHT ENTER');await expect({id:16443});await shot('favorite');await keys('ENTER ENTER');await result('66743/1000000000000000',1.5e-15,1);
   assert.deepEqual(errors,[]);records.push({browser:name,surface,passed:true});
  } catch(error) {
   let failureState=null,canvasFailure=null;
   try {await shot('failure');}catch(e){canvasFailure=String(e);}
   try {failureState=await driver.state();}catch{}
   records.push({browser:name,surface,passed:false,error:String(error),stack:error.stack,state:failureState,errors,canvasFailure});
   await writeFile(resolve(folder,'failure-page.html'),await page.content().catch(()=>''));throw error;
  }
  finally {clearTimeout(watchdog);await context.close();}
 }}finally{await browser.close();}
}}finally{
 await mkdir(out,{recursive:true});await writeFile(resolve(out,'results.json'),JSON.stringify(records,null,2));await writeFile(resolve(out,'events.json'),JSON.stringify(trace,null,2));await server.close();
}
console.log('PASS scientific quantities on requested browsers and surfaces');
