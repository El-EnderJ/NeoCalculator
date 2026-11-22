#!/usr/bin/env python3
"""Private persistent allocation fault overlay and scientific ownership tests.

Only generated overlay sources are instrumented. The normal candidate is never
patched. --build-overlay reuses the existing allocator probe builder and adds
uncertainty scopes, wall-clock samples, and allocation-free source counters.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import statistics
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--bin',type=Path)
parser.add_argument('--out',type=Path,required=True)
parser.add_argument('--build-overlay',action='store_true')
parser.add_argument('--build',type=Path)
parser.add_argument('--database',type=Path)
parser.add_argument('--pool',action='store_true',help='Overlay a matching LVGL fixed-pool build')
parser.add_argument('--build-only',action='store_true')
parser.add_argument('--profile-only',action='store_true')
parser.add_argument('--history-populations',action='store_true')
parser.add_argument('--help-c-allocator',action='store_true',help='Also wrap LVGL C allocators only inside Help refresh')
parser.add_argument('--toolbox-source',type=Path,help='Private candidate Toolbox.cpp for review before integration')
parser.add_argument('--baseline',type=Path,help='Paired whole product journeys against this uninstrumented executable')
parser.add_argument('--only')
opts=parser.parse_args()
opts.out=opts.out.resolve();opts.out.mkdir(parents=True,exist_ok=True)

COUNTER_CPP=r'''
#include "ScientificUncertainty.h"
namespace {
struct SourceCounter { uint32_t id=0;unsigned references=0; } sourceCounters[2048];
size_t payloads=0,contributions=0,uniqueSources=0;
bool sourceOverflow=false;
}
namespace tbprobe {
unsigned long long clockNs() noexcept {
 return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void created(const numos::scientific::Uncertainty& u) noexcept {
 ++payloads;contributions+=u.count;
 for(unsigned i=0;i<u.count;++i){
  SourceCounter* free=nullptr;bool found=false;
  for(auto& entry:sourceCounters){
   if(entry.id==u.contributions[i].source){++entry.references;found=true;break;}
   if(!entry.id && !free)free=&entry;
  }
  if(!found){if(free){*free={u.contributions[i].source,1};++uniqueSources;}else sourceOverflow=true;}
 }
}
void released(const numos::scientific::Uncertainty& u) noexcept {
 --payloads;contributions-=u.count;
 for(unsigned i=0;i<u.count;++i)for(auto& entry:sourceCounters)
  if(entry.id==u.contributions[i].source){if(!--entry.references){entry.id=0;--uniqueSources;}break;}
}
void snapshot(const char* app) noexcept {
 std::printf("SCIENTIFIC_LIVE|app=%s|cpp_bytes=%zu|payloads=%zu|contributions=%zu|unique_sources=%zu|counter_overflow=%u\n",app,live,payloads,contributions,uniqueSources,unsigned(sourceOverflow));
}
}
'''

HELP_ALLOCATOR_CPP=r'''
extern "C" void* __real_lv_malloc(size_t);
extern "C" void* __real_lv_malloc_zeroed(size_t);
extern "C" void* __real_lv_realloc(void*,size_t);
extern "C" void* __real_lv_reallocf(void*,size_t);
static bool failLvgl(const char* function,size_t bytes) {
 if(!active || !cScope)return false;
 ++attempts;
 if(point && attempts>=point){++failures;std::printf("LVGL_HELP_ALLOC function=%s bytes=%zu attempt=%zu failure=%zu\n",function,bytes,attempts,failures);std::fflush(stdout);return true;}
 return false;
}
extern "C" void* __wrap_lv_malloc(size_t n){return failLvgl("malloc",n)?nullptr:__real_lv_malloc(n);}
extern "C" void* __wrap_lv_malloc_zeroed(size_t n){return failLvgl("zeroed",n)?nullptr:__real_lv_malloc_zeroed(n);}
extern "C" void* __wrap_lv_realloc(void* p,size_t n){return failLvgl("realloc",n)?nullptr:__real_lv_realloc(p,n);}
extern "C" void* __wrap_lv_reallocf(void* p,size_t n){return failLvgl("reallocf",n)?nullptr:__real_lv_reallocf(p,n);}
'''

def build_overlay():
    assert opts.build and opts.database,'--build and --database required'
    overlay=opts.out/'overlay';overlay.mkdir(exist_ok=True)
    builder=(ROOT/'scripts/build-toolbox-allocation-probe.py').read_text(encoding='utf8')
    builder=builder.replace('void CalculationApp::openFormatMenu(bool pickDigits,bool pickUnits,bool pickComponents) {',
                            'void CalculationApp::openFormatMenu(bool pickDigits,bool pickUnits,bool pickComponents,bool pickUncertainty) {')
    builder=builder.replace("root=Path(__file__).resolve().parents[1]",f"root=Path({str(ROOT)!r})")
    if opts.toolbox_source:
        builder=builder.replace("(root/'src/ui/Toolbox.cpp').read_text(encoding='utf8')",f"Path({str(opts.toolbox_source.resolve())!r}).read_text(encoding='utf8')")
    if opts.help_c_allocator:
        source_modifier="\n".join([
            "source=source.replace('bool active=false;', 'bool active=false,cScope=false;')",
            "source=source.replace('if(active && ++attempts', 'if(active && !cScope && ++attempts')",
            "source=source.replace('own=active=true;', 'own=active=true;cScope=std::strcmp(name,\"help-c\")==0;')",
            'source += '+repr(HELP_ALLOCATOR_CPP),
        ])
        builder=builder.replace("(out/'probe.h').write_text(header)",source_modifier+"\n(out/'probe.h').write_text(header)")
        builder=builder.replace("injections={", "ui=ui.replace('} else {s->help=true;s->options=false;refresh();}', '} else {tbprobe::Scope helpFault(\"help-c\");s->help=true;s->options=false;refresh();}')\ninjections={")
        builder=builder.replace("'-Wl,--gc-sections'", "'-Wl,--gc-sections','-Wl,--wrap=lv_malloc','-Wl,--wrap=lv_malloc_zeroed','-Wl,--wrap=lv_realloc','-Wl,--wrap=lv_reallocf'")
    builder=builder.replace("'ScientificUncertainty':", "'ScientificUncertainty':")
    anchor="      'Quantity':{"
    extra="""      'ScientificUncertainty':{'E makeSource(SourceId source,double u,Owned& out) {':'source',
       'E publish(const Uncertainty& next,Owned& out) {':'payload',
       'E combine(const Owned& a,double da,const Owned& b,double db,Owned& out) {':'combine'},
      'NativeHal':{'        case ScriptCmdType::CalculusProbe: {':'snapshot'},
