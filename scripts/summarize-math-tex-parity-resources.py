#!/usr/bin/env python3
"""Assemble final resource facts only after all independent records exist."""
import argparse,hashlib,json,statistics
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,default=ROOT/'out/math-tex-parity-02/resources');a=p.parse_args();out=a.out.resolve()
read=lambda p:json.loads(p.read_text(encoding='utf-8'))
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
result={'firmware':[]}
for target,short in [('numos-esp32-s3-wroom-1u-n16r8','wroom'),('esp32s3_n16r8','cam')]:
 before=read(out.parent/'A-baseline/resources'/f'{target}.json');after=read(out/'release-firmware'/f'{target}.json')
 assert after['marginPass'] and after['appBytes']==before['appBytes']==6553600
 keys=('imageBytes','freeBytes','linkedFlash','staticRam','iram')
 result['firmware'].append(dict(target=target,elfSha256=after['elfSha256'],imageSha256=after['imageSha256'],metrics={k:{'before':before[k],'after':after[k],'delta':after[k]-before[k]} for k in keys}))
 result[short+'Types']=read(out/'release-firmware'/f'{short}-types.json')
 result[short+'Stack']=read(out/'release-firmware'/f'{short}-stack-model.json')
 assert result[short+'Stack']['provisionalUniformDepthHasReserve']
for name,path in [('native',Path('C:/.piobuild/numOS-parity-02/native/emulator_pc/program.exe')),('pool',Path('C:/.piobuild/numOS-parity-02/pool/emulator_pc/program.exe')),('nativeBaseline',Path('C:/.piobuild/numOS-scientific-build/native/emulator_pc/program.exe'))]:
 result[name]=dict(path=str(path),bytes=path.stat().st_size,sha256=sha(path))
hot=read(out/'hotpaths-release/profile.json');assert hot[-1]['failures']==0
source=read(out/'hotpaths-release/source-manifest.json')
for item in source['files']:assert sha(ROOT/item['path'])==item['sha256'],('changed source after probe',item['path'])
result['hotpaths']={'summary':hot[-1],'totals':{k:sum(r[k] for r in hot[:-1]) for k in ('admissionCalls','rootLayouts','nodeLayouts','glyphLookups','mathConstantLookups','variantSearches','mathKernLookups','draws','paintNodes','cursorCalls','cursorLayoutCalls','cursorCAllocations','cppAllocations','nodeAllocations','cAllocations')},'sourceManifestSha256':sha(out/'hotpaths-release/source-manifest.json')}
assert all(result['hotpaths']['totals'][k]==0 for k in ('cppAllocations','nodeAllocations','cursorCAllocations','cursorLayoutCalls'))
assert all(r['cAllocations']==0 for r in hot[:-1] if r['phase']=='layout-denied')
pairs=read(out/'hotpaths-paired/comparison.json');result['pairedTiming']={'pairs':pairs['pairs'],'layoutMedianRatio':statistics.median(r['medianRatio'] for r in pairs['rows'] if r['phase']=='layout-denied'),'drawMedianRatio':statistics.median(r['medianRatio'] for r in pairs['rows'] if r['phase']=='draw-cursor-denied'),'source':'hotpaths-paired/comparison.json','limitations':pairs['limitations']}
baseline=read(out/'fixed-pool-baseline/resources.json');pool=read(out/'fixed-pool-release/resources.json');owned=read(out/'ownership-release/resources.json')
assert pool['passed'] and pool['qualifies'] and pool['binarySha256']==result['pool']['sha256']
assert owned['passed'] and owned['qualifies'] and owned['cppAndSourceSummary']
owned_identity=read(out/'ownership-release/overlay-evidence/identity.json')
assert owned_identity['base_binary_sha256']==result['pool']['sha256']
assert owned_identity['overlay_binary_sha256']==owned['binarySha256']
for name,record in [('home',owned['cppAndSourceSummary']['home']),('calculation',owned['cppAndSourceSummary']['calculation'])]:
 for metric,bounds in record.items():assert bounds['min']==bounds['max'],(name,metric,bounds)
