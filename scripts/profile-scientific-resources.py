#!/usr/bin/env python3
"""Read Xtensa ELF types, sections and own frames; never flash or infer stack peaks."""
import argparse, hashlib, json, re, subprocess
from pathlib import Path
from elftools.elf.elffile import ELFFile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--elf', type=Path, required=True)
p.add_argument('--image', type=Path, required=True)
p.add_argument('--board', type=Path, required=True)
p.add_argument('--toolchain', type=Path, default=Path('C:/.platformio/packages/toolchain-xtensa-esp32s3/bin'))
p.add_argument('--out', type=Path, required=True)
a = p.parse_args()
a.out.mkdir(parents=True, exist_ok=True)
identity = lambda path: dict(path=str(path), bytes=path.stat().st_size, sha256=hashlib.sha256(path.read_bytes()).hexdigest())
names = {'Value', 'Payload', 'Uncertainty', 'Contribution', 'HistoryEntry', 'CalculationEvaluation', 'CalculationApp', 'NodeMeasurement', 'NodeQuantityReference', 'Reference', 'Constant', 'ConstantRecord', 'SourceId', 'Display'}
types = {}
with a.elf.open('rb') as f:
    elf = ELFFile(f)
    assert 'Xtensa' in elf.get_machine_arch(), elf.get_machine_arch()
    sections = {s.name: s['sh_size'] for s in elf.iter_sections()}
    def walk(die, scope=()):
        attr = die.attributes
        raw = attr.get('DW_AT_name')
        name = raw.value.decode(errors='replace') if raw and isinstance(raw.value, bytes) else ''
        if name in names and die.tag in ('DW_TAG_structure_type', 'DW_TAG_class_type', 'DW_TAG_enumeration_type') and 'DW_AT_byte_size' in attr:
            qualified = '::'.join((*scope, name))
            size = attr['DW_AT_byte_size'].value
            if qualified in types:
                assert types[qualified] == size, (qualified, types[qualified], size)
            types[qualified] = size
        child_scope = (*scope, name) if name and die.tag in ('DW_TAG_namespace', 'DW_TAG_structure_type', 'DW_TAG_class_type') else scope
        for child in die.iter_children():
            walk(child, child_scope)
    # Only these production units own the requested structures. Traversing all
    # Giac DIEs materializes millions of unrelated debug objects on the host.
    owned_units={'Quantity.cpp','MathAST.cpp','CalculationEngine.cpp',
                 'CalculationApp.cpp','PhysicalConstants.cpp','ScientificUncertainty.cpp'}
    scanned_units=[]
    for cu in elf.get_dwarf_info().iter_CUs():
        top=cu.get_top_DIE();attribute=top.attributes.get('DW_AT_name')
        filename=attribute.value.decode(errors='replace') if attribute else ''
        if filename.replace('\\','/').rsplit('/',1)[-1] not in owned_units:
            continue
        scanned_units.append(filename);walk(top)
suffix = '.exe' if (a.toolchain / 'xtensa-esp32s3-elf-nm.exe').exists() else ''
def tool(name, *args):
    return subprocess.check_output([str(a.toolchain / ('xtensa-esp32s3-elf-' + name + suffix)), *map(str, args)], text=True)
selected = []
for line in tool('nm', '--defined-only', a.elf).splitlines():
    fields = line.split()
    if len(fields) != 3 or fields[1] not in ('t', 'T', 'w', 'W'):
        continue
    symbol = fields[2]
    if not any(n in symbol.lower() for n in ('uncertainty', 'measurement', 'physicalconstant', 'sourceid', 'quantity',
                                            'drawhelp', 'inspectoptionalsimplify', 'skipoptionalsimplify')):
        continue
    selected.append((int(fields[0],16),symbol))
# Disassemble the ELF once, then key prologues by function start address. This
# also handles aliases for which objdump displays a different symbol name.
addresses={address for address,_ in selected};frame_by_address={};current=None
for line in tool('objdump','-d',a.elf).splitlines():
    header=re.match(r'^([0-9a-fA-F]+) <[^>]+>:$',line)
    if header:
        address=int(header[1],16);current=address if address in addresses else None
    elif current is not None:
        entry=re.search(r'\bentry\s+a1,\s*(0x[0-9a-fA-F]+|\d+)',line)
        if entry:frame_by_address[current]=int(entry[1],0);current=None
demangled=subprocess.check_output([str(a.toolchain/('xtensa-esp32s3-elf-c++filt'+suffix))],
    input='\n'.join(symbol for _,symbol in selected)+'\n',text=True).splitlines()
assert len(demangled)==len(selected)
frames=[dict(symbol=symbol,name=name,ownFrameBytes=frame_by_address.get(address))
        for (address,symbol),name in zip(selected,demangled)]
app_bytes = json.loads(a.board.read_text())['upload']['maximum_size']
record = dict(scope='local build only; no hardware execution or flash', elf=identity(a.elf), image=identity(a.image), board=identity(a.board), appPartitionBytes=app_bytes, appMarginBytes=app_bytes-a.image.stat().st_size, marginTargetBytes=131072, marginPass=app_bytes-a.image.stat().st_size >= 131072,
              staticRam=sum(sections.get(n, 0) for n in ('.dram0.data', '.dram0.bss', '.noinit')),
              iramText=sections.get('.iram0.text'), linkedFlash=sum(sections.get(n, 0) for n in ('.iram0.text', '.iram0.vectors', '.dram0.data', '.flash.text', '.flash.rodata')),
              types=types, dwarfCompilationUnits=scanned_units, sections=sections, ownStackFrames=sorted(frames, key=lambda r:r['ownFrameBytes'] or 0, reverse=True),
              limits=['Individual Xtensa frames do not measure recursive call-chain peak.', 'Static RAM and sizeof do not measure live PSRAM/heap fragmentation.', 'History occupancy and full-history allocations need runtime probes.'])
(a.out / 'resources.json').write_text(json.dumps(record, indent=2), encoding='utf8')
print(json.dumps({k:v for k,v in record.items() if k not in ('sections', 'ownStackFrames')}, indent=2))
