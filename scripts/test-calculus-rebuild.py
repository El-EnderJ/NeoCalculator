#!/usr/bin/env python3
"""CALCULUS-APP-REBUILD-01: live keypad, oracle, visual and 50-cycle QA.
Writes review candidates only. Never promotes or edits accepted goldens.
Usage: python3 scripts/test-calculus-rebuild.py --bin .pio/build/emulator_pc/program
"""
import argparse
import json
import os
from pathlib import Path
import subprocess

CASES = {
    'derivative-empty': ([], None),
    'derivative-x2-input': (['var', 'square'], None),
    'derivative-result': (['var', 'square', 'EXE'], '2*x'),
    'derivative-tall-input': (['sqrt', 'x', '^', '2', 'RIGHT', '+', '1', 'RIGHT', '/', '(', 'x', '+', '1', ')'], None),
    'derivative-tall-result': (['sqrt', 'x', '^', '2', 'RIGHT', '+', '1', 'RIGHT', '/', '(', 'x', '+', '1', ')', 'EXE'], '(x-1)/((x+1)^2*sqrt(x^2+1))'),
    'derivative-syntax-error': (['x', '^', 'EXE'], None),
    'mode-focus': (['UP'], None),
    'editor-focus': (['UP', 'RIGHT', 'DOWN'], None),
    'integral-empty': (['GRAPH'], None),
    'integral-x2-result': (['GRAPH', 'x', '^', '2', 'EXE'], 'x^3/3'),
    'integral-sqrt-result': (['GRAPH', 'sqrt', 'x', 'RIGHT', 'EXE'], '2*x*sqrt(x)/3'),
    'integral-tall-result': (['GRAPH', 'sqrt', 'x', '^', '2', 'RIGHT', '+', '1', 'RIGHT', 'EXE'], 'x*sqrt(x^2+1)/2-ln(sqrt(x^2+1)-x)/2'),
    'integral-unevaluated': (['GRAPH', 'sin', 'sin', 'x', 'RIGHT', 'RIGHT', 'EXE'], None),
    'integral-domain-error': (['GRAPH', '0', 'physical_frac', '0', 'EXE'], None),
    'long-input': (['x', '+']*28+['x'], None),
    'verified-steps': (['x', 'square', 'EXE', 'tools'], None),
}

def keys(sequence):
    return ''.join('key '+key+'\n' for key in sequence)

LOGIC = '''wait 200
open_app Calculus
wait 30
assert_calculus_operation differentiate
assert_calculus_state editing
assert_calculus_focus editor
key UP
assert_calculus_focus mode
key RIGHT
assert_calculus_operation integrate_indefinite
key LEFT
assert_calculus_operation differentiate
key DOWN
assert_calculus_focus editor
key var
key square
key EXE
assert_calculus_state result
assert_calculus_equivalent 2*x
key tools
assert_calculus_state steps
key EXE
assert_calculus_state result
key BACK
assert_calculus_focus editor
key DEL
key ^
key EXE
assert_calculus_status parse_error
assert_calculus_result_kind none
key AC
key sin
key x
key )
key EXE
assert_calculus_equivalent cos(x)
key GRAPH
assert_calculus_state editing
assert_calculus_operation integrate_indefinite
assert_calculus_result_kind none
key EXE
assert_calculus_equivalent -cos(x)
key AC
key sqrt
key x
key )
key EXE
assert_calculus_equivalent 2*x*sqrt(x)/3
key EXE
key UP
assert_calculus_focus mode
keydown RIGHT
keyrepeat RIGHT
keyrepeat RIGHT
keyrepeat GRAPH
wait 80
keyup RIGHT
assert_calculus_operation integrate_indefinite
key DOWN
assert_calculus_focus editor
key HOME
wait 30
assert_app Menu
assert_calculus_closed
assert_modifier none
open_app Calculus
wait 30
assert_calculus_operation differentiate
assert_calculus_focus editor
key UP
key BACK
assert_calculus_focus editor
key BACK
wait 30
assert_app Menu
assert_calculus_closed
open_app Calculus
wait 30
key SHIFT
assert_modifier shift
calculus_semantic asin
assert_modifier none
key x
key )
key EXE
assert_calculus_equivalent 1/sqrt(1-x^2)
key AC
key ALPHA
assert_modifier alpha
calculus_semantic alpha_A
assert_modifier none
key *
key var
key square
key EXE
assert_calculus_equivalent 2*A*x
key AC
calculus_semantic pow_e
key x
key EXE
assert_calculus_equivalent exp(x)
key HOME
wait 30
assert_modifier none
assert_calculus_closed
'''

