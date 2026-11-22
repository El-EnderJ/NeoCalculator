#!/usr/bin/env python3
"""Toolbox product events: open/search/navigate/insert/fill/evaluate, no AST injection."""
import sys
sys.stdout.reconfigure(errors="replace")
import tempfile
import argparse,importlib.util,json,os,re,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--bin',required=True);p.add_argument('--out',type=Path,default=ROOT/'out/toolbox-01/events');p.add_argument('--only');a=p.parse_args()
a.out.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,PATH='C:/mingw64/bin;C:/SDL2/x86_64-w64-mingw32/bin;'+os.environ['PATH'])
rows=json.loads((ROOT/'src/math/toolbox-catalog.json').read_text(encoding='utf-8'))
# `key` injects through the production keypad path. An `L:` code is the legacy
# direct-to-app logical key (`lkey`), only for chords with no production
# equivalent (DEVICE-INPUT-PATH-01).
def keys(*codes):return ''.join(('lkey '+str(c)[2:] if str(c).startswith('L:') else 'key '+str(c))+'\n' for c in codes)
def tab(n):return keys('TOOLBOX','UP',*(['RIGHT']*n),'ENTER')
def search(query):return keys('TOOLBOX')+'sdl_text '+query+'\nwait 2\n'+keys('DOWN')
def pick(e):return search(e['en'])+'assert_calc_input toolbox geometry\n'+f'assert_calc_input toolbox selected {e["id"]} {e["variant"]}\n'+keys('ENTER')+'assert_calc_input toolbox closed\n'
def fill(value):
    out=''
    for c in value:
        if c=='i':out+=pick(next(e for e in rows if e['id']==182))
        else:out+=keys({'-':'NEGATE','+':'ADD','x':'x','^':'POW','.':'DOT'}.get(c,c))
    if '^' in value:out+=keys('RIGHT')
    return out
results={}
def run(name,body,fs=None):
    if a.only and a.only not in name:return
    script='wait 200\nkey ENTER\nwait 30\nassert_app Calculation\n'+body+'\nlog TOOLBOX_DONE\n'
    path=a.out/(name+'.numos');path.write_text(script,encoding='utf-8')
    frames=sum(int(l.split()[1])+1 if l.startswith('wait ') else 1 for l in script.splitlines())+120
    cmd=[a.bin,'--headless','--deterministic','--quiet','--frames',str(frames),'--script',os.path.relpath(path,ROOT)]
    if fs is None:
        filesystem_runs=a.out/'filesystems';filesystem_runs.mkdir(exist_ok=True)
        fs=Path(tempfile.mkdtemp(prefix=name+'-',dir=filesystem_runs))
    if fs:cmd+=['--fs-sandbox-dir',os.path.relpath(fs,ROOT)]
    start=time.perf_counter();r=subprocess.run(cmd,cwd=ROOT,env=env,capture_output=True,timeout=120)
    (a.out/(name+'.log')).write_bytes(r.stdout+r.stderr)
    ok=r.returncode==0 and b'TOOLBOX_DONE' in r.stdout
    if name=='favorite-limit':ok=ok and b'favorites=24 ' in r.stdout
    if name=='query-utf8-limit':
        query=re.search(r'query=(.*?) focus=',r.stdout.decode('utf8',errors='replace'))
        ok=ok and query is not None and len(query.group(1).encode('utf8'))==64
    results[name]=dict(passed=ok,exit=r.returncode,seconds=time.perf_counter()-start)
    print('PASS' if ok else 'FAIL',name,flush=True)
    if not ok:print((r.stdout+r.stderr).decode(errors='replace')[-1600:])
    (a.out/'results.json').write_text(json.dumps(results,indent=2))
