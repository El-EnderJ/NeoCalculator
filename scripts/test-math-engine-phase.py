#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile and run tests/MathEnginePhaseRegression.cpp against a native build.

The regression is linked with exactly the math/font object files of the given
emulator_pc build directory (plus its LVGL/Giac/LibTomMath archives), so it
checks the same MathAST/MathTypography code the emulator ships. It is run by
scripts/math-tex-parity-harness.py and can be run on its own:

  python scripts/test-math-engine-phase.py --build C:/.piobuild/<dir>/emulator_pc \
      --out out/<task>/phase

Exit status is the regression's own status (0 = all checks passed).
"""
import argparse, hashlib, json, os, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--source', type=Path, default=ROOT, help='source tree that produced --build')
    p.add_argument('--build', type=Path, required=True, help='emulator_pc build directory')
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--compiler', default='g++')
    p.add_argument('--lvgl', type=Path, help='LVGL headers (default: <source>/.pio/libdeps/emulator_pc/lvgl, else repo copy)')
    a = p.parse_args()
    source, build, out = a.source.resolve(), a.build.resolve(), a.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    lvgl = a.lvgl or next((c for c in [source / '.pio/libdeps/emulator_pc/lvgl', ROOT / '.pio/libdeps/emulator_pc/lvgl'] if c.exists()), None)
    if lvgl is None:
        sys.exit('LVGL headers not found; pass --lvgl')
    flags = ['-std=c++17', '-ffunction-sections', '-fdata-sections', '-DNATIVE_SIM', '-DLV_CONF_INCLUDE_SIMPLE',
             '-DLV_USE_STDLIB_MALLOC=LV_STDLIB_CLIB', '-I.', '-Isrc', '-I' + str(lvgl.resolve())]
    env = dict(os.environ)
    env['PATH'] = 'C:/mingw64/bin;C:/SDL2/x86_64-w64-mingw32/bin;' + env.get('PATH', '')
    records = []

    def run(name, command, check=True):
        r = subprocess.run(command, cwd=source, env=env, capture_output=True)
        (out / (name + '.log')).write_bytes(r.stdout + r.stderr)
        records.append(dict(name=name, command=[str(c) for c in command], exit=r.returncode))
        (out / 'commands.json').write_text(json.dumps(records, indent=2))
        if check and r.returncode:
            sys.exit(f'{name} failed (exit {r.returncode}); see {out / (name + ".log")}')
        return r

    settings = out / 'settings.cpp'
    settings.write_text('bool setting_complex_enabled = true;\n')
    own = []
    for name, unit in [('settings', settings), ('MathStressExpressions', source / 'src/math/MathStressExpressions.cpp'),
                       ('phase', source / 'tests/MathEnginePhaseRegression.cpp')]:
        obj = out / (name + '.o')
        run(name + '-compile', [a.compiler, *flags, '-c', str(unit), '-o', str(obj)])
        own.append(obj)
    objects = [x for x in (build / 'src').rglob('*.o')
               if ('math' in x.parts or 'fonts' in x.parts or x.name == 'FileSystem.o') and x.name != 'MathStressExpressions.o']
    libs = [*build.rglob('liblvgl.a'), *build.rglob('libgiac.a'), *build.rglob('libtommath.a')]
    if len(libs) != 3:
        sys.exit(f'expected liblvgl/libgiac/libtommath in {build}, found {len(libs)}')
    rsp = out / 'phase.rsp'
    rsp.write_text('\n'.join('"' + x.as_posix() + '"' for x in [*own, *objects, *libs]))
    binary = out / 'phase.exe'
    run('phase-link', [a.compiler, '@' + str(rsp), '-Wl,--gc-sections', '-o', str(binary)])
    result = run('phase', [str(binary)], check=False)
    log = (result.stdout + result.stderr).decode(errors='replace')
    failures = [l[6:] for l in log.splitlines() if l.startswith('FAIL: ')]
    (out / 'objects.json').write_text(json.dumps(
        [dict(path=str(x), sha256=hashlib.sha256(x.read_bytes()).hexdigest()) for x in [*own, *objects]], indent=2))
    (out / 'summary.json').write_text(json.dumps(dict(exit=result.returncode, failures=failures), indent=2))
    print(f'MathEnginePhaseRegression: {"PASS" if result.returncode == 0 else "FAIL"} ({len(failures)} failed checks)')
    for f in failures:
        print('  FAIL', f)
    sys.exit(result.returncode)


if __name__ == '__main__':
    main()
