"""W1 edge-family surface, surface name `calculation.edge`.

Same keystroke paths, preamble and analysis as the default Calculation surface
(calculation/surface.py + driver.py); only the generator differs: dedicated
edge families from calculation/w1gen.py (huge/tiny magnitudes incl. beyond
double range and cancellation, int64-boundary rationals, periodic decimals,
domain boundaries, poles, branch cuts, DEG/RAD trig incl. huge/tiny
arguments, empty/incomplete slots), plus a share of the common `magnitude`
and `domain` families. No magnitude pre-filter (values up to 10^±400 are
deliberate); the oracle keeps its own 20 s bound.
"""
from __future__ import annotations

from .. import surface as base
from ..w1gen import EDGE_BUILDERS, EDGE_FAMILIES, generate_weighted

NAME = "calculation.edge"
PATHS = base.PATHS
oracle_of = base.oracle_of
oracle_lite = base.oracle_lite
header = base.header
check_header = base.check_header
case_lines = base.case_lines
case_lines_for = base.case_lines_for
analyze = base.analyze
third_voice = base.third_voice
triage = base.triage


def generate(seed, index):
    return generate_weighted(seed, index, EDGE_FAMILIES, prefix="e", builders=EDGE_BUILDERS,
                             magnitude_limit=None)