def shot(name):return 'wait 3\nscreenshot '+Path(os.path.relpath(a.out/(name+'.ppm'),ROOT)).as_posix()+'\n'
run('root',keys('2','POW','3','TOOLBOX')+shot('root')+'assert_calc_input toolbox open\n'+keys('BACK')+'assert_calc_input serialized ((2)^(3))\n'+keys('ENTER')+'assert_calc_input near 8 0\n')
run('favorites-empty',tab(1)+shot('favorites-empty')+keys('BACK','BACK'))
run('options-focus',search('Indexed root')+keys('FORMAT')+'assert_calc_input toolbox focus\n'+(keys('DOWN')+'assert_calc_input toolbox focus\n')*3+shot('options-focus'))
for e in rows:
    if e.get('keyboardShortcut'):continue
    body=pick(e)+'assert_calc_input structure\n'
    for i,v in enumerate(e['fixture']):
        if i:body+=keys('RIGHT')
        body+=fill(v)
    body+='assert_calc_input dump\nassert_calc_input complete\nassert_calc_input structure\n'+shot(f'insert-{e["id"]}-{e["variant"]}')+keys('ENTER')+'assert_calc_status ok\n'
    if re.fullmatch(r'-?\d+(\.\d+)?',e['expected']):body+=f'assert_calc_input near {e["expected"]} 1e-10\n'
    elif e['expected']=='1/2':body+='assert_calc_input near 0.5 1e-10\n'
    elif e['expected']=='exp(1)':body+='assert_calc_input near 2.718281828459045 1e-10\n'
    else:body+='assert_calc_exact '+e['expected']+'\n'
    run(f'catalog-{e["id"]}-{e["variant"]}',body)
run('favorites-manage',search('Indexed root')+keys('FORMAT')+shot('management')+keys('ENTER','BACK','BACK','TOOLBOX','UP','RIGHT','ENTER')+shot('favorites-populated')+'assert_calc_input toolbox selected 111 0\n'+keys('FORMAT','DOWN','ENTER','BACK','ENTER','3','RIGHT','8','ENTER')+'assert_calc_input near 2 0\n'+tab(1)+keys('FORMAT','ENTER')+shot('favorites-removed'))
run('search-es',search('raiz')+shot('search-es')+'assert_calc_input toolbox selected 111 0\n'+keys('FORMAT','DOWN','DOWN','DOWN','ENTER')+shot('help')+keys('BACK','BACK','BACK')+shot('search-back'))
run('search-empty',search('zzzz')+'assert_calc_input toolbox geometry\n'+shot('search-empty')+keys('UP','LEFT','DEL','BACK','BACK'))
run('search-alias-log',search('log')+'assert_calc_input toolbox selected 120 0\n')
run('query-utf8-limit',keys('TOOLBOX')+('sdl_text '+('á'*10)+'\n')*3+'sdl_text áá\nsdl_text á\nassert_calc_input toolbox dump\n'+keys('BACK','BACK','2','ENTER')+'assert_calc_input near 2 0\n')
run('query-overflow-refresh',keys('TOOLBOX')+('sdl_text '+('á'*10)+'\n')*3+'sdl_text ááá\nassert_calc_input toolbox query\n'+keys('BACK','BACK','2','ENTER')+'assert_calc_input near 2 0\n')
run('variants',search('Logarithm')+keys('RIGHT')+shot('variants')+'assert_calc_input toolbox selected 120 1\n'+keys('UP','ENTER','2','RIGHT','8','ENTER')+'assert_calc_input near 3 0\n')
for name,prefix,suffix,expected in [('exponent',keys('2','POW'),keys('2','RIGHT','8','ENTER'),8),('denominator',keys('6','FRAC'),keys('2','RIGHT','8','ENTER'),2),('mantissa',keys('2'),keys('2','RIGHT','8','RIGHT','ADD','1','ENTER'),7)]:
    run('nested-'+name,prefix+pick(next(e for e in rows if e['id']==120 and not e['variant']))+suffix+f'assert_calc_input near {expected} 1e-10\n')
body=''
for i in range(50):
    body+=keys('AC','2','ADD','3','ENTER','TOOLBOX')+'sdl_text root\n'+keys('DOWN','FORMAT','DOWN','DOWN','DOWN','ENTER','BACK')
    body+=keys('ENTER','3','RIGHT','8','ENTER') if i%2 else keys('BACK','BACK')
    body+='assert_calc_input structure\n'
