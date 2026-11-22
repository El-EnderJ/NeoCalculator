#!/usr/bin/env python3
"""Additional real-key screenshots: empty Measurement, limit and complete viewport."""
from pathlib import Path
fixture=Path(__file__).with_name('test-scientific-product.py')
exec(fixture.read_text(encoding='utf8').split("\nrun('measurement-template'")[0])

run('empty-template',keys('TOOLBOX')+search('measurement')+keys('ENTER')+shot('measurement-empty-template'))
run('constant-search',keys('TOOLBOX')+search('gravitational constant')+'assert_calc_input toolbox selected 16443 0\n'+shot('constant-search'))
am=str(Fraction('5.29177210544e-11')*Fraction('9.1093837139e-31'))
body=constant('Bohr radius',2)+keys('MUL')+constant('electron mass',3)+keys('ENTER')+check(am,am)+uncertainty(7.3474178868567063004e-51,2)
body+=shot('correlated-start')+keys('RIGHT '*120)+check(am,am)+uncertainty(7.3474178868567063004e-51,2)+shot('correlated-end')
body+=keys('RIGHT '*10)+shot('correlated-edge')+keys('LEFT '*120)+shot('correlated-return')
run('scientific-pan',body)
body=keys('7 ENTER AC')
for i in range(17):body+=(keys('ADD') if i else '')+measure('1','0.01')
run('limit-visible',body+keys('ENTER')+error('uncertainty_limit')+shot('source-limit')+keys('AC ans ENTER')+check('7'))
if all(v['passed'] for v in records.values()):
 from PIL import ImageChops
 def same(a_name,b_name):
  # Exclude the live status clock; inspect the 320 x 215 mathematical viewport.
  left=Image.open(a.out/(a_name+'.png')).crop((0,25,320,240)).convert('RGB')
  right=Image.open(a.out/(b_name+'.png')).crop((0,25,320,240)).convert('RGB')
  return ImageChops.difference(left,right).getbbox() is None
 assert not same('correlated-start','correlated-end')
 assert same('correlated-end','correlated-edge')
 assert same('correlated-start','correlated-return')
raise SystemExit(0 if records and all(r['passed'] for r in records.values()) else 1)
