#!/usr/bin/env python3
"""Locate and rehash retained private executables without running workloads."""
import argparse
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--phase', default='aftercritic1')
args = parser.parse_args()
phase = ROOT / 'out/math-tex-parity-02/resources' / args.phase
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
records = []
for name, cache_name, evidence_dir, result_path, result_key in (
    ('faults', 'resource-faults-', 'faults/overlay', 'faults/faults.json', 'binary_sha256'),
    ('ownership', 'resource-pool-ownership-', 'ownership/overlay-evidence', 'ownership/resources.json', 'binarySha256'),
):
    original = Path('C:/.codex-cache/numOS-parity-02') / (cache_name + args.phase) / 'overlay'
    retained = phase / evidence_dir
    identity = json.loads((retained / 'identity.json').read_text(encoding='utf-8'))
    result = json.loads((phase / result_path).read_text(encoding='utf-8'))
    exe = original / 'toolbox-probe.exe'
    assert exe.exists(), exe
    actual = sha(exe)
    assert actual == identity['overlay_binary_sha256'] == result[result_key]
    sources = []
    for filename, expected in identity['generated_sources'].items():
        for directory in (original, retained):
            path = directory / filename
            assert sha(path) == expected, path
        sources.append(dict(file=filename, sha256=expected, originalAndEvidenceMatch=True))
    records.append(dict(kind=name, executable=str(exe), bytes=exe.stat().st_size,
                        sha256=actual, recordedSha256=identity['overlay_binary_sha256'],
                        evidenceDirectory=str(retained), sourceHashes=sources,
                        baseBinarySha256=identity['base_binary_sha256']))
report = dict(verifiedAtUtc=datetime.now(timezone.utc).isoformat(),
              scope='Read-only hashes, no workload execution or historical-report rewrite.',
              cause='Evidence archival copied generated source, logs and JSON but excluded EXE/O files. The exact executables remained in the ASCII build cache.',
              records=records,
              limitation='These are local retained artifacts. No claim of independently rebuilding byte-identical PE files on another toolchain or of measured ESP32 runtime behavior.')
(phase / 'retained-executables.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
md = '# Retained private executables: supplemental verification\n\n'
md += 'The iteration-2 reviewer correctly reported that private EXEs were absent from the archived evidence directories. The exact executables were retained in the original ASCII build cache. The archive step omitted EXE/O files; neither helper deleted these binaries. Historical review reports are unchanged. This supplementary check rehashes both binaries against the original overlay identity and actual result, and verifies every generated source in both locations. It runs no workload.\n\n'
md += '| Scope | Actual retained executable | Bytes | SHA-256 |\n|---|---|---:|---|\n'
for row in records:
    md += f"| {row['kind']} | `{row['executable']}` | {row['bytes']:,} | `{row['sha256']}` |\n"
md += '\nRepeat verification with `python scripts/verify-math-tex-parity-retained-overlays.py --phase ' + args.phase + '`. Full source/hash reconciliation and verification UTC are in [retained-executables.json](retained-executables.json).\n\n'
md += 'To rerun a workload, pass the corresponding absolute executable to `scripts/test-scientific-faults.py --bin <exe> --out <new-directory> --help-c-allocator` or `scripts/test-scientific-lifecycle.py --bin <exe> --out <new-directory>`. The fault flag includes all four Help controls in addition to the 27 allocating-path cases. The ownership run additionally used `NUMOS_SCIENTIFIC_WARMUPS=54` and `NUMOS_SCIENTIFIC_REQUIRE_CPP_STABLE=1`; the ordinary fixed-pool run used four warmups. Keep reruns in new directories. To rebuild, use the source-preserving overlay builder with a matching compilation database and native/pool object tree; see the archived `build-overlay.py`, `identity.json` and compile/link logs. Use an ASCII build-output path, as in the original run. A rebuild may have different PE bytes because of toolchain or timestamp metadata and must produce its own identity instead of borrowing the previous hash.\n'
(phase / 'RETAINED_EXECUTABLES.md').write_text(md, encoding='utf-8')
print('PASS exact retained private executables:', len(records))
