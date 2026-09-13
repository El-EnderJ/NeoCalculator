#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build an isolated Windows/MinGW page profiler from a matching native snapshot.

No ordinary sources or object directories are modified. The shared production
Giac boundary remains the only owner of mathematics. Timers are nested, and
printing happens after the page timer stops. See TUTOR_PAGE_PERF_01.md.
"""
from pathlib import Path
import json,subprocess,shlex,argparse,os,hashlib
p=argparse.ArgumentParser()
p.add_argument('--source',required=True,help='ASCII source snapshot, baseline or candidate')
p.add_argument('--build',required=True,help='Matching emulator_pc object directory')
p.add_argument('--database',required=True,help='Matching compile_commands.json')
p.add_argument('--out',required=True,help='Ignored logs/output directory')
p.add_argument('--scratch',required=True,help='ASCII directory for derived probe objects')
p.add_argument('--compiler',default='C:/mingw64/bin/g++.exe')
p.add_argument('--sdl-lib',default='C:/SDL2/x86_64-w64-mingw32/lib')
p.add_argument('--giac-source',help='Optional single-file candidate overlay; recorded in identity')
a=p.parse_args()
source=Path(a.source).resolve();build=Path(a.build).resolve();out=Path(a.out).resolve();out.mkdir(parents=True,exist_ok=True)
scratch=Path(a.scratch).resolve();scratch.mkdir(parents=True,exist_ok=True)
header='''#pragma once
#include <chrono>
#include <cstdio>
namespace pageprobe {
enum Phase {page,projection,formula,clone,build,parse,context,capture,domain,eval,convert,publish,attach,layout,prose,caption,body,count};
inline const char* names[]={"page","projection","formula","clone","build","parse","context","capture","domain","eval","convert","publish","attach","layout","prose","caption","body"};
inline unsigned long long ns[count]{}; inline unsigned calls[count]{};
using Clock=std::chrono::steady_clock;
struct Span { Phase phase; Clock::time_point start=Clock::now(); bool running=true; explicit Span(Phase p):phase(p){} void stop(){if(running){ns[phase]+=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-start).count();++calls[phase];running=false;}} ~Span(){stop();}};
struct Page {Span timer{page}; unsigned index; explicit Page(unsigned i):index(i){for(int j=0;j<count;++j){ns[j]=0;calls[j]=0;}} ~Page(){timer.stop();printf("[PAGE_PROFILE] {\\\"index\\\":%u",index);for(int j=0;j<count;++j)printf(",\\\"%s_us\\\":%.3f,\\\"%s_calls\\\":%u",names[j],ns[j]/1000.0,names[j],calls[j]);printf("}\\n");}};
}
'''
(scratch/'PageProbe.h').write_text(header,encoding='utf-8')
def edit(text,old,new):
 assert text.count(old)==1,(old,text.count(old))
 return text.replace(old,new)
ui=(source/'src/apps/TutorStepsView.inc').read_text(encoding='utf-8')
ui=edit(ui,'const auto started=std::chrono::steady_clock::now();','pageprobe::Page probe(_teachingPage); pageprobe::Span projection(pageprobe::projection);\n    const auto started=std::chrono::steady_clock::now();')
ui=edit(ui,'for(unsigned i=0;i<count;++i) {\n            const Equation* authored','projection.stop(); pageprobe::Span formulaProbe(pageprobe::formula);\n        for(unsigned i=0;i<count;++i) {\n            const Equation* authored')
ui=edit(ui,'prepared[i]=cloneNode(_eqRowData[j]);','pageprobe::Span cloneProbe(pageprobe::clone); prepared[i]=cloneNode(_eqRowData[j]);')
ui=edit(ui,'if(!prepared[i])prepared[i]=tutorview::build(_derivation,refs[i],calls);','if(!prepared[i]){pageprobe::Span buildProbe(pageprobe::build);prepared[i]=tutorview::build(_derivation,refs[i],calls);}')
ui=edit(ui,'const auto title=page.kind','formulaProbe.stop(); pageprobe::Span publishProbe(pageprobe::publish);\n        const auto title=page.kind')
ui=edit(ui,'header(title.c_str(),hint.c_str());','pageprobe::Span proseProbe(pageprobe::prose);header(title.c_str(),hint.c_str());')
ui=edit(ui,'auto placeProse=[&] {','proseProbe.stop();auto placeProse=[&] {')
ui=edit(ui,'lv_label_set_text(_stepLabels[i],captions[i].c_str());','pageprobe::Span captionProbe(pageprobe::caption);lv_label_set_text(_stepLabels[i],captions[i].c_str());')
ui=edit(ui,'_canvas[i].setExpression(nullptr,nullptr);_viewNodes[i].reset();\n            auto row','captionProbe.stop();_canvas[i].setExpression(nullptr,nullptr);_viewNodes[i].reset();\n            auto row')
ui=edit(ui,'lv_obj_update_layout(_body);lv_obj_scroll_to_y','pageprobe::Span bodyProbe(pageprobe::body);lv_obj_update_layout(_body);lv_obj_scroll_to_y')
ui=edit(ui,'canvas.setExpression(r,nullptr);','{pageprobe::Span attachProbe(pageprobe::attach);canvas.setExpression(r,nullptr);}')
ui=edit(ui,'r->calculateLayout(canvas.normalMetrics());','{pageprobe::Span layoutProbe(pageprobe::layout);r->calculateLayout(canvas.normalMetrics());}')
(scratch/'TutorStepsView.inc').write_text(ui,encoding='utf-8')
giac=(Path(a.giac_source) if a.giac_source else source/'src/math/giac/GiacTutor.inc').read_text(encoding='utf-8')
head,tail=giac.split('StructuredEngineResult GiacEngine::tutorFormula(',1)
tail=edit(tail,'giac::gen l(equation.lhs, _state->ctx), r(equation.rhs, _state->ctx);','pageprobe::Span parseProbe(pageprobe::parse); giac::gen l(equation.lhs, _state->ctx), r(equation.rhs, _state->ctx); parseProbe.stop();pageprobe::Span contextProbe(pageprobe::context);')
if 'bool safe = tutorkernel::domainSafe' in tail:
 tail=edit(tail,'bool safe = tutorkernel::domainSafe','contextProbe.stop();pageprobe::Span domainProbe(pageprobe::domain);\n        bool safe = tutorkernel::domainSafe')
 tail=edit(tail,'if (!safe) {','domainProbe.stop();\n        if (!safe) {')
 tail=edit(tail,'try {\n                tutorkernel::capture','try {\n                pageprobe::Span captureProbe(pageprobe::capture);\n                tutorkernel::capture')
 tail=edit(tail,'nodes = 0;\n            safe =','nodes = 0;pageprobe::Span domainFallback(pageprobe::domain);\n            safe =')
else:
 tail=edit(tail,'try {\n            tutorkernel::capture','contextProbe.stop();\n        try {\n            pageprobe::Span captureProbe(pageprobe::capture);\n            tutorkernel::capture')
 tail=edit(tail,'if (!tutorkernel::domainSafe','pageprobe::Span domainProbe(pageprobe::domain);\n        if (!tutorkernel::domainSafe')
tail=edit(tail,'int budget = kTreeNodeBudget;','domainProbe.stop();\n        int budget = kTreeNodeBudget;\n        pageprobe::Span evalProbe(pageprobe::eval);\n        const auto equality=giac::symb_equal(l.eval(1, _state->ctx), r.eval(1, _state->ctx));\n        evalProbe.stop();pageprobe::Span convertProbe(pageprobe::convert);')
tail=edit(tail,'genToNode(giac::symb_equal(l.eval(1, _state->ctx), r.eval(1, _state->ctx)),','genToNode(equality,')
(scratch/'GiacTutor.inc').write_text(head+'StructuredEngineResult GiacEngine::tutorFormula('+tail,encoding='utf-8')
database=json.loads((source/a.database).read_text(encoding='utf-8'))
for file in ['EquationsApp.cpp','GiacEngine.cpp']:
 record=next(x for x in database if x['file'].endswith(file));original=source/record['file']
 (scratch/file).write_text('#include "PageProbe.h"\n'+original.read_text(encoding='utf-8'),encoding='utf-8')
 args=shlex.split(record['command'].replace('\\','/'));args[0]=a.compiler
 args[args.index('-o')+1]=str(scratch/(file+'.o'));args[args.index(record['file'].replace('\\','/'))]=str(scratch/file)
 args+=['-I'+str(scratch),'-I'+str(original.parent),'-Isrc/apps','-Isrc/math','-Isrc/math/giac']
 r=subprocess.run(args,cwd=source,capture_output=True);(out/(file+'.compile.log')).write_bytes(r.stdout+r.stderr);assert r.returncode==0,r.stderr.decode(errors='replace')[-3000:]
objects=[p for p in (build/'src').rglob('*.o') if p.name not in ['EquationsApp.o','GiacEngine.o']]+[scratch/'EquationsApp.cpp.o',scratch/'GiacEngine.cpp.o']
libs=list(build.rglob('*.a'))
rsp='\n'.join('"'+str(p).replace('\\','/')+'"' for p in objects)+'\n-Wl,--start-group\n'+'\n'.join('"'+str(p).replace('\\','/')+'"' for p in libs)+'\n-Wl,--end-group\n-L'+a.sdl_lib+'\n-lmingw32\n-lSDL2main\n-lSDL2\n'
(scratch/'link.rsp').write_text(rsp)
r=subprocess.run([a.compiler,'@'+str(scratch/'link.rsp'),'-Wl,--gc-sections','-static-libstdc++','-static-libgcc','-o',str(scratch/'program.exe')],cwd=source,capture_output=True)
(out/'link.log').write_bytes(r.stdout+r.stderr);assert r.returncode==0,r.stderr.decode(errors='replace')[-3000:]
print(scratch/'program.exe')

identity=dict(source=str(source),build=str(build),database=str(Path(a.database).resolve()),files={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [source/'src/apps/EquationsApp.cpp',source/'src/apps/TutorStepsView.inc',source/'src/apps/TutorPresentation.inc',Path(a.giac_source) if a.giac_source else source/'src/math/giac/GiacTutor.inc']},probe={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in scratch.glob('*') if p.suffix in ('.h','.inc','.cpp','.exe')})
(out/'identity.json').write_text(json.dumps(identity,indent=2),encoding='utf-8')
