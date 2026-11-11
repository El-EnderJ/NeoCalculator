#!/usr/bin/env python3
"""Scientific product acceptance through real keys, stable Toolbox IDs and read-only probes.

Expected arithmetic uses Python Fraction/math and frozen CODATA 2022 values;
never reads production sensitivities to construct an expected uncertainty.
"""
from pathlib import Path
from fractions import Fraction
import math,re,sys
sys.stdout.reconfigure(encoding='utf8',errors='replace')
exec(Path(__file__).with_name('test-calculation-quantities.py').read_text(encoding='utf8').split('\nsummation=')[0])

def digits(text): return keys(' '.join(str(text)))
def measure(n,u):
 return keys('TOOLBOX')+search('measurement')+'assert_calc_input toolbox selected 750 0\n'+keys('ENTER')+digits(n)+keys('RIGHT')+digits(u)+keys('RIGHT')
def measured(n,u,unit=1,prefix=0,query='metre'):
 return measure(n,u)+pick(query,unit,prefix)
def constant(query,reference):
 return keys('TOOLBOX')+search(query)+f'assert_calc_input toolbox selected {16384|reference} 0\n'+keys('ENTER')
def uncertainty(value,count=None,display=None,relative=None):
 text='assert_calc_input uncertainty standard %.17g %.17g\n'%(value,max(1e-300,abs(value)*3e-13))
 if count is not None:text+=f'assert_calc_input uncertainty count {count}\n'
 if display is not None:text+='assert_calc_input uncertainty display %.17g %.17g\n'%(display,max(1e-300,abs(display)*3e-13))
 if relative is not None:text+='assert_calc_input uncertainty relative %.17g %.17g\n'%(relative,max(1e-300,abs(relative)*3e-13))
 return text+'assert_calc_input uncertainty dump\n'
def metrology(mode):
 return keys(menu+' '+'DOWN '*3+'ENTER '+'DOWN '*mode+'ENTER')
def error(name):return 'assert_calc_status quantity_error\nassert_calc_input quantity error '+name+'\n'

run('measurement-template',measure('12.00','0.10')+shot('measurement-filled-template')+keys('ENTER')+check('12','12')+uncertainty(.1,1))
help_body=keys('TOOLBOX')+search('measurement')+'assert_calc_input toolbox selected 750 0\n'+keys('FORMAT DOWN DOWN DOWN ENTER')+shot('measurement-help')+keys('BACK BACK BACK AC')
help_body+=keys('TOOLBOX')+search('gravitational constant')+'assert_calc_input toolbox selected 16443 0\n'+keys('FORMAT DOWN DOWN DOWN ENTER')+shot('constant-help')+keys('BACK BACK BACK AC 2 ADD 2 ENTER')+check('4')
run('scientific-help',help_body)

run('same-source-ans',measured('10','1')+keys('ENTER')+check('10','10')+uncertainty(1,1)+keys('STO 1 AC ans ADD ans ENTER')+check('20','20')+uncertainty(2,1)+shot('same-source-double')+keys('AC ALPHA 1 SUB ALPHA 1 ENTER')+check('0','0')+uncertainty(0,0)+shot('same-source-cancel'))
run('independent-measurements',measured('10','1')+keys('SUB')+measured('10','1')+keys('ENTER')+check('0','0')+uncertainty(math.sqrt(2),2)+shot('independent-difference'))
run('sum-oracle',measured('10','1')+keys('ADD')+measured('20','2')+keys('ENTER')+check('30','30')+uncertainty(math.sqrt(5),2))
run('product-oracle',measured('2.00','0.10')+keys('MUL')+measured('3.00','0.20')+keys('ENTER')+check('6','6')+uncertainty(.5,2)+shot('product-two-sources'))
run('centimetre-area',measured('2','0.1',prefix=14,query='cm')+keys('MUL')+measured('3','0.2',prefix=14,query='cm')+keys('ENTER')+check('3/5000','3/5000')+uncertainty(.00005,2)+target('cm²',101,14)+check('6','3/5000')+uncertainty(.00005,2,.5)+shot('area-cm2'))
run('conversion-identity',measured('100','1',prefix=14,query='cm')+keys('ENTER')+check('1','1')+uncertainty(.01,1)+target('mm',1,15)+check('1000','1')+uncertainty(.01,1,10)+target('cm',1,14)+check('100','1')+uncertainty(.01,1,1)+keys('AC ans SUB ans ENTER')+check('0','0')+uncertainty(0,0))