run('cycles-50',body+keys('HOME')+'wait 30\nassert_app Menu\n')
fs=a.out/'roundtrip-data'
run('persist-write',search('Indexed root')+keys('FORMAT','ENTER','BACK','BACK'),fs)
run('persist-read',tab(1)+'assert_calc_input toolbox selected 111 0\n'+shot('persisted')+keys('ENTER','3','RIGHT','8','ENTER')+'assert_calc_input near 2 1e-10\n',fs)
run('rapid-close-repeat',keys('TOOLBOX')+'sdl_text Logarithm\n'+keys('DOWN')+'keydown ENTER\n'+'keyrepeat ENTER\n'*5+'keyup ENTER\n'+keys('2','RIGHT','8','ENTER')+'assert_calc_input near 3 0\n'+keys('TOOLBOX','BACK','ADD','1','ENTER')+'assert_calc_status ok\n')
# On the keypad SHIFT+TOOLBOX is the (unimplemented) catalog, so a Toolbox
# opened over a latched SHIFT is only reachable on the logical path.
run('modifier-restore',keys('L:SHIFT','L:TOOLBOX')+'assert_modifier none\n'+keys('BACK')+'assert_modifier shift\n'+keys('SHIFT','SHIFT','ALPHA','TOOLBOX')+'assert_modifier none\n'+keys('BACK')+'assert_modifier alpha\n'+keys('TOOLBOX','HOME')+'wait 30\nassert_app Menu\nassert_modifier none\n')
run('history-full',keys(*(['AC','2','ADD','3','ENTER']*60))+search('Absolute value')+shot('history-full-toolbox')+keys('BACK','BACK')+'assert_calc_input near 5 0\n'+keys('UP')+'assert_calc_input structure\n')
body=tab(3)+'calc_physical 3 0\ncalc_physical 3 0\n'
mapping=json.loads((ROOT/'hardware/keyboard/neocalculator-v1-final-5x10-revision-c-canonical.json').read_text(encoding='utf-8'))
# The generated Text plane is the production ALPHA surface; derive letters
# from the same audited matrix, without synthesizing editor actions.
header=(ROOT/'src/input/generated/ProductionKeypadMap.generated.h').read_text(encoding='utf-8')
positions=re.findall(r'\{(\d+), (\d+), \d+, \d+, \d+, \d+, \d+, -?\d+, SemanticId::\w+, KeyCode::(\w+),',header)
textrows=header.split('kTextPlaneDefinitions = {{')[1].split('}};')[0].splitlines()[1:]
letterkeys={}
for (r,c,_),line in zip(positions,textrows):
    entries=re.findall(r'\{SemanticId::\w+, KeyCode::\w+, "((?:\\.|[^"\\])*)", (?:true|false)\}',line)
    if len(entries)==4 and len(entries[2])==1:letterkeys[entries[2]]=(r,c)
for letter in 'raiz':
    r,c=letterkeys[letter];body+=f'calc_physical {r} {c}\n'
body+=keys('DOWN')+'assert_calc_input toolbox selected 111 0\n'+shot('physical-search')+keys('BACK','BACK','2','ENTER')+'assert_calc_input near 2 0\n'
run('physical-alpha-search',body)
run('spanish-ui',keys('HOME')+'wait 30\nopen_app Settings\nwait 30\n'+keys('DOWN','DOWN','DOWN','DOWN','ENTER','ENTER','HOME')+'wait 30\nopen_app Calculation\nwait 30\n'+search('raíz')+shot('spanish-search')+keys('FORMAT','DOWN','DOWN','DOWN','ENTER')+shot('spanish-help'))
body=''
for i,e in enumerate([e for e in rows if not e.get('keyboardShortcut')][:25]):
    body+=search(e['en'])+keys('FORMAT','ENTER')
    if i==24:body+=shot('favorites-limit-warning')
    body+=keys('BACK','BACK')
