#!/usr/bin/env python3
"""Explicit maintainer-only capture of the small NumOS CODATA 2022 dataset.

Builds and tests never execute this script. Published numerical facts are kept
with URLs and response hashes; no web page or JCGM publication enters firmware.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import html
import itertools
import json
from pathlib import Path
import re
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "data/scientific/codata2022/source_capture.json"
BASE = "https://physics.nist.gov"
NAMES = {
    "c": "speed of light in vacuum", "bohrrada0": "Bohr radius",
    "me": "electron mass", "hr": "Hartree energy", "aut": "atomic unit of time",
    "hbar": "reduced Planck constant", "plkl": "Planck length",
    "plkt": "Planck time", "plkm": "Planck mass", "plktmp": "Planck temperature",
    "gn": "standard acceleration of gravity", "e": "elementary charge",
    "bg": "Newtonian constant of gravitation", "h": "Planck constant",
    "k": "Boltzmann constant",
}
MEASURED = ("bohrrada0", "me", "hr", "aut", "plkl", "plkt", "plkm", "plktmp", "bg")

def fetch(url):
    request = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0 (NumOS CODATA offline capture)"})
    with urllib.request.urlopen(request, timeout=30) as response:
        raw = response.read()
    return raw.decode("utf-8"), hashlib.sha256(raw).hexdigest()

def number(s):
    return s.replace(" ", "").replace("...", "")

def pair_capture(pair):
    a, b = pair
    url = f"{BASE}/cgi-bin/cuu/CCValue?{b}|ShowSecond=Browse&First={a}"
    raw, digest = fetch(url)
    text = html.unescape(re.sub(r"<[^>]+>", " ", raw))
    text = re.sub(r"\s+", " ", text)
    match = re.search(r"Correlation coefficient r of the above two quantities:\s*r\s*=\s*([-+]?\d+(?:\.\d+)?)", text)
    if not match or "Source: 2022 CODATA" not in text:
        raise ValueError(f"unrecognized or unversioned NIST response: {url}")
    # WHY: a successfully parsed explicit zero differs from a missing pair.
    return {"a": a, "b": b, "rho": match[1], "url": url,
            "response_sha256": digest, "source": "NIST 2022 CODATA pair query"}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--consulted", required=True, help="Explicit ISO date in user's time zone")
    args = parser.parse_args()
    url = f"{BASE}/cuu/Constants/Table/allascii.txt"
    raw, digest = fetch(url)
    if "2022 CODATA adjustment" not in raw:
        raise ValueError("NIST current table is not CODATA 2022; create a new dataset instead")
    rows = {}
    for code, name in NAMES.items():
        line = next(line for line in raw.splitlines() if line[:60].strip() == name)
        value, uncertainty, unit = line[60:85].strip(), line[85:110].strip(), line[110:].strip()
        rows[code] = {"name": name, "value": number(value),
                      "standard_uncertainty": "0" if uncertainty == "(exact)" else number(uncertainty),
                      "exact": uncertainty == "(exact)", "truncated": "..." in value,
                      "unit": unit, "source_line": line}
    with ThreadPoolExecutor(max_workers=4) as pool:
        pairs = list(pool.map(pair_capture, itertools.combinations(MEASURED, 2)))
    result = {"dataset": "CODATA 2022", "nist_web_version": "9.0", "consulted": args.consulted,
              "table_url": url, "table_response_sha256": digest, "constants": rows,
              "correlations": pairs}
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Captured {len(rows)} value rows and {len(pairs)} explicit correlation pairs to {OUT}")

if __name__ == "__main__":
    main()