history=measured('10','1')+keys('ENTER STO 1 AC 7 ENTER UP UP')+check('10','10')+uncertainty(1,1)+keys('ENTER')+uncertainty(1,1)+keys('AC ans SUB ALPHA 1 ENTER')+check('0','0')+uncertainty(0,0)
run('history-source-identity',history)
run('preans-source-identity',measured('10','1')+keys('ENTER STO 1 AC 7 ENTER AC preans SUB ALPHA 1 ENTER')+check('0','0')+uncertainty(0,0))
body=measured('10','1')+keys('ENTER')
for memory in '123456':body+=keys('STO '+memory)
for memory in '123456':body+=keys('AC ALPHA '+memory+' SUB ALPHA '+memory+' ENTER')+check('0','0')+uncertainty(0,0)
run('all-session-memories',body)

canonical=str(Fraction('10.123456'));u=.123456;rel=float(Fraction('0.123456')/Fraction('10.123456'))
body=measured('10.123456','0.123456')+keys('ENTER')+check(canonical,canonical)+uncertainty(u,1,relative=rel)
for mode,label in enumerate(('standard','relative','expanded','parenthetic')):
 body+=metrology(mode)+check(canonical,canonical)+uncertainty(u,1,relative=rel)+shot('format-'+label)
body+=target('cm',1,14)+check(str(Fraction(canonical)*100),canonical)+uncertainty(u,1,12.3456,rel)+metrology(2)+uncertainty(u,1,12.3456,rel)+shot('expanded-centimetres')
body+=keys(menu+' '+'DOWN '*3+'ENTER DOWN BACK BACK')+uncertainty(u,1,12.3456,rel)+target('mm',1,15)+check(str(Fraction(canonical)*1000),canonical)+uncertainty(u,1,123.456,rel)+keys('AC ans SUB ans ENTER')+check('0','0')+uncertainty(0,0)
run('format-canonical',body)

run('temperature-point',measured('20.0','0.5',23,query='Degree Celsius')+keys('ENTER')+check('5863/20','5863/20')+'assert_calc_input quantity role point\n'+uncertainty(.5,1)+target('Degree Fahrenheit',60)+check('68','5863/20')+uncertainty(.5,1,.9,.5/293.15)+shot('temperature-fahrenheit')+target('Degree Celsius',23)+check('20','5863/20')+uncertainty(.5,1,.5,.5/293.15)+keys('STO 1 AC ans SUB ALPHA 1 ENTER')+check('0','0')+'assert_calc_input quantity role difference\n'+uncertainty(0,0))
run('temperature-independent',measured('20','0.5',23,query='Degree Celsius')+keys('SUB')+measured('20','0.5',23,query='Degree Celsius')+keys('ENTER')+check('0','0')+'assert_calc_input quantity role difference\n'+uncertainty(math.sqrt(.5),2))

run('constant-exact',constant('Speed of light in vacuum',1)+keys('ENTER')+check('299792458','299792458')+uncertainty(0,0)+shot('constant-light'))
run('constant-prefix',keys('TOOLBOX')+search('millistandard gravity')+'assert_calc_input toolbox selected 16397 15\n'+keys('ENTER ENTER')+check('196133/20000000','196133/20000000')+uncertainty(0,0)+shot('constant-milli-g'))
g=str(Fraction('6.67430e-11'))
run('constant-measured',constant('gravitational constant',59)+keys('ENTER')+check(g,g)+uncertainty(1.5e-15,1)+shot('constant-gravity')+keys('AC 2 MUL')+constant('gravitational constant',59)+keys('ENTER')+check(str(Fraction(g)*2),str(Fraction(g)*2))+uncertainty(3e-15,1)+keys('AC')+constant('gravitational constant',59)+keys('SUB')+constant('gravitational constant',59)+keys('ENTER')+check('0','0')+uncertainty(0,0))
favorite=''
for query,reference in [('Speed of light in vacuum',1),('gravitational constant',59)]:
 favorite+=keys('TOOLBOX')+search(query)+f'assert_calc_input toolbox selected {16384|reference} 0\n'+keys('FORMAT ENTER BACK BACK')
