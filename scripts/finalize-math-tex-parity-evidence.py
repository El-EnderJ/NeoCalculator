#!/usr/bin/env python3
"""Read-only source audit and font-free immutable review snapshot (no Git writes)."""
import argparse
import ast
import hashlib
import json
from pathlib import Path
import subprocess

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--label', required=True)
    args = parser.parse_args()
    if not args.label.replace('-', '').isalnum():
        parser.error('label must contain only letters, digits and hyphens')
    root = Path(__file__).resolve().parents[1]
    evidence = root / 'out/math-tex-parity-02'
    target = evidence / 'Z-final' / args.label
    if target.exists():
        parser.error('immutable snapshot already exists: choose a fresh label')
    received = json.loads((evidence / 'A-received/manifest.json').read_text(encoding='utf-8'))
    # Receipt mixes relative source paths with absolute archived runtime paths.
    # Keep runtime provenance separate: an absolute operand would otherwise
    # escape the destination snapshot and produce an invalid manifest copy URI.
    receivedRuntime = []
    before = {}
    runtimeRoot = (evidence / 'A-received/runtime').resolve()
    for entry in received['files']:
        path = Path(entry['path'])
        if path.is_absolute():
            assert path.resolve().is_relative_to(runtimeRoot), entry['path']
            assert digest(path) == entry['sha256'], entry['path']
            receivedRuntime.append(dict(path=path.relative_to(root).as_posix(),
                                        bytes=path.stat().st_size, sha256=entry['sha256']))
        else:
            assert (root / path).resolve().is_relative_to(root.resolve()), entry['path']
            before[entry['path']] = entry
    receiptStatus = (evidence/'A-received/status.txt').read_text(encoding='utf-8')
    supplemented = []
    # The initial font exclusion also omitted assets/fonts license records.
    # Those tracked, clean-at-receipt objects still have an immutable baseline
    # in the saved index. Restore checkout filters when hashing text bytes.
    for line in (evidence/'A-received/index-files.txt').read_text(encoding='utf-8').splitlines():
        metadata, name = line.split('\t', 1)
        if name.startswith('"'):
            name = ast.literal_eval(name).encode('latin1').decode('utf-8')
        if name in before:
            continue
        assert name not in receiptStatus, 'Missing modified received baseline: ' + name
        oid = metadata.split()[1]
        data = subprocess.check_output(['git', 'cat-file', '--filters', '--path='+name, oid], cwd=root)
        before[name] = dict(path=name, bytes=len(data), sha256=hashlib.sha256(data).hexdigest(),
                            receivedIndexObject=oid)
        supplemented.append(dict(path=name, receivedIndexObject=oid))
    paths = set(subprocess.check_output(
        ['git', 'ls-files', '-z', '--cached', '--others', '--exclude-standard'], cwd=root
    ).decode('utf-8').split('\0')) - {''}
    paths.add('.vscode/settings.json')
    records, changes = [], []
    for name in sorted(paths | before.keys()):
        path = root / name
        old = before.get(name)
        if not path.is_file():
            if old:
                changes.append(dict(path=name, change='DELETED', before=old['sha256'], after=None))
            continue
        data = path.read_bytes()
        sha = hashlib.sha256(data).hexdigest()
        entry = dict(path=name, bytes=len(data), sha256=sha)
        if old and old.get('receivedIndexObject'):
            currentObject = subprocess.check_output(
                ['git', 'hash-object', '--path='+name, str(path)], cwd=root).decode().strip()
            entry['receivedIndexObject'] = old['receivedIndexObject']
            entry['rawReceivedHashUnavailable'] = True
            entry['gitContentUnchanged'] = currentObject == old['receivedIndexObject']
            if entry['gitContentUnchanged']:
                # Clean Git content can differ only in checkout line endings;
                # do not falsely list unchanged original font licenses as edits.
                old = dict(old, sha256=sha)
        # Font bitmap/outline data stays in the workspace; declarations and
        # scalar OpenType metadata are ordinary reviewable source files.
        font = (path.suffix.lower() in ('.ttf', '.otf', '.woff', '.woff2', '.bdf', '.fnt')
                or (name.startswith('src/fonts/') and path.suffix.lower() != '.h'))
        generated = name == 'compile_commands.json'
        if font or generated:
            entry['copied'] = False
            entry['reason'] = 'Font data: hash only' if font else 'Generated compile database: hash only'
        else:
            copy = target / 'snapshot' / name
            assert copy.resolve().is_relative_to((target / 'snapshot').resolve()), name
            copy.parent.mkdir(parents=True, exist_ok=True)
            # One read captures a consistent version even if a build log is
            # still open with Windows share-read but without CopyFile2 access.
            copy.write_bytes(data)
            assert digest(copy) == sha, name
            entry['copy'] = 'snapshot/' + name
        records.append(entry)
        if old and sha != old['sha256']:
            changes.append(dict(path=name, change='MODIFIED_SINCE_RECEIPT', before=old['sha256'], after=sha))
        elif not old and name != '.vscode/settings.json':
            changes.append(dict(path=name, change='ADDED_SINCE_RECEIPT', before=None, after=sha))
    protected = {
        '.vscode/settings.json': '00b9475d436d8c7bfa5b998dec3de2a8fe0c1caef7092597883599d60e395fd7',
        'docs/RESUMEN_CHAT_TOOLBOX.png': before['docs/RESUMEN_CHAT_TOOLBOX.png']['sha256'],
    }
    for name, expected in protected.items():
        assert digest(root/name) == expected, 'Protected file changed: ' + name
    index = subprocess.check_output(['git', 'diff', '--cached', '--binary'], cwd=root)
    assert index == (evidence/'A-received/index.patch').read_bytes(), 'Index changed'
    head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root).decode().strip()
    assert head == (evidence/'A-received/HEAD.txt').read_text().strip(), 'HEAD changed'
    manifest = dict(label=args.label, head=head, indexUnchanged=True, protected=protected,
                    readVerified=True, baselineSupplementedFromReceivedIndex=supplemented,
                    receivedRuntimeFilesVerified=receivedRuntime,
                    files=records, changesSinceReceived=changes)
    target.mkdir(parents=True, exist_ok=True)
    (target/'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    rows = ['# Exact changes relative to the complete received working tree', '',
            'Existing scientific changes are the baseline. This list is not a diff against HEAD.', '',
            '| Path | Status | Before SHA-256 | After SHA-256 |', '|---|---|---|---|']
    rows += ['| `{path}` | {change} | {before} | {after} |'.format(**item) for item in changes]
    (target/'changed-files.md').write_text('\n'.join(rows)+'\n', encoding='utf-8')
    print(json.dumps(dict(snapshot=str(target), files=len(records), changes=len(changes),
                          protectedUnchanged=True, indexUnchanged=True, headUnchanged=True)))

if __name__ == '__main__':
    main()
