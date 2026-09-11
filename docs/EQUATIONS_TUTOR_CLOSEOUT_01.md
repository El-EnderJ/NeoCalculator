# Equations + Tutor local closeout

The accepted implementation is organized into three dependency-coherent local
commits, with isolated gates before each commit:

| Boundary | Scope | Isolated verification |
|---|---|---|
| Storage | Production no-format mount, safe runtime retries, startup diagnostics and non-destructive tests | Actual-source fake/VFS tests; disposable image-reader tests; native, production-normal and CAM builds; demo contracts |
| Equations rebuild | Transactional editing, semantic physical input, Templates, paginated/domain-honest ordinary results, app-local BACK | Recorded rebuild snapshot plus storage ancestor; native/production builds; rebuild corpus and lifecycle; ordinary Giac/calculus/cross-app/context harness |
| Checked tutor and guided teaching | Bounded derivations/checker/planner, localization, guided projection/formula provenance, allocation recovery, ± metric fix, composite quadratic conclusion | Native/production builds; seeded/challenge/replay/mutations; context and allocation guards; teaching/navigation/locales/wide formulas; STIX/±; fixed 64 KiB pool lifecycle |

Engine and teaching stay together. Their recorded intermediate snapshot contains
the superseded compact Steps presentation and allocation paths corrected during
teaching review. Separating a headless engine commit now would require speculative
hunk reconstruction. The combined commit preserves the final verified implementation,
including the narrow vendor LLP64 compatibility correction and its provenance note.

Snapshot construction uses the recorded source manifests and explicit overlays,
not an old clean HEAD in place of the uncommitted candidate. Proposed trees are
materialized into separate ASCII build directories with distinct PlatformIO
`build_dir` values. Only pinned dependency source is copied; object caches are not
shared between different snapshots. The index stages each reviewed tree's explicit
path delta. `.vscode/extensions.json` remains a separate pre-existing local change.

The [storage review](PRODUCTION_STORAGE_MOUNT_REVIEW.md) documents the installed
filesystem API, exact startup ownership, downstream behavior and test limits.
Failure keeps the launcher usable and emits diagnostics; no ordinary production
mount or later app retry formats the filesystem. Legacy CAM startup and explicit
factory/provisioning remain outside that ordinary-production policy. No physical
storage fault or formatting operation is used for closeout testing.

## Historical acceptance is source-specific

The Equations, engine, teaching and physical reports remain historical evidence
for their recorded snapshots. Their “no commit” or “not yet flashed” statements
describe those deliveries, not this subsequent organization task. They are not
rewritten to claim tests of a later HEAD.

The hardware-tested source fingerprint is
`db792c9446727ba7e277d4d4656ab490006f2265e0092c1b00b7c03b2486cc04`.
Its ordinary production firmware was **5,484,160 bytes**, SHA-256
`d2c2df2bd16cf6f1442aa1c9858227b6f74ac5c1cd5401924762bb6ad150a67b`.
It was restored after the temporary measurement build. Ten mixed automated board
cycles, cache reuse/invalidation, board/native semantic agreement and emulator
visuals were recorded. Human confirmation covered only **x=1 and TOOLBOX**.

The measured cached quadratic final page takes about **394 ms** on the identified
probe build. This remains a known limitation; no performance optimization or new
equation method is part of closeout.

Tutor/equation mathematical and presentation source is preserved from that accepted
candidate. Closeout adds the separately reviewed storage changes and its tests/docs.
Git's line-ending normalization and omission of the unrelated editor overlay also
change whole-source fingerprints. Source equivalence is checked explicitly; new
commit metadata/build identity does **not** make the old physical image a test of
the new exact binary. No closeout firmware is flashed.

## Reproducibility and final ledger

Exact local commit/tree identities, source hashes, commands, dependency versions,
logs, build sizes and the final combined regression result are recorded in
[`out/equations-tutor-closeout-01`](../out/equations-tutor-closeout-01/).
Preservation includes separate index/working-tree binary patches and verified
copies/hashes of relevant untracked files. Physical evidence remains in
[`out/equations-tutor-physical-acceptance`](../out/equations-tutor-physical-acceptance/).
These ignored outputs, firmware, flash backups, snapshots and temporary probes
are not committed.

The affected combined matrix is run once against the final committed source:
Equations/tutor/teaching and failure recovery, fixed-pool lifecycle, ordinary
Giac/cross-app, Calculus/F7, STIX, Grapher Templates and app replays; native;
production normal/bring-up/demo; CAM normal/validation; web package/smoke;
headless WASM Release/Debug; storage and whitespace checks. Failures and rerun
rationales remain in the ledger. NOT RUN is never reported as PASS, and historical
golden drift remains distinct from semantic failures. No goldens are promoted.

No push, history rewrite, release tag, hardware flash, security provisioning,
physical storage operation or unrelated cleanup is authorized by this closeout.
