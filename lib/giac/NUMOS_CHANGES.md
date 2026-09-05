# Giac/KhiCAS provenance and NumOS changes

This directory is a modified vendored snapshot of Giac/KhiCAS. It is not
upstream Giac 1.9.0, as the former `library.json` value suggested. Embedded
metadata in `src/config.h` reports Giac 1.4.9 (`VERSION` is `1.4.9-57`).

## Best-established upstream

The closest public source located by this audit is the KhiCAS
`ti-ce-giac` repository at commit
`8d24f392f3edcb4fbf44b11325e92ca37edee470` (2026-02-24, "revert to giac
printing for double"). At NumOS import commit `a52832ed` (2026-04-09), 144
paths were shared with that snapshot and 126 were byte-identical. Eighteen
shared files already contained differences and nine compatibility files had
no counterpart. The public snapshot is therefore the strongest reproducible
baseline found, not a claim of exact archive identity.

The precise download URL, original archive checksum, and authorship/date of
the 18 pre-import changes are not recoverable from this repository's history.
Those facts remain explicitly unresolved; no version or author was guessed.

Giac and these modifications are distributed under GPL-3.0-or-later. The
complete GPLv3 text is at [`../../LICENSE-SOFTWARE`](../../LICENSE-SOFTWARE).
Modified upstream files carry a short notice referring here. Files added for
the port carry an equivalent added-file notice.

## File classification

Compared with the baseline above, these 18 shared files were already modified
when the full tree entered NumOS on 2026-04-09:

`config.h`, `debug.h`, `first.h`, `gen.h`, `giacPCH.h`, `global.h`,
`gmp_replacements.h`, `identificateur.h`, `input_lexer.cc`, `kgen.cc`,
`kglobal.cc`, `kidentificateur.cc`, `kifactor.cc`, `kmaple.cc`, `memmgr.h`,
`monomial.h`, `myostream.h`, and `poly.h`.

These 17 shared files were byte-identical at import and first acquired
functional NumOS differences after import:

`input_lexer.h`, `input_parser.cc`, `kgausspol.cc`, `kintg.cc`, `kintgab.cc`,
`kmisc.cc`, `kmodfactor.cc`, `kmodpoly.cc`, `kplot.cc`, `kprog.cc`, `krpn.cc`,
`kseries.cc`, `ksolve.cc`, `ksubst.cc`, `kthreaded.cc`, `kusual.cc`, and
`usual.h`.

Eight of the 18 already-divergent files were also changed again after import:

`first.h`, `gen.h`, `global.h`, `input_lexer.cc`, `kgen.cc`, `kglobal.cc`,
`kidentificateur.cc`, and `kmaple.cc`.

These nine port/integration files have no path in the `ti-ce-giac` snapshot:

`console.h`, `k_csdk.h`, `keypadc.h`, `main.h`, `menuGUI.h`,
`platform_stubs.cpp`, `sys/rtc.h`, `textGUI.h`, and `umap.h`.

"Local-only" here means absent from `ti-ce-giac`; it is not an authorship
claim. Five shims (`console.h`, `k_csdk.h`, `main.h`, `menuGUI.h`, and
`textGUI.h`) implement interfaces also exposed by the sibling KhiCAS
`ti-ce` front end. They are not byte copies of those files, but their API
names and compatibility purpose derive from that integration surface. The
other four are NumOS-side platform/build shims. All nine are treated
conservatively as GPL-3.0-or-later integration code.

The classifications describe functional differences before the uniform 2026
licensing notices were added. They are disjoint by state at import: 18
already-divergent shared files, 17 shared files that first diverged later,
and nine paths absent from `ti-ce-giac`. The eight-file list above records
later work inside the first category and therefore is intentionally not a
fourth disjoint category.

Generated `input_lexer.cc` and `input_parser.cc` are authoritative vendored
build inputs here and contain NumOS changes. Their referenced
`input_lexer.ll` and `input_parser.yy` inputs are absent both from this vendor
snapshot and from the identified `ti-ce-giac` baseline; this repository has
no Flex/Bison regeneration workflow for them. Replacing either generated
file from another source would require reapplying both the port changes and
the top-of-file modification notice. The `.bak` files are unmodified
baseline artifacts and are not used as regeneration inputs.

## Change history reconstructed from Git

| NumOS commit | Date | Relevant change |
| --- | --- | --- |
| `070630be` | 2026-04-07 | Initial Giac integration metadata and `umap.h`. |
| `a52832ed` | 2026-04-09 | Full vendored tree imported with port work already present. |
| `0aa2ef3b` | 2026-04-09 | First portability, plotting, program, substitution, and stub changes. |
| `d5399b1a` | 2026-04-09 | ESP32 compile port and build integration. |
| `ad5cdb3e` | 2026-04-10 | `kgen.cc` factor-related changes. |
| `13f5aaca` | 2026-04-10 | Lexer, global/program, and platform-stub changes. |
| `3b04f2aa` | 2026-04-11 | Algebra, calculus, factorization, series, and solve porting. |
| `b479e79d` | 2026-04-12 | Infinity/constants and limits/series work. |
| `321aac87` | 2026-04-12 | Geometry/limits work and LibTomMath integration. |
| `f4157aa0` | 2026-04-12 | Further series/limit handling. |
| `a39a887b` | 2026-04-14 | Final main port pass: value representation, parser, integration, infinity, RPN, and solve changes. |
| `43296361` | 2026-07-13 | Native build seam and `extra_script.py`. |
| `36cb67eb` | 2026-07-24 | SDL2/WebAssembly integration. |

The functional changes fall into five broad groups: ESP32/native portability
and stubs; `DOUBLEVAL`/pointer representation; lexer/parser treatment of
infinity; symbolic limits, integration, summation, solve, and factorization;
and PlatformIO/native build integration. Consult `git show <commit> --
lib/giac` for the authoritative patch at each step.

## Reproducing the comparison

```sh
git clone https://github.com/KhiCAS/ti-ce-giac.git
git -C ti-ce-giac checkout 8d24f392f3edcb4fbf44b11325e92ca37edee470
git diff --no-index -- ti-ce-giac lib/giac/src
git log --follow -- lib/giac/src/<file>
```

Directory-wide diffs include upstream files intentionally omitted from the
embedded subset. Compare shared paths or blob hashes when reproducing the
126-of-144 import result.
