#!/usr/bin/env python3
"""Compile twenty causal implementation defects; unchanged independent gates must reject.

Compilation failure is never a killed mutant. Source files are copied and changed
under --out only. The production tree and published emulator are never modified.
"""
import argparse,ctypes,hashlib,json,os,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--source',type=Path,required=True)
p.add_argument('--out',type=Path,required=True)
p.add_argument('--native',type=Path)
p.add_argument('--database',type=Path)
p.add_argument('--standalone-only',action='store_true')
p.add_argument('--only')
a=p.parse_args();a.source=a.source.resolve();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True)
def identities():
 paths=[a.source/'src/math/ScientificUncertainty.cpp',a.source/'src/math/ScientificUncertainty.h',a.source/'src/math/scientific/PhysicalConstants.cpp',a.source/'tests/host/scientific_uncertainty_checks.cpp']
 if a.native:paths += [a.native/'program.exe',*(a.native/'src').rglob('*.o'),*a.native.rglob('*.a')]
 return {str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}
beforeIdentities=identities();(a.out/'input-identities.json').write_text(json.dumps(beforeIdentities,indent=2))
env=dict(os.environ,PATH='C:/mingw64/bin;C:/SDL2/x86_64-w64-mingw32/bin;'+os.environ.get('PATH',''),PYTHONIOENCODING='utf-8')
K='src/math/ScientificUncertainty.cpp';H='src/math/ScientificUncertainty.h';D='src/math/scientific/PhysicalConstants.cpp';Q='src/math/Quantity.cpp';A='src/apps/CalculationApp.cpp';E='src/math/CalculationEngine.cpp';AST='src/math/MathAST.cpp';TB='src/math/ToolboxCatalog.cpp'
renew='if(out.uncertainty){auto lost=std::make_shared<numos::scientific::Uncertainty>(*out.uncertainty);lost->contributions[0].source=numos::scientific::allocateUserSource();out.uncertainty=std::move(lost);}'
view='if(numos::quantity::convert(*_quantity,descriptor,*view)!=numos::quantity::Error::None)return false;'
native_mutate='if(_quantity && _quantity->uncertainty){auto q=std::make_shared<numos::quantity::Value>(*_quantity);auto u=std::make_shared<numos::scientific::Uncertainty>(*q->uncertainty);u->contributions[0].source=numos::scientific::allocateUserSource();q->uncertainty=u;_quantity=q;}'
specs=[
 (1,'same-source-as-independent',K,'identity',False,[('const double c=ca+cb;','const double c=(ca!=0 && cb!=0)?std::copysign(std::hypot(ca,cb),ca):ca+cb;')]),
 (2,'separate-measures-share-source',H,'identity',False,[('return next;','return 1;')]),
 (3,'ans-loses-source',E,'same-source-ans',True,[('out=*slot.quantity;return true;','out=*slot.quantity;'+renew+'return true;')]),
 (4,'history-clone-loses-source',AST,'history-source-identity',True,[('static_cast<const NodeMeasurement*>(node)->sourceId());','0);')]),
 (5,'unit-conversion-loses-source',A,'conversion-identity',True,[(view,view+native_mutate)]),
 (6,'celsius-offset-on-uncertainty',Q,'temperature-point',True,[('next.standardUncertainty=coherentU/slope;','next.standardUncertainty=(coherentU+(value.role==Role::TemperaturePoint?273.15:0))/slope;')]),
 (7,'convert-rounded-uncertainty',Q,'format-canonical',True,[('next.standardUncertainty=coherentU/slope;','next.standardUncertainty=(std::round(coherentU*100)/100)/slope;')]),
 (8,'measured-constant-exact',D,'dataset',False,[('1.5e-15,false,"G"','1.5e-15,true,"G"')]),
 (9,'exact-constant-uncertain',D,'dataset',False,[('0,true,"c"','1,false,"c"')]),
 (10,'codata-correlation-zero',D,'codata-product',False,[('{2,3,-99999}','{2,3,0}')]),
 (11,'codata-correlation-sign',D,'codata-product',False,[('{2,3,-99999}','{2,3,99999}')]),
 (12,'accept-out-of-range-rho',K,'covariance',False,[('if(!std::isfinite(rho) || std::abs(rho)>1)return E::Numeric;','if(!std::isfinite(rho))return E::Numeric;')]),
 (13,'drop-seventeenth-source',K,'limits',False,[('if(next.count==kMaximumSources)return E::Limit;','if(next.count==kMaximumSources)break;')]),
 (14,'negative-variance-publishes-nan',K,'covariance',False,[('if(total<0){if(total < -tolerance)return E::Numeric;total=0;}',''),('if(!finite(answer) || (answer==0 && total!=0))return E::Numeric;','')]),
 (15,'uncertain-zero-square-false-certainty',K,'nonlinear',False,[('if(x==0 && u>0)return E::Nonlinear;','if(x==0 && u>0)return scale(a,0,out);')]),
 (16,'source-id-wrap-reuses-one',H,'limits',False,[('        return 0;\n    }\n};','        return 1;\n    }\n};')]),
 (17,'measurement-is-ordinary-addition',Q,'same-source-ans',True,[('Value candidate;const auto e=measurement(std::move(out),u,m->sourceId(),candidate);','return apply(out,std::move(u),OpKind::Add); Value candidate;const auto e=measurement(std::move(out),u,m->sourceId(),candidate);')]),
 (18,'constant-favorites-swap-reference',TB,'constant-favorites',True,[('return {vpam::makeQuantityReference({units::ReferenceId(entry.argument),units::PrefixId(entry.identity.variant)})};','return {vpam::makeQuantityReference({units::ReferenceId(entry.argument==1?59:entry.argument==59?1:entry.argument),units::PrefixId(entry.identity.variant)})};')]),
 (19,'expanded-mutates-canonical',A,'format-canonical',True,[('auto prepared=quantityFormatted(*_quantityView,_outputUnit,mode,shift,places);','if(mode==numos::CalculationFormat::UncertaintyExpanded && _quantity){auto q=std::make_shared<numos::quantity::Value>(*_quantity);numos::scientific::Owned u;numos::scientific::scale(q->uncertainty,2,u);q->uncertainty=u;_quantity=q;}auto prepared=quantityFormatted(*_quantityView,_outputUnit,mode,shift,places);')]),
 (20,'relative-from-previous-display',A,'temperature-point',True,[(view,view+'if(_quantityView && view->hasUncertainty && _quantityView->nominal!=0)view->relativeUncertainty=_quantityView->standardUncertainty/std::abs(_quantityView->nominal);')]),
]
records=[];commands=[]
def run(folder,label,cmd,timeout=240):
 start=time.perf_counter();r=subprocess.run(cmd,cwd=a.source,env=env,capture_output=True,timeout=timeout);(folder/(label+'.log')).write_bytes(r.stdout+r.stderr)
 commands.append(dict(label=label,command=cmd,exit=r.returncode,seconds=time.perf_counter()-start));(a.out/'commands.json').write_text(json.dumps(commands,indent=2),encoding='utf8');return r
