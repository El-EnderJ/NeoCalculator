#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Keep viewport assertions meaningful with explicit fit/overflow preconditions.

Under the readable-fraction policy (TXP PD-4, restored by
MATH-READABILITY-REGRESSION-01B) the original twelve-fraction expression
overflows again, exactly as at HEAD, and stays the legacy pan case. A
six-fraction expression is the explicit no-pan check and an eighteen-fraction
case adds a taller overflow for movement, endpoint saturation and exact return.
Each case asserts its own precondition before its verdict. Legacy source untouched.
"""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
original=ROOT/'scripts/test-calculation-viewport.py'
text=original.read_text(encoding='utf8')
anchor="    'input-vertical-pan': [('start', '1 / '*12+'2 ENTER'), ('end', 'P_SHIFT P_DOWN '*30), ('edge', 'P_SHIFT P_DOWN '*10), ('back', 'P_SHIFT P_UP '*40)],"
assert text.count(anchor)==1
text=text.replace(anchor,anchor.replace("'input-vertical-pan'","'input-vertical-fits'").replace("*12+","*6+")+"\n"+anchor
                  +"\n"+anchor.replace("'input-vertical-pan'","'input-vertical-pan-18'").replace("*12+","*18+"))
anchor="        passed = metrics['endInk'] > 15 if name == 'extended-bound' else metrics['moved']"
assert text.count(anchor)==1
text=text.replace(anchor,anchor+r'''
        if name in ('input-vertical-pan','input-vertical-pan-18','input-vertical-fits'):
            shape=re.findall(r'\[BREAK-GEOMETRY\] canvas=([\d,-]+) cursor=[\d,-]+ editable=\d+ root=(\d+),(\d+)',log)
            assert shape, 'private observer missing root dimensions'
            canvas=list(map(int,shape[0][0].split(',')));height=canvas[3]-canvas[1]+1
            rootHeight=int(shape[0][2]);metrics.update(rootHeight=rootHeight,canvasHeight=height)
            if name=='input-vertical-fits':
                assert rootHeight<height, 'no-pan fixture must actually fit'
                passed=not metrics['moved']
            else:
                assert rootHeight>height, 'pan fixture must actually overflow'
''')
exec(compile(text,str(original),'exec'),dict(__file__=str(original),__name__='__main__'))