body+=tab(1)+shot('favorites-limit')+'assert_calc_input toolbox selected 103 0\nassert_calc_input toolbox dump\n'
run('favorite-limit',body)
def openapp(app):return keys('HOME')+'wait 30\nopen_app '+app+'\nwait 40\n'
run('equations-integration',openapp('Equations')+keys('ENTER','ENTER')+search('Logarithm')+keys('ENTER','2','RIGHT','8','RIGHT','ADD','x','=','4','ENTER','DOWN','DOWN','ENTER')+'wait 20\nassert_equations_status ok\nassert_equations_solution_near x 0 1 1e-10\n'+keys('TOOLBOX')+'assert_equations state steps\n'+shot('equations-steps'))
run('calculus-integration',openapp('Calculus')+search('Logarithm')+keys('ENTER','2','RIGHT','8','RIGHT','ADD','x','ENTER')+'assert_calculus_status ok\nassert_calculus_result_exact 1\n')
run('grapher-integration',openapp('Grapher')+keys('DOWN','ENTER')+search('Absolute value')+keys('ENTER')+shot('unsupported-context')+keys('BACK','BACK')+search('Logarithm')+keys('ENTER','2','RIGHT','8','ENTER')+'wait 20\nassert_graph_compile_status 0 ok\nassert_graph_eval_near 0 2 3 0\n')
for category in range(6):
    run('visual-category-'+str(category),keys('TOOLBOX',*(['DOWN']*(3+category)),'ENTER')+shot('category-'+str(category))+keys(*(['DOWN']*12))+shot('category-'+str(category)+'-end'))
run('visual-tall-integral',search('Definite integral')+shot('tall-integral'))
run('visual-recents',pick(next(e for e in rows if e['id']==111))+keys('3','RIGHT','8','ENTER')+tab(2)+shot('recents')+'assert_calc_input toolbox selected 111 0\n')
run('visual-continuation',keys('2')+pick(next(e for e in rows if e['id']==120 and not e['variant']))+keys('2','RIGHT','8','RIGHT','ADD','1','ENTER')+'assert_calc_input near 7 0\n'+shot('continuation'))
body=search('Absolute value')+keys('FORMAT','ENTER','BACK','BACK')+search('Indexed root')+keys('FORMAT','ENTER','BACK','BACK','TOOLBOX','UP','RIGHT','ENTER')
body+='assert_calc_input toolbox selected 103 0\n'+keys('FORMAT','DOWN','DOWN','ENTER','BACK','UP')+'assert_calc_input toolbox selected 111 0\n'+shot('favorites-reordered')
body+=keys('FORMAT','ENTER')+'assert_calc_input toolbox selected 103 0\n'+shot('favorites-after-remove')
run('favorite-reorder-two',body)
run('edit-recall',pick(next(e for e in rows if e['id']==111))+keys('3','RIGHT','8','DEL','2','7','ENTER')+'assert_calc_input near 3 1e-10\n'+keys('UP')+'assert_calc_input structure\n'+shot('recalled-root'))
run('popup-tall-context',keys('1','FRAC','2','FRAC','3','FRAC','4','FRAC','5','TOOLBOX')+'wait 3\nassert_calc_input toolbox geometry\n'+shot('popup-tall-context')+keys('BACK','ENTER')+'assert_calc_input near 1.875 1e-10\n')

# Alphabet navigation uses the same primary/RIGHT action as logarithm variants.
def variables():return keys('TOOLBOX',*(['DOWN']*8),'ENTER')
run('letters-submenus',keys('2','POW','3')+variables()+shot('variables')+'assert_calc_input toolbox geometry\n')
for group,name,first in [(0,'latin',200),(1,'greek',300),(2,'special',180)]:
    body=variables()+keys(*(['DOWN']*group),'ENTER')+shot(name)+'assert_calc_input toolbox geometry\n'+f'assert_calc_input toolbox selected {first} 0\n'
    if group<2:
        body+=keys('RIGHT')+shot(name+'-case')+f'assert_calc_input toolbox selected {first} 1\n'+keys('BACK',*(['DOWN']*(25 if group==0 else 23)))+shot(name+'-end')
    run('letters-'+name,body)
run('letters-delta',variables()+keys('DOWN','ENTER','DOWN','DOWN','DOWN','RIGHT')+shot('delta-case')+'assert_calc_input toolbox selected 303 1\n'+keys('ENTER','POW','2','ENTER')+'assert_calc_status ok\nassert_calc_exact Δ^2\n'+shot('delta-power'))
for ident in [(204,0),(208,0),(208,1),(315,0)]:
    e=next(e for e in rows if (e['id'],e['variant'])==ident)
    run(f'letters-reserved-{ident[0]}-{ident[1]}',pick(e)+keys('ENTER')+'assert_calc_exact '+e['expected']+'\n'+keys('ADD','1','ENTER')+'assert_calc_status ok\n'+shot(f'reserved-{ident[0]}-{ident[1]}')+keys('UP')+'assert_calc_input structure\n'+keys('ENTER')+'assert_calc_status ok\n')
