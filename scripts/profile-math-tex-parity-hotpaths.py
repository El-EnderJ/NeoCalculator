#!/usr/bin/env python3
"""Private source-frozen hot-path counters and causal allocation denial.

No instrumentation is linked into the product. Use matching source and native
objects. --prepare-only permits review before the candidate is stable.
"""
import argparse,ctypes,hashlib,json,os,re,shutil,subprocess,time
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
HEADER=r'''#pragma once
#include <algorithm>
#include <cstdint>
#include <chrono>
namespace resource_profile {
inline bool active=false,denyCpp=false,denyC=false;
inline unsigned layoutDepth=0,cursorDepth=0;
struct Counters {
 unsigned long long rootLayouts=0,nodeLayouts=0,rowMeasures=0,admissionCalls=0,maxLayoutDepth=0,glyphLookups=0;
 unsigned long long mathConstants=0,variants=0,mathKerns=0,draws=0,paintNodes=0;
 unsigned long long cursorCalls=0,cursorNs=0,cursorLayouts=0,cursorCAllocations=0,cppAllocations=0,nodeAllocations=0,cAllocations=0;
};
inline Counters counts;
inline uint64_t now(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
inline void begin(bool cpp,bool c){counts={};layoutDepth=cursorDepth=0;denyCpp=cpp;denyC=c;active=true;}
inline void end(){active=false;denyCpp=denyC=false;}
struct LayoutScope {
 bool measured;
 LayoutScope(bool rowPass=false):measured(active){if(measured){if(!layoutDepth)++counts.rootLayouts;if(rowPass)++counts.rowMeasures;else ++counts.nodeLayouts;++layoutDepth;counts.maxLayoutDepth=std::max<unsigned long long>(counts.maxLayoutDepth,layoutDepth);}}
 ~LayoutScope(){if(measured)--layoutDepth;}
};
struct CursorScope {
 bool measured,oldDenyC;uint64_t start,layouts;
 CursorScope():measured(active),oldDenyC(denyC),start(measured?now():0),layouts(counts.rootLayouts){if(measured){++counts.cursorCalls;++cursorDepth;denyC=true;}}
 ~CursorScope(){if(measured){counts.cursorNs+=now()-start;counts.cursorLayouts+=counts.rootLayouts-layouts;denyC=oldDenyC;--cursorDepth;}}
};
inline void constant(){if(active)++counts.mathConstants;}
inline void variant(){if(active)++counts.variants;}
inline void kern(){if(active)++counts.mathKerns;}
inline void draw(){if(active)++counts.draws;}
inline void paint(bool actual){if(active&&actual)++counts.paintNodes;}
}
'''