"""
    assert builder.count(anchor)==1
    builder=builder.replace(anchor,extra+anchor)
    builder=builder.replace("'CalculationApp':{'void CalculationApp::openOutputSelector", "'CalculationApp':{'void CalculationApp::evaluateExpression() {':'evaluation',\n       'void CalculationApp::openOutputSelector")
    builder=builder.replace("directory='apps' if name=='CalculationApp' else 'math'", "directory='apps' if name=='CalculationApp' else 'hal' if name=='NativeHal' else 'math'")
    builder=builder.replace("['-Isrc/ui','-Isrc/math','-Isrc/apps',", "['-Isrc/ui','-Isrc/math','-Isrc/apps','-Isrc/hal',")
    header_extra='\nnamespace numos::scientific {struct Uncertainty;}\nnamespace tbprobe { unsigned long long clockNs() noexcept; void created(const numos::scientific::Uncertainty&) noexcept; void released(const numos::scientific::Uncertainty&) noexcept; void snapshot(const char*) noexcept;}\n'
    modifications="\n".join([
        "header=header.replace('bool own=false;const char* name;', 'bool own=false;const char* name;unsigned long long beginNs=0;')",
        'header += '+repr(header_extra),
        "source=source.replace('#include <cstring>', '#include <cstring>\\n#include <chrono>')",
        "source=source.replace('Scope::Scope(const char* value):name(value) {', 'Scope::Scope(const char* value):name(value) { beginNs=clockNs();')",
        "source=source.replace('if(call!=target)return;', 'if(target && call!=target)return;')",
        "source=source.replace('delta=%lld\\\\n', 'delta=%lld ns=%llu live=%zu\\\\n')",
        "source=source.replace('(long long)live-(long long)before);', '(long long)live-(long long)before,clockNs()-beginNs,live);')",
        'source += '+repr(COUNTER_CPP),
        "(out/'probe.h').write_text(header)",
    ])
    builder=builder.replace("(out/'probe.h').write_text(header)",modifications)
    instrumentation='''        if name=='ScientificUncertainty':
            marker='E publish(const Uncertainty& next,Owned& out) {'
            wrapped='struct ProbeUncertainty:Uncertainty { explicit ProbeUncertainty(const Uncertainty& u):Uncertainty(u){tbprobe::created(*this);} ~ProbeUncertainty(){tbprobe::released(*this);} };\\n'
            text=text.replace(marker,wrapped+marker).replace('std::make_shared<const Uncertainty>(next)','std::make_shared<const ProbeUncertainty>(next)')
        if name=='NativeHal':
            text=text.replace('        case ScriptCmdType::CalculusProbe: {','        case ScriptCmdType::CalculusProbe: {\\n tbprobe::snapshot(activeAppName());')
'''
    builder=builder.replace("        (out/(name+'.cpp')).write_text",instrumentation+"        (out/(name+'.cpp')).write_text")
    driver=overlay/'build-overlay.py';driver.write_text(builder,encoding='utf8')
    database=opts.database.resolve()
    if opts.pool:
        records=json.loads(database.read_text(encoding='utf8'))
        for record in records:
            record['command']=record['command'].replace('-DLV_USE_STDLIB_MALLOC=LV_STDLIB_CLIB','-DLV_USE_STDLIB_MALLOC=LV_STDLIB_BUILTIN')
        database=overlay/'pool-compile-commands.json'
        database.write_text(json.dumps(records,indent=2),encoding='utf8')
    command=[sys.executable,str(driver),'--database',str(database),'--build',str(opts.build.resolve()),'--out',str(overlay),'--quantities']
    result=subprocess.run(command,cwd=ROOT,capture_output=True)
    (overlay/'build.log').write_bytes(result.stdout+result.stderr)
    if result.returncode:raise SystemExit((result.stdout+result.stderr).decode(errors='replace')[-5000:])
    executable=overlay/'toolbox-probe.exe'
    identity={'base_binary_sha256':hashlib.sha256((opts.build/'program.exe').read_bytes()).hexdigest(),
              'overlay_binary_sha256':hashlib.sha256(executable.read_bytes()).hexdigest(),
              'compile_database_sha256':hashlib.sha256(database.read_bytes()).hexdigest(),
              'generated_sources':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in overlay.iterdir() if p.suffix in ('.cpp','.h')}}
    (overlay/'identity.json').write_text(json.dumps(identity,indent=2)+'\n',encoding='utf8')
    print('Overlay ready:',executable,flush=True)
    return executable

binary=build_overlay() if opts.build_overlay else opts.bin
assert binary and binary.exists(),'provide a compiled --bin or --build-overlay'
if opts.build_only:raise SystemExit(0)
sys.argv=[sys.argv[0],'--bin',str(binary),'--out',str(opts.out/'runs')]+(['--only',opts.only] if opts.only else [])
common=(ROOT/'scripts/test-calculation-quantities.py').read_text(encoding='utf8').split('\nsummation=')[0]
exec(common)
scientific=(ROOT/'scripts/test-scientific-product.py').read_text(encoding='utf8')
exec(scientific[scientific.index('def digits'):scientific.index('\nrun(')])

def samples(name):
    path=a.out/(name+'.log')
    return [dict(re.findall(r'(\w+)=([\w-]+)',line)) for line in path.read_text(encoding='utf8').splitlines() if line.startswith('TOOLBOX_ALLOC')]

if opts.baseline:
    cases={'scalar':keys('AC 2 ADD 2 ENTER')+check('4'),
           'unit':keys('AC')+amount('2','metre',1)+keys('ENTER')+check('2','2'),
           'temperature':keys('AC')+amount('20','Degree Celsius',23)+keys('ENTER')+check('5863/20','5863/20')}
    paired={}
    for case,body in cases.items():
        rows=[]
        for repeat in range(5):
            row={}
            for label,exe in ([('baseline',opts.baseline),('candidate',binary)] if repeat%2==0 else [('candidate',binary),('baseline',opts.baseline)]):
                a.bin=exe
                name=f'{case}-{repeat}-{label}'
                run(name,'log WARMUP_20\n'+body*20+'log REPEATED_50\n'+body*50)
                assert records[name]['passed']
                row[label]=records[name]['seconds']
            rows.append(row)
        paired[case]={'pairs_seconds':rows,'baseline_median_seconds':statistics.median(row['baseline'] for row in rows),
                      'candidate_median_seconds':statistics.median(row['candidate'] for row in rows),
                      'median_paired_ratio':statistics.median(row['candidate']/row['baseline'] for row in rows)}
    (opts.out/'paired-product.json').write_text(json.dumps({'results':paired,'baseline_sha256':hashlib.sha256(opts.baseline.read_bytes()).hexdigest(),
         'candidate_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'scope':'Whole uninstrumented headless real-key journey wall time, including process startup, 20 warmup evaluations then50 repeated evaluations, assertions and teardown. Cannot isolate pure operator latency. Each pair starts a fresh matching CAS session and executes the identical sequence. Five counterbalanced pairs per workload; no hardware inference.'},indent=2)+'\n',encoding='utf8')
    raise SystemExit(0)

if opts.history_populations:
    populations={'exact':keys('AC')+amount('2','metre',1)+keys('ENTER')+check('2','2'),
                 'measurement':keys('AC')+measured('10','1')+keys('ENTER')+uncertainty(1,1),
                 'constant':keys('AC')+constant('gravitational constant',59)+keys('ENTER')+uncertainty(1.5e-15,1)}
    populations['mixed']=''.join(populations.values())
    summary={}
    for name,one in populations.items():
        run(name,one*(20 if name=='mixed' else 60)+'calculus_probe\n'+keys('HOME')+'wait 20\ncalculus_probe\nopen_app Calculation\nwait 20\n')
        assert records[name]['passed']
        lines=(a.out/(name+'.log')).read_text(encoding='utf8').splitlines()
        summary[name]={'successful_evaluations':60,'history_capacity':50,'history_fullness':'60 successful real evaluations with source-defined cap50',
                       'live':[dict(re.findall(r'(\w+)=([\w-]+)',line)) for line in lines if line.startswith('SCIENTIFIC_LIVE|')],
                       'lvgl':[dict(re.findall(r'(\w+)=([\w-]+)',line)) for line in lines if line.startswith('CALCULUS_PROBE|')]}
    (opts.out/'history-populations.json').write_text(json.dumps({'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'populations':summary},indent=2)+'\n',encoding='utf8')
    raise SystemExit(0)

if opts.profile_only:
    cases={
        'scalar':keys('AC 2 ADD 2 ENTER')+check('4'),
        'unit':keys('AC')+amount('2','metre',1)+keys('ENTER')+check('2','2'),
        'temperature':keys('AC')+amount('20','Degree Celsius',23)+keys('ENTER')+check('5863/20','5863/20'),
        'measurement-one':keys('AC')+measured('10','1')+keys('ENTER')+check('10','10')+uncertainty(1,1),
        'measurement-two':keys('AC')+measured('10','1')+keys('ADD')+measured('20','2')+keys('ENTER')+check('30','30')+uncertainty(5**.5,2),
        'constant-exact':keys('AC')+constant('Speed of light in vacuum',1)+keys('ENTER')+check('299792458','299792458'),
        'constant-measured':keys('AC')+constant('gravitational constant',59)+keys('ENTER')+uncertainty(1.5e-15,1),
        'correlated':keys('AC')+constant('Bohr radius',2)+keys('MUL')+constant('electron mass',3)+keys('ENTER')+uncertainty(7.3474178868567063e-51,2),
    }
    for count in (8,16,17):
        body=keys('AC')
        for i in range(count):body+=(keys('ADD') if i else '')+measure('1','0.01')
        body+=keys('ENTER')
        body+=error('uncertainty_limit') if count==17 else check(str(count),str(count))+uncertainty(.01*count**.5,count)
        cases[f'many-{count}-sources']=body
    profiles={}
    for name,body in cases.items():
        for scope in ('evaluation','analysis','convert','compose','format'):
            if name=='many-17-sources' and scope not in ('evaluation','analysis'):continue
            if scope=='format' and name not in ('measurement-one','measurement-two','constant-measured','correlated','many-8-sources','many-16-sources'):continue
            env.update(NUMOS_TOOLBOX_SCOPE=scope,NUMOS_TOOLBOX_FAIL_AT='0',NUMOS_TOOLBOX_FAIL_CALL='0')
            experiment=body+(metrology(2) if scope=='format' else '')
            label=name+'-'+scope
            recovery=keys('AC 2 ADD 2 ENTER')+check('4') if name=='many-17-sources' else ''
            run(label,(experiment+'calculus_probe\n')*12+recovery)
            if label in records:
                profiles[label]=samples(label)
                if name=='many-17-sources' and scope=='evaluation':profiles[label]=profiles[label][:-1] # Final healthy recovery is not rejection timing.
                assert records[label]['passed'],label
    (opts.out/'profiles.json').write_text(json.dumps({'samples':profiles,'warmup_policy':'First two occurrences of each scope per case are warmup; report median of remaining ten or more.',
        'scope_note':'chrono nanoseconds in native private overlay. Analysis deliberately suspends fault allocation tracking inside Giac. Scalar bypasses Quantity analysis/convert/compose. C malloc/LVGL/Giac C heaps excluded from C++ live bytes.'},indent=2)+'\n',encoding='utf8')
    summaries={}
    for name,rows in profiles.items():
        measured=rows[2:]
        summaries[name]={'samples':len(measured),
                         'median_us':statistics.median(int(row['ns'])/1000 for row in measured) if measured else None,
                         'median_tracked_allocation_attempts':statistics.median(int(row['attempts']) for row in measured) if measured else None,
                         'maximum_cpp_peak_bytes':max((int(row['peak']) for row in measured),default=0)}
    (opts.out/'profile-summary.json').write_text(json.dumps({'scopes':summaries,'unavailable':'The rejected 17-source result has no conversion, composition or FORMAT stage; its final healthy 2+2 recovery is excluded from rejection timing.','note':'Per invocation, two warmup invocations removed. Convert and compose each run twice per evaluation. Evaluation includes Giac, result publication and history; phases are separate matched expressions, not additive timings. Analysis allocation-attempt counting excludes suspended Giac calls, while global C++ peak bytes still observe Giac C++ allocations. C allocation families are excluded. No target-hardware latency inference.'},indent=2)+'\n',encoding='utf8')
else:
    ordinary=keys('AC 2 ADD 2 ENTER')+check('4')
    nominal='123456789123456789'
    uncertain=measured(nominal,'10000')+keys('ENTER')
    experiments=[]
    for scope in ('source','payload','combine','analysis','convert','compose','ans','memory','publish','format','history'):
        for point in ((1,) if scope in ('source','payload','combine','ans','history') else (1,2)):
            pre=keys('7 ENTER AC')
            call=2 if scope in ('history','ans') else 1
            if scope in ('source','payload','analysis','convert','compose'):
                body=pre+uncertain+'assert_calc_status out_of_memory\nassert_calc_input structure\n'+keys('AC ans ENTER')+check('7')+ordinary
            elif scope=='combine':
                body=pre+measured('2','0.1')+keys('ADD')+measured('3','0.2')+keys('ENTER')+'assert_calc_status out_of_memory\n'+keys('AC ans ENTER')+check('7')+ordinary
            elif scope=='ans':
                body=pre+uncertain+'assert_calc_input no_result\n'+keys('AC ans ENTER')+check('7')+ordinary
            elif scope=='memory':
                body=uncertain+uncertainty(10000,1)+keys('STO 1 AC ALPHA 1 ENTER')+check('0')+ordinary
            elif scope=='publish':
                body=uncertain+output()+all_units()+search('cm')+selected(1,14)+keys('ENTER')+'assert_calc_input toolbox open\n'+check(nominal,nominal)+uncertainty(10000,1,10000)+keys('BACK BACK')+ordinary
            elif scope=='format':
                body=uncertain+keys('FORMAT')+check(nominal,nominal)+uncertainty(10000,1,10000)+ordinary
            else:
                body=uncertain+keys('AC 7 ENTER UP UP')+check('7')+'assert_calc_input structure\n'+ordinary
            experiments.append((scope+'-'+str(point),scope,point,call,body))
    for kind,query,reference in [('measurement','measurement',750),('constant','gravitational constant',16443)]:
        for point in (1,2):
            body=keys('2 POW 3 TOOLBOX')+search(query)+f'assert_calc_input toolbox selected {reference} 0\n'+keys('ENTER')+'assert_calc_input toolbox open\nassert_calc_input serialized ((2)^(3))\nassert_calc_input structure\n'+keys('BACK BACK ENTER')+check('8')+ordinary
            experiments.append((kind+'-insert-'+str(point),'insert',point,1,body))
    for point in (1,2):
        body=keys('2 POW 3 TOOLBOX')+search('gravitational constant')+keys('FORMAT ENTER BACK BACK BACK')+'assert_calc_input toolbox closed\nassert_calc_input serialized ((2)^(3))\n'+keys('ENTER')+check('8')+ordinary
        experiments.append(('constant-favorite-save-'+str(point),'save',point,1,body))
    for scope in ('analysis','convert'):
        for point in (1,2):
            body='set_angle_mode deg\n'+keys('7 ENTER AC SIN')+measured('1','0.01',8,query='Radian')+keys('RIGHT ENTER')+'assert_calc_status out_of_memory\nassert_calc_input structure\n'+keys('AC ans ENTER')+check('7')+ordinary
            experiments.append((f'explicit-angle-{scope}-{point}',scope,point,1,body))
    if opts.help_c_allocator:
        for kind,query in (('measurement','measurement'),('constant','gravitational constant')):
            for point in (1,2):
                body=measured('10','1')+keys('ENTER')+check('10','10')+uncertainty(1,1)+keys('TOOLBOX')+search(query)+keys('FORMAT DOWN DOWN DOWN ENTER')+check('10','10')+uncertainty(1,1)+keys('BACK BACK BACK AC ans ENTER')+check('10','10')+uncertainty(1,1)+ordinary
                experiments.append((f'{kind}-help-c-{point}','help-c',point,1,body))
    for name,scope,point,call,body in experiments:
        env.update(NUMOS_TOOLBOX_SCOPE=scope,NUMOS_TOOLBOX_FAIL_AT=str(point),NUMOS_TOOLBOX_FAIL_CALL=str(call))
        run(name,body)
        if name not in records:continue
        rows=samples(name);records[name]['samples']=rows
        records[name]['persistent_failure_observed']=any(int(s['failures'])>0 for s in rows)
        records[name]['allocation_free_scope_observed']=scope=='help-c' and bool(rows) and all(int(s['attempts'])==0 for s in rows)
        records[name]['passed'] &= records[name]['persistent_failure_observed'] or records[name]['allocation_free_scope_observed']
    report={'results':records,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),
            'limitations':['SourceId creation and read-only correlation lookup perform no allocations; exhaustion and injected unavailable/invalid covariance are separate standalone kernel gates.',
                           'Help C-allocator mode wraps lv_malloc/zeroed/realloc/reallocf only during its synchronous refresh; general asynchronous LVGL rendering allocation is outside that transaction.',
                           'Faults are persistent within the selected scope/invocation; later healthy invocations run in the same process without resetting Giac.']}
    (opts.out/'faults.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    raise SystemExit(0 if records and all(r['passed'] for r in records.values()) else 1)