run('letters-tabs-focus',keys('TOOLBOX','UP','RIGHT')+'assert_calc_input toolbox geometry\n'+shot('tabs-focus'))
run('letters-search-greek',search('α')+'assert_calc_input toolbox selected 300 0\n'+shot('greek-query'))
run('letters-search-variant',search('ϑ')+'assert_calc_input toolbox selected 400 0\n'+shot('greek-variant-query'))
run('letters-search-delta',search('Δ')+'assert_calc_input toolbox selected 303 1\n'+shot('greek-delta-query'))
run('letters-nested-greek',keys('6','FRAC')+pick(next(e for e in rows if e['id']==301 and e['variant']==0))+keys('POW','2')+'assert_calc_input complete\nassert_calc_input structure\n'+shot('greek-fraction-power')+keys('ENTER')+'assert_calc_status ok\n')
# Infinity stays one discoverable row; RIGHT exposes explicit sign atoms.
def infinity():return next(e for e in rows if e['id']==410 and e['variant']==0)
run('infinity-special-menu',variables()+keys('DOWN','DOWN','ENTER',*(['DOWN']*8))+'assert_calc_input toolbox selected 410 0\n'+shot('infinity-special')+keys('RIGHT')+'assert_calc_input toolbox selected 410 1\nassert_calc_input toolbox geometry\n'+shot('infinity-signs'))
for variant,expected in [(1,'+infinity'),(2,'-infinity'),(3,'[-infinity, +infinity]')]:
    body=search('Infinity')+'assert_calc_input toolbox selected 410 0\n'+keys('RIGHT',*(['DOWN']*(variant-1)))+f'assert_calc_input toolbox selected 410 {variant}\n'
    body+=shot(f'infinity-option-{variant}')+keys('ENTER')+'assert_calc_input structure\n'+shot(f'infinity-input-{variant}')+keys('ENTER')+'assert_calc_status ok\nassert_calc_exact '+expected+'\n'+shot(f'infinity-result-{variant}')
    body+=keys('UP')+'assert_calc_input structure\n'+keys('ENTER')+'assert_calc_status ok\nassert_calc_exact '+expected+'\n'
    run('infinity-sign-'+str(variant),body)
for name,prefix,expected in [('bare','','+infinity'),('plus',keys('ADD'),'+infinity'),('minus',keys('NEGATE'),'-infinity')]:
    run('infinity-'+name,prefix+pick(infinity())+shot('infinity-'+name)+keys('ENTER')+'assert_calc_status ok\nassert_calc_exact '+expected+'\n')
negative=next(e for e in rows if e['id']==410 and e['variant']==2)
run('infinity-negative-product',keys('2')+pick(negative)+keys('ENTER')+'assert_calc_status ok\nassert_calc_exact -infinity\n')
run('infinity-negative-power',pick(negative)+keys('POW','2')+'assert_calc_input structure\n'+shot('infinity-negative-power')+keys('ENTER')+'assert_calc_status ok\nassert_calc_exact +infinity\n')
run('infinity-pair-help',search('Both infinities')+keys('FORMAT','DOWN','DOWN','DOWN','ENTER')+shot('infinity-help'))
for alias,variant in [('∞',0),('+∞',1),('−∞',2),('±∞',3)]:
    run('infinity-alias-'+str(variant),search(alias)+f'assert_calc_input toolbox selected 410 {variant}\n'+shot('infinity-alias-'+str(variant)))
fs=a.out/'infinity-favorite-data'
run('infinity-favorite-write',search('Negative infinity')+keys('FORMAT','ENTER','BACK','BACK'),fs)
run('infinity-favorite-read',tab(1)+'assert_calc_input toolbox selected 410 2\n'+keys('ENTER','ENTER')+'assert_calc_status ok\nassert_calc_exact -infinity\n',fs)
raise SystemExit(0 if all(r['passed'] for r in results.values()) else 1)
