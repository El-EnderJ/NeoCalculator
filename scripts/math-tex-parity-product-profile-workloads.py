"""Additional real-product workloads executed in the private profiler namespace.

This file is an input to math-tex-parity-product-profile.py, not a standalone
runner. It uses only existing key, Toolbox and observation commands. No AST is
injected. The same workload bytes are used with baseline and final sources.
"""

def pick_profile(query, item):
    return (keys('TOOLBOX')+'sdl_text '+query+'\n'+keys('DOWN')+
            f'assert_calc_input toolbox selected {item} 0\n'+keys('ENTER')+
            'assert_calc_input toolbox closed\n')


def profile_window(label, events):
    return ('log product-profile-begin\n'+events+
            'log product-profile-report '+label+'\n')


measurement_profile = (keys('AC')+pick_profile('measurement',750)+
                       keys('1 0 RIGHT 1 RIGHT'))
profile_themes = [
    ('nested-fractions',keys('AC '+'1 / '*6+'2')),
    ('radical',keys('AC SQRT x ^ 2 RIGHT + 1')),
    ('delimiter-assembly',keys('AC ( '+'1 / '*18+'2 '+'RIGHT '*18+')')),
    ('measurement',measurement_profile),
    ('units',keys('AC 2')+pick_profile('metre',32769)+keys('^ 2 RIGHT')),
    ('scientific-reference',keys('AC')+pick_profile('gravitational constant',16443)),
]
supplement_body=''
for theme,prepare in profile_themes:
    for repetition in range(3):
        supplement_body+=prepare+'wait 5\nassert_calc_input structure\n'
        for action,events in [('unchanged','wait 80\n'),('navigate',keys('LEFT RIGHT')+'wait 4\n')]:
            supplement_body+=profile_window(f'{theme}-{action}-{repetition}',events)
            supplement_body+='assert_calc_input structure\n'

# Real Equations/Tutor product page: equation entry, solve, TOOLs activation,
# page reconstruction, return and bounded-scroll assertions. Formula-only
# geometry timings are supplied separately by the resource workstream.
supplement_body+='open_app Equations\nwait 40\nset_equations_complex_policy real\n'
supplement_body+=keys('ENTER ENTER 2 * x ^ 2 RIGHT + 3 * x - 4 = 0 ENTER DOWN DOWN ENTER')
supplement_body+='wait 20\nassert_equations_status ok\nassert_equations_solution_count 2\n'
for repetition in range(3):
    supplement_body+=profile_window(f'tutor-page-activate-{repetition}',keys('tools')+'wait 5\n')
    supplement_body+='assert_equations state steps\nassert_equations view page 0\nassert_equations view bounded\n'
    supplement_body+=profile_window(f'tutor-page-unchanged-{repetition}','wait 80\n')
    supplement_body+=profile_window(f'tutor-page-navigate-{repetition}',keys('RIGHT')+'wait 4\n'+keys('LEFT')+'wait 4\n')
    supplement_body+='assert_equations view page 0\nassert_equations view bounded\n'
    supplement_body+=keys('BACK')+'wait 4\nassert_equations state result\n'

supplement_log=replay('cost-supplement',supplement_body)
supplement_records=[]
for line in supplement_log.splitlines():
    if line.startswith('[COST] '):
        fields=line.split()
        supplement_records.append(dict(id=fields[1],**{k:int(v) for k,v in (f.split('=') for f in fields[2:])}))
assert len(supplement_records)==45,len(supplement_records)
assert all(r['node_allocations']==0 for r in supplement_records if '-unchanged-' in r['id'])
assert all(r['layouts']>0 and r['draws']>0 for r in supplement_records if r['id'].startswith('tutor-page-activate-'))
(out/'cost-supplement.json').write_text(json.dumps({
    'scope':'Real product Calculation and Equations/Tutor actions; Windows host private method-entry timing, not ESP32 latency',
    'records':supplement_records},indent=2))
print('PASS 45 additional real-product measured actions including full Tutor page')
