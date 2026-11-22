#!/usr/bin/env python3
"""Record linked Xtensa resources and own frames (never a stack peak claim)."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
from elftools.elf.elffile import ELFFile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--out', type=Path, required=True)
a = p.parse_args()
a.out.mkdir(parents=True, exist_ok=True)
root = Path(__file__).resolve().parents[1]
tool = Path('C:/.platformio/packages/toolchain-xtensa-esp32s3/bin')
records = []
for target, board in [('numos-esp32-s3-wroom-1u-n16r8', 'numos-esp32-s3-wroom-1u-n16r8.json'),
                      ('esp32s3_n16r8', 'numos-esp32-s3-n16r8-cam.json')]:
    folder = a.build / target
    if not (folder / 'firmware.elf').exists():
        continue
    elf_path, image = folder / 'firmware.elf', folder / 'firmware.bin'
    with elf_path.open('rb') as stream:
        elf = ELFFile(stream)
        sections = {s.name: s['sh_size'] for s in elf.iter_sections()}
    def call(name, *args):
        return subprocess.check_output([str(tool/('xtensa-esp32s3-elf-'+name+'.exe')), *map(str,args)], text=True)
    symbols = []
    for line in call('nm', '--defined-only', elf_path).splitlines():
        fields = line.split()
        if len(fields) == 3 and fields[1] in ('t', 'T', 'w', 'W'):
            if any(x in fields[2] for x in ('calculateLayout', 'MathCanvas', 'DelimiterAssembler', 'RowLayout', 'MathKern')):
                symbols.append((int(fields[0],16),fields[2]))
    addresses = {address for address,_ in symbols}
    frames = {}
    current = None
    for line in call('objdump','-d',elf_path).splitlines():
        head = re.match(r'^([0-9a-fA-F]+) <[^>]+>:$',line)
        if head:
            address = int(head[1],16)
            current = address if address in addresses else None
        elif current is not None:
            entry = re.search(r'\bentry\s+a1,\s*(0x[0-9a-fA-F]+|\d+)',line)
            if entry:
                frames[current] = int(entry[1],0)
                current = None
    names = subprocess.check_output([str(tool/'xtensa-esp32s3-elf-c++filt.exe')],
        input='\n'.join(s for _,s in symbols)+'\n',text=True).splitlines()
    budget = json.loads((root/'boards'/board).read_text())['upload']['maximum_size']
    record = dict(target=target, imageBytes=image.stat().st_size,
        imageSha256=hashlib.sha256(image.read_bytes()).hexdigest(),
        elfSha256=hashlib.sha256(elf_path.read_bytes()).hexdigest(),
        appBytes=budget, freeBytes=budget-image.stat().st_size,
        marginPass=budget-image.stat().st_size>=131072,
        staticRam=sum(sections.get(n,0) for n in ('.dram0.data','.dram0.bss','.noinit')),
        iram=sections.get('.iram0.text',0),
        linkedFlash=sum(sections.get(n,0) for n in ('.iram0.text','.iram0.vectors','.dram0.data','.flash.text','.flash.rodata')),
        ownFrames=[dict(symbol=s,name=n,bytes=frames.get(address)) for (address,s),n in zip(symbols,names)],
        limitations=['Own frames do not establish recursive stack peak.', 'No hardware execution, PSRAM latency or live fragmentation measured.'])
    (a.out/(target+'.json')).write_text(json.dumps(record,indent=2),encoding='utf8')
    records.append({k:v for k,v in record.items() if k not in ('ownFrames','limitations')})
print(json.dumps(records,indent=2))
