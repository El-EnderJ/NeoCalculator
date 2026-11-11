#!/usr/bin/env python3
"""Read actual Xtensa renderer type sizes and immutable font symbol costs."""
import argparse,hashlib,json,subprocess
from pathlib import Path
from elftools.elf.elffile import ELFFile

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--elf',type=Path,required=True)
p.add_argument('--out',type=Path,required=True)
p.add_argument('--expected-sha256')
p.add_argument('--baseline',type=Path)
a=p.parse_args();a.out.parent.mkdir(parents=True,exist_ok=True)
sha=hashlib.sha256(a.elf.read_bytes()).hexdigest()
if a.expected_sha256:assert sha==a.expected_sha256,(sha,a.expected_sha256)
names={'MathNode','LayoutResult','FontMetrics','PaintPlacement','MathCanvas','MathStyle','Cursor','CursorController','AssemblyResult','StixOperatorInk','MathKernTableRecord','MathKernGlyphRecord','LargeOperatorGeometry','VectorDelimiterGeometry','TextAtomMetrics'}
types={};units=[];hotpathSymbols=[]
with a.elf.open('rb') as stream:
    elf=ELFFile(stream);assert 'Xtensa' in elf.get_machine_arch(),elf.get_machine_arch()
    sections={s.name:s['sh_size'] for s in elf.iter_sections()}
    symtab=elf.get_section_by_name('.symtab')
    for symbol in symtab.iter_symbols():
        if symbol['st_info']['type']!='STT_FUNC':continue
        if not any(key in symbol.name for key in ('calculateLayout','MathCanvas','DelimiterAssembler','RowLayout')):continue
        index=symbol['st_shndx']
        if isinstance(index,int):hotpathSymbols.append(dict(symbol=symbol.name,bytes=symbol['st_size'],section=elf.get_section(index).name))
    def walk(die,scope=()):
        attr=die.attributes;n=attr.get('DW_AT_name');name=n.value.decode(errors='replace') if n and isinstance(n.value,bytes) else ''
        typed=die.tag in ('DW_TAG_structure_type','DW_TAG_class_type','DW_TAG_enumeration_type')
        if typed and (name in names or name.startswith('Node')) and 'DW_AT_byte_size' in attr:
            key='::'.join((*scope,name));size=attr['DW_AT_byte_size'].value
            if key in types:assert types[key]==size,(key,types[key],size)
            types[key]=size
        childScope=(*scope,name) if name and die.tag in ('DW_TAG_namespace','DW_TAG_structure_type','DW_TAG_class_type') else scope
        for child in die.iter_children():walk(child,childScope)
    for cu in elf.get_dwarf_info().iter_CUs():
        top=cu.get_top_DIE();raw=top.attributes.get('DW_AT_name');name=raw.value.decode(errors='replace') if raw else ''
        if name.replace('\\','/').rsplit('/',1)[-1] in {'MathAST.cpp','MathRenderer.cpp','MathGlyphAssembly.cpp','CursorController.cpp'}:units.append(name);walk(top)
assert 'vpam::MathNode' in types and 'vpam::MathCanvas' in types,types
tool=Path('C:/.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-nm.exe')
symbols=[]
for line in subprocess.check_output([str(tool),'-S','--defined-only',str(a.elf)],text=True).splitlines():
    fields=line.split()
    if len(fields)==4 and any(key in fields[3] for key in ('stix_operators','StixOperators','StixMathKern','stix_math_kern')):
        symbols.append(dict(address=int(fields[0],16),bytes=int(fields[1],16),kind=fields[2],symbol=fields[3]))
result=dict(elf=str(a.elf),elfSha256=sha,dwarfCompilationUnits=units,types=types,newFontSymbols=symbols,hotpathCodeSymbols=hotpathSymbols,
            ownFontSymbolBytes=sum(s['bytes'] for s in symbols),
            limitations=['DWARF sizes are target ABI facts, not live heap usage.','nm sizes omit anonymous supplemental font bitmap/descriptor symbols unless separately associated; total linked cost comes from section/image delta.','No runtime or PCB measurement.'])
if a.baseline:
    old=json.loads(a.baseline.read_text(encoding='utf-8'))
    result['baselineElfSha256']=old['elfSha256']
    result['typeDeltas']={name:dict(before=old['types'].get(name),after=size,delta=size-old['types'][name] if name in old['types'] else None) for name,size in types.items()}
a.out.write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(dict(elfSha256=sha,types=types,output=str(a.out)),indent=2))
