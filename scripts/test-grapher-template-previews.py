#!/usr/bin/env python3
"""VPAM template candidates, insertion oracles, cancellation and lifecycle soak."""
import argparse, json, math, re, subprocess
from pathlib import Path

OPEN = 'open_app Grapher\nwait 30\nkey DOWN\nkey ENTER\nkey RIGHT\nwait 30\n'

def run(binary, out, name, script, frames=1000):
    path=out/(name+'.numos');path.write_text(script)
    result=subprocess.run([str(binary),'--headless','--deterministic','--quiet',
        '--frames',str(frames),'--script',str(path),'--screenshot',str(out/(name+'.ppm'))],
        stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=180)
    (out/(name+'.log')).write_bytes(result.stdout)
    assert result.returncode==0,(name,result.returncode)
    assert (out/(name+'.ppm')).read_bytes().startswith(b'P6\n320 240\n255\n')
    print('PASS',name,flush=True)
    return result.stdout.decode(errors='replace')

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--bin',type=Path,default=Path('.pio/build/emulator_pc/program'))
    ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--cycles',type=int,default=50)
    args=ap.parse_args();args.out.mkdir(parents=True,exist_ok=True)
    binary=args.bin.resolve()
    expected=[7,0,math.sin(2),math.cos(2),8,0.5]
    for i,value in enumerate(expected):
        script='wait 200\nset_angle_mode rad\n'+OPEN+'key DOWN\n'*i+'wait 5\nassert_graph_templates '+str(i)+'\n'
        run(binary,args.out,'template-'+str(i),script)
        run(binary,args.out,'insert-'+str(i),script+'key EXE\nwait 10\nassert_graph_templates closed\nassert_graph_compile_status 0 ok\nassert_graph_eval_near 0 2 '+str(value)+' 1e-6\n')
    cancel='wait 200\n'+OPEN+'key BACK\nassert_graph_templates closed\nassert_app Grapher\nkey RIGHT\nkey AC\nassert_graph_templates closed\nkey RIGHT\nkey HOME\nwait 30\nassert_app Menu\nassert_graph_templates closed\n'
    run(binary,args.out,'cancel-and-home',cancel)
    cycle=OPEN+'assert_graph_templates 0\n'+'key DOWN\n'*5+'assert_graph_templates 5\ncalculus_probe\nkey EXE\nwait 5\nassert_graph_templates closed\nassert_graph_eval_near 0 2 0.5 1e-9\nkey HOME\nwait 30\nassert_app Menu\nassert_graph_templates closed\ncalculus_probe\n'
    log=run(binary,args.out,'lifecycle','wait 200\n'+cycle*args.cycles,250+130*args.cycles)
    records=[dict(p.split('=',1) for p in line.split('|')[1:]) for line in log.splitlines() if line.startswith('CALCULUS_PROBE|')]
    summary={}
    for app in ['Grapher','Menu']:
        rows=[r for r in records if r['app']==app]
        assert len(rows)==args.cycles
        steady=rows[min(10,len(rows)-1):]
        for field in ['objects','timers','handles','pool_total']:
            assert len({r[field] for r in steady})==1,(app,field)
        assert all(r['handles']=='0' for r in rows),(app,'retained handles')
        heaps=[int(r['heap']) for r in steady];free=[int(r['pool_free']) for r in steady]
        assert max(heaps)-min(heaps)<=8192,(app,'heap',heaps)
        assert max(free)-min(free)<=(0 if app=='Menu' else 128),(app,'pool',free)
        assert free[-1]>=free[0]-32,(app,'pool drift')
        summary[app]=dict(cycles=len(rows),objects=steady[-1]['objects'],timers=steady[-1]['timers'],heap_min=min(heaps),heap_max=max(heaps),pool_free_min=min(free),pool_free_max=max(free))
    (args.out/'lifecycle-summary.json').write_text(json.dumps(summary,indent=2))
    print(json.dumps(summary,indent=2))

if __name__=='__main__':main()