favorite+=keys('TOOLBOX UP RIGHT ENTER')+'assert_calc_input toolbox selected 16385 0\n'+shot('constant-favorites')+keys('ENTER ENTER')+check('299792458','299792458')+uncertainty(0,0)
favorite+=keys('AC TOOLBOX UP RIGHT ENTER DOWN')+'assert_calc_input toolbox selected 16443 0\n'+keys('ENTER ENTER')+check(g,g)+uncertainty(1.5e-15,1)
run('constant-favorites',favorite)
am=str(Fraction('5.29177210544e-11')*Fraction('9.1093837139e-31'))
run('codata-correlation',constant('Bohr radius',2)+keys('MUL')+constant('electron mass',3)+keys('ENTER')+check(am,am)+uncertainty(7.347417886856706300435607832631308e-51,2)+shot('codata-correlated-product'))

for name,expression in [('square-zero',keys('LPAREN')+measure('0','1')+keys('RPAREN POW 2 RIGHT')),('reciprocal',keys('1 FRAC LPAREN')+measure('1','1')+keys('RPAREN RIGHT')),('sqrt-domain',keys('SQRT')+measure('1','1')+keys('RIGHT')),('tan-pole',keys('TAN')+measured('1.5707963267948966','0.01',8,query='Radian')+keys('RIGHT'))]:
 run('nonlinear-'+name,keys('7 ENTER AC')+expression+keys('ENTER')+error('uncertainty_nonlinear')+shot(name)+keys('AC ans ENTER')+check('7')+keys('AC 2 ADD 2 ENTER')+check('4'))
run('healthy-square',keys('LPAREN')+measure('10','0.1')+keys('RPAREN POW 2 RIGHT ENTER')+check('100','100')+uncertainty(2,1))

# Non-cardinal angles catch accidental re-evaluation of a canonical radian
# expression in the current UI mode. The independent relative oracle checks
# the nominal as well as its sensitivity, without prescribing CAS text. The
# numeric companion evaluates the exact typed function tree in binary64; the
# same strict tolerance applies to relative and standard uncertainty.
for mode in ('rad','deg'):
 for kind in ('radian','degree','scalar'):
  radians=1 if kind=='radian' or (kind=='scalar' and mode=='rad') else math.pi/180
  u=.01 if radians==1 else .01*math.pi/180
  argument=measure('1','0.01')
  if kind=='radian':argument+=pick('Radian',8)
  if kind=='degree':argument+=pick('Degree',57)
  expected=u*math.cos(radians)
  run('angle-'+kind+'-'+mode,'set_angle_mode '+mode+'\n'+keys('SIN')+argument+keys('RIGHT ENTER')+'assert_calc_input quantity dump\nassert_calc_status ok\n'+uncertainty(expected,1,relative=expected/math.sin(radians)))

angle_u=.01*math.cos(1);angle_relative=angle_u/math.sin(1)
angle_result='assert_calc_status ok\n'+uncertainty(angle_u,1,relative=angle_relative)
angle_entry='set_angle_mode rad\n'+keys('SIN')+measured('1','0.01',8,query='Radian')+keys('RIGHT ENTER STO 1')+angle_result
for name,recovery in (
 ('ans',keys('AC ans ENTER')),
 ('preans',keys('AC 7 ENTER AC preans ENTER')),
 ('history',keys('AC 7 ENTER UP UP')+angle_result+keys('ENTER'))):
 body=angle_entry+'set_angle_mode deg\n'+recovery+angle_result
 for mode in range(4):body+=metrology(mode)+angle_result
 body+=shot('angle-'+name+'-deg')+keys('AC ans SUB ALPHA 1 ENTER')+check('0','0')+uncertainty(0,0)
 run('angle-'+name+'-mode-change',body)

run('angle-derived-product',angle_entry+'set_angle_mode deg\n'+keys('AC ALPHA 1 MUL 2 ENTER')+'assert_calc_status ok\n'+uncertainty(2*angle_u,1,relative=angle_relative))
def measured_memory(uncertainty_digits):
 return keys('AC TOOLBOX')+search('measurement')+'assert_calc_input toolbox selected 750 0\n'+keys('ENTER ALPHA 1 RIGHT')+digits(uncertainty_digits)+keys('RIGHT ENTER')+'assert_calc_input quantity dump\nassert_calc_status ok\n'