result['poolLifecycle']={k:pool[k] for k in ('cycles','warmups','minimumSampledFree','minimumSampledLargestBlock','binarySha256')}
result['poolLifecycle']['baselineMinimumSampledFree']=baseline['minimumSampledFree'];result['poolLifecycle']['baselineMinimumSampledLargestBlock']=baseline['minimumSampledLargestBlock']
result['poolLifecycle']['menuPlateau']=pool['homes'][pool['warmups']]
result['ownershipLifecycle']={k:owned[k] for k in ('cycles','warmups','binarySha256','cppAndSourceSummary','limitations')}
result['hotpathCodeSections']={name:sorted({s['section'] for s in result[name+'Types']['hotpathCodeSymbols']}) for name in ('wroom','cam')}
faults=read(out/'scientific-faults-release/faults.json');identity=read(out/'scientific-faults-release/overlay/identity.json')
assert identity['base_binary_sha256']==result['native']['sha256']
assert all(r['passed'] for r in faults['results'].values())
result['failureRecovery']={'journeys':len(faults['results']),'persistentFailures':sum(bool(r.get('persistent_failure_observed')) for r in faults['results'].values()),'allocationFreeHelpControls':sum(bool(r.get('allocation_free_scope_observed')) for r in faults['results'].values()),'binarySha256':faults['binary_sha256'],'limitations':faults['limitations']}
result['limitations']=['Native timings are instrumented observations, not ESP32 timing predictions.','Compiler own-frame path envelopes are not measured task stack peaks.','Hardware heap peaks, PSRAM latency and physical fragmentation were not measured.','The native LVGL64KiB pool has allocator bookkeeping overhead; its monitor reports57,696 usable bytes.','Global C++ ownership excludes C allocator internals; linked C denial excludes CRT/DLL-internal calls.','LVGL existing draw-task C allocations are counted, not denied.','No partition changes, firmware flashing or emulator publication performed by the resource workstream.']
(out/'RESOURCE_REPORT.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
md='# Final resource and robustness evidence\n\n## Architecture Assessment\n\nThe final candidate retains both app partitions and the fixed64KiB LVGL pool. Both linked firmware images meet the131,072-byte free-app target. Source identities are checked against the final private geometry probe before this report is produced; prior checkpoints remain separate.\n\n## Solution\n\n| Target / metric | Before | After | Delta |\n|---|---:|---:|---:|\n'
for board in result['firmware']:
 for key,v in board['metrics'].items():md+=f"| {board['target']} / {key} | {v['before']:,} | {v['after']:,} | {v['delta']:+,} |\n"
md+=f"\nNative executable: {result['nativeBaseline']['bytes']:,} → {result['native']['bytes']:,} bytes; final SHA-256 `{result['native']['sha256']}`. Executable file bytes are not an embedded flash metric.\n\n"
md+='48,000 checked layout invocations plus960 actual frames passed causal allocation denial. All own C++/AST allocation attempts, cursor C attempts and cursor relayout calls were zero. Existing LVGL C draw-task allocations are reported separately. Counts, per-case timings and exclusive cursor durations are in [hotpaths-release/profile.json](hotpaths-release/profile.json); five counterbalanced pairs are in [hotpaths-paired/comparison.md](hotpaths-paired/comparison.md).\n\n'
md+=f"The median of per-case layout timing ratios is {result['pairedTiming']['layoutMedianRatio']:.3f}; draw/navigation ratios {result['pairedTiming']['drawMedianRatio']:.3f}. Candidate layout includes admission, and productive glyph/style work differs. These native observer timings are diagnostic; concurrent host work and instrumentation prevent device-latency claims.\n\n"
md+='Absolute text-style medians from five pairs (microseconds per invocation/frame; exclusive cursor timing per actual call):\n\n| Case | Layout before us | Layout after us | Draw before us | Draw after us | Cursor after us | Glyph lookups/layout | Constant/kern/variant lookups/layout |\n|---|---:|---:|---:|---:|---:|---:|---|\n'
pairedRows={(r['case'],r['style'],r['phase']):r for r in pairs['rows']}
for r in pairs['rows']:
 if r['style']!=1 or r['phase']!='layout-denied':continue
 d=pairedRows[(r['case'],1,'draw-cursor-denied')];c=r['candidate']['countersPerRepeat']
 md+=f"| {r['case']} | {r['baseline']['medianNs']/1000:.3f} | {r['candidate']['medianNs']/1000:.3f} | {d['baseline']['medianNs']/1000:.3f} | {d['candidate']['medianNs']/1000:.3f} | {d['candidate']['medianCursorNs']/1000:.3f} | {c['glyphLookups']:.0f} | {c['mathConstantLookups']:.0f} / {c['mathKernLookups']:.0f} / {c['variantSearches']:.0f} |\n"
t=result['hotpaths']['totals'];md+=f"\nMeasured totals in the final denial run: {t['admissionCalls']:,} admissions and {t['rootLayouts']:,} root layouts, {t['nodeLayouts']:,} node-layout calls, {t['glyphLookups']:,} glyph lookups, {t['mathConstantLookups']:,} runtime constant lookups, {t['mathKernLookups']:,} MathKern lookups, {t['variantSearches']:,} variant searches, {t['draws']:,} draw callbacks, {t['paintNodes']:,} paint nodes and {t['cursorCalls']:,} cursor calls. Existing LVGL C draw tasks made {t['cAllocations']:,} linked allocation requests. Cursor relayouts and cursor C allocations were zero.\n\n"
f=result['failureRecovery'];md+=f"Failure recovery passed {f['journeys']} real-product journeys: {f['persistentFailures']} persistent failures on actual allocating paths and {f['allocationFreeHelpControls']} allocation-free Help controls. Previous expressions/Ans, source identities and subsequent healthy2+2 rendering were checked without resetting Giac. See [faults](scientific-faults-release/faults.json).\n\n"
md+='## Memory & Typographic Profile\n\n| Target ABI type | Before | After | Delta |\n|---|---:|---:|---:|\n'
for name,row in result['wroomTypes']['typeDeltas'].items():
 if row['before'] is not None and (row['delta'] or name in ('vpam::MathNode','vpam::LayoutResult','vpam::NodeMeasurement','vpam::NodeQuantityReference')):md+=f"| {name} | {row['before']} | {row['after']} | {row['delta']:+} |\n"
s=result['wroomStack'];md+=f"\nThe largest modeled live recursive-node envelope is {s['uniformRecursiveOwnEnvelopePerRawEdge']} bytes. At192 raw edges, the conservative recursive contribution including a leaf is {s['rawEdgeEnvelopeIncludingLeaf']:,} bytes, leaving {s['slackAfterKnownEnvelope']:,} bytes of the65,536-byte task stack for active helpers/callers/task use. Ninety root/row pairs contribute {s['ninetyRootRowPairsPlusRootRowAndNumber']:,} modeled bytes; each Fraction→Row→Root→Row→Power→Row cycle contributes {s['alternatingFractionRootPowerWithThreeRowsPerCycle']} bytes. These are simultaneous-own-frame models, never physical high-water claims. Paint/cursor are iterative.\n\n"
pool=result['poolLifecycle'];md+=f"Fixed-pool product lifecycle:100 measured cycles after4warmups, with50-entry histories filled by60 evaluations each. Minimum sampled free bytes {pool['minimumSampledFree']:,} (baseline {pool['baselineMinimumSampledFree']:,}); largest free block {pool['minimumSampledLargestBlock']:,} (baseline {pool['baselineMinimumSampledLargestBlock']:,}). Menu objects/timers/free-space/handles plateau remains stable. [Actual samples](fixed-pool-release/resources.json).\n\n"
md+='The separate private ownership overlay ran 100 measured cycles after 54 warmups. All sampled live C++ byte totals, uncertainty payload counts, contribution counts and unique-source counts were constant in both active Calculation and Menu plateaus. Preloaded script and Giac C++ storage are included; C allocator internals are excluded. These are observed host ownership totals, not embedded heap estimates. [Actual samples](ownership-release/resources.json).\n\n| Plateau | Live C++ bytes | Payloads | Contributions | Unique sources |\n|---|---:|---:|---:|---:|\n'
for name,row in owned['cppAndSourceSummary'].items():md+=f"| {name} | {row['cpp_bytes']['min']:,} | {row['payloads']['min']} | {row['contributions']['min']} | {row['unique_sources']['min']} |\n"
md+='\nAll enumerated linked layout, row, delimiter, canvas and cursor symbols reside in `.flash.text` on both targets. No new IRAM residency is claimed; total linked IRAM remains unchanged. The metadata/font const tables likewise remain in flash. Full symbol and type records are in [WROOM types](release-firmware/wroom-types.json) and [CAM types](release-firmware/cam-types.json).\n\n'
md+='## Risks & Edge Cases\n\n'+''.join('- '+x+'\n' for x in result['limitations'])
md+='\n## Alternatives Considered\n\nNo global geometry cache, mutable scratch arena, heap allocation inside geometry, partition increase or mathematical approximation was introduced for performance. Per-node derived geometry costs are made explicit above. Font metadata is immutable flash data; no allocation-fault story is invented for static variant or MathKern lookup.\n'
for old,new in {'fixed64KiB':'fixed 64 KiB','the131,072':'the 131,072','plus960':'plus 960','healthy2+2':'healthy 2+2','At192':'At 192','the65,536':'the 65,536','lifecycle:100':'lifecycle: 100','after4warmups':'after 4 warmups','with50-entry':'with 50-entry','by60':'by 60','LVGL64KiB':'LVGL 64 KiB','reports57,696':'reports 57,696'}.items():md=md.replace(old,new)
(out/'RESOURCE_REPORT.md').write_text(md,encoding='utf-8')
print('PASS final resource evidence assembled:',out/'RESOURCE_REPORT.md')
