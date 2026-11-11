#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile source-frozen layout and private instrumented renderer; never publish."""
import argparse,ctypes,hashlib,importlib.util,json,os,re,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--build',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--random',type=int,default=2000);p.add_argument('--random-seed',type=lambda s:int(s,0),default=0x50415232);p.add_argument('--random-draw',action='store_true');p.add_argument('--skip-build',action='store_true');p.add_argument('--mutant-no-kern',action='store_true');p.add_argument('--mutant-operator-base',action='store_true');p.add_argument('--mutant-stale-measurement-ink',action='store_true');a=p.parse_args()
 source=a.source.resolve();build=a.build.resolve();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
 if os.name=='nt':
  def short(p):
   b=ctypes.create_unicode_buffer(32768);ctypes.windll.kernel32.GetShortPathNameW(str(p),b,len(b));return Path(b.value)
  source=short(source);build=short(build);out=short(out)
 repo=short(ROOT) if os.name=='nt' else ROOT
 spec=importlib.util.spec_from_file_location('corpus',ROOT/'scripts/math-tex-parity-corpus.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod);mod.write(out)
 env=dict(os.environ,PATH='C:/mingw64/bin;C:/SDL2/x86_64-w64-mingw32/bin;'+os.environ.get('PATH',''))
 records=[]
 def run(name,cmd):
  start=time.time();r=subprocess.run(cmd,cwd=source,env=env,capture_output=True);(out/(name+'.log')).write_bytes(r.stdout+r.stderr);records.append(dict(name=name,command=cmd,exit=r.returncode,seconds=time.time()-start));(out/'commands.json').write_text(json.dumps(records,indent=2));
  if r.returncode:raise RuntimeError(name+' failed: '+(r.stdout+r.stderr).decode(errors='replace')[-5000:])
 if not a.skip_build:
  text=(source/'src/ui/MathRenderer.cpp').read_text(encoding='utf-8')
  declarations='''namespace vpam {
void parityNode(const MathNode*,int,int,const FontMetrics&);
void parityGlyph(int,int,unsigned,uint32_t,const lv_font_glyph_dsc_t&);
void parityCursor(const NodeRow*,int,int,int,int,const FontMetrics&);
void parityLine(int,int,int,int,int);
void parityStroke(int,int,int,int,int);
void parityDelimiterRequest(int,int,int,uint32_t);
void parityRect(int,int,int,int);
void parityMissing(uint32_t,unsigned);
void parityDelimiter(int,int,int,uint32_t,const lv_font_t*,const lv_area_t&);
void parityScopeEnter();
void parityScopeExit();
struct ParityNodeScope { ParityNodeScope(){parityScopeEnter();} ~ParityNodeScope(){parityScopeExit();} };
'''
  text=text.replace('namespace vpam {',declarations,1)
  def inject(anchor,replacement):
   nonlocal text
   assert text.count(anchor)==1,(anchor,text.count(anchor));text=text.replace(anchor,replacement)
  anchor='    if (fm.scriptLevel >= 2) {\n        font = _fontScriptScript;';inject(anchor,'    ParityNodeScope parityScope;\n    if (layer) parityNode(node,x,yBaseline,fm);\n'+anchor)
  anchor='            lv_draw_letter(layer, &dsc, &letterPos);\n            penX += glyph.adv_w;';inject(anchor,'            parityGlyph(penX,yBaseline,scriptLevel,dsc.unicode,glyph);\n'+anchor)
  anchor='            penX += ui::missingMathGlyphAdvance(font);';inject(anchor,'            parityMissing(cp,scriptLevel);\n'+anchor)
  anchor='    _cursorX = static_cast<int16_t>(rowOrigin + offsetX);';inject(anchor,'    parityCursor(cur.row,cur.index,rowOrigin,rowBaseline,offsetX,fm);\n'+anchor)
  anchor='int16_t width, lv_color_t color) {\n    if (!layer) return;';inject(anchor,anchor+'\n    parityLine(x1,y1,x2,y2,width);')
  anchor='int16_t x2, int16_t y2, int16_t w, lv_color_t color) {\n    if (!layer) return;'
  if anchor in text:inject(anchor,anchor+'\n    parityStroke(x1,y1,x2,y2,w);')
  else:
   anchor='int16_t w, lv_color_t color) {\n    if (!layer) return;';inject(anchor,anchor+'\n    parityStroke(x1,y1,x2,y2,w);')
  anchor='lv_color_t color, lv_opa_t opa) {\n    if (!layer) return;';inject(anchor,anchor+'\n    parityRect(x,y,w,h);')
  anchor='lv_draw_letter(layer, &dsc, &pos);'
  emptyAnchor='    lv_draw_rect(layer, &border, &area);'
  if emptyAnchor in text:inject(emptyAnchor,'    parityRect(area.x1,area.y1,area.x2-area.x1+1,area.y2-area.y1+1);\n'+emptyAnchor)
  requestAnchor='    if (yBottom <= yTop || font == nullptr) return false;'
  inject(requestAnchor,requestAnchor+'\n    parityDelimiterRequest(x,yTop,yBottom,delimCp);')
  assert text.count(anchor)==4,text.count(anchor)
  text=text.replace(anchor,'parityDelimiter(pos.x,pos.y,emSizePx,dsc.unicode,dsc.font,layer->_clip_area);\n            '+anchor)
  if a.mutant_operator_base:
   anchor='largeOperatorUtf8(g.glyph, glyph);';inject(anchor,'largeOperatorUtf8(g.glyph & ~uint32_t(1), glyph); // Private negative control: paints base after display layout.')
  (out/'MathRenderer-probe.cpp').write_text(text,encoding='utf-8')
  styleCount=8 if 'CRAMPED' in (source/'src/math/MathTypography.h').read_text(encoding='utf-8') else 4
  flags=['-std=c++17','-O1','-ffunction-sections','-fdata-sections','-DNATIVE_SIM','-DSDL_MAIN_HANDLED','-DLV_CONF_INCLUDE_SIMPLE','-DLV_USE_STDLIB_MALLOC=LV_STDLIB_CLIB','-DPARITY_STYLE_COUNT='+str(styleCount),'-I'+str(out),'-I'+str(source/'src/ui'),'-I'+str(source/'src/math'),'-I'+str(source/'src'),'-I'+str(source),'-I'+str(repo/'src/ui'),'-I'+str(repo/'src'),'-I'+str(repo/'.pio/libdeps/emulator_pc/lvgl')]
  flags+=['-I'+str(repo/'src/math')]
  if 'bool upright()' in (source/'src/math/MathAST.h').read_text(encoding='utf-8'):flags+=['-DPARITY_UPRIGHT']
  astSource=source/'src/math/MathAST.cpp'
  if a.mutant_no_kern:
   original=astSource.read_text(encoding='utf-8');changed,count=re.subn(r'_mathKernPx\s*=\s*scriptMathKernPx\(.*?\);','_mathKernPx = 0;',original,flags=re.S);assert count==2,count
   astSource=out/'MathAST-no-kern.cpp';astSource.write_text(changed,encoding='utf-8')
  if a.mutant_stale_measurement_ink:
   changed=astSource.read_text(encoding='utf-8')
   for metric in ['ascent','descent']:
    anchor='_layout.'+metric+'=_layout.ink'+metric.title()+'=geometry.'+metric+';'
    assert changed.count(anchor)==1,(anchor,changed.count(anchor))
    changed=changed.replace(anchor,'_layout.'+metric+'=geometry.'+metric+'; // Private stale-ink mutation.')
   astSource=out/'MathAST-stale-measurement-ink.cpp';astSource.write_text(changed,encoding='utf-8')
  own=[]
  for name,unit in [('probe',repo/'tests/host/math-tex-parity-probe.cpp'),('renderer',out/'MathRenderer-probe.cpp'),('MathAST',astSource),('CursorController',source/'src/math/CursorController.cpp'),('MathTypography',source/'src/ui/MathTypography.cpp')]:
   obj=out/(name+'.o');run(name+'-compile',['g++',*flags,'-c',str(unit),'-o',str(obj)]);own.append(obj)
  # Supplemental operator fonts are candidate source, never borrowed from the
  # immutable baseline dependency build. Baseline snapshots do not contain them.
  fontUnits=sorted((source/'src/fonts').glob('stix_operators_*.c'))
  if not fontUnits and (source/'src/math/MathOperatorGeometry.h').exists():fontUnits=sorted((repo/'src/fonts').glob('stix_operators_*.c'))
  for unit in fontUnits:
   obj=out/(unit.stem+'.o');run(unit.stem+'-compile',['gcc',*[f for f in flags if not f.startswith('-std=')],'-std=c11','-c',str(unit),'-o',str(obj)]);own.append(obj)
  replaced={'MathAST.o','CursorController.o','MathTypography.o',*[x.stem+'.o' for x in fontUnits]}
  objects=[x for x in (build/'src').rglob('*.o') if ('math' in x.parts or 'fonts' in x.parts or x.name=='FileSystem.o') and x.name not in replaced]
  libs=[*build.rglob('liblvgl.a'),*build.rglob('libgiac.a'),*build.rglob('libtommath.a')];assert len(libs)==3
  rsp=out/'probe.rsp';rsp.write_text('\n'.join('"'+x.as_posix()+'"' for x in [*own,*objects,*libs]));run('link',['g++','@'+str(rsp),'-Wl,--gc-sections','-o',str(out/'probe.exe')])
  files=[source/'src/math/MathAST.cpp',source/'src/ui/MathRenderer.cpp',source/'src/ui/MathRenderer.h',source/'src/ui/MathTypography.h',*sorted((source/'src/math').rglob('*.h')),*fontUnits,*objects,*libs]
  if astSource!=source/'src/math/MathAST.cpp':files.append(astSource)
  (out/'source-object-manifest.json').write_text(json.dumps([dict(path=str(x),sha256=hashlib.sha256(x.read_bytes()).hexdigest()) for x in files],indent=2))
 run('geometry',[str(out/'probe.exe'),str(out),str(a.random),str(a.random_seed),str(int(a.random_draw))])
 from PIL import Image
 for path in out.glob('*.ppm'):
  with Image.open(path) as im:im.save(path.with_suffix('.png'))
  path.unlink()
 print('PASS private geometry capture',out)
if __name__=='__main__':main()