def short(path):
    path=path.resolve()
    if os.name!='nt':return path
    buffer=ctypes.create_unicode_buffer(32768)
    assert ctypes.windll.kernel32.GetShortPathNameW(str(path),buffer,len(buffer)),path
    return Path(buffer.value)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('source','build','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--layouts',type=int,default=1000)
    p.add_argument('--prepare-only',action='store_true')
    a=p.parse_args();source=a.source.resolve();build=a.build.resolve();out=a.out.resolve()
    out.mkdir(parents=True,exist_ok=True)
    overlay=out/'overlay';overlay.mkdir(exist_ok=True)
    hashes=[]
    # A private tree preserves relative header include resolution. Font bitmaps
    # remain in their original linked objects; no TTF/OTF is copied to evidence.
    for directory in ('src/math','src/ui','src/fonts'):
        for original in (source/directory).rglob('*'):
            if original.is_file() and original.suffix in ('.h','.hpp','.inc','.cpp'):
                dest=overlay/original.relative_to(source);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(original,dest)
                hashes.append(dict(path=original.relative_to(source).as_posix(),sha256=hashlib.sha256(original.read_bytes()).hexdigest()))
    # Received snapshots intentionally exclude the complete fonts directory.
    # This declaration-only dependency adds no definitions to the baseline.
    if not (overlay/'src/fonts/StixMathFont.h').exists():
        original=ROOT/'src/fonts/StixMathFont.h'
        dest=overlay/'src/fonts/StixMathFont.h';dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(original,dest)
        hashes.append(dict(path='src/fonts/StixMathFont.h',dependencyFromCurrentTree=True,sha256=hashlib.sha256(original.read_bytes()).hexdigest()))
    (out/'resource-profile.h').write_text(HEADER,encoding='utf-8')
    patched=[]
    def edit(path,transform):
        f=overlay/path;t=f.read_text(encoding='utf-8');u,count=transform(t)
        assert count,(path,'instrumentation anchor missing')
        f.write_text('#include "resource-profile.h"\n'+u,encoding='utf-8')
        patched.append(dict(path=path,replacements=count))
    def replace(anchor,extra):
        def change(t):return t.replace(anchor,anchor+extra),t.count(anchor)
        return change
    has_guard='LayoutStatus calculateLayoutChecked(' in (overlay/'src/math/MathAST.cpp').read_text(encoding='utf-8')
    def ast(t):
        t,n=re.subn(r'(void \w+::calculateLayout\(const FontMetrics& fm\) \{)',r'\1\n    resource_profile::LayoutScope observed;',t)
        if has_guard:
            t,m=re.subn(r'(LayoutStatus calculateLayoutChecked\([^\{]+\{)',r'\1\n    if(resource_profile::active)++resource_profile::counts.admissionCalls;',t)
            assert m==1
            n+=m
        return t,n
    edit('src/math/MathAST.cpp',ast)
    # Transparent Row recursion bypasses calculateLayout: count those frames
    # separately in the observed recursive depth, not as extra root layouts.
    edit('src/math/MathRowLayout.h',replace('static void measure(NodeRow& row, const FontMetrics& fm) {','\n        resource_profile::LayoutScope observed(true);'))
    edit('src/math/MathTypography.h',replace('static constexpr int16_t rawDu(MathConstant c) {','\n        if (!__builtin_is_constant_evaluated()) resource_profile::constant();'))
    edit('src/math/font/StixParentheses.h',replace('inline StixParenthesisPlan stixParenthesisPlan(int16_t targetHeight, int16_t em, bool bracket = false) {','\n    resource_profile::variant();'))
    edit('src/math/font/MathGlyphAssembly.cpp',lambda t:re.subn(r'(DelimiterAssembler::glyphMetricsForHeightPx\([^\{]+\{)',r'\1\n    resource_profile::variant();',t))
    kern=overlay/'src/math/font/stix_math_kern.h'
    if kern.exists():edit('src/math/font/stix_math_kern.h',replace('inline int16_t lookupMathKernDu(uint32_t codepoint, MathKernCorner corner, int32_t heightDu) {','\n    resource_profile::kern();'))
    operator=overlay/'src/math/font/stix_operator_ink.h'
    if operator.exists():edit('src/math/font/stix_operator_ink.h',replace('inline const StixOperatorInk* stixOperatorInk(uint32_t baseCodepoint, int16_t em, bool display) {','\n    resource_profile::variant();'))
    def renderer(t):
        t,n1=re.subn(r'(void MathCanvas::computeCursorPosition\([^\{]+\{)',r'\1\n    resource_profile::CursorScope observed;',t)
        anchor='void MathCanvas::onDraw(lv_event_t* e) {';n2=t.count(anchor);t=t.replace(anchor,anchor+'\n    resource_profile::draw();')
        t,n3=re.subn(r'(void MathCanvas::paintNode\([^\{]+\{)',r'\1\n    resource_profile::paint(layer != nullptr);',t)
        assert n1==n2==n3==1,(n1,n2,n3)
        return t,n1+n2+n3
    edit('src/ui/MathRenderer.cpp',renderer)
    (out/'source-manifest.json').write_text(json.dumps(dict(source=str(source),files=hashes,instrumentation=patched,probeSha256=hashlib.sha256((ROOT/'tests/host/math-tex-parity-resources.cpp').read_bytes()).hexdigest()),indent=2),encoding='utf-8')
    if a.prepare_only:print('Prepared private instrumentation',out);return
    src=short(source);dep=short(ROOT);bld=short(build);dst=short(out);ovl=short(overlay)
    env=dict(os.environ,PATH='C:/mingw64/bin;C:/SDL2/x86_64-w64-mingw32/bin;'+os.environ.get('PATH',''))
    commands=[]
    def run(name,command):
        start=time.perf_counter();r=subprocess.run(list(map(str,command)),cwd=src,env=env,capture_output=True)
        (out/(name+'.log')).write_bytes(r.stdout+r.stderr)
        commands.append(dict(name=name,command=list(map(str,command)),exit=r.returncode,seconds=time.perf_counter()-start))
        (out/'commands.json').write_text(json.dumps(commands,indent=2),encoding='utf-8')
        if r.returncode:raise RuntimeError(name+': '+(r.stdout+r.stderr).decode(errors='replace')[-4000:])
    flags=['-std=c++17','-O1','-ffunction-sections','-fdata-sections','-DNATIVE_SIM','-DSDL_MAIN_HANDLED','-DLV_CONF_INCLUDE_SIMPLE','-DLV_USE_STDLIB_MALLOC=LV_STDLIB_CLIB','-DNUMOS_MATH_AST_TEST_ALLOCATOR','-I'+str(dst),'-I'+str(ovl/'src'),'-I'+str(ovl/'src/math'),'-I'+str(ovl/'src/ui'),'-I'+str(src/'src'),'-I'+str(src/'src/math'),'-I'+str(src/'src/ui'),'-I'+str(src),'-I'+str(dep/'.pio/libdeps/emulator_pc/lvgl')]
    if has_guard:flags.append('-DNUMOS_RESOURCE_GUARD')
    units=[('probe',dep/'tests/host/math-tex-parity-resources.cpp'),('MathAST',ovl/'src/math/MathAST.cpp'),('MathRenderer',ovl/'src/ui/MathRenderer.cpp'),('CursorController',ovl/'src/math/CursorController.cpp'),('MathTypography',ovl/'src/ui/MathTypography.cpp'),('MathGlyphAssembly',ovl/'src/math/font/MathGlyphAssembly.cpp')]
    own=[]
    for name,file in units:
        obj=dst/(name+'.o');run(name+'-compile',['g++',*flags,'-c',file,'-o',obj]);own.append(obj)
    replaced={name+'.o' for name,_ in units}
    objects=[x for x in (bld/'src').rglob('*.o') if ('math' in x.parts or 'fonts' in x.parts or x.name=='FileSystem.o') and x.name not in replaced]
    libraries=[*bld.rglob('liblvgl.a'),*bld.rglob('libgiac.a'),*bld.rglob('libtommath.a')];assert len(libraries)==3
    response=dst/'resource.rsp';response.write_text('\n'.join('"'+x.as_posix()+'"' for x in [*own,*objects,*libraries]))
    exe=dst/'resources.exe';run('link',['g++','@'+str(response),'-Wl,--gc-sections','-Wl,--wrap=malloc','-Wl,--wrap=calloc','-Wl,--wrap=realloc','-Wl,--wrap=lv_font_get_glyph_dsc','-o',exe])
    (out/'linked-manifest.json').write_text(json.dumps([dict(path=str(x),sha256=hashlib.sha256(x.read_bytes()).hexdigest()) for x in [*objects,*libraries,exe]],indent=2),encoding='utf-8')
    run('hotpaths',[exe,str(a.layouts)])
    rows=[json.loads(x) for x in (out/'hotpaths.log').read_text(encoding='utf-8').splitlines() if x.startswith('{')]
    assert rows[-1]['summary'] and rows[-1]['failures']==0
    (out/'profile.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
    print('PASS hot-path matrix:',len(rows)-1,'phase records;',a.layouts*48,'denied layouts')

if __name__=='__main__':main()
