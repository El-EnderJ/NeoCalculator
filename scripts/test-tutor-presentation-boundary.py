#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Replay typed formula/status signatures with matching Giac host objects.

Build the object closure with scripts/build-giac-host-harness.sh first. It must
match this source and the pinned toolchain; this runner recompiles GiacEngine.
--baseline-engine optionally adds a byte-for-byte differential replay. No board
access, repository mutation, dependency installation, or wall-clock test gate.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--objects', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--baseline-engine', type=Path)
    parser.add_argument('--compiler', default='g++')
    args = parser.parse_args()
    source, out = args.source.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    objects = sorted(p.resolve() for p in args.objects.glob('*.o')
                     if not p.name.endswith('_main.cpp.o') and p.name != 'GiacEngine.cpp.o')
    if not any(p.name == 'kgen.cc.o' for p in objects):
        parser.error('--objects must contain the matching Giac host closure')
    records = []

    def run(name, command):
        result = subprocess.run(command, cwd=source, capture_output=True)
        (out / (name + '.log')).write_bytes(result.stdout + result.stderr)
        records.append(dict(name=name, command=command, exit=result.returncode))
        (out / 'commands.json').write_text(json.dumps(records, indent=2), encoding='utf-8')
        result.check_returncode()
        return result.stdout

    common = ['-std=gnu++17', '-O1', '-ffunction-sections', '-fdata-sections',
              '-D_USE_MATH_DEFINES', '-DNUMOS_GIAC_HOST_HARNESS=1', '-Isrc']
    definitions = ['-D' + name for name in
                   'HAVE_CONFIG_H IN_GIAC GIAC_KHICAS NO_GUI GIAC_GENERIC EMBEDDED USE_GMP_REPLACEMENTS UMAP DOUBLEVAL'.split()]
    engine, unit = out / 'GiacEngine.cpp.o', out / 'boundary.o'
    run('engine', [args.compiler, *common, *definitions, '-Ilib/giac',
                   '-Ilib/giac/src', '-Ilib/libtommath', '-Wno-deprecated-declarations',
                   '-c', 'src/math/giac/GiacEngine.cpp', '-o', str(engine)])
    run('unit', [args.compiler, *common, '-c', 'tests/host/tutor_presentation_boundary.cpp',
                 '-o', str(unit)])
    results = {}
    variants = [('candidate', engine)]
    if args.baseline_engine:
        variants.insert(0, ('baseline', args.baseline_engine.resolve()))
    for name, implementation in variants:
        response = out / (name + '.rsp')
        response.write_text('\n'.join('"' + p.as_posix() + '"'
                                      for p in [*objects, implementation, unit]), encoding='utf-8')
        binary = out / (name + ('.exe' if os.name == 'nt' else ''))
        libraries = ['-static', '-lpsapi'] if os.name == 'nt' else []
        run(name + '-link', [args.compiler, '@' + str(response), '-Wl,--gc-sections',
                             *libraries, '-o', str(binary)])
        results[name] = run(name + '-replay', [str(binary)])
    if 'baseline' in results and results['baseline'] != results['candidate']:
        raise AssertionError('typed formula/status/context replay changed')
    report = dict(passed=True, cases=results['candidate'].count(b'FORMULA|'),
                  differential='baseline' in results,
                  signature_sha256=hashlib.sha256(results['candidate']).hexdigest(),
                  dependency_objects=[dict(path=str(p), sha256=hashlib.sha256(p.read_bytes()).hexdigest())
                                      for p in objects])
    (out / 'result.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print('PASS', report['cases'], 'formulas; differential:', report['differential'])


if __name__ == '__main__':
    main()
