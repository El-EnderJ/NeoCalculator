#!/usr/bin/env python3
"""Non-destructive production startup tests, using actual source bodies.

Compiles the installed Arduino LittleFS::begin with a fake ESP VFS and compiles
the actual ordinary startup branch, settings codec/load/save and VariableManager.
No physical port is opened. Optional mklittlefs checks exercise its image reader,
not the ESP32 block driver. Generated bodies and hashes remain in ignored out/.
"""
import argparse, hashlib, json, os, pathlib, subprocess

ROOT = pathlib.Path(__file__).resolve().parents[2]
ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument('--framework', type=pathlib.Path, default=pathlib.Path('C:/.platformio/packages/framework-arduinoespressif32') if os.name == 'nt' else pathlib.Path.home()/'.platformio/packages/framework-arduinoespressif32')
ap.add_argument('--out', type=pathlib.Path, default=ROOT/'out/storage-startup')
ap.add_argument('--mklittlefs', type=pathlib.Path)
args = ap.parse_args(); out = args.out.resolve(); out.mkdir(parents=True, exist_ok=True)
api = args.framework/'libraries/LittleFS/src/LittleFS.cpp'
source = api.read_text(encoding='utf-8')

def function(text, signature):
    start = text.index(signature)
    end = text.index('\n}', start) + 2
    return text[start:end] + '\n'

pinned = function(source, 'bool LittleFSFS::begin(')
assert 'formatOnFail' in pinned and '.format_if_mount_failed = false' in pinned
(out/'pinned_begin.inc').write_text(pinned, encoding='utf-8')
(out/'LittleFS.h').write_text('#include "FS.h"\n')
system = (ROOT/'src/SystemApp.cpp').read_text(encoding='utf-8')
begin = function(system, 'void SystemApp::begin()')
startup = begin[begin.index('    if (LittleFS.begin('):].rsplit('\n}', 1)[0]
(out/'startup.inc').write_text(startup, encoding='utf-8')
assert begin.index('_mainMenu.load();') < begin.index('LittleFS.begin(')
assert '->begin()' not in '\n'.join(line for line in begin.splitlines() if not line.lstrip().startswith('//'))
settings = (ROOT/'src/apps/SettingsApp.cpp').read_text(encoding='utf-8')
settings = settings.split('#if NUMOS_BOARD_PROD_WROOM1U_N16R8\nnamespace {', 1)[1].split('#elif defined(__EMSCRIPTEN__)', 1)[0]
(out/'settings.inc').write_text('namespace {' + settings, encoding='utf-8')
retry_counts = {}
for name, expected in [('CircuitCoreApp.cpp', 3), ('Fluid2DApp.cpp', 3), ('NeoLanguageApp.cpp', 2)]:
    text = (ROOT/'src/apps'/name).read_text(encoding='utf-8')
    assert 'LittleFS.begin(true)' not in text
    assert text.count('LittleFS.begin(false)') == expected
    retry_counts[name] = expected
fakes = ROOT/'tests/host/fakes/storage'
relative = lambda p: os.path.relpath(p, ROOT).replace('\\', '/')
cmd = ['g++', '-std=c++17', '-Wall', '-Wextra', '-Wno-unused-parameter',
       '-DARDUINO=10819', '-DNUMOS_BOARD_PROD_WROOM1U_N16R8=1', '-DNUMOS_PRODUCTION_DEMO_PROFILE=0',
       '-I'+relative(out), '-I'+relative(fakes), '-Isrc', 'tests/host/production_storage_test.cpp',
       'src/math/VariableManager.cpp', '-o', relative(out/'storage-test.exe')]
subprocess.run(cmd, cwd=ROOT, check=True)
cases = ['valid', 'mount-fail', 'other-mount-error', 'unavailable-downstream',
         'empty-mounted', 'invalid-records', 'uninitialized-error', 'corrupt-error']
results = []
for case in cases:
    result = subprocess.run([str(out/'storage-test.exe'), case], cwd=ROOT, check=True, text=True, capture_output=True)
    print(result.stdout, end=''); results.append(dict(case=case, output=result.stdout))
paths = [api, ROOT/'src/SystemApp.cpp', ROOT/'src/apps/SettingsApp.cpp', ROOT/'src/math/VariableManager.cpp']
record = dict(command=cmd, source_hashes={str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},
              compiled_cases=results, static_retry_sites=retry_counts,
              limitations=['Fake ESP VFS error returns do not model real flash corruption.',
                           'Launcher readiness/order and app retry sites are static checks, not electrical or LCD tests.',
                           'No physical board or storage was opened.'])
image_tool = args.mklittlefs or args.framework.parent/'tool-mklittlefs'/('mklittlefs.exe' if os.name == 'nt' else 'mklittlefs')
record['image_reader'] = dict(status='NOT RUN', reason='installed mklittlefs unavailable')
if image_tool.is_file():
    images = out/'images'; images.mkdir(exist_ok=True)
    for name in ['input', 'empty']: (images/name).mkdir(exist_ok=True)
    (images/'input/sentinel').write_bytes(b'preserve-this-record')
    image_runs = []
    def image_run(arguments, success):
        result = subprocess.run([str(image_tool), *arguments], cwd=images, capture_output=True, text=True, timeout=20)
        assert (result.returncode == 0) == success, (arguments, result.stdout, result.stderr)
        image_runs.append(dict(arguments=arguments, exit=result.returncode, stdout=result.stdout, stderr=result.stderr))
    for name, directory in [('valid', 'input'), ('empty', 'empty')]:
        image_run(['-c', directory, '-s', '65536', name+'.img'], True)
    valid = (images/'valid.img').read_bytes()
    (images/'corrupt.img').write_bytes(bytes(8192) + valid[8192:])
    (images/'uninitialized.img').write_bytes(bytes([255])*65536)
    for name in ['valid', 'empty', 'corrupt', 'uninitialized']:
        path = images/(name+'.img'); before = path.read_bytes()
        image_run(['-l', '-s', '65536', path.name], name in ['valid', 'empty'])
        assert path.read_bytes() == before
    record['image_reader'] = dict(status='PASS', tool_sha256=hashlib.sha256(image_tool.read_bytes()).hexdigest(), runs=image_runs,
        scope='Host image-reader success/error and unchanged bytes only; not ESP32 mount or flash-fault coverage.')
(out/'results.json').write_text(json.dumps(record, indent=2)+'\n')
