#!/usr/bin/env python3
"""Product quantity gate: physical FORMAT, real Toolbox insertion, no AST injection."""
import argparse,json,os,subprocess,time,tempfile
from pathlib import Path
from PIL import Image
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--bin',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--only');a=p.parse_args()
root=Path(__file__).resolve().parents[1];a.out.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,PATH='C:/mingw64/bin;C:/SDL2/x86_64-w64-mingw32/bin;'+os.environ.get('PATH',''))
records={}
def keys(s):
 physical={'SHIFT':(4,0),'ALPHA':(3,0),'FORMAT':(4,5)}
 # DEVICE-INPUT-PATH-01: `key` uses the production keypad path. Memories A-F
 # are stored with STO 1-6, but on the keypad the letters are ALPHA + x,t,θ,n
 # / frac / sqrt / x^□ / x² / log (ALPHA 1-6 is { } # X Y Z), so recall
 # through the real letter keys.
 letters=('x','physical_frac','sqrt','pow','square','log')
 s=' '.join(letters[int(t)-1] if i and s.split()[i-1]=='ALPHA' and t in '123456' and len(t)==1 else t for i,t in enumerate(s.split()))
 return ''.join('calc_physical %d %d\n'%physical[k] if k in physical else 'key '+k+'\n' for k in s.split())
def selected(item,prefix=0):return f'assert_calc_input toolbox selected {32768|item} {prefix}\n'
def search(q):return 'sdl_text '+q+'\n'+keys('DOWN')
def pick(q,item,prefix=0):return keys('TOOLBOX')+search(q)+selected(item,prefix)+keys('ENTER')+'assert_calc_input toolbox closed\n'
def amount(n,q,item,prefix=0):return keys(' '.join(n))+pick(q,item,prefix)
def check(coefficient,canonical=None,terms=None):
 s='assert_calc_status ok\nassert_calc_exact '+coefficient+'\n'
 if canonical is not None:s+='assert_calc_input quantity canonical '+canonical+'\n'
 if terms is not None:s+='assert_calc_input quantity terms '+terms+'\n'
 return s
menu='SHIFT ALPHA FORMAT'
def output():return keys(menu+' ENTER')
def all_units():return keys('DOWN '*10+'UP UP')+'assert_calc_input unit_menu all\n'+keys('ENTER')
def components():return keys('DOWN '*10+'UP ENTER')+'assert_calc_input unit_menu components\n'
def default_si():return keys('DOWN '*10+'ENTER')
def target(q,item,prefix=0,component=None):
 # Navigate real rows. Clamping at the end avoids assuming the number of
 # compatible favorites; quick-path tests count the minimal product presses.
 route=all_units() if component is None else components()+keys('DOWN '*component+'ENTER')
 return output()+route+search(q)+selected(item,prefix)+keys('ENTER')+'assert_calc_input toolbox closed\n'
def shot(name):return 'wait 2\nscreenshot '+Path(os.path.relpath(a.out/(name+'.ppm'),root)).as_posix()+'\n'
def run(name,body,fs=None):
 if a.only and a.only not in name:return
 fs=fs or Path(tempfile.mkdtemp(prefix=name+'-',dir=a.out))
 s='wait 200\nopen_app Calculation\nwait 30\n'+body+'assert_calc_input quantity dump\nlog QUANTITIES_DONE\n'
 path=a.out/(name+'.numos');path.write_text(s,encoding='utf8')
 frames=sum(int(l.split()[1])+1 if l.startswith('wait ') else 1 for l in s.splitlines())+100
 start=time.perf_counter();r=subprocess.run([str(a.bin.resolve()),'--headless','--quiet','--deterministic','--frames',str(frames),'--script',os.path.relpath(path,root),'--fs-sandbox-dir',os.path.relpath(fs,root)],cwd=root,env=env,capture_output=True,timeout=180)
 log=(r.stdout+r.stderr).decode('utf8',errors='replace');(a.out/(name+'.log')).write_text(log,encoding='utf8')
 ok=r.returncode==0 and 'QUANTITIES_DONE' in log
 records[name]=dict(passed=ok,exit=r.returncode,seconds=time.perf_counter()-start,assertions=log.count(': PASS -'))
 (a.out/'results.json').write_text(json.dumps(records,indent=2))
 print('PASS' if ok else 'FAIL',name,flush=True)
 if not ok:print(log[-2500:],flush=True)
 for line in s.splitlines():
  if line.startswith('screenshot '):
   path=root/line[11:]
   if path.exists():im=Image.open(path);assert im.size==(320,240);im.save(path.with_suffix('.png'))

