# Licensing and provenance audit (2026)

Audit date: 2026-08-31
Repository baseline: `ddfa2fabef62ca9f08fc1e569f4454ff02c4ba2a`

This is an engineering provenance audit, not a legal opinion or a claim of
complete legal compliance. It records what was inspected, the evidence found,
the corrections made, and the facts that could not be established.

## 1. Scope

The audit covered tracked source and binary assets, Git history, generated font
artifacts and their generators, root licensing documentation, vendored Giac,
LibTomMath and Lua, embedded third-party notices, and material dependencies in
`platformio.ini`. Build caches and toolchains fetched outside the repository
were treated as external package-manager content.

Methods included filename/license searches, binary font metadata and SHA-256
inspection, per-path Git history, blob/hash comparison with official upstream
repositories or release archives, generated-output reproducibility checks, and
representative project builds.

## 2. External findings addressed

The review issues were substantiated:

- both bundled OFL fonts lacked adjacent full license/copyright files;
- generated font-derived C and MATH-table files lacked accurate font notices,
  and some instead appeared solely GPL-licensed;
- generated headers embedded a personal absolute Windows path and timestamp;
- Giac's manifest claimed an unsupported version and did not document its
  upstream baseline or the extensive local port changes;
- modified upstream Giac files did not carry a consistent change notice;
- the root summary implied a simpler GPL/software split than the actual
  multi-license distribution.

All have been corrected at the repository level. Remaining provenance limits
are stated rather than filled with assumptions.

An adversarial second pass then found and corrected additional defects:

- two words in `LICENSE-HARDWARE` differed from CERN's authoritative,
  expressly unmodified CERN-OHL-S-2.0 text;
- GL2PS's two referenced licence files and the BSD-3-Clause notice covering
  the vendored CLAPACK/f2c headers were absent;
- the Giac history wording omitted later changes to eight files that were
  already divergent at import, and its reproduction command used a nonexistent
  upstream `src/` directory;
- the generated lexer/parser regeneration limitation was not documented;
- the report incorrectly characterized the 2026-04-27 MIT-to-GPL declaration
  change as a mere clarification; and
- the Montserrat notice used the tag-v9 copyright line despite the local
  binary's exact source being unresolved, rather than the notice embedded in
  the binary itself; and
- the Spanish README still represented the project as MIT-licensed, while two
  other documents used stale GPL-3.0 shorthand.

## 3. Font findings

### Montserrat

`assets/fonts/Montserrat-Regular.ttf` has SHA-256
`ef9cf99f0175bef530b88934bd904fcf56f773cec6fd4dfb8ccaf7ce2bbd395e`,
embedded version 9.000, and the copyright “Copyright 2011 The Montserrat
Project Authors.” Git shows it entered in commit
`9ad3342346b2ae9e86a90c97229228a69cc22d48` on 2026-04-07 and has not changed.

The binary does not hash-match the static TTF in upstream tag `v9.000` at
commit `f1f4e9b630b6ac52f1c55c572b29d2a5c3535139`; that tag's static TTF has
SHA-256 `28eb076f1a16cd730ed6c8fe2390383bf85ef9353a798850bc4353191aaa5752`.
Repository history does not record the local binary's download URL or source
checksum. The exact pre-import artifact therefore remains unresolved. The
adjacent notice now uses the local binary's embedded 2011 copyright and OFL
grant plus the canonical OFL 1.1 text, rather than implying that the unmatched
tag artifact supplied this binary.

### STIX Two Math

`assets/fonts/STIXTwoMath-Regular.ttf` has SHA-256
`562551b15b836e6e01d1b7350909baf3c8c8d83260c1190fbf4544333e6936de`
and embedded version 2.12 b168a. It is byte-identical to
`ofl/stixtwomath/STIXTwoMath-Regular.ttf` in Google Fonts commit
`76d97cb481958f9bb0976a453f8126f8e2ea87ab`. Git shows it entered NumOS in
commit `64fa431443adbef78fb2f4aa48a294010b1bd8e3` on 2026-05-06 and has not
changed. The associated OFL notice reserves the name `TM Math`.

