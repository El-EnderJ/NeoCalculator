#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Readability guard for level-0 fractions on the Calculation surface.

Drives a native emulator with real key events, captures 320x240 frames and
measures glyph ink on pixels. The guard FAILS when, for any fraction at script
level 0, the tallest ink in its numerator or denominator is below 90 % of the
base digit ink height (TXP PD-4: numerator/denominator keep the base size at
script level 0).

  python scripts/test-math-readability-guard.py --bin <program.exe> --out <dir>

Base digit ink is calibrated per executable from a capture of "1234567890".
Ink = connected pixels with max(R,G,B) < 200 below the status bar. The editing
cursor is removed by capturing ten frames 8 frames apart (1.28 s, both phases
of the 500 ms blink) and keeping the lightest value of each pixel. A thin horizontal component is a fraction bar
only if a glyph lies directly above and below it (minus signs and the history
separator are not). Fractions inside scripts (X^(frac)) are excluded by case.
Not wired into CI.
"""
import argparse, json, os, statistics, subprocess, sys
from pathlib import Path

INK_THRESHOLD = 200
CURSOR_THRESHOLD = 128
STATUS_BAR_ROWS = 26
MIN_RATIO = 0.90

ALPHA = {'A': (4, 2), 'B': (3, 2), 'C': (2, 2), 'D': (1, 2), 'X': (4, 7)}


def keys(*items):
    lines = []
    for item in items:
        if item in ALPHA:
            r, c = ALPHA[item]
            lines += ['production_modifier alpha', f'calc_physical {r} {c}']
        else:
            lines.append('key ' + item)
    return lines


# name -> (key lines, level0 fractions expected, extra lines after entry)
CASES = {
    'calibration_digits': (keys(*'1234567890'), 0, []),
    'c1_1p2over3pow3': (keys('1', '+', 'frac', '2', 'down', '3', 'pow', '3'), 1, []),
    'c2_1over2': (keys('frac', '1', 'down', '2'), 1, []),
    'c3_1p1over2': (keys('1', '+', 'frac', '1', 'down', '2'), 1, []),
    'c4_ApB_over_CpD': (keys('frac', 'A', '+', 'B', 'down', 'C', '+', 'D'), 1, []),
    'c5_1over1p1overX': (keys('frac', '1', 'down', '1', '+', 'frac', '1', 'down', 'X'), 2, []),
    'c6_X_pow_frac': (keys('X', 'pow', 'frac', 'A', '+', 'B', 'down', 'C', '+', 'D'), 0, []),
    'c7_6663_over_2e-3': (keys('frac', '6', '6', '.', '6', '3', 'down', '2', 'mul', '1', '0', 'pow', 'neg', '3'), 1, []),
    'h1_history_1p2over3pow3': (keys('1', '+', 'frac', '2', 'down', '3', 'pow', '3', 'enter'), 2, ['wait 80']),
}


def components(path, threshold, mask=()):
    from PIL import Image
    im = Image.open(path).convert('RGB')
    w, h = im.size
    px = im.load()
    ink = [[False] * w for _ in range(h)]
    for y in range(STATUS_BAR_ROWS, h):
        for x in range(w):
            if max(px[x, y]) < threshold and not any(m[0] <= x < m[0] + m[2] and m[1] <= y < m[1] + m[3] for m in mask):
                ink[y][x] = True
    seen = [[False] * w for _ in range(h)]
    out = []
    for y in range(STATUS_BAR_ROWS, h):
        for x in range(w):
            if not ink[y][x] or seen[y][x]:
                continue
            stack, pts = [(x, y)], []
            seen[y][x] = True
            while stack:
                cx, cy = stack.pop()
                pts.append((cx, cy))
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        nx, ny = cx + dx, cy + dy
                        if STATUS_BAR_ROWS <= ny < h and 0 <= nx < w and ink[ny][nx] and not seen[ny][nx]:
                            seen[ny][nx] = True
                            stack.append((nx, ny))
            xs = [p[0] for p in pts]
            ys = [p[1] for p in pts]
            c = dict(x=min(xs), y=min(ys), w=max(xs) - min(xs) + 1, h=max(ys) - min(ys) + 1, n=len(pts))
            c['thin'] = c['h'] <= 2 and c['w'] >= 5
            c['solid'] = c['n'] >= 0.95 * c['w'] * c['h'] and c['w'] <= 3 and c['h'] >= 6
            out.append(c)
    return out


def analyse(path):
    cursor = [(c['x'], c['y'], c['w'], c['h']) for c in components(path, CURSOR_THRESHOLD) if c['solid']]
    comps = components(path, INK_THRESHOLD, cursor)
    thin = [c for c in comps if c['thin']]
    glyphs = [c for c in comps if not c['thin'] and c['n'] >= 3]

    def covers(bar, g):
        cx = g['x'] + g['w'] / 2
        return bar['x'] - 1 <= cx <= bar['x'] + bar['w']

    def nearest(bar, above):
        best = None
        for o in comps:
            if o is bar or not covers(bar, o):
                continue
            d = bar['y'] - (o['y'] + o['h'] - 1) if above else o['y'] - bar['y']
            if d > 0 and (best is None or d < best[0]):
                best = (d, o)
        return best

    bars = []
    for b in thin:
        up, down = nearest(b, True), nearest(b, False)
        if up and down and not up[1]['thin'] and not down[1]['thin'] and up[0] <= 10 and down[0] <= 10:
            bars.append(b)

    def region(bar, above):
        result = []
        for g in glyphs:
            if not covers(bar, g):
                continue
            top, bottom = g['y'], g['y'] + g['h'] - 1
            if above and bottom >= bar['y'] or not above and top <= bar['y']:
                continue
            # The nearest fraction bar between the glyph and this bar owns it.
            between = [o for o in bars if o is not bar and covers(o, g) and
                       ((above and bottom < o['y'] < bar['y']) or (not above and bar['y'] < o['y'] < top))]
            if not between:
                result.append(g)
        return result

    fractions = []
    for b in sorted(bars, key=lambda b: (b['y'], b['x'])):
        num, den = region(b, True), region(b, False)
        fractions.append(dict(bar=b, numeratorInk=max((g['h'] for g in num), default=0),
                              denominatorInk=max((g['h'] for g in den), default=0)))
    return dict(cursorMask=cursor, glyphs=glyphs, fractions=fractions)


SHOTS, SHOT_GAP_FRAMES = 10, 8


def run_case(binary, out, name, lines, extra, frames):
    script = out / (name + '.numos')
    shots = []
    for k in range(SHOTS):
        shots += [f'screenshot {name}_f{k}.ppm', f'wait {SHOT_GAP_FRAMES}']
    script.write_text('\n'.join([f'log "{name}: start"', 'wait 200', 'open_app Calculation', 'wait 60',
                                 'assert_app Calculation', *lines, *extra, 'wait 3', 'assert_calc_input dump',
                                 *shots, f'log "{name}: done"']) + '\n', newline='\n')
    env = dict(os.environ)
    env['PATH'] = os.pathsep.join([str(binary.parent), 'C:/msys64/mingw64/bin', 'C:/mingw64/bin',
                                   'C:/SDL2/x86_64-w64-mingw32/bin', env.get('PATH', '')])
    r = subprocess.run([str(binary), '--headless', '--deterministic', '--frames', str(frames), '--fs-sandbox',
                        '--script', script.name], cwd=out, env=env, capture_output=True)
    (out / (name + '.log')).write_bytes(r.stdout + r.stderr)
    frames_ppm = [out / f'{name}_f{k}.ppm' for k in range(SHOTS)]
    if r.returncode or not all(f.exists() for f in frames_ppm):
        raise RuntimeError(f'{name}: emulator exit {r.returncode}; see {out / (name + ".log")}')
    from PIL import Image, ImageChops
    images = [Image.open(f).convert('RGB') for f in frames_ppm]
    composite = images[0]
    for im in images[1:]:
        composite = ImageChops.lighter(composite, im)
    # Content below the status bar must change between frames only by the cursor.
    diff = ImageChops.difference(images[0], composite).crop((0, STATUS_BAR_ROWS, 320, 240)).convert('L')
    changed = diff.point(lambda v: 255 if v else 0).histogram()[255]
    images[0].save(out / (name + '_raw.png'))
    images[0].resize((1280, 960), Image.NEAREST).save(out / (name + '_raw_x4.png'))
    ppm = out / (name + '.ppm')
    composite.save(ppm)
    composite.save(out / (name + '.png'))
    composite.resize((1280, 960), Image.NEAREST).save(out / (name + '_x4.png'))
    for f in frames_ppm:
        f.unlink()
    return ppm, changed


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--bin', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--frames', type=int, default=2000)
    a = p.parse_args()
    out = a.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    binary = a.bin.resolve()
    results, failures = {}, []
    base = None
    for name, (lines, level0, extra) in CASES.items():
        ppm, cursor_pixels = run_case(binary, out, name, lines, extra, a.frames)
        info = analyse(ppm)
        if name == 'calibration_digits':
            heights = [g['h'] for g in info['glyphs'] if g['h'] >= 6]
            if len(heights) < 8:
                raise RuntimeError(f'calibration found {len(heights)} digit glyphs')
            base = statistics.median(heights)
            results[name] = dict(baseDigitInk=base, digitInk=heights)
            continue
        fr = info['fractions']
        record = dict(level0Fractions=level0, detectedFractions=len(fr), cursorPixelsRemoved=cursor_pixels,
                      fractions=[])
        # After ENTER the history view shows no editing cursor.
        if cursor_pixels == 0 and not name.startswith('h1_'):
            failures.append(f'{name}: no blinking cursor removed; frames did not cover both blink phases')
        if name.startswith('c6_'):
            # The only fraction is inside the exponent (script level 1): no
            # level-0 numerator/denominator exists, so nothing is asserted.
            record['note'] = 'fraction at script level 1; excluded from the level-0 ratio'
        elif len(fr) != level0:
            failures.append(f'{name}: expected {level0} level-0 fractions, detected {len(fr)}')
        else:
            for f in fr:
                for role in ('numeratorInk', 'denominatorInk'):
                    ratio = f[role] / base
                    record['fractions'].append(dict(role=role, ink=f[role], ratio=round(ratio, 3),
                                                    bar=[f['bar']['x'], f['bar']['y'], f['bar']['w']]))
                    if ratio < MIN_RATIO:
                        failures.append(f'{name}: {role} {f[role]} px = {ratio:.0%} of base {base} px')
        results[name] = record
    summary = dict(binary=str(binary), baseDigitInk=base, minRatio=MIN_RATIO, cases=results,
                   failures=failures, status='FAIL' if failures else 'PASS')
    (out / 'summary.json').write_text(json.dumps(summary, indent=2))
    print(f'readability guard: {summary["status"]} (base digit ink {base} px)')
    for name, r in results.items():
        if 'fractions' in r:
            print(f'  {name}: ' + ', '.join(f'{x["role"][:3]}={x["ink"]}px({x["ratio"]:.0%})' for x in r['fractions']))
    for f in failures:
        print('  FAIL', f)
    sys.exit(1 if failures else 0)


if __name__ == '__main__':
    main()
