#!/usr/bin/env python3
"""EQUATIONS-APP-REBUILD-01 live app/physical matrix regressions and review images.
Run from any directory. Creates its own output folders; never promotes goldens.
"""
import argparse, importlib.util, json, os, re, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('candidates',ROOT/'scripts/generate-emulator-candidates.py')
helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
OPEN='wait 200\nset_equations_complex_policy real\nopen_app Equations\nwait 40\n'
def keys(sequence):return ''.join('key '+k+'\n' for k in sequence.split())
def add(expr):return keys('ENTER ENTER '+expr+' ENTER')
def single(expr):return OPEN+add(expr)+keys('DOWN DOWN ENTER')+'wait 20\n'
def system(exprs):
    s=OPEN
    for i,e in enumerate(exprs):s+=(keys('DOWN') if i else '')+add(e)
    return s+keys('DOWN ENTER' if len(exprs)==3 else 'DOWN DOWN ENTER')+'wait 20\n'
MATH={
 'linear':('2 * x + 4 = 0','ok',[('x',0,'-2')]),
 'radical-roots':('x ^ 2 RIGHT - 2 = 0','ok',[('x',0,'-sqrt(2)'),('x',1,'sqrt(2)')]),
 'repeated':('( x - 1 ) ^ 2 RIGHT = 0','ok',[('x',0,'1')]),
 'cubic':('x ^ 3 RIGHT - 6 * x ^ 2 RIGHT + 1 1 * x - 6 = 0','ok',[('x',0,'1'),('x',1,'2'),('x',2,'3')]),
 'logarithm':('ln x ) = 1','ok',[('x',0,'exp(1)')]),
 'pi':('x + pi = 0','ok',[('x',0,'-pi')]),
 'sqrt':('x + sqrt 2 ) = 0','ok',[('x',0,'-sqrt(2)')]),
 'no-real':('x ^ 2 RIGHT + 1 = 0','no_solution',[]),
 'identity':('x + 1 = x + 1','all_values',[]),
 'contradiction':('x + 1 = x + 2','no_solution',[]),
 'reciprocal':('1 / ( x - 1 ) RIGHT = 0','no_solution',[]),
 'excluded-root':('( x ^ 2 RIGHT - 1 ) / ( x - 1 ) RIGHT = 0','ok',[('x',0,'-1')]),
 'conditional-identity':('x / x RIGHT = 1','all_values',[]),
 'missing-lhs':('= 1','parse_error',[]),
 'missing-rhs':('x =','parse_error',[]),
 'multiple-equals':('x = 1 = 2','parse_error',[]),
 'incomplete-fraction':('1 / RIGHT = 1','parse_error',[]),
 'incomplete-root':('sqrt RIGHT = 1','parse_error',[]),
 'incomplete-power':('x ^ RIGHT = 1','parse_error',[]),
 'unsupported-variable':('ALPHA sin = 0','parse_error',[]),
}
SYSTEMS={
 'system-2x2':(['x + y = 3','x - y = 1'],'ok',[('x',0,'2'),('y',0,'1')]),
 'system-3x3':(['x + y + ALPHA y = 6','x - y + ALPHA y = 2','x + y - ALPHA y = 0'],'ok',[('x',0,'1'),('y',0,'2'),('z',0,'3')]),
 'inconsistent':(['x + y = 2','2 * x + 2 * y = 5'],'no_solution',[]),
 'dependent':(['x + y = 2','2 * x + 2 * y = 4'],'all_values',[]),
 'conditional-family':(['x / x RIGHT = 1','y = y'],'all_values',[]),
 'rational-reordered':(['x - y = 0','x + y = 1'],'ok',[('x',0,'1/2'),('y',0,'1/2')]),
 'nonlinear':(['x ^ 2 RIGHT = 1','y = x'],'unsupported',[]),
}
def oracle(status,values):
    s='assert_equations_status '+status+'\n'
    if status=='ok':s+='assert_equations_solution_count '+str(len({i for _,i,_ in values}))+'\n'
    for var,index,value in values:s+=f'assert_equations equivalent {var} {index} {value}\n'
    for index in sorted({i for _,i,_ in values}):s+=f'assert_equations substitution {index}\n'
    return s

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--bin');ap.add_argument('--out',type=Path,default=Path('out/equations-rebuild/final'));ap.add_argument('--cycles',type=int,default=50)
    args=ap.parse_args();os.chdir(ROOT);out=args.out;out.mkdir(parents=True,exist_ok=True)
    binary=helper.resolve_binary(args.bin);env=dict(os.environ,NUMOS_EQUATIONS_BOUNDS='1');dll=helper.sdl2_dll_dir(binary)
    if dll:env['PATH']=dll+os.pathsep+env.get('PATH','')
    records={}
    def run(name,script,negative=False,frames=1800,layout=True):
        # Capture the reached state before idle cursor blinking changes it.
        shot=os.path.relpath(out/(name+'.ppm'),ROOT).replace(os.sep,'/')
        if layout and not negative:script+='wait 4\nassert_equations layout\n'
        if not negative:script+='screenshot '+shot+'\n'
        script+='log EQUATIONS_REBUILD_DONE\n'
        # One command per deterministic frame. Require completion as well as
        # exit status so a short frame budget cannot produce a false green.
        if frames==1800:
            frames=sum(int(line.split()[1]) if line.startswith('wait ') else 1 for line in script.splitlines())+100
        path=out/(name+'.numos');path.write_text(script,encoding='utf-8')
        # Narrow CRT paths must be relative on Windows with non-ASCII home names.
        rel=lambda p:os.path.relpath(p,ROOT)
        r=subprocess.run([binary,'--headless','--deterministic','--quiet','--frames',str(frames),'--script',rel(path)],env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
        (out/(name+'.log')).write_bytes(r.stdout);ok=(r.returncode==4 if negative else r.returncode==0 and b'EQUATIONS_REBUILD_DONE' in r.stdout)
        required_text={'conditional-family':'text=Conditional family',
                       'conditional-identity':'text=Conditional identity',
                       'division-result':'text=1 solution'}.get(name)
        if required_text:ok=ok and required_text.encode() in r.stdout
        records[name]={'pass':ok,'exit':r.returncode};print('PASS' if ok else 'FAIL',name,flush=True)
        (out/'results.json').write_text(json.dumps(records,indent=2))
        return r.stdout.decode(errors='replace')
    for name,(expr,status,values) in MATH.items():run(name,single(expr)+oracle(status,values))
    for name,(exprs,status,values) in SYSTEMS.items():run(name,system(exprs)+oracle(status,values))
    run('empty',OPEN+'assert_equations count 0\n')
    run('one',OPEN+add('x = 1'))
    run('two',OPEN+add('x = 1')+keys('DOWN')+add('y = 2'))
    run('three',system(SYSTEMS['system-3x3'][0])+keys('BACK'))
    run('three-solve-focus',system(SYSTEMS['system-3x3'][0])+keys('BACK')+'assert_equations focus 3\n')
    run('system-3x3-scroll',system(SYSTEMS['system-3x3'][0])+keys('DOWN DOWN DOWN')+'assert_equations scroll positive\n')
    run('complex',single('x ^ 2 RIGHT + 1 = 0').replace('policy real','policy complex')+'assert_equations_status ok\nassert_equations_solution_count 2\n')
    run('numerical-text',single('x ^ 5 RIGHT - x + 1 = 0').replace('policy real','policy complex')+'assert_equations_result_kind text_fallback\n')
    run('five-exact-groups',single('x ^ 5 RIGHT - 5 * x ^ 3 RIGHT + 4 * x = 0')+'assert_equations_solution_count 5\n'+keys('var var var var')+'assert_equations page 4\nassert_equations_solution_exact x 4 2\n'+keys('var')+'assert_equations page 0\n')
    for i in range(4):
        s=OPEN+keys('ENTER')+keys('DOWN '*i)+f'assert_equations template {i}\n'
        run('template-'+str(i),s)
        run('template-insert-'+str(i),s+keys('ENTER')+f'assert_equations template {i}\nassert_equations count 0\n'+('assert_equations draft '+['empty','A*((x)^(2))+B*x+C=0','A*((exp(1))^(x))+B=0','ln((x))+A=0'][i]+'\n'))
    run('editor-empty',OPEN+keys('ENTER ENTER')+'assert_equations draft empty\n')
    run('editor-structured',OPEN+keys('ENTER ENTER sqrt x RIGHT / 2')+'assert_equations cursor slot\n')
    run('editor-incomplete',OPEN+keys('ENTER ENTER 1 /'))
    # Tall but within the existing renderer depth limit: balanced branches.
    tall='1 / '*5+'2 '+'RIGHT '*5+' / '+'1 / '*5+'2'
    run('editor-tall',OPEN+keys('ENTER ENTER '+tall)+'wait 12\nassert_equations cursor visible\nassert_equations scroll positive\n')
    run('tall-row',OPEN+add(tall+' RIGHT '*6+' = x'))
    run('tall-result',single('x = 1 / 2 RIGHT')+oracle('ok',[('x',0,'1/2')]))
    run('division-result',single('x = 1 / sqrt 2 RIGHT RIGHT')+oracle('ok',[('x',0,'1/sqrt(2)')])+'assert_equations_result_kind structured\n')
    run('wide-result',single('x = '+' '.join('1234567890123456789012345'*2))+keys('RIGHT '*20)+'assert_equations state result\nassert_equations_status ok\n')
    run('result-scroll',single('x ^ 5 RIGHT - x + 1 = 0').replace('policy real','policy complex')+keys('DOWN '*15)+'assert_equations scroll positive\n')
    run('cancel-new',OPEN+keys('ENTER ENTER x = 1 BACK')+'assert_equations count 0\nassert_equations focus 0\n')
    run('cancel-template',OPEN+keys('ENTER BACK')+'assert_equations count 0\nassert_equations focus 0\n')
    run('empty-confirm',OPEN+keys('ENTER ENTER ENTER')+'assert_equations count 0\n')
    run('empty-created-row',OPEN+keys('ENTER ENTER x AC ENTER DOWN DOWN ENTER')+'assert_equations count 1\nassert_equations_status parse_error\n')
    run('empty-committed-row',OPEN+add('x = 1')+keys('ENTER AC ENTER DOWN DOWN ENTER')+'assert_equations count 1\nassert_equations_status parse_error\n')
    run('incomplete-system',system(['x + y = 3','x ='])+'assert_equations count 2\nassert_equations_status parse_error\n'+keys('BACK')+'assert_equations focus 1\n')
    run('edit-contract',OPEN+add('x = 1')+keys('ENTER AC x = 2 BACK')+'assert_equations equation 0 x=1\n'+keys('ENTER AC x = 2 ENTER')+'assert_equations equation 0 x=2\nassert_equations epochs invalid\n')
    run('error-recovery',single('x =')+'assert_equations_status parse_error\n'+keys('BACK ENTER AC x = 1 ENTER DOWN DOWN ENTER')+oracle('ok',[('x',0,'1')]))
    run('invalidation',single('x = 1')+keys('tools')+'assert_equations epochs steps\n'+keys('BACK BACK UP UP ENTER AC x = 2 ENTER')+'assert_equations epochs invalid\n'+keys('tools')+'assert_equations state list\n')
    for index in range(3):
        s=system(SYSTEMS['system-3x3'][0])+keys('BACK UP UP UP')+keys('DOWN '*index)+keys('DEL')+'assert_equations count 2\n'+f'assert_equations focus {index}\n'
        run('delete-'+str(index),s)
    run('steps-available',single('2 * x + 4 = 0')+keys('tools')+'assert_equations_tutor_status complete\nassert_equations epochs steps\n')
    run('steps-unavailable',single('ln x ) = 1')+keys('tools')+'assert_equations state steps\nassert_equations_tutor_status unavailable\n')
    # Physical entry goes through the generated electrical map and real resolver.
    mapping={}
    data=(ROOT/'src/input/generated/ProductionKeypadMap.generated.h').read_text(encoding='utf-8').split('kProductionKeypadMap = {{',1)[1].split('}};',1)[0]
    for line in data.splitlines():
        m=re.search(r'\{(\d+), (\d+),.*KeyCode::(\w+),',line)
        if m:mapping[m[3]]=(m[1],m[2])
    def physical(*codes):
        return ''.join('equations_physical '+' '.join(mapping[c])+'\n' for c in codes)
    p=physical
    run('physical-square-divide',OPEN+p('EXE','EXE','VAR_X','SQUARE','DIVIDE','NUM_2','EQUAL','NUM_1')+'assert_equations draft ((x)^(2))/2=1\n')
    run('physical-constants',OPEN+p('EXE','EXE','CONST_PI','ADD','CONST_E','SUB','NUM_0','DOT','NUM_5')+'assert_equations input pi+exp(1)-0.5\n')
    physical3=OPEN
    for i,signs in enumerate([('ADD','ADD'),('SUB','ADD'),('ADD','SUB')]):
        if i:physical3+=p('DOWN')
        physical3+=p('EXE','EXE','VAR_X',signs[0],'SHIFT','VAR_X',signs[1],'VAR','RIGHT','RIGHT','EXE','EQUAL',['NUM_6','NUM_2','NUM_0'][i],'EXE')
    run('physical-system-3x3',physical3+p('DOWN','EXE')+oracle('ok',[('x',0,'1'),('y',0,'2'),('z',0,'3')]))
    run('physical-shifted-root',OPEN+p('EXE','EXE','SHIFT','POW','NUM_3','RIGHT','VAR_X','RIGHT','EQUAL','NUM_2')+'assert_equations draft surd((x),(3))=2\n')
    run('physical-absolute',OPEN+p('EXE','EXE','SHIFT','SQRT','VAR_X','RPAREN')+'assert_equations input abs((x))\n')
    run('physical-exp',OPEN+p('EXE','EXE','SHIFT','LN','VAR_X','RIGHT')+'assert_equations input ((exp(1))^(x))\n')
    run('physical-pow10',OPEN+p('EXE','EXE','SHIFT','LOG','NUM_2','RIGHT')+'assert_equations input ((10)^(2))\n')
    run('physical-unsupported-variable',OPEN+p('EXE','EXE','ALPHA','LN','EQUAL','NUM_0','EXE','DOWN','DOWN','EXE')+'assert_equations_status parse_error\n')
    run('physical-linear',OPEN+p('EXE','EXE','NUM_2','MUL','VAR_X','ADD','NUM_4','EQUAL','NUM_0')+'assert_equations draft 2*x+4=0\n'+p('EXE','DOWN','DOWN','EXE')+oracle('ok',[('x',0,'-2')]))
    run('physical-variables',OPEN+p('EXE','EXE','VAR','RIGHT','RIGHT','EXE','EQUAL','NUM_3')+'assert_equations draft z=3\n')
    run('physical-fraction',OPEN+p('EXE','EXE','NUM_1','FRAC','NUM_2','UP','NUM_3')+'assert_equations input ((13)/(2))\n'+p('DOWN')+'assert_equations cursor slot\n')
    run('physical-modifiers',OPEN+p('EXE','EXE','SHIFT','UP')+'assert_modifier shift\n'+p('SIN')+'assert_modifier none\n'+p('VAR_X','RPAREN')+'assert_equations input asin((x))\n'+p('AC','SHIFT','SHIFT','SIN')+'assert_modifier_badge S-LOCK\n'+p('SHIFT','AC','ALPHA','ALPHA','SHIFT','LEFT')+'assert_modifier_badge S+A\n'+p('SHIFT','SHIFT','HOME')+'wait 30\nassert_equations closed\nassert_modifier none\n',layout=False)
    repeat='equations_physical '+' '.join(mapping['EXE'])+' repeat\n'
    run('repeat',OPEN+repeat+'assert_equations state list\n'+p('EXE')+repeat+'assert_equations state template\n'+p('EXE')+repeat+'assert_equations state editing\n')
    for phase,tail in {'templates':'ENTER','draft':'ENTER ENTER x','variables':'ENTER ENTER var','results':'ENTER ENTER x = 1 ENTER DOWN DOWN ENTER','steps':'ENTER ENTER x = 1 ENTER DOWN DOWN ENTER tools'}.items():
        run('home-'+phase,OPEN+keys(tail)+p('HOME')+'wait 30\nassert_app Menu\nassert_equations closed\nopen_app Equations\nwait 30\nassert_equations count 0\n')
    # Deliberate failures prove the assertions reach live state and exact roots.
    run('negative-root',single('x = 1')+'assert_equations_solution_exact x 0 99\n',negative=True)
    run('negative-epoch',OPEN+'assert_equations epochs current\n',negative=True)
    run('negative-template',OPEN+keys('ENTER ENTER')+'assert_equations template 1\n',negative=True)
    # Mixed lifecycle includes systems, cancelled modal, edit/cancel, steps, deletion.
    cycle=system(SYSTEMS['system-2x2'][0]).replace(OPEN,'open_app Equations\nwait 30\n')+keys('tools BACK BACK UP UP ENTER AC x = 7 BACK DEL ENTER BACK')+'calculus_probe\n'+keys('HOME')+'wait 30\nassert_equations closed\nassert_app Menu\ncalculus_probe\n'
    log=run('lifecycle','wait 200\n'+cycle*args.cycles,frames=300+180*args.cycles,layout=False)
    probes=[dict(p.split('=',1) for p in line.split('|')[1:]) for line in log.splitlines() if line.startswith('CALCULUS_PROBE|')]
    summary={}
    for app in ['Equations','Menu']:
        rows=[r for r in probes if r['app']==app];steady=rows[min(10,len(rows)-1):]
        stable=bool(steady) and len(rows)==args.cycles and all(len({r[f] for r in steady})==1 for f in ['objects','timers','handles','pool_total','pool_free']) and all(r['handles']=='0' for r in rows)
        summary[app]={'cycles':len(rows),'stable':stable,'last':steady[-1] if steady else {}}
        records['lifecycle-'+app]={'pass':stable}
    (out/'lifecycle-summary.json').write_text(json.dumps(summary,indent=2));(out/'results.json').write_text(json.dumps(records,indent=2))
    # Pillow is optional for tests; captures are always actual 320x240 P6 frames.
    try:
        from PIL import Image,ImageDraw
        names=[n for n,r in records.items() if r['pass'] and not n.startswith(('negative','lifecycle')) and (out/(n+'.ppm')).exists()]
        sheet=Image.new('RGB',(960,264*((len(names)+2)//3)),'#dddddd');draw=ImageDraw.Draw(sheet)
        for i,n in enumerate(names):
            frame=Image.open(out/(n+'.ppm'));assert frame.size==(320,240)
            x=i%3*320;y=i//3*264;draw.text((x+4,y+4),n,fill='black');sheet.paste(frame,(x,y+24))
        sheet.save(out/'contact.png')
    except ImportError:print('Contact sheet NOT RUN: Pillow unavailable')
    raise SystemExit(0 if all(r['pass'] for r in records.values()) else 1)
if __name__=='__main__':main()
