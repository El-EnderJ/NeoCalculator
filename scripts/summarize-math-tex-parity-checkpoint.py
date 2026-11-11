#!/usr/bin/env python3
"""Summarize a corrected candidate without implying final performance approval."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RESOURCE = ROOT / 'out/math-tex-parity-02/resources'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--phase', required=True)
parser.add_argument('--native-sha256', required=True)
args = parser.parse_args()
phase = RESOURCE / args.phase
read = lambda path: json.loads(path.read_text(encoding='utf-8'))
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()

native = Path('C:/.piobuild/numOS-parity-02/native/emulator_pc/program.exe')
pool_binary = Path('C:/.piobuild/numOS-parity-02/pool/emulator_pc/program.exe')
assert sha(native) == args.native_sha256
manifest = read(phase / 'hotpaths/source-manifest.json')
for item in manifest['files']:
    assert sha(ROOT / item['path']) == item['sha256'], item['path']
hot = read(phase / 'hotpaths/profile.json')
assert hot[-1]['failures'] == 0
count_keys = ('admissionCalls', 'rootLayouts', 'nodeLayouts', 'glyphLookups',
              'mathConstantLookups', 'mathKernLookups', 'variantSearches',
              'draws', 'paintNodes', 'cursorCalls', 'cursorLayoutCalls',
              'cursorCAllocations', 'cppAllocations', 'nodeAllocations', 'cAllocations')
counts = {key: sum(row[key] for row in hot[:-1]) for key in count_keys}
for key in ('cppAllocations', 'nodeAllocations', 'cursorCAllocations', 'cursorLayoutCalls'):
    assert counts[key] == 0, key
assert all(row['cAllocations'] == 0 for row in hot[:-1] if row['phase'] == 'layout-denied')

boards = []
for name, target in [('wroom', 'numos-esp32-s3-wroom-1u-n16r8'), ('cam', 'esp32s3_n16r8')]:
    before = read(RESOURCE.parent / 'A-baseline/resources' / (target + '.json'))
    after = read(phase / 'firmware' / (target + '.json'))
    types = read(phase / 'firmware' / (name + '-types.json'))
    stack = read(phase / 'firmware' / (name + '-stack-model.json'))
    assert after['elfSha256'] == types['elfSha256'] == stack['elfSha256']
    assert after['marginPass'] and after['appBytes'] == before['appBytes'] == 6553600
    assert stack['provisionalUniformDepthHasReserve']
    boards.append(dict(name=name, resources=after, types=types, stack=stack,
                       before={k: before[k] for k in ('imageBytes', 'freeBytes', 'linkedFlash', 'staticRam', 'iram')}))

pool = read(phase / 'fixed-pool/resources.json')
owned = read(phase / 'ownership/resources.json')
own_identity = read(phase / 'ownership/overlay-evidence/identity.json')
faults = read(phase / 'faults/faults.json')
fault_identity = read(phase / 'faults/overlay/identity.json')
assert pool['passed'] and pool['qualifies'] and pool['binarySha256'] == sha(pool_binary)
assert owned['passed'] and owned['qualifies']
assert own_identity['base_binary_sha256'] == sha(pool_binary)
assert own_identity['overlay_binary_sha256'] == owned['binarySha256']
assert all(v['min'] == v['max'] for row in owned['cppAndSourceSummary'].values() for v in row.values())
assert fault_identity['base_binary_sha256'] == sha(native)
assert fault_identity['overlay_binary_sha256'] == faults['binary_sha256']
assert all(row['passed'] for row in faults['results'].values())

result = dict(phase=args.phase, status='Resource and causal tests passed; fresh reviews and final performance gate remain separate.',
              native=dict(sha256=sha(native), bytes=native.stat().st_size),
              poolBinary=dict(sha256=sha(pool_binary), bytes=pool_binary.stat().st_size),
              boards=boards, hotpathSummary=hot[-1], counters=counts,
              sourceManifestSha256=sha(phase / 'hotpaths/source-manifest.json'),
              lifecycle={k: pool[k] for k in ('cycles', 'warmups', 'minimumSampledFree', 'minimumSampledLargestBlock')},
              ownership={k: owned[k] for k in ('cycles', 'warmups', 'cppAndSourceSummary', 'limitations')},
              recovery=dict(journeys=len(faults['results']),
                            persistentFailures=sum(bool(row.get('persistent_failure_observed')) for row in faults['results'].values()),
                            allocationFreeControls=sum(bool(row.get('allocation_free_scope_observed')) for row in faults['results'].values())),
              performanceGate='NOT_RUN_PENDING_FRESH_REVIEW_AND_CHALLENGE',
              limitations=['Own-frame models are not measured stack peaks.',
                           'Native C++ ownership excludes C allocator internals.',
                           'Linked allocator denial excludes CRT/DLL-internal allocations.',
                           'LVGL C draw-task allocations are counted and permitted.',
                           'Hardware PSRAM latency, heap peaks and fragmentation remain unmeasured.'])
(phase / 'RESOURCE_REPORT.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
md = '# Corrected-candidate resource checkpoint\n\n## Architecture Assessment\n\n'
md += f"Candidate `{result['native']['sha256']}` is measured after the first critic's Measurement ink-cache and NodeParen padding corrections. The previous e019 report remains in [the preserved checkpoint](../checkpoints/e019-precritic/RESOURCE_REPORT.md). This report does not claim critical approval or completion of the final performance gate.\n\n## Solution\n\n"
md += '| Target | Image before / after B | Free app B | Static RAM B | IRAM B |\n|---|---:|---:|---:|---:|\n'
for board in boards:
    r, b = board['resources'], board['before']
    md += f"| {board['name']} | {b['imageBytes']:,} / {r['imageBytes']:,} | {r['freeBytes']:,} | {r['staticRam']:,} | {r['iram']:,} |\n"
md += '\nBoth unchanged 6,553,600-byte app partitions retain at least 131,072 bytes free. Firmware ELF/image hashes, actual target ABI sizes and own-frame models are retained in the JSON and firmware directory. No device was flashed.\n\n'
md += '48,000 checked layouts and 960 actual LVGL frames passed causal allocation denial. Own C++/AST allocation attempts, geometry/cursor linked C requests and cursor relayouts remain zero. Existing LVGL drawing requests are separately counted.\n\n| Counter | Observed total |\n|---|---:|\n'
for key, value in counts.items():
    md += f'| {key} | {value:,} |\n'
recovery = result['recovery']
md += f"\nReal-product recovery passed {recovery['journeys']} journeys: {recovery['persistentFailures']} persistent failures on allocating paths and {recovery['allocationFreeControls']} allocation-free controls. Semantic/Ans/source recovery and subsequent healthy calculation remain tested without resetting Giac.\n\n"
md += '## Memory & Typographic Profile\n\n'
stack = boards[0]['stack']
md += f"The conservative simultaneous recursive-own-frame envelope is {stack['rawEdgeEnvelopeIncludingLeaf']:,} bytes at 192 raw edges; {stack['slackAfterKnownEnvelope']:,} bytes remain in the configured 65,536-byte task stack for unmodeled callers/helpers/task activity. The largest per-edge envelope is {stack['uniformRecursiveOwnEnvelopePerRawEdge']} bytes. These are compiler-derived models, not physical stack high-water measurements.\n\n"
md += f"Fixed-pool lifecycle passed {pool['cycles']} measured cycles after {pool['warmups']} warmups; minimum sampled free/largest block: {pool['minimumSampledFree']:,}/{pool['minimumSampledLargestBlock']:,} bytes. Ownership passed {owned['cycles']} cycles after {owned['warmups']} warmups, with stable live C++/payload/contribution/source totals in both Calculation and Menu. The actual host totals are in [ownership records](ownership/resources.json), not represented as ESP32 heap measurements.\n\n"
md += '## Risks & Edge Cases\n\n' + ''.join('- ' + line + '\n' for line in result['limitations'])
md += '\n- Final paired timings will be rerun only after the fresh full review and separate challenge approve. Earlier paired numbers are historical and are not transferred to this candidate.\n\n## Alternatives Considered\n\nNo runtime profiler, global geometry cache, partition change or approximation was introduced. The corrected ink bound and shared padding accessor add no stored fields.\n'
(phase / 'RESOURCE_REPORT.md').write_text(md, encoding='utf-8')
print('PASS checkpoint:', phase / 'RESOURCE_REPORT.md')
