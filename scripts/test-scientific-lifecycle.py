#!/usr/bin/env python3
"""100 mixed fixed-pool cycles with full histories, unchanged Giac and real keys.

NUMOS_SCIENTIFIC_CYCLES can shorten an exploratory run; only 100 measured
cycles after four warmups qualify as the task lifecycle gate.
"""
from pathlib import Path
import hashlib
import re

common=Path(__file__).with_name('test-calculation-quantities.py').read_text(encoding='utf8').split('\nsummation=')[0]
exec(common.replace('timeout=180','timeout=1200'))
scientific=Path(__file__).with_name('test-scientific-product.py').read_text(encoding='utf8')
exec(scientific[scientific.index('def digits'):scientific.index("\nrun(")])

cycles=int(os.environ.get('NUMOS_SCIENTIFIC_CYCLES','100'))
warmups=int(os.environ.get('NUMOS_SCIENTIFIC_WARMUPS','4'))
body=''
for cycle in range(warmups+cycles):
    body+=f'log SCIENTIFIC_CYCLE_{cycle}\n'
    # Ten repetitions of six distinct kinds guarantee the 50-entry history
    # is full of a mixture, rather than filling it only with scalar rows.
    for repeat in range(10):
        body+=keys('AC 2 ADD 3 ENTER')+check('5')
        body+=keys('AC')+amount('2','metre',1)+keys('ENTER')+check('2','2')
        body+=keys('AC')+amount('20','Degree Celsius',23)+keys('ENTER')+check('5863/20','5863/20')
        body+=keys('AC')+measured('12','0.1')+keys('ENTER')+check('12','12')+uncertainty(.1,1)
        body+=keys('AC')+constant('gravitational constant',59)+keys('ENTER')+uncertainty(1.5e-15,1)
        body+=keys('AC')+constant('Bohr radius',2)+keys('MUL')+constant('electron mass',3)+keys('ENTER')+uncertainty(7.3474178868567063e-51,2)
    body+='calculus_probe\n'
    body+=keys('AC')+measured('10','1')+keys('ENTER STO 1')+check('10','10')+uncertainty(1,1)
    for mode in range(4):
        body+=metrology(mode)+check('10','10')+uncertainty(1,1)
    body+=keys('AC ans ADD ans ENTER')+check('20','20')+uncertainty(2,1)
    body+=keys('AC ALPHA 1 SUB ALPHA 1 ENTER')+check('0','0')+uncertainty(0,0)
    body+=keys('AC')+measured('20','0.5',23,query='Degree Celsius')+keys('ENTER')+target('Degree Fahrenheit',60)+check('68','5863/20')+uncertainty(.5,1,.9)
    body+=output()+all_units()+'assert_calc_input toolbox dump\n'+keys('BACK')+'assert_calc_input unit_menu closed\nassert_calc_input toolbox closed\n'
    # Constant favorite is inserted using the Favorites tab, then removed.
    body+=keys('AC TOOLBOX')+search('gravitational constant')+'assert_calc_input toolbox selected 16443 0\n'+keys('FORMAT ENTER BACK BACK')
    body+=keys('TOOLBOX UP RIGHT ENTER')+'assert_calc_input toolbox selected 16443 0\n'
    if cycle==0:body+=shot('constant-favorite')
    body+=keys('ENTER ENTER')+uncertainty(1.5e-15,1)
    body+=keys('AC TOOLBOX')+search('gravitational constant')+keys('FORMAT DOWN DOWN DOWN ENTER')
    if cycle==0:body+=shot('constant-help')
    body+=keys('BACK FORMAT ENTER BACK BACK')
    body+=keys('AC 7 ENTER AC LPAREN')+measure('0','1')+keys('RPAREN POW 2 RIGHT ENTER')+error('uncertainty_nonlinear')
    body+=keys('AC ans ENTER')+check('7')+keys('AC 2 ADD 2 ENTER')+check('4')
    body+=keys('HOME')+'wait 20\nassert_app Menu\ncalculus_probe\nopen_app Calculation\nwait 20\n'

