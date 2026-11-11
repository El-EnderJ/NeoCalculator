#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Focused reference-typography product test; private objects, no builds/publishing."""
import argparse, ctypes, hashlib, json, os, re, shutil, subprocess, time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--source',type=Path,default=ROOT);p.add_argument('--build',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--evidence',type=Path);p.add_argument('--probe',choices=('references','limits'),default='references');p.add_argument('--negative-control',action='store_true');a=p.parse_args()
 source=a.source.resolve();build=a.build.resolve();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
 def short(x):
  if os.name!='nt':return x
  b=ctypes.create_unicode_buffer(32768);assert ctypes.windll.kernel32.GetShortPathNameW(str(x),b,len(b));return Path(b.value)
 source,build,out,repo=map(short,(source,build,out,ROOT));env=dict(os.environ,PATH='C:/mingw64/bin;C:/SDL2/x86_64-w64-mingw32/bin;'+os.environ.get('PATH',''));records=[]
 operator_fonts=sorted((source/'src/fonts').glob('stix_operators_*.c'))
 probe=repo/('tests/host/math-tex-parity-product-'+a.probe+'.cpp')
 tracked=[source/'src/math/MathAST.cpp',source/'src/math/MathAST.h',source/'src/math/MathTypography.h',source/'src/math/MathRowLayout.h',source/'src/math/CursorController.cpp',source/'src/math/CursorController.h',source/'src/ui/MathRenderer.cpp',source/'src/ui/MathRenderer.h',source/'src/ui/MathTypography.cpp',source/'src/ui/MathTextMetrics.h',probe,*operator_fonts]
 vector_geometry=source/'src/math/MathVectorDelimiterGeometry.h'
 if vector_geometry.exists():tracked.append(vector_geometry)
 initial={str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in tracked}
 (out/'source-before.json').write_text(json.dumps(initial,indent=2))
 def run(name,cmd,expected=0):
  started=time.time();r=subprocess.run(cmd,cwd=source,env=env,capture_output=True);(out/(name+'.log')).write_bytes(r.stdout+r.stderr);records.append(dict(name=name,command=cmd,exit=r.returncode,expected=expected,seconds=time.time()-started));(out/'commands.json').write_text(json.dumps(records,indent=2))
  if r.returncode!=expected:raise RuntimeError(name+' failed: '+(r.stdout+r.stderr).decode(errors='replace')[-8000:])
  return r
 original=(source/'src/ui/MathRenderer.cpp').read_text(encoding='utf-8');renderer=original.replace('namespace vpam {','namespace vpam {\nvoid productReferenceGlyph(int,int,unsigned,uint32_t,const lv_font_glyph_dsc_t&);',1)
 anchor='            lv_draw_letter(layer, &dsc, &letterPos);\n            penX += glyph.adv_w;';assert renderer.count(anchor)==1
 renderer=renderer.replace(anchor,'            productReferenceGlyph(penX,yBaseline,scriptLevel,dsc.unicode,glyph);\n'+anchor);(out/'MathRenderer-reference-probe.cpp').write_text(renderer,encoding='utf-8')
 flags=['-std=c++17','-O1','-ffunction-sections','-fdata-sections','-DNATIVE_SIM','-DSDL_MAIN_HANDLED','-DLV_CONF_INCLUDE_SIMPLE','-DLV_USE_STDLIB_MALLOC=LV_STDLIB_CLIB',*['-I'+str(x) for x in (out,source/'src/ui',source/'src/math',source/'src',source,repo/'src/ui',repo/'src',repo/'.pio/libdeps/emulator_pc/lvgl')]]
 own=[]
 for name,unit in [('probe',probe),('renderer',out/'MathRenderer-reference-probe.cpp'),('MathAST',source/'src/math/MathAST.cpp'),('CursorController',source/'src/math/CursorController.cpp'),('MathTypography',source/'src/ui/MathTypography.cpp')]:
  obj=out/(name+'.o');run(name+'-compile',['g++',*flags,'-c',str(unit),'-o',str(obj)]);own.append(obj)
 for font in operator_fonts:
  obj=out/(font.stem+'.o');cflags=['-std=c11' if x=='-std=c++17' else x for x in flags]
  run(font.stem+'-compile',['gcc',*cflags,'-c',str(font),'-o',str(obj)]);own.append(obj)
 refreshed={'MathAST.o','CursorController.o','MathTypography.o',*[font.stem+'.o' for font in operator_fonts]}
 objects=[x for x in (build/'src').rglob('*.o') if ('math' in x.parts or 'fonts' in x.parts or x.name=='FileSystem.o') and x.name not in refreshed]
 libs=[*build.rglob('liblvgl.a'),*build.rglob('libgiac.a'),*build.rglob('libtommath.a')];assert len(libs)==3
 (out/'linked-inputs.json').write_text(json.dumps([dict(path=str(x),sha256=hashlib.sha256(x.read_bytes()).hexdigest()) for x in [*own,*objects,*libs]],indent=2))
 def link(name,own):
  rsp=out/(name+'.rsp');rsp.write_text('\n'.join('"'+x.as_posix()+'"' for x in [*own,*objects,*libs]));run(name+'-link',['g++','@'+str(rsp),'-Wl,--gc-sections','-o',str(out/(name+'.exe'))])
 link('references',own);positive=out/'positive';positive.mkdir(exist_ok=True);run('positive',[str(out/'references.exe'),str(positive)])
 negative_passed=None
 if a.negative_control:
  ast=(source/'src/math/MathAST.cpp').read_text(encoding='utf-8')
  if a.probe=='references':
   anchor='std::make_unique<NodeSymbol>(subscript, true)';assert ast.count(anchor)==1
   changed=ast.replace(anchor,'std::make_unique<NodeSymbol>(subscript, false)');mutation='descriptor-role-removed'
  else:
   match=re.search(r'LayoutStatus\s+calculateLayoutChecked\s*\(\s*MathNode\s*&\s*(\w+)\s*,\s*const FontMetrics\s*&\s*(\w+)\s*\)\s*\{',ast);assert match,'checked layout API definition missing'
   end=match.end();depth=1
   while depth:
    depth+=(ast[end]=='{')-(ast[end]=='}');end+=1
   changed=ast[:match.end()]+f'\n    {match[1]}.calculateLayout({match[2]});\n    return LayoutStatus::Ok;\n}}'+ast[end:];mutation='bounds-removed'
  mutant=out/('MathAST-'+mutation+'.cpp');mutant.write_text(changed,encoding='utf-8');obj=out/'MathAST-mutant.o';run('mutant-compile',['g++',*flags,'-c',str(mutant),'-o',str(obj)])
  link('references-mutant',[obj if x.name=='MathAST.o' else x for x in own]);negative=out/'negative';negative.mkdir(exist_ok=True);r=run('negative',[str(out/'references-mutant.exe'),str(negative)],expected=1)
  required=(b'descriptive subscript has upright role',b'submitted glyph sequence matches independent Unicode expectation') if a.probe=='references' else (b'exact checked layout status',b'refusal suppresses cursor')
  negative_passed=all(text in r.stdout for text in required)
  assert negative_passed,'negative control must fail both semantic/geometry checks and actual renderer checks'
 from PIL import Image
 for path in out.rglob('*.ppm'):
  with Image.open(path) as im:im.save(path.with_suffix('.png'))
  path.unlink()
 assert initial=={str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in tracked},'source changed during focused gate; repeat on stable source'
 files=[*tracked,*objects,*libs]
 counts=re.search(r'SUMMARY checks=(\d+) failures=(\d+) frames=(\d+) styles=(\d+) sizeofSymbol=(\d+)',(out/'positive.log').read_text())
 assert counts
 result=dict(passed=True,probe=a.probe,checks=int(counts[1]),failures=int(counts[2]),frames=int(counts[3]),styles=int(counts[4]),sizeofSymbol=int(counts[5]),negativeControlPassed=negative_passed,productionMutated=False,sourceStableThroughoutBuild=True,files=[dict(path=str(x),sha256=hashlib.sha256(x.read_bytes()).hexdigest()) for x in files])
 (out/'summary.json').write_text(json.dumps(result,indent=2))
 if a.evidence:
  destination=a.evidence.resolve();destination.mkdir(parents=True,exist_ok=True)
  for path in out.rglob('*'):
   if path.is_file() and path.suffix in ('.png','.json','.log','.cpp'):
    target=destination/path.relative_to(out);target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,target)
 print('PASS focused product',a.probe,out)
if __name__=='__main__':main()
