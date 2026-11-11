#!/usr/bin/env python3
"""Model live renderer call paths from compiler own frames, never stack peaks."""
import argparse,json,re
from pathlib import Path

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--resources',type=Path,required=True)
p.add_argument('--out',type=Path,required=True)
p.add_argument('--max-edges',type=int,default=192)
p.add_argument('--loop-bytes',type=int,default=65536)
p.add_argument('--unmodeled-reserve',type=int,default=8192)
a=p.parse_args()
r=json.loads(a.resources.read_text(encoding='utf-8'))
frames={x['name']:x['bytes'] for x in r['ownFrames'] if x['bytes'] is not None}
def frame(prefix,optional=False):
    found=[v for k,v in frames.items() if k.startswith(prefix+'(')]
    if not optional:assert found,prefix
    return max(found,default=0)
nodes={re.search(r'vpam::(Node\w+)::calculateLayout',k)[1]:v
       for k,v in frames.items() if re.search(r'vpam::Node\w+::calculateLayout',k)}
leaf={'NodeNumber','NodeOperator','NodeEmpty','NodeConstant','NodeVariable','NodePeriodicDecimal','NodeUnit','NodeSymbol','NodeSpecialValue'}
recursive={k:v for k,v in nodes.items() if k not in leaf}
row_extra={phase:frame('vpam::RowLayout::'+phase,True) for phase in ('calculate','measure','classify','place')}
# The three row phases are sequential. A recursive child keeps only measure
# live; a nested transparent row instead retains one active row phase each.
row_live=nodes['NodeRow']+row_extra['calculate']+row_extra['measure']
row_envelope=nodes['NodeRow']+row_extra['calculate']+max(row_extra[p] for p in ('measure','classify','place'))
weighted={**recursive,'NodeRow':row_envelope}
uniform=max(weighted.values())
max_leaf=max(nodes[k] for k in leaf if k in nodes)
known_envelope=a.max_edges*uniform+max_leaf
alternating_cycle=sum(nodes[n] for n in ('NodeFraction','NodeRoot','NodePower'))+3*row_live
ninety_roots=90*(nodes['NodeRoot']+row_live)+row_live+nodes.get('NodeNumber',0)
result=dict(target=r['target'],elfSha256=r['elfSha256'],maximumRawEdges=a.max_edges,
    ownNodeFrames=nodes,rowOwnHelpers=row_extra,recursiveOwnFrames=recursive,
    rowChildDescentLiveFrames=row_live,rowConservativeLiveEnvelope=row_envelope,
    uniformRecursiveOwnEnvelopePerRawEdge=uniform,
    largestRecursiveOwners=[k for k,v in weighted.items() if v==uniform],
    alternatingFractionRootPowerWithThreeRowsPerCycle=alternating_cycle,
    ninetyRootRowPairsPlusRootRowAndNumber=ninety_roots,
    rawEdgeEnvelopeIncludingLeaf=known_envelope,
    configuredLoopBytes=a.loop_bytes,unmodeledReserveBytes=a.unmodeled_reserve,
    slackAfterKnownEnvelope=a.loop_bytes-known_envelope,
    provisionalUniformDepthHasReserve=known_envelope+a.unmodeled_reserve<=a.loop_bytes,
    paintAndCursorOwnFrames={k:v for k,v in frames.items() if 'MathCanvas::' in k and any(n in k for n in ('onDraw','drawNode','preparePlacements','paintNode','nextPaintNode','computeCursorPosition'))},
    delimiterOwnFrames={k:v for k,v in frames.items() if 'DelimiterAssembler::' in k},
    limitations=[
      'Compiler entry instructions give own frames, not measured task stack peaks.',
      'Only functions simultaneously live on each stated path are added. Row measure/classify/place never accumulate on the same row.',
      'Uniform bound conservatively charges the largest live recursive-node envelope at every raw edge; it is a source-reviewed model, not an exhaustive machine-code call-graph proof.',
      'Reserve must cover active leaf/helper, external app/LVGL/task frames and interrupts. Hardware high-water and PSRAM latency are not measured.',
      'Painting and cursor use cached iterative placements; their own frames do not multiply by AST depth.',
      'Re-run after guard or layout edits: a larger FontMetrics may increase many frames.'
    ])
a.out.parent.mkdir(parents=True,exist_ok=True)
a.out.write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in result.items() if k not in ('ownNodeFrames','recursiveOwnFrames','paintAndCursorOwnFrames','delimiterOwnFrames','limitations')},indent=2))