# F7 corpus through real VPAM keys, compared by Giac simplification.
F7 = [
    (['x'], 'x^2/2'),
    (['x', '^', '2'], 'x^3/3'),
    (['sin','x',')'], '-cos(x)'),
    (['sqrt','x',')'], '2*x*sqrt(x)/3'),
    (['x','*','sqrt','x',')'], '2*x^2*sqrt(x)/5'),
    (['sqrt','x','+','1',')'], '2*(x+1)*sqrt(x+1)/3'),
    (['x','^','2','RIGHT','*','sqrt','x',')'], '2*x^3*sqrt(x)/7'),
    (['sqrt','2','*','x','+','3',')'], '(2*x+3)*sqrt(2*x+3)/3'),
    (['sqrt','x','^','2','RIGHT','+','1',')'], 'x*sqrt(x^2+1)/2-ln(sqrt(x^2+1)-x)/2'),
]

def run(binary, out, name, script, frames, screenshot=False):
    path = out/(name+'.numos')
    path.write_text(script)
    cmd = [str(binary), '--headless', '--deterministic', '--quiet', '--frames', str(frames), '--script', str(path)]
    if screenshot:
        cmd += ['--screenshot', str(out/(name+'.ppm'))]
    result = subprocess.run(cmd, env=dict(os.environ, NUMOS_CALCULUS_LAYOUT='1', NUMOS_CALCULUS_METRICS='1'),
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
    (out/(name+'.log')).write_bytes(result.stdout)
    if result.returncode:
        raise RuntimeError(f'{name}: emulator exit {result.returncode}; see {out/(name+".log")}')
    if screenshot:
        data=(out/(name+'.ppm')).read_bytes()
        assert data.startswith(b'P6\n320 240\n255\n') and len(data)==230415, name
    print('PASS', name, flush=True)
    return result.stdout.decode(errors='replace')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bin', type=Path, default=Path('.pio/build/emulator_pc/program'))
    parser.add_argument('--out', type=Path, default=Path('out/calculus-rebuild/final'))
    parser.add_argument('--cycles', type=int, default=50)
    args=parser.parse_args(); args.out.mkdir(parents=True,exist_ok=True)
    binary=args.bin.resolve()
    # Physical acceptance found that the resolver consumes modifier presses
    # before apps see them. Assert the visible UI, not just keyboard state.
    modifier_steps = [('shift','S'), ('shift','S-LOCK'), ('alpha','S+A'),
        ('alpha','S+A-LOCK'), ('shift','A-LOCK'), ('alpha','none'),
        ('alpha','A'), ('alpha','A-LOCK'), ('alpha','none'),
        ('shift','S'), ('alpha','S+A'), ('alpha','S+A'),
        ('shift','S+A-LOCK'), ('shift','A-LOCK'), ('alpha','none')]
    for app in ['Menu','Calculation','Calculus','Grapher']:
        script='wait 200\n'
        if app != 'Menu': script+='open_app '+app+'\nwait 30\n'
        script+='assert_modifier_badge none\n'
        initial=args.out/('modifier-'+app+'-initial.ppm')
        script+='screenshot '+str(initial)+'\n'
        for index,(key,badge) in enumerate(modifier_steps):
            script+='production_modifier '+key+'\nwait 1\nassert_modifier_badge '+badge+'\n'
            script+='screenshot '+str(args.out/('modifier-'+app+'-'+str(index)+'.ppm'))+'\n'
        script+='key HOME\nwait 30\nassert_app Menu\nassert_modifier_badge none\n'
        run(binary,args.out,'modifier-'+app,script,450)
        def title_pixels(path):
            data=path.read_bytes()
            header=b'P6\n320 240\n255\n'
            assert data.startswith(header)
            pixels=data[len(header):]
            title_end=228 if app=='Menu' else 205
            spans=[(90,title_end), (245,286), (290,320)]
            if app != 'Menu': spans.append((212,250))
            return b''.join(pixels[(y*320+x1)*3:(y*320+x2)*3]
                            for y in range(24) for x1,x2 in spans)
        original=title_pixels(initial)
        for index in range(len(modifier_steps)):
            assert title_pixels(args.out/('modifier-'+app+'-'+str(index)+'.ppm')) == original, (app,'title/angle/battery changed during modifier change',index)
    for name,(sequence,expected) in CASES.items():
        assertions='assert_calculus_layout\n'
        if expected:
            assertions+='assert_calculus_status ok\nassert_calculus_result_kind structured\nassert_calculus_equivalent '+expected+'\n'
        if name=='derivative-syntax-error': assertions+='assert_calculus_status parse_error\nassert_calculus_result_kind none\n'
        if name=='integral-domain-error': assertions+='assert_calculus_status undefined\nassert_calculus_result_kind none\n'
        if name=='integral-unevaluated': assertions+='assert_calculus_result_exact integrate(sin(sin(x)),x)\nassert_calculus_tutor_status unavailable\n'
        script='wait 200\nopen_app Calculus\nwait 60\n'+keys(sequence)+'wait 2\n'+assertions+'wait 1\n'
        run(binary,args.out,name,script,269+len(sequence)+assertions.count('\n'),True)
    run(binary,args.out,'keyboard',LOGIC,800)
    script='wait 200\nopen_app Calculus\nwait 30\nkey GRAPH\n'
    for sequence,expected in F7:
        script+='key AC\n'+keys(sequence)+'key EXE\nassert_calculus_status ok\nassert_calculus_result_kind structured\nassert_calculus_equivalent '+expected+'\nassert_calculus_layout\n'
    run(binary,args.out,'f7-keypad',script,600)
    cycle='''open_app Calculus
wait 30
assert_calculus_operation differentiate
key var
key square
key EXE
assert_calculus_equivalent 2*x
key tools
key EXE
key GRAPH
key AC
key sqrt
key x
key )
key EXE
assert_calculus_equivalent 2*x*sqrt(x)/3
assert_calculus_layout
calculus_probe
key HOME
wait 30
assert_app Menu
assert_calculus_closed
calculus_probe
'''
    log=run(binary,args.out,'lifecycle','wait 200\n'+cycle*args.cycles,250+100*args.cycles)
    records=[]
    for line in log.splitlines():
        if line.startswith('CALCULUS_PROBE|'):
            records.append(dict(field.split('=',1) for field in line.split('|')[1:]))
    summary={}
    for app in ['Menu','Calculus']:
        rows=[r for r in records if r['app']==app]
        assert len(rows)==args.cycles, (app,len(rows))
        steady=rows[10:]
        for field in ['objects','timers','handles','pool_total']:
            assert len({r[field] for r in steady})==1,(app,field)
        assert all(r['handles']=='0' for r in rows)
        free=[int(r['pool_free']) for r in steady]
        # TLSF block alignment/draw buffers oscillate by a few bytes in the
        # active view. Teardown must return to an identical free-pool state.
        assert max(free)-min(free) <= (0 if app=='Menu' else 128),(app,'pool variation',free)
        assert free[-1] >= free[0]-32,(app,'pool drift',free)
        heap=[int(r['heap']) for r in steady]
        assert max(heap)-min(heap) <= 8192,(app,'heap growth',min(heap),max(heap))
        summary[app]={'cycles':len(rows),'objects':steady[-1]['objects'],'timers':steady[-1]['timers'],
                      'heap_min':min(heap),'heap_max':max(heap),'pool_total':steady[-1]['pool_total'],'pool_free_min':min(free),'pool_free_max':max(free)}
    (args.out/'lifecycle-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary,indent=2))

if __name__=='__main__': main()