def cpp_command(entry):
 if 'arguments' in entry:return list(entry['arguments'])
 parse=ctypes.windll.shell32.CommandLineToArgvW;parse.argtypes=[ctypes.c_wchar_p,ctypes.POINTER(ctypes.c_int)];parse.restype=ctypes.POINTER(ctypes.c_wchar_p);n=ctypes.c_int();args=parse(entry['command'],ctypes.byref(n));result=[args[i] for i in range(n.value)];ctypes.windll.kernel32.LocalFree(args);return result
for number,name,file,case,native,replacements in specs:
 if a.standalone_only and native:continue
 if a.only and a.only not in name:continue
 folder=a.out/f'{number:02d}-{name}';folder.mkdir(exist_ok=True)
 source=(a.source/file).read_text(encoding='utf8')
 if file==TB:source=source.replace('#include "units/UnitToolboxProvider.inc"',(a.source/'src/math/units/UnitToolboxProvider.inc').read_text(encoding='utf8'))
 original=source
 for before,after in replacements:
  assert source.count(before)==1,(name,'mutation anchor count',source.count(before),before)
  source=source.replace(before,after)
 assert source!=original
 modified=folder/Path(file).name;modified.write_text(source,encoding='utf8')
 record=dict(number=number,name=name,source=file,sourceSha256=hashlib.sha256(original.encode()).hexdigest(),mutantSha256=hashlib.sha256(source.encode()).hexdigest(),detector=case,compiled=False,rejected=False)
 binary=folder/'mutant.exe'
 if native:
  assert a.native and a.database,'native mutations require --native emulator_pc-directory and --database compiledb'
  db=json.loads(a.database.read_text(encoding='utf8'));entry=next(e for e in db if Path(e['file']).name==Path(file).name);cmd=cpp_command(entry)
  obj=folder/(Path(file).stem+'.o');cmd[cmd.index('-o')+1]=str(obj)
  # PlatformIO puts the source last. Preserve all profile flags and includes.
  assert Path(cmd[-1]).name==Path(file).name,cmd[-1]
  cmd[-1]=str(modified);cmd+=['-I'+str((a.source/file).parent),'-I'+str(a.source/'src/math/units')]
  r=run(folder,'compile',cmd);record['compiled']=r.returncode==0
  if r.returncode==0:
   objects=[obj if p.name==obj.name else p for p in (a.native/'src').rglob('*.o')];libs=[*a.native.rglob('libgiac.a'),*a.native.rglob('libtommath.a'),*a.native.rglob('liblvgl.a')]
   assert sum(p==obj for p in objects)==1 and len(libs)==3
   rsp=folder/'link.rsp';rsp.write_text('\n'.join('"'+p.resolve().as_posix()+'"' for p in [*objects,*libs]),encoding='utf8')
   r=run(folder,'link',['C:/mingw64/bin/g++.exe','@'+str(rsp),'-LC:/SDL2/x86_64-w64-mingw32/lib','-lmingw32','-lSDL2main','-lSDL2','-Wl,--gc-sections','-o',str(binary)])
   record['linked']=r.returncode==0
   if r.returncode==0:
    # An ASCII snapshot can host the test while production object identities are
    # recorded above. Product tests inspect no source-generated expectations.
    test=Path(__file__).with_name('test-scientific-product.py')
    r=run(folder,'detect',['python',str(test),'--bin',str(binary),'--out',str(folder/'ui'),'--only',case])
    resultFile=folder/'ui/results.json';product=json.loads(resultFile.read_text()) if resultFile.exists() else {}
    record['rejected']=r.returncode==1 and (any(not v['passed'] for v in product.values()) or b'SourceId/contribution changed' in r.stderr)
 else:
  copied=folder/'source';(copied/'math/scientific').mkdir(parents=True,exist_ok=True)
  for rel in (K,H,D,'src/math/scientific/PhysicalConstants.h'):
   destination=copied/Path(rel).relative_to('src');destination.write_bytes((a.source/rel).read_bytes())
  (copied/Path(file).relative_to('src')).write_text(source,encoding='utf8')
  test=a.source/'tests/host/scientific_uncertainty_checks.cpp'
  r=run(folder,'compile',['C:/mingw64/bin/g++.exe','-std=c++17','-O1','-I'+str(copied),str(test),str(copied/'math/ScientificUncertainty.cpp'),str(copied/'math/scientific/PhysicalConstants.cpp'),'-o',str(binary)])
  record['compiled']=r.returncode==0
  if r.returncode==0:
   r=run(folder,'detect',[str(binary),case]);record['rejected']=r.returncode==1 and b'FAIL ' in r.stdout+r.stderr
 records.append(record);(a.out/'results.json').write_text(json.dumps(records,indent=2),encoding='utf8')
 print(number,name,'REJECTED' if record['rejected'] else 'BUILD-FAILED' if not record['compiled'] else 'SURVIVED',flush=True)
assert records and all(r['compiled'] and r['rejected'] for r in records),records
if not a.standalone_only and not a.only:assert len(records)==20
assert identities()==beforeIdentities,'production inputs changed while mutations were running'
