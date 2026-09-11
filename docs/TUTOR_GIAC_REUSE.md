# TUTOR-ENGINE-01: vendored Giac reuse decision

Use Giac's parser, exact arithmetic, symbolic normalization and ordinary answer
service through `GiacEngine`. Build independent typed teaching rules and a
rule-specific checker. **Do not use Giac console output as the derivation.** No
third-party tutor code or language resources were copied.

## Executable evidence

The probe links only vendored Giac/LibTomMath host objects and existing host
stubs, with a separate probe main. Reproduce from a fresh checkout with:

```sh
GIAC_HOST_OUT=out/tutor-engine-01/probe-cache bash scripts/build-giac-host-harness.sh --build-only
powershell -File scripts/tutor-probe-giac.ps1 -ObjectDirectory out/tutor-engine-01/probe-cache/obj
```

The Windows probe script also accepts the original investigation cache as its
default. It rejects stale vendor objects. It never calls the production
singleton or modifies production sources.

Evidence is in `out/tutor-engine-01/review-math/`: `giac-step-probe.jsonl`,
`summary.json`, `compiled-macros.txt`, and `vendor-hashes.json`. The baseline
source snapshot fingerprint supplied for this task was
`a0176658aca637b864da0418595223ff85b6ee691b3553766d277bb0de3ccdf6`.

The 252 runs cover 14 cases × levels 0/1/2 × context language values 0/1/3 ×
ordinary logging/structured callback capture. Each case gets a fresh isolated
context. RAII restores every deliberately changed flag, log destination,
`cout`/`cerr` stream buffer and process-global `my_gprintf` callback. The fresh
context is destroyed after each run, disposing parser-local state too. All
252 restoration checks passed; no production variable, assumption or Grapher
handle was involved. Locale comparison here concerns context language codes,
not arbitrary host OS locale installations.

| Input / command family | Exact result | Step text / callback events |
| --- | --- | --- |
| `solve(x=1,x)` | `[1]` | none |
| `solve(3*x+5=20,x)` | `[5]` | none |
| `solve(x^2-5*x+6=0,x)` | `[2,3]` | none |
| `solve((x^2-1)/(x-1)=0,x)` | `[-1]` | none |
| `solve(x/x=1,x)` | `[x]` | none; exclusion absent |
| `linsolve([x+y=3,x-y=1],[x,y])` | `[2,1]` | none |
| `solve(sqrt(x+1)=x-1,x)` | `[3]` | none |
| `solve(ln(x-1)=0,x)` | `[2]` | none |
| `rref([[1,1,3],[1,-1,1]])` | reduced augmented matrix | none |
| `diff(sin(x)*exp(x),x)` | product-rule derivative | none |
| `integrate(x*exp(x),x)` | `(x-1)*exp(x)` | none |
| `printf(...)`, `tabvar(x^2,x)` | unevaluated calls | unavailable profile |
| direct C++ `gprintf` positive control | `1` | text OR format + typed `gen` vector |

Answers and mathematical output were identical across tested context language
codes. The positive control confirms that zero solver events are not a broken
capture harness. The output is not a structured pedagogical proof: the useful
callback, where called, supplies an unsigned presentation flag, an arbitrary
format string and borrowed `vecteur` arguments. It has no stable mathematical
rule ID, preconditions, equation-state relation, branch completeness or domain
exclusion protocol. It is process global, so retaining it in production would
also introduce lifetime/reentrancy obligations.

## Exact source/profile findings

The vendor declares **Giac 1.4.9-57**, NumOS package
`1.4.9+khicas.57`; provenance uncertainties are already recorded in
`lib/giac/NUMOS_CHANGES.md`. The probe matches the firmware's macro profile:
`HAVE_CONFIG_H IN_GIAC GIAC_KHICAS NO_GUI GIAC_GENERIC EMBEDDED
USE_GMP_REPLACEMENTS UMAP DOUBLEVAL`. It uses the same exception-enabled vendor
build despite the internal `NO_STDEXCEPT` configuration. `NO_RTTI` and the
LibTomMath backend are enabled. `WITH_TABVAR`, `HAVE_GETTEXT`, `EMCC` and
`GIAC_HAS_STO_38` are absent in this native embedded-profile probe.

* `ksolve.cc` has no `gprintf`/`step_infolevel` call sites.
* `kderive.cc` retains `do_step` local flags without emitting the derivative
  traces in this snapshot; `kintg.cc` has no `gprintf` calls.
* `kvecteur.cc:3161` changes the small-matrix algorithm when step output is
  enabled; `rref_reduce` accepts `step_rref` without emitting an operation
  trace. Verbosity can therefore change computational strategy even when it
  produces no explanation.
* `kglobal.cc:2439` implements `gprintf`; the default backend interpolates
  operands into text. Its `EMCC` branch can use mixed text/MathML, but this
  does not create rule semantics or restore missing solver call sites.
* `kmisc.cc` guards most function-study/printf facilities with `WITH_TABVAR`.
* `giacintl.h` uses identity `gettext` in this embedded profile. Language
  flags do not provide a complete multilingual teaching catalog.
* The parser preserves `x/x=1`; evaluation immediately changes it to `1=1`.
  Raw solve returns `[x]`, losing the authored `x != 0` requirement. Original
  denominator analysis must precede evaluation. Solver return values alone
  cannot reconstruct domain restrictions or a derivation.

The follow-up direct-call probe also found a concrete boundary precondition:
`giac::solve` on the raw parsed AST for `1/(x-1)=2/(x+2)` did not finish within
two seconds, while solving the evaluated AST returned `[4]` in approximately
0.5 ms. Both ASTs print identically. The public textual `solve(...)` wrapper
already evaluates its arguments. `ordinary-direct-solve-comparison.json`
retains this evidence; `tutor_probe_giac.exe --case direct_raw_solve ...` and
`direct_evaluated_solve` expose the comparison. Any boundary correction must
retain the raw authored AST for candidate-domain checks before solving its
canonical copy; evaluation can erase exclusions.

## Selective external comparison and licenses

The [official Xcas algorithm documentation](https://www-fourier.univ-grenoble-alpes.fr/~parisse/giac/doc/fr/algo.html)
describes `step_infolevel` for some intermediate calculations. That describes
the public facility, not guaranteed coverage in this embedded snapshot.

[google/mathsteps](https://github.com/google/mathsteps/wiki/How-to-use-mathsteps)
uses before/after nodes, change types, changed-node groups and nested substeps.
That is useful architectural comparison; the runtime is JS/mathjs and is not
ported. Its [component license](https://github.com/google/mathsteps/blob/master/LICENSE)
is Apache-2.0. No code or content was imported.

[GeoGebra solver-engine's methods model](https://github.com/geogebra/solver-engine/blob/main/docs/methods.md)
separates primitive rules, sequential plans and task sets, with expression
paths for provenance. The Kotlin/JVM/Spring stack is not appropriate for this
ESP32 runtime and is not ported. Its exact
[repository license](https://github.com/geogebra/solver-engine/blob/main/LICENSE.md)
states GPL-3.0-or-later for source and CC-BY-NC-SA-3.0-or-later for language
files. The broader GeoGebra license page has since changed terms for its
general product; the component license is the relevant inspected evidence
here. No GeoGebra language files, prose or code were copied. NumOS explanations
must be independently authored semantic catalogs.

Existing vendored Giac reuse continues under GPL-3.0-or-later and the existing
`LICENSE-SOFTWARE` / `lib/giac/NUMOS_CHANGES.md` notices.
