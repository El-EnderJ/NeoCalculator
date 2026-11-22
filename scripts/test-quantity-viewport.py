#!/usr/bin/env python3
"""Pan a real long quantity, reach its bound and return without altering Ans."""
from pathlib import Path
from PIL import ImageChops
exec(Path(__file__).with_name('test-calculation-quantities.py').read_text(encoding='utf8').split('\nsummation=')[0])
value=str(9**64)
body=keys('9 POW 6 4 RIGHT')+pick('metre',1)+keys('ENTER')+check(value,value,'1:0:1')+shot('start')
# A result scrolls with SHIFT + arrow and keeps scrolling while the arrow is
# held; a plain arrow re-edits the expression (DEVICE-INPUT-PATH-01).
body+=keys('SHIFT')+'keydown RIGHT\n'+'keyrepeat RIGHT\n'*160+'keyup RIGHT\n'+shot('end')+keys('SHIFT RIGHT '*15)+shot('bound')+keys('SHIFT LEFT '*175)+shot('origin')
body+=check(value,value,'1:0:1')+keys('AC ans FRAC')+amount('1','metre',1)+keys('ENTER')+check(value)+'assert_calc_input quantity none\n'
run('long-quantity',body)
assert records['long-quantity']['passed']
images={n:Image.open(a.out/(n+'.png')).convert('RGB').crop((4,26,316,234)) for n in ('start','end','bound','origin')}
same=lambda a,b:ImageChops.difference(images[a],images[b]).getbbox() is None
metrics=dict(moved=not same('start','end'),boundStable=same('end','bound'),originRestored=same('start','origin'))
(a.out/'viewport.json').write_text(json.dumps(metrics,indent=2))
assert all(metrics.values()),metrics
print('PASS long quantity pan, bound, return and canonical Ans',flush=True)