run('angle-exact-reuse','set_angle_mode rad\n'+keys('SIN 1')+pick('Radian',8)+keys('RIGHT ENTER STO 1')+check('sin(1)')+'set_angle_mode deg\n'+measured_memory('0.01')+uncertainty(.01,1,relative=.01/math.sin(1)))
run('angle-legacy-scalar-import','set_angle_mode deg\n'+keys('SIN 1 RIGHT ENTER STO 1')+'assert_calc_status ok\n'+measured_memory('0.001')+uncertainty(.001,1,relative=.001/math.sin(math.pi/180)))

tiny_extremum=keys('SIN TOOLBOX')+search('measurement')+keys('ENTER PI FRAC 2 RIGHT RIGHT')+digits('0.00000000000000000001')+keys('RIGHT')+pick('Radian',8)+keys('RIGHT')
for name,expression in (
 ('trig-extremum',tiny_extremum),
 ('trig-large',keys('SIN')+measured('100000000000000000000','0.01',8,query='Radian')+keys('RIGHT')),
 ('power-large',keys('LPAREN')+measure('100000000000000000000','0.01')+keys('RPAREN POW 2 RIGHT'))):
 run('numeric-range-'+name,keys('7 ENTER AC')+expression+keys('ENTER')+'assert_calc_input quantity dump\n'+error('uncertainty_numeric')+keys('AC ans ENTER')+check('7')+keys('AC 2 ADD 2 ENTER')+check('4'))
run('uncertainty-negative',keys('7 ENTER AC TOOLBOX')+search('measurement')+keys('ENTER 1 0 RIGHT NEGATE 1 RIGHT ENTER')+error('uncertainty_invalid')+keys('AC ans ENTER')+check('7'))
run('uncertainty-dimension',keys('TOOLBOX')+search('measurement')+keys('ENTER')+amount('10','metre',1)+keys('RIGHT')+amount('1','Second',3)+keys('RIGHT ENTER')+error('dimension_mismatch')+keys('AC 2 ADD 2 ENTER')+check('4'))
run('uncertainty-point-rejected',keys('TOOLBOX')+search('measurement')+keys('ENTER')+amount('20','Degree Celsius',23)+keys('RIGHT')+amount('1','Degree Celsius',23)+keys('RIGHT ENTER')+error('uncertainty_invalid')+keys('AC 2 ADD 2 ENTER')+check('4'))
body=keys('7 ENTER AC')
for i in range(17):body+=(keys('ADD') if i else '')+measure('1','0.01')
run('source-limit',body+keys('ENTER')+error('uncertainty_limit')+keys('AC ans ENTER')+check('7')+keys('AC')+measure('2','0.1')+keys('ENTER')+check('2','2')+uncertainty(.1,1))
body=''
for i in range(16):body+=(keys('ADD') if i else '')+measure('1','0.01')
run('source-capacity',body+keys('ENTER')+check('16','16')+uncertainty(.04,16)+shot('sixteen-sources'))

for i,tag in enumerate(('en-US','en-GB','es-ES','es-419')):
 locale=keys('HOME')+'wait 20\nopen_app Settings\nwait 20\n'+keys('DOWN '*4+'RIGHT '*i+'HOME')+'wait 20\nopen_app Calculation\nwait 20\n'
 run('locale-'+tag,locale+measure('12.00','0.10')+keys('ENTER')+check('12','12')+uncertainty(.1,1)+shot(tag+'-measure')+keys(menu)+shot(tag+'-menu')+keys('DOWN '*3+'ENTER')+shot(tag+'-modes')+keys('BACK BACK AC')+constant('electron mass',3)+keys('ENTER')+uncertainty(2.8e-40,1)+shot(tag+'-constant'))

# Log source identities are checked as an extra independent invariant for every
# unchanged view in the long FORMAT/conversion cases. The final cancellation
# intentionally removes all sources and is excluded from those comparisons.
for name in ('format-canonical','conversion-identity','angle-ans-mode-change','angle-preans-mode-change','angle-history-mode-change'):
 path=a.out/(name+'.log')
 if path.exists() and records.get(name,{}).get('passed'):
  ids=[line.split(' ids=',1)[1] for line in path.read_text(encoding='utf8').splitlines() if line.startswith('[UNCERTAINTY]') and 'sources=1 ' in line]
  assert ids and len(set(ids))==1,(name,'SourceId/contribution changed across presentation',ids)
raise SystemExit(0 if records and all(r['passed'] for r in records.values()) else 1)