summation=amount('2','metre',1)+keys('ADD')+amount('30','cm',1,14)+keys('ENTER')
run('sum-metres',summation+check('23/10','23/10','1:0:1')+shot('sum-metres')+target('cm',1,14)+check('230','23/10','1:14:1')+shot('sum-centimetres')+keys('FORMAT FORMAT')+check('230','23/10','1:14:1'))
run('small-sum',amount('5','mm',1,15)+keys('ADD')+amount('2','cm',1,14)+keys('ENTER')+check('1/40','1/40','1:0:1'))
run('area',amount('2','cm',1,14)+keys('MUL')+amount('3','cm',1,14)+keys('ENTER')+check('3/5000','3/5000','1:0:2')+shot('area')+target('cm²',101,14)+check('6','3/5000','1:14:2')+shot('area-cm2'))
run('cube',keys('LPAREN')+amount('2','cm',1,14)+keys('RPAREN POW 3 ENTER')+check('1/125000','1/125000','1:0:3'))
run('root',keys('SQRT 9')+pick('cm²',101,14)+keys('ENTER')+check('3/100','3/100','1:0:1'))
run('mass',amount('1','kg',2,10)+keys('ADD')+amount('500','gram',2)+keys('ENTER')+check('3/2','3/2','2:10:1')+keys('AC')+amount('250','mg',2,15)+keys('ENTER')+check('1/4000','1/4000','2:10:1'))
velocity=amount('6','km',1,10)+keys('FRAC')+amount('300','Second',3)+keys('RIGHT ENTER')
run('velocity',velocity+check('20','20','1:0:1,3:0:-1')+shot('velocity')+target('km/h',111)+check('72','20','1:10:1,48:0:-1')+shot('velocity-kmh'))
run('components',velocity+output()+components()+shot('components-menu')+keys('ENTER')+search('km')+selected(1,10)+shot('components-length')+keys('ENTER')+check('1/50','20','1:10:1,3:0:-1')+target('hour',48,component=1)+check('72','20','1:10:1,48:0:-1')+shot('components-kmh')+output()+default_si()+check('20','20','1:0:1,3:0:-1'))
run('electricity',amount('12','Volt',16)+keys('FRAC')+amount('3','Ampere',4)+keys('RIGHT ENTER')+check('4','4','18:0:1')+shot('electricity')+keys('AC')+amount('2','Ampere',4)+keys('MUL')+amount('3','Ohm',18)+keys('ENTER')+check('6','6','16:0:1'))
energy=amount('100','Watt',14)+keys('MUL')+amount('2','hour',48)+keys('ENTER')
run('energy',energy+check('720000','720000','13:0:1')+shot('energy-joule')+target('kWh',114,10)+check('1/5','720000','14:10:1,48:0:1')+shot('energy-kwh'))
run('cancellation',amount('1','metre',1)+keys('FRAC')+amount('1','cm',1,14)+keys('ENTER')+check('100')+'assert_calc_input quantity none\n')
example=keys('LPAREN')+amount('2','Second',3)+keys('FRAC')+amount('5','metre',1)+keys('RIGHT RPAREN MUL LPAREN')+amount('10','km',1,10)+keys('FRAC')+amount('40','Second',3)+keys('POW 2 RIGHT RIGHT RPAREN ENTER')
run('user-example',example+check('100','100','3:0:-1')+shot('user-example')+target('Hertz',10)+check('100','100','10:0:1')+shot('user-example-hz'))
run('ans-after-output',amount('2','metre',1)+keys('ENTER')+target('cm',1,14)+check('200','2','1:14:1')+keys('AC ans FRAC')+amount('2','Second',3)+keys('ENTER')+check('1','1','1:0:1,3:0:-1')+shot('ans-after-output'))
run('cancel-history',summation+target('mm',1,15)+check('2300','23/10','1:15:1')+output()+all_units()+search('km')+selected(1,10)+keys('RIGHT DOWN BACK BACK BACK')+'assert_calc_input toolbox closed\n'+check('2300','23/10','1:15:1')+keys('UP')+check('23/10','23/10','1:0:1')+keys('ENTER')+check('23/10','23/10','1:0:1'))
run('incompatible-and-recovery',amount('2','metre',1)+keys('ENTER AC')+amount('2','metre',1)+keys('ADD')+amount('3','Second',3)+keys('ENTER')+'assert_calc_status quantity_error\nassert_calc_input quantity error dimension_mismatch\n'+shot('dimension-error')+keys('AC ans ENTER')+check('2','2','1:0:1')+shot('recovered-ans')+keys('AC 2 ADD 2 ENTER')+check('4'))
fs=Path(tempfile.mkdtemp(prefix='quantity-memory-',dir=a.out))
body=keys('7 7 ENTER STO 1 AC')+amount('2','metre',1)+keys('ENTER')
for k in '123456':body+=keys('STO '+k)
for k in '123456':body+=keys('AC ALPHA '+k+' ENTER')+check('2','2','1:0:1')
run('session-memories-write',body+keys('AC')+amount('3','Ampere',4)+keys('ENTER')+check('3','3','4:0:1'),fs)
body=''
for k in '123456':body+=keys('AC ALPHA '+k+' ENTER')+check('0')+'assert_calc_input quantity none\n'
run('session-memories-restart',body,fs)
run('numeric-formats',velocity+target('km/h',111)+keys(menu+' DOWN DOWN DOWN ENTER')+check('72','20','1:10:1,48:0:-1')+shot('quantity-scientific')+keys('FORMAT')+keys('SHIFT EXP')+check('72','20','1:10:1,48:0:-1')+shot('quantity-engineering')+keys('FORMAT')+keys(menu+' DOWN DOWN DOWN DOWN DOWN ENTER ENTER')+check('72','20','1:10:1,48:0:-1')+shot('quantity-fixed')+keys('FORMAT AC ans ENTER')+check('20','20','1:0:1,3:0:-1'))
run('negative-zero-incomplete',keys('NEGATE')+amount('2','metre',1)+keys('ENTER')+check('-2','-2','1:0:1')+keys('AC')+amount('2','metre',1)+keys('SUB')+amount('2','metre',1)+keys('ENTER')+check('0','0','1:0:1')+keys('AC')+amount('2','metre',1)+keys('ADD ENTER')+'assert_calc_status parse_error\nassert_calc_input quantity error incomplete\n'+amount('3','metre',1)+keys('ENTER')+check('5','5','1:0:1'))
for i,tag in enumerate(('en-US','en-GB','es-ES','es-419')):
 locale=keys('HOME')+'wait 20\nopen_app Settings\nwait 20\n'+keys('DOWN '*4+'RIGHT '*i+'HOME')+'wait 20\nopen_app Calculation\nwait 20\n'
 run(tag,locale+summation+target('cm',1,14)+check('230','23/10','1:14:1')+shot(tag+'-result')+output()+shot(tag+'-output-menu')+keys('BACK AC TOOLBOX')+search('Mach number')+'assert_calc_input toolbox selected 16497 0\n'+keys('ENTER ENTER')+'assert_calc_status units_unavailable\nassert_calc_input quantity error context_pending\n'+shot(tag+'-context')+keys('AC TOOLBOX')+search('Speed of light in vacuum')+'assert_calc_input toolbox selected 16385 0\n'+keys('ENTER ENTER')+check('299792458','299792458')+shot(tag+'-physical-reference'))
raise SystemExit(0 if records and all(r['passed'] for r in records.values()) else 1)
