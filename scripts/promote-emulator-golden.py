#!/usr/bin/env python3
# promote-emulator-golden.py — human-gated promotion of a REVIEWED candidate
# screenshot to an accepted golden, for the NumOS SDL2 emulator visual tests
# (Phase 4B-D).
#
# This tool ONLY does the byte-exact copy step of an otherwise human decision:
# it copies out/emulator-candidates/<stem>.ppm -> tests/emulator/golden/<stem>.ppm
# after a human has visually confirmed the candidate is correct. It NEVER
# generates images (that is generate-emulator-candidates.py), NEVER edits masks
# (those are authored by hand — see tests/emulator/masks/README.md), and NEVER
# commits for you. The git commit is the human acceptance record. CI never runs
# this script: a candidate can therefore never become a passing golden silently.
#
# Stdlib only, cross-platform (Windows + Linux). No image libraries. The copy is
# binary so the P6 header ("P6\n320 240\n255\n") and any 0x0A byte inside the RGB
# payload survive intact (a text-mode copy would corrupt the file on Windows).
#
# Usage:
#   python scripts/promote-emulator-golden.py --list-candidates
#   python scripts/promote-emulator-golden.py --dry-run calc_1_plus_2
#   python scripts/promote-emulator-golden.py calc_1_plus_2
#   python scripts/promote-emulator-golden.py --force calc_1_plus_2   # re-bless
#   python scripts/promote-emulator-golden.py calc_1_plus_2 calc_fraction_sum
#
# A name may be a bare stem (calc_1_plus_2), a .ppm filename, or a path ending in
# one; it is normalized to the stem. There is no implicit "promote all" — names
# are always explicit, so a bulk promotion can never happen by accident.
#
# Exit codes:
#   0  all requested candidates promoted (or --dry-run / --list-candidates done)
#   1  at least one refused because its golden already exists and --force was not
#      passed (nothing was overwritten)
#   2  invalid input / candidate missing / not a valid 320x240 PPM / copy error /
#      no names given

import argparse
import hashlib
import os
import shutil
import sys

# Same frame contract the candidate generator enforces
# (generate-emulator-candidates.py:32-33, :118-130): a P6 320x240 header is
# 15 bytes, so a full frame is exactly 15 + 320*240*3 = 230415 bytes.
EXPECTED_HEADER = b"P6\n320 240\n255\n"
EXPECTED_SIZE = 230415

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CANDIDATE_DIR = os.path.join(REPO_ROOT, "out", "emulator-candidates")
GOLDEN_DIR = os.path.join(REPO_ROOT, "tests", "emulator", "golden")
MASK_DIR = os.path.join(REPO_ROOT, "tests", "emulator", "masks")


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def validate(path):
    """Return (ok, message). Checks the P6 320x240 header and exact 230415-byte
    size, identical to generate-emulator-candidates.py:118-130, so a corrupt or
    wrong-size candidate can never be promoted into a golden."""
    try:
        size = os.path.getsize(path)
    except OSError as exc:
        return False, "missing: %s" % exc
    if size != EXPECTED_SIZE:
        return False, "size %d != expected %d" % (size, EXPECTED_SIZE)
    with open(path, "rb") as f:
        head = f.read(len(EXPECTED_HEADER))
    if head != EXPECTED_HEADER:
        return False, "header %r != expected P6 320x240" % head
    return True, "ok"


def rel(path):
    """Repo-relative, forward-slashed path for readable, copy-pasteable output."""
    try:
        return os.path.relpath(path, REPO_ROOT).replace(os.sep, "/")
    except ValueError:
        return path  # different drive on Windows; fall back to absolute


def stem_of(name):
    """Normalize a user-supplied candidate name/path to its bare stem.
    Accepts 'calc_1_plus_2', 'calc_1_plus_2.ppm', or any path ending in one."""
    base = os.path.basename(name.replace("\\", "/").rstrip("/"))
    return os.path.splitext(base)[0]


def compare_mode(stem):
    """Describe how CI will compare this stem's golden: masked (a mask exists for
    it) or byte-exact (no mask). This tool only STATS the mask; it never creates
    or edits one."""
    mask = os.path.join(MASK_DIR, stem + ".mask")
    if os.path.isfile(mask):
        return "masked compare (%s)" % rel(mask)
    return "exact byte compare (no mask)"


def list_candidates():
    """Print every candidate PPM with its sha256, whether a golden already exists,
    and how CI will compare it. Read-only."""
    if not os.path.isdir(CANDIDATE_DIR):
        print("No candidate directory yet: %s" % rel(CANDIDATE_DIR))
        print("Generate candidates first: python scripts/generate-emulator-candidates.py")
        return
    names = sorted(n for n in os.listdir(CANDIDATE_DIR) if n.endswith(".ppm"))
    if not names:
        print("No candidate .ppm files in %s" % rel(CANDIDATE_DIR))
        print("Generate candidates first: python scripts/generate-emulator-candidates.py")
        return
    print("Candidates in %s:" % rel(CANDIDATE_DIR))
    for n in names:
        stem = os.path.splitext(n)[0]
        cand = os.path.join(CANDIDATE_DIR, n)
        golden = os.path.join(GOLDEN_DIR, stem + ".ppm")
        gold_state = "golden EXISTS" if os.path.isfile(golden) else "no golden yet"
        ok, msg = validate(cand)
        sha = sha256_file(cand) if ok else "INVALID (%s)" % msg
        print("  %-18s sha256=%s" % (stem, sha))
        print("  %-18s   %s; %s" % ("", gold_state, compare_mode(stem)))