Full OFL texts are now in `assets/fonts/LICENSES/`, and
`assets/fonts/README.md` records hashes, history, exact/limited provenance, and
all derived outputs. LVGL generated C data is marked OFL-1.1. Generated MATH
headers contain both OFL-derived tables and GPL NumOS scaffolding and use
`GPL-3.0-or-later AND OFL-1.1`. In those headers, font-derived material is the
numeric MATH constants, codepoint/value records, variant advances, and assembly
connector data; NumOS material is the C++ types, bounded containers, lookup
code, namespace structure, and integration commentary. Possible
non-copyrightability of mechanical facts was not used to narrow the licence.
The LVGL outputs contain converted font data plus generic tool boilerplate, but
no independent NumOS implementation, and remain OFL-only. Generators now
produce those notices, repository-relative paths, and no timestamp.

## 4. Giac findings

`lib/giac/src/config.h` identifies Giac 1.4.9 / `1.4.9-57`; the previous
manifest value 1.9.0 was unsupported and has been corrected to
`1.4.9+khicas.57`.

The best public baseline found is KhiCAS `ti-ce-giac` commit
`8d24f392f3edcb4fbf44b11325e92ca37edee470` (2026-02-24). At the full NumOS
import commit `a52832ed`, 126 of 144 shared paths were byte-identical; 18 were
already modified, and nine integration paths were local-only. Seventeen paths
that were exact at import first diverged later. In addition, eight of the 18
already-divergent paths received further post-import changes; those overlapping
history facts are now listed separately instead of being hidden by the three
disjoint import-state categories. The precise original archive/download and
pre-import authorship of the 18 initial differences cannot be recovered.

`lib/giac/NUMOS_CHANGES.md` provides the complete path classifications,
commit-by-commit history, functional categories, limitations, and reproduction
commands. Every upstream file found to have functional NumOS differences now
has a GPL change notice pointing to that record; all nine local integration
files have an added-file variant. Five of those nine implement APIs also
exposed by the sibling KhiCAS `ti-ce` front end; "local-only" is therefore not
presented as an independent-authorship conclusion.

The top-of-file notices satisfy GPLv3 section 5(a)'s requirement for prominent
notice of modification and a relevant date: they say the file was modified for
NumOS in 2026, and the linked record gives the import date and later commit
dates. No upstream copyright or licence statement was removed. Generated
`input_lexer.cc` and `input_parser.cc` are included, but their referenced `.ll`
and `.yy` inputs are absent from both vendor trees and no regeneration workflow
exists here. The record now states that these `.cc` files are authoritative
vendored inputs and that replacement requires reapplying notices and changes.

## 5. Other third-party findings

- `lib/libtommath/` was mislabeled 1.2.0. All 170 shared paths at import were
  exact matches to upstream commit
  `652d70a31fcecc7beb7a01b3edef645039f54778` (`v1.3.0-416-g652d70a`). The
  manifest is corrected and the upstream LibTom license restored. The later
  NumOS `native_random_stubs.c` is separately marked GPL-3.0-or-later.
- `src/lua/` contains a Lua 5.4.7 subset. Fifty-nine core paths match the
  official archive whose SHA-256 is
  `9fbf5e28ef86c69858f6d3d34eccc32e911c1a28b4120ff3e84aaa70cfbf1e30`.
  All 63 pre-existing local paths, including the four test/manual paths, match
  official source tag `v5.4.7` exactly. Its MIT license is present as
  `src/lua/LICENSE`.
- TinyMT's BSD-3-Clause notice is preserved in
  `lib/giac/src/tinymt32_license.h`.
- `lib/giac/src/gl2ps.h` preserves Christophe Geuzaine's copyright and its
  LGPL-2.0-or-later/GL2PS alternative grant. The exact GL2PS 1.3.5
  `COPYING.LGPL` and `COPYING.GL2PS` texts are now adjacent. It is not enabled
  in the current embedded configuration.
- `f2c.h`, `clapack.h`, and `blaswrap.h` derive from the CLAPACK 3.2.1 CMAKE
  distribution and carry Giac-side adaptations. The release BSD-3-Clause text
  is now present as `lib/giac/CLAPACK-COPYING`; the exact pre-Giac
  transformation revision remains unknown.
- LVGL and TFT_eSPI are fetched through PlatformIO. Their declarations are not
  fully locked, so no single resolved-source hash is asserted. `lv_font_conv`
  is a build-time tool and is not vendored.

These boundaries and notices are consolidated in `THIRD_PARTY_NOTICES.md`.

## 6. Exact repository changes

- Added full font, LibTomMath, and Lua license texts.
- Restored CERN-OHL-S-2.0 to the authoritative unmodified wording and added
  the missing GL2PS/LGPL and CLAPACK licence texts.