run('scientific-mixed-cycles',body)
log=(a.out/'scientific-mixed-cycles.log').read_text(encoding='utf8')
homes=[dict(re.findall(r'(\w+)=(\d+)',line)) for line in log.splitlines() if line.startswith('CALCULUS_PROBE|app=Menu|')]
calculations=[dict(re.findall(r'(\w+)=(\d+)',line)) for line in log.splitlines() if line.startswith('CALCULUS_PROBE|app=Calculation|')]
modal=[dict(re.findall(r'(\w+)=(\d+)',line)) for line in log.splitlines() if line.startswith('[TOOLBOX-MEM]')]
assert records['scientific-mixed-cycles']['passed']
assert len(homes)==warmups+cycles and len(calculations)==warmups+cycles
assert len(modal)==warmups+cycles
assert 0<int(homes[warmups]['pool_total'])<=65536,'requires fixed 64 KiB LVGL pool build'
for sample in homes[warmups:]:
    for field in ('objects','timers','pool_total','pool_free','handles'):
        assert sample[field]==homes[warmups][field],(field,sample,homes[warmups])
    assert sample['handles']=='0'
profile=[dict(re.findall(r'(\w+)=([\w-]+)',line)) for line in log.splitlines() if line.startswith('SCIENTIFIC_LIVE|')]
cppSummary=None
if profile:
    liveHomes=[row for row in profile if row['app']=='Menu']
    liveCalculations=[row for row in profile if row['app']=='Calculation']
    assert len(liveHomes)==len(homes) and len(liveCalculations)==len(calculations)
    for rows in (liveHomes,liveCalculations):
        for sample in rows[warmups:]:
            for field in ('payloads','contributions','unique_sources','counter_overflow'):
                assert sample[field]==rows[warmups][field],(field,sample,rows[warmups])
            assert sample['counter_overflow']=='0'
    cppSummary={name:{field:{'min':min(int(row[field]) for row in rows[warmups:]),'max':max(int(row[field]) for row in rows[warmups:])}
                     for field in ('cpp_bytes','payloads','contributions','unique_sources')}
                for name,rows in (('home',liveHomes),('calculation',liveCalculations))}
    if os.environ.get('NUMOS_SCIENTIFIC_REQUIRE_CPP_STABLE')=='1':
        assert all(row['cpp_bytes']==rows[warmups]['cpp_bytes'] for rows in (liveHomes,liveCalculations) for row in rows[warmups:]),cppSummary
report=dict(passed=True,warmups=warmups,cycles=cycles,qualifies=cycles>=100,
            binarySha256=hashlib.sha256(a.bin.read_bytes()).hexdigest(),
            historyCapacity=50,evaluationsToFillBeforeEach=60,
            historyMixture=['scalar','unit','temperature','measurement','measured constant','correlated constants'],
            homes=homes,calculations=calculations,modal=modal,
            minimumSampledFree=min(int(m['free']) for m in modal),
            minimumSampledLargestBlock=min(int(m['largest']) for m in modal),
            cppAndSourceProfiles=profile,
            cppAndSourceSummary=cppSummary,
            limitations=['Private global C++ new/AST bytes include preloaded script and Giac C++ allocations; C allocators are excluded. History fullness follows 60 successful evaluations and the source-defined 50-entry cap.'] if profile else [
                'The ordinary Windows CALCULUS_PROBE heap=0 field is not a C++ heap measurement.',
                'Live C++ allocation bytes and globally retained uncertainty-source counts require the private allocation overlay; current-result source counts are checked by real-product probes.',
                'History is filled by 60 successful evaluations against its 50-entry source-defined capacity; this binary has no direct history-count accessor.'])
(a.out/'resources.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print(f'PASS {cycles} measured scientific cycles after {warmups} warmups, full mixed history, fixed LVGL pool')
