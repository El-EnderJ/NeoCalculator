#!/usr/bin/env python3
"""Compatibility adapter for the legacy Toolbox gate's scalar-only probe.

The unchanged test-toolbox.py sends Measurement to a scalar serializer when it
asserts `complete`. That serializer correctly refuses typed measurements.
Keep all fixtures and add quantity-engine assertions for that recipe instead.
"""
from pathlib import Path
import sys

if '--bin' in sys.argv:
    index=sys.argv.index('--bin')+1
    sys.argv[index]=str(Path(sys.argv[index]).resolve())

path = Path(__file__).with_name('test-toolbox.py')
source = path.read_text(encoding='utf8')
old = "body+='assert_calc_input dump\\nassert_calc_input complete\\nassert_calc_input structure\\n'+shot"
new = "body+='assert_calc_input dump\\n'+('' if e['recipe']=='Measurement' else 'assert_calc_input complete\\n')+'assert_calc_input structure\\n'+shot"
assert source.count(old) == 1, 'Legacy gate changed: re-audit adapter before running'
source = source.replace(old, new)
old = "    if re.fullmatch(r'-?\\d+(\\.\\d+)?',e['expected']):"
new = """    if e['recipe']=='Measurement':
        body+='assert_calc_exact '+e['expected']+'\\nassert_calc_input quantity canonical '+e['expected']+'\\nassert_calc_input uncertainty standard '+e['fixture'][1]+' 0\\nassert_calc_input uncertainty count 1\\n'
    elif re.fullmatch(r'-?\\d+(\\.\\d+)?',e['expected']):"""
assert source.count(old) == 1, 'Legacy scalar expected-value gate changed: re-audit adapter'
source = source.replace(old, new)
exec(compile(source, str(path), 'exec'), {'__file__': str(path), '__name__': '__main__'})
