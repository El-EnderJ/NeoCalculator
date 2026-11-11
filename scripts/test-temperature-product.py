#!/usr/bin/env python3
"""Affine quantities via real Calculation keys, Toolbox and FORMAT, 320x240 captures."""
from pathlib import Path
import sys
sys.stdout.reconfigure(encoding='utf8',errors='replace')
exec(Path(__file__).with_name('test-calculation-quantities.py').read_text(encoding='utf8').split('\nsummation=')[0])
def point(n,unit=23):return amount(n,{5:'Kelvin',23:'Degree Celsius',60:'Degree Fahrenheit',1181:'Degree Reaumur',1180:'Degree Rankine'}[unit],unit)
def delta(n,unit=62):return amount(n,{61:'delta k',62:'delta c',63:'delta f',1311:'delta re',1312:'delta r'}[unit],unit)
def result(exact,canonical,role):return check(exact,canonical)+'assert_calc_input quantity role '+role+'\n'
def conversion(n,unit,target_query,target_id,si,expected,name):
 return run(name,point(n,unit)+keys('ENTER')+result(si,si,'point')+shot(name+'-si')+target(target_query,target_id)+result(expected,si,'point')+shot(name))
conversion('0',23,'Kelvin',5,'5463/20','5463/20','zero-celsius-kelvin')
conversion('100',23,'Degree Celsius',23,'7463/20','100','boiling-celsius')
conversion('32',60,'Degree Celsius',23,'5463/20','0','fahrenheit-celsius')
conversion('212',60,'Degree Celsius',23,'7463/20','100','boiling-fahrenheit')
run('negative-forty',keys('NEGATE')+point('40')+keys('ENTER')+result('4663/20','4663/20','point')+target('Degree Fahrenheit',60)+result('-40','4663/20','point')+shot('negative-forty'))
conversion('80',1181,'Degree Celsius',23,'7463/20','100','reaumur-celsius')
sub=point('20')+keys('SUB')+point('10')+keys('ENTER')
run('point-minus-point',sub+result('10','10','difference')+shot('point-minus-point')+target('delta c',62)+result('10','10','difference')+shot('difference-celsius')+target('delta f',63)+result('18','10','difference')+shot('difference-fahrenheit'))
run('fahrenheit-minus-fahrenheit',point('68',60)+keys('SUB')+point('32',60)+keys('ENTER')+result('20','20','difference')+target('delta c',62)+result('20','20','difference'))
run('explicit-difference',delta('18',63)+keys('ENTER')+result('10','10','difference')+shot('explicit-difference')+keys('AC')+delta('8',1311)+keys('ENTER')+result('10','10','difference'))
run('point-plus-difference',point('20')+keys('ADD')+delta('10')+keys('ENTER')+result('6063/20','6063/20','point')+target('Degree Celsius',23)+result('30','6063/20','point')+shot('point-plus-difference'))
run('point-minus-difference',point('20')+keys('SUB')+delta('18',63)+keys('ENTER')+target('Degree Celsius',23)+result('10','5663/20','point'))
run('difference-plus-point',delta('10',61)+keys('ADD')+point('32',60)+keys('ENTER')+target('Degree Fahrenheit',60)+result('50','5663/20','point'))
for name,expr in [('point-plus-point',point('20')+keys('ADD')+point('10')),('difference-minus-point',delta('10')+keys('SUB')+point('20')),('point-times-two',point('20')+keys('MUL 2')),('point-power',keys('LPAREN')+point('20')+keys('RPAREN POW 2 RIGHT')),('point-plus-kelvin',point('20')+keys('ADD')+point('10',5))]:
 run(name,expr+keys('ENTER')+'assert_calc_status quantity_error\nassert_calc_input quantity error affine_operation\n'+shot(name)+keys('AC 2 ADD 2 ENTER')+check('4'))
run('ans-history',point('20')+keys('ENTER')+target('Degree Fahrenheit',60)+result('68','5863/20','point')+target('Kelvin',5)+result('5863/20','5863/20','point')+keys('AC ans SUB')+point('32',60)+keys('ENTER')+result('20','20','difference')+keys('UP')+result('20','20','difference')+keys('UP')+result('5863/20','5863/20','point')+keys('ENTER')+result('5863/20','5863/20','point'))
for role,expr in [('point',point('20')),('difference',delta('10'))]:
 canonical='5863/20' if role=='point' else '10'
 run('format-'+role,expr+keys('ENTER')+output()+'assert_calc_input unit_menu geometry\n'+shot('format-'+role)+keys('DOWN ENTER')+result('20' if role=='point' else '10',canonical,role)+keys('FORMAT FORMAT')+result('20' if role=='point' else '10',canonical,role))
 body=expr+keys('ENTER')
 for k in '123456':body+=keys('STO '+k)
 for k in '123456':body+=keys('AC ALPHA '+k+' ENTER')+result(canonical,canonical,role)
 run('session-'+role,body)
favorite=''
for q,item in [('Degree Celsius',23),('delta c',62)]:favorite+=keys('TOOLBOX')+search(q)+selected(item)+keys('FORMAT ENTER BACK BACK')
run('favorites',favorite+keys('TOOLBOX UP RIGHT ENTER')+selected(23)+shot('favorites')+keys('ENTER ENTER')+result('5483/20','5483/20','point')+keys('AC TOOLBOX UP RIGHT ENTER DOWN')+selected(62)+keys('ENTER ENTER')+result('1','1','difference'))
for i,tag in enumerate(('en-US','en-GB','es-ES','es-419')):
 locale=keys('HOME')+'wait 20\nopen_app Settings\nwait 20\n'+keys('DOWN '*4+'RIGHT '*i+'HOME')+'wait 20\nopen_app Calculation\nwait 20\n'
 run(tag,locale+sub+result('10','10','difference')+shot(tag+'-difference')+output()+shot(tag+'-format')+keys('BACK AC TOOLBOX')+search('temperature difference' if i<2 else 'diferencia temperatura')+shot(tag+'-search')+keys('BACK AC')+point('20')+keys('ADD')+point('10')+keys('ENTER')+'assert_calc_input quantity error affine_operation\n'+shot(tag+'-error')+keys('AC 2 ADD 2 ENTER')+check('4'))
for spanish in (False,True):
 locale='' if not spanish else keys('HOME')+'wait 20\nopen_app Settings\nwait 20\n'+keys('DOWN '*4+'RIGHT RIGHT HOME')+'wait 20\nopen_app Calculation\nwait 20\n'
 tag='es' if spanish else 'en'
 route=keys('TOOLBOX '+'DOWN '*10+'ENTER '+'DOWN '*7+'ENTER')
 run('route-'+tag,locale+route+shot(tag+'-temperature-route')+keys('RIGHT')+selected(61)+shot(tag+'-difference-route')+keys('RIGHT')+selected(61)+shot(tag+'-difference-variants')+keys('ENTER ENTER')+result('1','1','difference'))
raise SystemExit(0 if records and all(r['passed'] for r in records.values()) else 1)