- Added font provenance, Giac provenance/change history, third-party notices,
  and this audit report.
- Corrected Giac and LibTomMath manifest versions.
- Added modification notices to the 35 changed upstream Giac paths and added
  notices to nine port paths.
- Made all eight font-derived outputs deterministic and legally informative by
  fixing the generators, then regenerating them.
- Replaced personal absolute paths with repository-relative source paths.
- Clarified `LICENSE.md`, both language READMEs, and related licence summaries
  without relicensing any third-party component.
- Corrected the record of the project's own MIT-to-GPL declaration history;
  this documentation change itself does not attempt a new relicensing.

## 7. Remaining uncertainties

1. Montserrat's exact pre-import download artifact is unknown; its binary does
   not byte-match the nominal upstream v9.000 static TTF.
2. Giac's exact source package before import, and the authorship/date of the 18
   differences already present at import, are unknown.
3. The repository advertised MIT before the project owner added GPL-3.0-or-later
   notices on 2026-04-27. All pre-transition non-owner code commits are recorded
   under the `copilot-swe-agent[bot]` automation identity, and the README says
   the project owner guided its AI-assisted development. Git history alone
   cannot prove copyright ownership or a separate relicensing grant for
   AI-produced material. No outside human code author was found before the
   transition. Reviewer confirmation of the intended inbound/AI contribution
   rights is still appropriate.
4. Five local-only Giac compatibility shims implement interfaces found in the
   sibling KhiCAS `ti-ce` front end. They are not byte copies, and GPL treatment
   is conservative, but their exact design lineage is not fully reconstructable.
5. PlatformIO's ranged/unpinned dependency declarations do not identify one
   immutable LVGL/TFT_eSPI source resolution.
6. The exact pre-Giac revisions for the TinyMT and CLAPACK adaptations are not
   established, although their licence texts and proximate upstreams are now
   documented.

## 8. Validation performed

- Regenerated five LVGL font C files and three OpenType MATH headers twice;
  all eight SHA-256 values were identical across consecutive runs.
- Confirmed generated data changes are limited to provenance headers and
  repository-relative output paths; bitmap/table payloads did not change.
- Compared the GPL text in `LICENSE-SOFTWARE` with GNU's official GPLv3 text;
  it is identical after newline normalization.
- Compared the stored OFL and LibTom license texts with upstream copies (line
  ending/final-newline normalization only where byte hashes differ).
- Compared CERN-OHL-S-2.0, GL2PS, LGPL, CLAPACK, Lua, and TinyMT notices with
  their licensor/upstream sources. CERN-OHL-S-2.0, GL2PS, LGPL, and CLAPACK
  were restored as normalization-equivalent complete texts; the preserved
  TinyMT notice was checked for its complete BSD-3-Clause conditions without
  claiming an exact pre-Giac revision.
- Built `esp32s3_n16r8` successfully (117,568 bytes RAM, 5,354,117 bytes
  flash) and built `emulator_pc` successfully.
- Ran the Giac host harness successfully: engine 177/177, calculus 26/26,
  Neo backend 44/44, and cross-app 14/14 checks passed.
- Ran repository whitespace, absolute-path, SPDX, copyright, OFL, and Giac
  notice checks. The optional `reuse` command was not installed, so REUSE lint
  was not run.

## 9. Final licensing model

NumOS-authored software carrying the project notice is offered as
GPL-3.0-or-later. The repository's public declaration changed from MIT to GPL
on 2026-04-27; this audit does not call that transition a clarification and
records the contribution-history uncertainty above. Modified Giac/KhiCAS
remains GPL-3.0-or-later. Hardware design sources remain CERN-OHL-S-2.0. Font
software and font-derived data remain OFL-1.1. Lua, LibTomMath, TinyMT,
CLAPACK, GL2PS material, and externally resolved packages retain their
component licenses.

## 10. Commands and tests run

```text
bash scripts/generate_stix_math_font.sh
bash scripts/generate_montserrat_math_font.sh
python scripts/extract_stix_math.py
# repeated, with SHA-256 comparison of all eight outputs

git diff --check
rg (SPDX, OFL, font copyrights, Giac notices, stale headers, absolute paths)
pio run -e esp32s3_n16r8
pio run -e emulator_pc
scripts/build-giac-host-harness.sh
```

The host harness file had CRLF line endings in the checkout, so its contents
were newline-normalized in-flight into Bash and run from a fresh temporary
output directory; the tracked runner itself was not changed.