def promote_one(stem, force, dry_run):
    """Promote a single reviewed candidate -> golden. Returns an exit-class int:
    0 promoted (or would, under --dry-run); 1 refused (golden exists, no --force);
    2 candidate missing / invalid / copy error."""
    src = os.path.join(CANDIDATE_DIR, stem + ".ppm")
    dst = os.path.join(GOLDEN_DIR, stem + ".ppm")

    if not os.path.isfile(src):
        print("FAIL %-18s candidate not found: %s" % (stem, rel(src)), file=sys.stderr)
        print("     generate it first: python scripts/generate-emulator-candidates.py",
              file=sys.stderr)
        return 2

    ok, msg = validate(src)
    if not ok:
        print("FAIL %-18s candidate is not a valid P6 320x240 screenshot (%s)"
              % (stem, msg), file=sys.stderr)
        return 2

    exists = os.path.isfile(dst)
    if exists and not force:
        print("FAIL %-18s golden already exists: %s" % (stem, rel(dst)), file=sys.stderr)
        print("     re-bless deliberately with --force, and review the old-vs-new "
              "diff in your PR.", file=sys.stderr)
        return 1

    mode = compare_mode(stem)
    if dry_run:
        verb = "would re-bless" if exists else "would promote"
        print("DRY  %-18s %s -> %s" % (stem, rel(src), rel(dst)))
        print("     %-18s %s" % ("", verb))
        print("     %-18s sha256(candidate) = %s" % ("", sha256_file(src)))
        print("     %-18s CI will use: %s" % ("", mode))
        return 0

    try:
        os.makedirs(GOLDEN_DIR, exist_ok=True)
        shutil.copyfile(src, dst)  # binary, byte-exact (preserves the P6 payload)
    except OSError as exc:
        print("FAIL %-18s could not copy: %s" % (stem, exc), file=sys.stderr)
        return 2

    verb = "re-blessed" if exists else "promoted"
    print("OK   %-18s %s -> %s" % (stem, verb, rel(dst)))
    print("     %-18s sha256(golden) = %s" % ("", sha256_file(dst)))
    print("     %-18s CI will use: %s" % ("", mode))
    if "no mask" in mode:
        # Heads-up only — this tool NEVER creates a mask. If a screen has a
        # blinking cursor it will fail a byte-exact gate and needs a tight mask
        # authored by hand. See tests/emulator/masks/README.md.
        print("     %-18s note: no mask -> byte-exact gate. If this screen has a "
              "blinking cursor" % "")
        print("     %-18s       it needs a hand-authored mask "
              "(tests/emulator/masks/README.md)." % "")
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Promote a human-reviewed emulator candidate screenshot to an "
                    "accepted golden (NumOS Phase 4B-D)."
    )
    parser.add_argument(
        "names", nargs="*",
        help="one or more candidate stems (e.g. calc_1_plus_2), .ppm filenames, "
             "or paths. No implicit 'all' — names are always explicit.",
    )
    parser.add_argument(
        "--force", action="store_true",
        help="overwrite (re-bless) a golden that already exists",
    )
    parser.add_argument(
        "--dry-run", action="store_true",
        help="show exactly what would be promoted; write nothing",
    )
    parser.add_argument(
        "--list-candidates", action="store_true",
        help="list available candidates (stem, sha256, golden/mask state) and exit",
    )
    args = parser.parse_args(argv)

    if args.list_candidates:
        list_candidates()
        if not args.names:
            return 0
        print("")  # separator before promoting the named candidates

    if not args.names:
        parser.error("no candidate names given (pass at least one stem, or "
                     "--list-candidates)")  # argparse exits 2

    # De-duplicate while preserving order; normalize each to a bare stem.
    stems = []
    for raw in args.names:
        stem = stem_of(raw)
        if stem and stem not in stems:
            stems.append(stem)

    print("=== promote-emulator-golden (%s) ==="
          % ("DRY-RUN -- no files written" if args.dry_run else "writing goldens"))
    print("Only promote candidates you have VISUALLY reviewed and confirmed correct.")

    worst = 0
    promoted = 0
    for stem in stems:
        rc = promote_one(stem, args.force, args.dry_run)
        worst = max(worst, rc)
        if rc == 0 and not args.dry_run:
            promoted += 1

    if not args.dry_run:
        print("=== %d/%d promoted into %s ===" % (promoted, len(stems), rel(GOLDEN_DIR)))
        if promoted:
            print("This tool did NOT commit. Commit deliberately -- the commit is the "
                  "human acceptance record:")
            print("  git add tests/emulator/golden/   # plus any hand-authored mask")
            print("  git commit -m \"Accept golden: <stem> renders <value>\"")
    return worst


if __name__ == "__main__":
    sys.exit(main())
