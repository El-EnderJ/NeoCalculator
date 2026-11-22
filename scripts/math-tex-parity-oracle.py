#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Independent primary-rule constraints over observed native glyph geometry.

Does not import NumOS geometry helpers. MATH constants are extracted directly
from the licensed in-tree input font; no font is copied. Exact style and
lockstep checks use zero tolerance. Integer position checks report signed bias.
"""
import argparse,collections,copy,hashlib,json,math
from pathlib import Path
from fontTools.ttLib import TTFont
ROOT=Path(__file__).resolve().parents[1]
TYPES=['Row','Number','Operator','Empty','Fraction','Power','Root','Paren','Function','LogBase','Constant','Variable','PeriodicDecimal','DefIntegral','Summation','Subscript','BigOp','Symbol','SpecialValue','Collection','Equation','Matrix','Interval','Piecewise','Call','Unevaluated','Unit','QuantityReference','Measurement']
SOURCE={'tex':'https://tug.org/texlive/devsrc/Build/source/texk/web2c/tex.web','math':'https://learn.microsoft.com/en-us/typography/opentype/spec/math'}
# TeX WEB math_spacing literal, independently verified in MATH-TEX-01.
TEX_SPACING='0234000122*4000133**3**344*0400400*000000234000111*1111112341011'
def load(path):
 cases=[];case=None;frame=None;summary={}
 for line in path.read_text(encoding='utf-8').splitlines():
  if not line.startswith('{'):continue
  e=json.loads(line);kind=e['event']
  if kind=='case':
   case=dict(id=e['id'],style=e['style'],frames=[dict(nodes={},glyphs=[],lines=[],rects=[],missing=[],delimiters=[],strokes=[],cursors=[])]);cases.append(case);frame=case['frames'][0]
  elif kind=='frame':frame=dict(nodes={},glyphs=[],lines=[],rects=[],missing=[],delimiters=[],strokes=[],cursors=[],focus=e);case['frames'].append(frame)
  elif kind=='node':frame['nodes'][e['path']]=e
  elif kind in ('glyph','line','rect','missing','delimiter','cursor'):frame[{'glyph':'glyphs','line':'lines','rect':'rects','missing':'missing','delimiter':'delimiters','cursor':'cursors'}[kind]].append(e)
  elif kind=='stroke':frame['strokes'].append(e)
  elif kind=='delimiterRequest':frame.setdefault('delimiterRequests',[]).append(e)
  elif kind=='cursorBounds':frame['bounds']=e
  elif kind=='profile':case['profile']=e
  elif kind in ('random','sizes'):summary[kind]=e
 return cases,summary
def styles(parent,kind,index):
 size=parent&3;cr=parent&4
 sup=(2 if size<2 else 3)|cr
 if kind=='Power' and index==1:return sup
 if kind=='Subscript' and index==1:return (2 if size<2 else 3)|4
 # TXP PD-4 / §H.2: at script level 0 (Display/Text) numerator and denominator
 # keep the base size and use Text style; from Script on they step down.
 # Cramping is unaffected: numerator inherits it, denominator is cramped.
 if kind=='Fraction':return (1 if size<=1 else 3)|(4 if index==1 else cr)
 # Root degree takes one ordinary (uncramped) script step, never two at level 0
 # (MATH-READABILITY-REGRESSION-01B; 8 px indices are the size PD-4 rejects).
 if kind=='Root':return parent|4 if index==0 else (2 if size<2 else 3)
 if kind=='LogBase' and index==0:return (2 if size<2 else 3)|4
 if kind in ('DefIntegral','Summation','BigOp') and index<2:return (2 if size<2 else 3)|(4 if index==0 else cr)
 return parent

def primitive_bounds(event):
 """Half-open submitted raster bounds; independent of AST ink caches.

 Glyph boxes use the actual resolved raster. Assembly seam clipping is applied
 before aggregation. Integer line footprints follow LVGL's pixel convention:
 the odd central pixel belongs to the positive half-open side. This avoids
 interpreting a one-pixel line as an artificial half-pixel cache underrun.
 """
 kind=event['event']
 if kind in ('glyph','delimiter'):
  if 'inkX' not in event:return None
  b=[event['inkX'],event['inkY'],event['inkX']+event['inkW'],event['inkY']+event['inkH']]
  if kind=='delimiter':
   c=event['clip'];b=[max(b[0],c[0]),max(b[1],c[1]),min(b[2],c[2]+1),min(b[3],c[3]+1)]
 elif kind=='rect':b=[event['x'],event['y'],event['x']+event['width'],event['y']+event['height']]
 elif kind in ('line','stroke'):
  lo=event['thickness']//2;hi=(event['thickness']-1)//2
  b=[min(event['x1'],event['x2'])-lo,min(event['y1'],event['y2'])-lo,max(event['x1'],event['x2'])+hi+1,max(event['y1'],event['y2'])+hi+1]
 else:return None
 return b if b[0]<b[2] and b[1]<b[3] else None
def analyze(cases,constants,upm,font=None):
 findings=[];refs=[];counts=collections.Counter();coverage=set();comparisons=0
 cmap=font.getBestCmap() if font else {};kern={};italic={};variants={}
 if font:
  info=font['MATH'].table.MathGlyphInfo
  kern=dict(zip(info.MathKernInfo.MathKernCoverage.glyphs,info.MathKernInfo.MathKernInfoRecords))
  italic={g:v.Value for g,v in zip(info.MathItalicsCorrectionInfo.Coverage.glyphs,info.MathItalicsCorrectionInfo.ItalicsCorrection)}
  variants=dict(zip(font['MATH'].table.MathVariants.VertGlyphCoverage.glyphs,font['MATH'].table.MathVariants.VertGlyphConstruction))
 def rounded(v):return math.floor(v+0.5) if v>=0 else -math.floor(-v+0.5)
 def cutin(cp,corner,y,em):
  rec=kern.get(cmap.get(cp));table=getattr(rec,corner+'MathKern',None) if rec else None
  if not table:return 0
  # Compare pixel height directly against design height scaled to this glyph.
  # No NumOS generated kern table or lookup helper enters this reference.
  i=sum(y>=h.Value*em/upm for h in table.CorrectionHeight)
  return table.KernValue[i].Value*em/upm
 def report(case,path,rule,actual,expected,severity='S2'):
  nonlocal comparisons
  comparisons+=1
  if actual!=expected:findings.append(dict(case=case['id'],style=case['style'],path=path,rule=rule,actual=actual,expected=expected,severity=severity))
 def px(name,em,minimum=0):return max(minimum,math.floor(constants[name]*em/upm+0.5))
 for case in cases:
  base=case['frames'][0];nodes=base['nodes']
  emitted=[(e,primitive_bounds(e)) for name in ('glyphs','delimiters','rects','lines','strokes') for e in base.get(name,[])]
  emitted=[(e,b) for e,b in emitted if b is not None]
  observed={}
  for p in nodes:
   parts=[b for e,b in emitted if e['path']==p or e['path'].startswith(p+'/')]
   if parts:observed[p]=[min(b[0] for b in parts),min(b[1] for b in parts),max(b[2] for b in parts),max(b[3] for b in parts)]
  report(case,'r','no-missing-glyphs',base['missing'],[],'S1')
  def nucleus(n):
   while TYPES[n['type']] in ('Row','QuantityReference') and n['children']==1:n=nodes[n['path']+'/0']
   return n
  def ia(n):return n['inkAscent'] if n['inkAscent'] or n['inkDescent'] else n['ascent']
  def idepth(n):return n['inkDescent'] if n['inkAscent'] or n['inkDescent'] else n['descent']
  def children(n):return [nodes[n['path']+'/'+str(i)] for i in range(n['children'])]
  def atoms(n):
   if TYPES[n['type']]=='Row':
    for c in children(n):yield from atoms(c)
   else:yield n
  def boundary(n):
   kind=TYPES[n['type']]
   if kind in ('Power','Subscript'):return boundary(children(n)[0])
   if kind=='Row':return boundary(children(n)[0]) if n['children']==1 else (0,0)
   if kind=='Paren':return (7,7)
   if kind=='Collection':return (4,5) # Declared collection fence policy.
   if kind=='Function' and (n.get('functionKind')==8 or case['id'] in ('F-factorial','F-factorial-adjacency')):return (0,0)
   if kind in ('Function','LogBase'):return (1,0)
   return n['class'],n['class']
  for rowNode in nodes.values():
   if TYPES[rowNode['type']]!='Row':continue
   parent=nodes.get(rowNode['path'].rsplit('/',1)[0]) if rowNode['path']!='r' else None
   if parent and TYPES[parent['type']]=='Row':continue
   items=list(atoms(rowNode));effective=[list(boundary(n)) for n in items];previous=None
   for i,n in enumerate(items):
    if TYPES[n['type']]=='Empty':continue
    if effective[i][0]==2 and (previous is None or effective[previous][1] in (1,2,3,4,6)):effective[i]=[0,0]
    elif effective[i][0] in (3,5,6) and previous is not None and effective[previous][1]==2:effective[previous]=[0,0]
    previous=i
   if previous is not None and effective[previous][1]==2:effective[previous]=[0,0]
   previous=None;pen=rowNode['x']
   for i,n in enumerate(items):
    if TYPES[n['type']]!='Empty':
     if previous is not None:
      code=TEX_SPACING[previous*8+effective[i][0]]
      mu=3 if code=='2' else {'1':3,'3':4,'4':5}.get(code,0) if rowNode['style']&3<2 else 0
      pen+=mu*rowNode['em']//18
     previous=effective[i][1]
    report(case,n['path'],'tex-row-atom-origin',n['x'],pen)
    report(case,n['path'],'tex-contextual-classes',[n['left'],n['right']],effective[i])
    pen+=n['width']
   report(case,rowNode['path'],'tex-row-width',rowNode['width'],pen-rowNode['x'])
  pairs={};pairNuclei={}
  for pth,pn in nodes.items():
   if TYPES[pn['type']]!='Power':continue
   bn=nucleus(nodes[pth+'/0'])
   if TYPES[bn['type']]!='Subscript':continue
   nn=nodes[bn['path']+'/0'];sn=nodes[bn['path']+'/1'];un=nodes[pth+'/1'];em=pn['em'];atom=TYPES[nucleus(nn)['type']] in ('Number','Variable','Constant','Symbol','Unit')
   u0=max(px('SuperscriptShiftUpCramped' if pn['style']&4 else 'SuperscriptShiftUp',em),px('SuperscriptBottomMin',em)+idepth(un),0 if atom else nn['ascent']-px('SuperscriptBaselineDropMax',em))
   v0=max(px('SubscriptShiftDown',em),ia(sn)-px('SubscriptTopMax',em),0 if atom else nn['descent']+px('SubscriptBaselineDropMin',em))
   gap=px('SubSuperscriptGapMin',em);cap=max(u0,px('SuperscriptBottomMaxWithSubscript',em)+idepth(un))
   maxExtra=max(0,gap+idepth(un)+ia(sn)-u0-v0)
   candidates=[(u+v,-u,u,v) for u in range(u0,cap+1) for v in range(v0,v0+maxExtra+1) if u+v-idepth(un)-ia(sn)>=gap]
   _,_,ue,ve=min(candidates);pairs[pth]=ue;pairs[bn['path']]=ve;pairNuclei[pth]=nn
  for path,node in nodes.items():
   kind=TYPES[node['type']];coverage.add(kind);counts[kind]+=1
   if node['children'] and path in observed:
    actual=observed[path];reported=[node['x'],node['baseline']-ia(node),node['x']+node['width'],node['baseline']+idepth(node)]
    enclosed=actual[1]>=reported[1] and actual[3]<=reported[3]
    report(case,path,'compound-ink-encloses-emitted',enclosed,True,'S1')
    if not enclosed:findings[-1].update(observedInk=actual,reportedInk=reported,nodeType=kind)
    refs.append(dict(case=case['id'],style=case['style'],path=path,rule='ObservedInk',nodeType=kind,observedInk=actual,reportedInk=reported,enclosed=enclosed))
   if kind in ('Fraction','Root'):
    rules=[(e,b) for e,b in emitted if e['event']=='rect' and e['path']==path]
    for rule,rb in rules:
     for e,b in emitted:
      if not e['path'].startswith(path+'/'):continue
      if kind=='Root' and not (e['path']==path+'/0' or e['path'].startswith(path+'/0/')):continue
      intersects=max(rb[0],b[0])<min(rb[2],b[2]) and max(rb[1],b[1])<min(rb[3],b[3])
      report(case,path,'rule-excludes-descendant-ink',not intersects,True,'S1')
      if intersects:findings[-1].update(ruleBox=rb,descendantBox=b,descendantPath=e['path'],primitive=e['event'])
   leafGlyphs=[g for g in base['glyphs'] if g['path']==path]
   for g in leafGlyphs:
    report(case,path,'logical-encloses-glyph-y',g['inkY']>=node['baseline']-node['ascent'] and g['inkY']+g['inkH']<=node['baseline']+node['descent'],True,'S1')
   for g in (d for d in base['delimiters'] if d['path']==path and 'inkX' in d):
    # Extender pieces may overrun their seam band before LVGL clipping. Compare
    # the submitted visible raster, preserving half-open rectangle semantics.
    clip=g['clip'];left=max(g['inkX'],clip[0]);right=min(g['inkX']+g['inkW'],clip[2]+1);top=max(g['inkY'],clip[1]);bottom=min(g['inkY']+g['inkH'],clip[3]+1)
    if right>left and bottom>top:
     report(case,path,'logical-encloses-delimiter-x',left>=node['x'] and right<=node['x']+node['width'],True,'S1')
     report(case,path,'logical-encloses-delimiter-y',top>=node['baseline']-node['ascent'] and bottom<=node['baseline']+node['descent'],True,'S1')
   for s in (d for d in base.get('strokes',[]) if d['path']==path):
    # Rounded caps expand the segment centerline by half the stroke width.
    radius=s['thickness']/2
    report(case,path,'logical-encloses-vector-x',min(s['x1'],s['x2'])-radius>=node['x'] and max(s['x1'],s['x2'])+radius<=node['x']+node['width'],True,'S1')
    report(case,path,'logical-encloses-vector-y',min(s['y1'],s['y2'])-radius>=node['baseline']-node['ascent'] and max(s['y1'],s['y2'])+radius<=node['baseline']+node['descent'],True,'S1')
    if kind in ('Paren','Piecewise','Collection','Interval'):
     contents=[n for p,n in nodes.items() if p.startswith(path+'/')]
     if contents:
      left=min(n['x'] for n in contents);right=max(n['x']+n['width'] for n in contents)
      outside=max(s['x1'],s['x2'])+radius<=left or min(s['x1'],s['x2'])-radius>=right
      report(case,path,'vector-content-separation',outside,True,'S1')
   for request in (d for d in base.get('delimiterRequests',[]) if d['path']==path and d['cp'] in (123,124,125)):
    if request['bottom']-request['top']<=node['em']*2:continue
    # A scalable synthetic delimiter must occupy the requested vertical slot.
    # Match its side by the request origin, independently of production policy.
    requests=[d for d in base['delimiterRequests'] if d['path']==path]
    nextX=min((d['x'] for d in requests if d['x']>request['x']),default=node['x']+node['width'])
    pieces=[(g['inkY'],g['inkY']+g['inkH']) for g in base['delimiters'] if g['path']==path and request['x']<=g['inkX']<nextX]
    pieces += [(min(s['y1'],s['y2'])-s['thickness']/2,max(s['y1'],s['y2'])+s['thickness']/2) for s in base.get('strokes',[]) if s['path']==path and request['x']<=min(s['x1'],s['x2'])<nextX]
    report(case,path,'synthetic-delimiter-target-height',bool(pieces) and min(a for a,b in pieces)<=request['top']+1 and max(b for a,b in pieces)>=request['bottom']-1,True,'S1')
   if font and kind in ('Variable','Symbol') and len(leafGlyphs)==1:
    g=leafGlyphs[0]
    if 0x1D434<=g['cp']<=0x1D467 or 0x1D6E2<=g['cp']<=0x1D71B:
     reserve=max(0,rounded(italic.get(cmap.get(g['cp']),0)*node['em']/upm))
     report(case,path,'identifier-italic-reserve',node['width'],g['x']-node['x']+g['advance']+reserve)
   report(case,path,'style-physical-em',node['em'],[18,18,12,8][node['style']&3])
   if path!='r':
    pp,si=path.rsplit('/',1);parent=nodes[pp];pk=TYPES[parent['type']];expected=styles(parent['style'],pk,int(si));report(case,path,'style-transition',node['style'],expected)
   child=[nodes.get(path+'/'+str(i)) for i in range(node['children'])]
   # Constraint solver: select integer upward/downward baseline displacements
   # satisfying independent OpenType minima, nearest nominal MATH shift.
   if kind in ('Power','Subscript') and all(child):
    b,s=child;em=node['em'];b=pairNuclei.get(path,b);atomic=b
    while TYPES[atomic['type']] in ('Row','QuantityReference') and atomic['children']==1:atomic=nodes[atomic['path']+'/0']
    is_atomic=TYPES[atomic['type']] in ('Number','Variable','Constant','Symbol','Unit')
    scriptInkA=s['inkAscent'] if s['inkAscent'] or s['inkDescent'] else s['ascent']
    scriptInkD=s['inkDescent'] if s['inkAscent'] or s['inkDescent'] else s['descent']
    if kind=='Power':
     constraints=[px('SuperscriptShiftUpCramped' if node['style']&4 else 'SuperscriptShiftUp',em),px('SuperscriptBottomMin',em)+scriptInkD]
     if not is_atomic:constraints.append(b['ascent']-px('SuperscriptBaselineDropMax',em))
     actual=node['baseline']-s['baseline'];expect=next(y for y in range(32768) if all(y>=c for c in constraints))
     expect=pairs.get(path,expect);report(case,path,'superscript-baseline',actual,expect)
    else:
     constraints=[px('SubscriptShiftDown',em),scriptInkA-px('SubscriptTopMax',em)]
     if not is_atomic:constraints.append(b['descent']+px('SubscriptBaselineDropMin',em))
     actual=s['baseline']-node['baseline'];expect=next(y for y in range(32768) if all(y>=c for c in constraints))
     expect=pairs.get(path,expect);report(case,path,'subscript-baseline',actual,expect)
    refs.append(dict(case=case['id'],style=case['style'],path=path,rule=kind,expectedBaselineShift=expect,actualBaselineShift=actual,constraintMinima=constraints))
    bg=[g for g in base['glyphs'] if g['path']==b['path'] or g['path'].startswith(b['path']+'/')]
    sg=[g for g in base['glyphs'] if g['path']==s['path'] or g['path'].startswith(s['path']+'/')]
    if font and is_atomic and len(bg)==len(sg)==1 and TYPES[atomic['type']]!='Unit':
     gb,gs=bg[0],sg[0];up=kind=='Power';baseEdge=(gb['baseline']-gb['inkY']) if up else -(gb['inkY']+gb['inkH']-gb['baseline']);scriptEdge=-(gs['inkY']+gs['inkH']-gs['baseline']) if up else (gs['baseline']-gs['inkY']);shift=actual if up else -actual
     cornerB='TopRight' if up else 'BottomRight';cornerS='BottomLeft' if up else 'TopLeft'
     samples=[baseEdge,shift+scriptEdge]
     sums=[cutin(gb['cp'],cornerB,y,b['em'])+cutin(gs['cp'],cornerS,y-shift,s['em']) for y in samples]
     ic=max(0,rounded(italic.get(cmap.get(gb['cp']),0)*em/upm)) if up else 0
     # Actual unkerned glyph advance is independent of the logical atom's
     # trailing italic reserve. One IC attaches the superscript; zero attaches
     # the subscript. This also detects accidental double application.
     expectedX=gb['x']-b['x']+gb['advance']+ic+rounded(min(sums));actualX=s['x']-node['x']
     report(case,path,'mathkern-script-x',actualX,expectedX)
     refs.append(dict(case=case['id'],style=case['style'],path=path,rule='MathKern',baseCodepoint=gb['cp'],scriptCodepoint=gs['cp'],contactHeights=samples,cornerSums=sums,italicCorrection=ic,expectedX=expectedX,actualX=actualX))
   if kind=='Fraction' and all(child):
    num,den=child;em=node['em'];display=(node['style']&3)==0;axis=px('AxisHeight',em);rule=px('FractionRuleThickness',em,1)
    gapN=px('FractionNumDisplayStyleGapMin' if display else 'FractionNumeratorGapMin',em)
    gapD=px('FractionDenomDisplayStyleGapMin' if display else 'FractionDenominatorGapMin',em)
    prefN=px('FractionNumeratorDisplayStyleShiftUp' if display else 'FractionNumeratorShiftUp',em)
    prefD=px('FractionDenominatorDisplayStyleShiftDown' if display else 'FractionDenominatorShiftDown',em)
    # Integer pixel bar occupies [axis-ceil(t/2),axis+floor(t/2)).
    numD=num['inkDescent'] if num['inkAscent'] or num['inkDescent'] else num['descent']
    denA=den['inkAscent'] if den['inkAscent'] or den['inkDescent'] else den['ascent']
    nmin=[prefN,axis+(rule+1)//2+gapN+numD]
    dmin=[prefD,-axis+rule//2+gapD+denA]
    en=next(y for y in range(32768) if all(y>=x for x in nmin));ed=next(y for y in range(32768) if all(y>=x for x in dmin))
    an=node['baseline']-num['baseline'];ad=den['baseline']-node['baseline']
    report(case,path,'fraction-numerator-baseline',an,en);report(case,path,'fraction-denominator-baseline',ad,ed)
    painted=[r for r in base['rects'] if r['path']==path]
    if painted:
     report(case,path,'fraction-bar-rect',[(r['x'],r['y'],r['width'],r['height']) for r in painted],[(node['x'],node['baseline']-axis-(rule+1)//2,node['width'],rule)])
     for bar in painted:
      if num['path'] in observed:
       actualGap=bar['y']-observed[num['path']][3]
       report(case,path,'fraction-observed-numerator-gap',actualGap>=gapN,True,'S1')
       if actualGap<gapN:findings[-1].update(actualGap=actualGap,minimumGap=gapN)
       refs.append(dict(case=case['id'],style=case['style'],path=path,rule='ObservedGap',target='numerator',actualGap=actualGap,minimumGap=gapN))
      if den['path'] in observed:
       actualGap=observed[den['path']][1]-bar['y']-bar['height']
       report(case,path,'fraction-observed-denominator-gap',actualGap>=gapD,True,'S1')
       if actualGap<gapD:findings[-1].update(actualGap=actualGap,minimumGap=gapD)
       refs.append(dict(case=case['id'],style=case['style'],path=path,rule='ObservedGap',target='denominator',actualGap=actualGap,minimumGap=gapD))
    refs.append(dict(case=case['id'],style=case['style'],path=path,rule='Fraction',nominalNumerator=prefN,nominalDenominator=prefD,expectedNumerator=en,expectedDenominator=ed,actualNumerator=an,actualDenominator=ad,barBaseline=node['baseline']-axis,ruleThickness=rule))
   if kind=='Root' and all(child):
    rad=child[0];em=node['em'];gap=px('RadicalDisplayStyleVerticalGap' if node['style']&3==0 else 'RadicalVerticalGap',em);rule=px('RadicalRuleThickness',em,1)
    expectedTop=rad['baseline']-ia(rad)-gap-rule;expectedWidth=rad['width']+max(1,2*em//18)
    painted=[(r['x'],r['y'],r['width'],r['height']) for r in base['rects'] if r['path']==path]
    if not painted:painted=[(r['x1'],r['y1']-r['thickness']//2,r['x2']-r['x1']+1,r['thickness']) for r in base['lines'] if r['path']==path and r['y1']==r['y2']]
    report(case,path,'radical-bar-geometry',painted,[(rad['x'],expectedTop,expectedWidth,rule)])
    if rad['path'] in observed:
     for bx,by,bw,bh in painted:
      actualGap=observed[rad['path']][1]-by-bh
      report(case,path,'radical-observed-radicand-gap',actualGap>=gap,True,'S1')
      if actualGap<gap:findings[-1].update(actualGap=actualGap,minimumGap=gap)
      refs.append(dict(case=case['id'],style=case['style'],path=path,rule='ObservedGap',target='radicand',actualGap=actualGap,minimumGap=gap))
    report(case,path,'radical-baseline-preserved',rad['baseline'],node['baseline'])
    report(case,path,'radical-extra-ascender',node['baseline']-node['ascent']<=expectedTop-px('RadicalExtraAscender',em,1),True)
    if len(child)>1:
     degree=child[1];bottom=rad['baseline']+rad['descent'];height=bottom-expectedTop
     targetBottom=bottom-height*constants['RadicalDegreeBottomRaisePercent']//100
     report(case,path,'radical-degree-bottom',degree['baseline']+idepth(degree),targetBottom)
    refs.append(dict(case=case['id'],style=case['style'],path=path,rule='Root',ruleTop=expectedTop,ruleThickness=rule,inkGap=gap,vectorPolicyMu=dict(hook=3,slope=6,rightPad=2)))
   if kind=='PeriodicDecimal':
    count=node.get('repeatCount',1 if case['id']=='H-decimal' else 0)
    if count and len(leafGlyphs)>=count:
     repeated=leafGlyphs[-count:];em=node['em'];rule=px('OverbarRuleThickness',em,1);gap=px('OverbarVerticalGap',em);extra=px('OverbarExtraAscender',em,1)
     expectedTop=min(g['inkY'] for g in repeated)-gap-rule;expectedWidth=sum(g['advance'] for g in repeated)
     painted=[(r['x'],r['y'],r['width'],r['height']) for r in base['rects'] if r['path']==path]
     if not painted:painted=[(r['x1'],r['y1']-r['thickness']//2,r['x2']-r['x1']+1,r['thickness']) for r in base['lines'] if r['path']==path and r['y1']==r['y2']]
     report(case,path,'periodic-overbar-rect',painted,[(repeated[0]['x'],expectedTop,expectedWidth,rule)])
     report(case,path,'periodic-extra-ascender',node['baseline']-node['ascent']<=expectedTop-extra,True)
     refs.append(dict(case=case['id'],style=case['style'],path=path,rule='PeriodicOverbar',expectedTop=expectedTop,expectedWidth=expectedWidth,ruleThickness=rule,gap=gap,extraAscender=extra))
   if kind in ('DefIntegral','Summation','BigOp') and all(child):
    opCodes=[0x2211,0x222B,0x220F,0x2210,0x22C2,0x22C3,0x22C0,0x22C1]
    byCase={'I-product':0x220F,'L-bigop':0x220F,'I-coproduct':0x2210,'I-intersection':0x22C2,'I-union':0x22C3,'I-logicaland':0x22C0,'I-logicalor':0x22C1}
    cp=0x222B if kind=='DefIntegral' else 0x2211 if kind=='Summation' else node.get('operatorCp',byCase.get(case['id']))
    display=(node['style']&3)==0;em=node['em'];lower,upper,body=child[:3]
    drawn=[g for g in leafGlyphs if g['cp'] in opCodes or 0xE040<=g['cp']<=0xE04F]
    report(case,path,'operator-one-symbol',len(drawn),1)
    if cp in opCodes and drawn:
     glyph=drawn[0];expectedCp=0xE040+2*opCodes.index(cp)+int(display)
     report(case,path,'operator-selected-variant',glyph['cp'],expectedCp)
     ladder=variants[cmap[cp]].MathGlyphVariantRecord;variant=ladder[-1 if display else 0].VariantGlyph
     correction=rounded(italic.get(variant,0)*em/upm);w=max(glyph['advance'],glyph['inkX']+glyph['inkW']-glyph['x'])
     a=node['baseline']-glyph['inkY'];d=glyph['inkY']+glyph['inkH']-node['baseline'];axis=px('AxisHeight',em)
     report(case,path,'operator-axis-center',2*glyph['inkY']+glyph['inkH'],2*(node['baseline']-axis)-glyph['inkH']%2)
     # TXP section H.9.4 (GAP-37): one gate for Summation, DefIntegral and BigOp. Stacked limits need display style AND
     # the ladder's selected glyph >= DisplayOperatorMinHeight. The ladder height is derived here from the font's own base
     # variant record, not from the product's assembler. The drawn glyph (variant above) never decides this.
     gateHeight=math.ceil(ladder[0].AdvanceMeasurement*em/upm)
     stacked=display and gateHeight>=px('DisplayOperatorMinHeight',em)
     if stacked:
      umin=[a+px('UpperLimitBaselineRiseMin',em),a+px('UpperLimitGapMin',em)+idepth(upper)]
      vmin=[d+px('LowerLimitBaselineDropMin',em),d+px('LowerLimitGapMin',em)+ia(lower)]
      ue=next(y for y in range(32768) if all(y>=c for c in umin));ve=next(y for y in range(32768) if all(y>=c for c in vmin))
      exU=glyph['x']+math.trunc((w-upper['width']+correction)/2);exL=glyph['x']+math.trunc((w-lower['width']-correction)/2)
     else:
      u0=max(px('SuperscriptShiftUpCramped' if node['style']&4 else 'SuperscriptShiftUp',em),idepth(upper)+px('SuperscriptBottomMin',em),a-px('SuperscriptBaselineDropMax',em))
      v0=max(px('SubscriptShiftDown',em),ia(lower)-px('SubscriptTopMax',em),d+px('SubscriptBaselineDropMin',em))
      gap=px('SubSuperscriptGapMin',em);cap=max(u0,px('SuperscriptBottomMaxWithSubscript',em)+idepth(upper));maxExtra=max(0,gap+idepth(upper)+ia(lower)-u0-v0)
      candidates=[(u+v,-u,u,v) for u in range(u0,cap+1) for v in range(v0,v0+maxExtra+1) if u+v-idepth(upper)-ia(lower)>=gap]
      _,_,ue,ve=min(candidates);exU=glyph['x']+w+correction;exL=glyph['x']+w
     report(case,path,'operator-upper-baseline',node['baseline']-upper['baseline'],ue)
     report(case,path,'operator-lower-baseline',lower['baseline']-node['baseline'],ve)
     report(case,path,'operator-upper-x',upper['x'],exU);report(case,path,'operator-lower-x',lower['x'],exL)
     report(case,path,'operator-body-baseline',body['baseline'],node['baseline'])
     refs.append(dict(case=case['id'],style=case['style'],path=path,rule='LargeOperator',baseCodepoint=cp,selectedGlyphName=variant,selectedCodepoint=expectedCp,italicCorrection=correction,expectedUpperBaseline=ue,expectedLowerBaseline=ve,expectedUpperX=exU,expectedLowerX=exL,policy='Display uses source terminal display variant and over/under limits; other styles use source base variant with side scripts.'))
   for c in child:
    if not c:continue
    # Bounding boxes must enclose all placed children. Advance overhang is
    # reported independently from ink; no font side-bearing is guessed.
    top=node['baseline']-node['ascent'];bottom=node['baseline']+node['descent'];ct=c['baseline']-c['ascent'];cb=c['baseline']+c['descent']
    report(case,c['path'],'parent-encloses-child-top',ct>=top,True,'S1')
    report(case,c['path'],'parent-encloses-child-bottom',cb<=bottom,True,'S1')
    report(case,c['path'],'parent-encloses-child-x',c['x']>=node['x'] and c['x']+c['width']<=node['x']+node['width'],True,'S1')
  for frame in case['frames']:
   # onDraw computes once before pan, then again after pan. Only the last
   # cursor record is the submitted cursor; the first is viewport planning.
   for c in frame['cursors'][-1:]:
    n=frame['nodes'].get(c['path'])
    if n:
     report(case,c['path'],'cursor-draw-origin',c['origin'],n['x'],'S0');report(case,c['path'],'cursor-draw-baseline',c['baseline'],n['baseline'],'S0');report(case,c['path'],'cursor-draw-style',c['style'],n['style'],'S0')
     child=frame['nodes'].get(c['path']+'/'+str(c['index']))
     report(case,c['path'],'cursor-logical-boundary',c['origin']+c['offset'],child['x'] if child else n['x']+n['width'],'S0')
  report(case,'r','zero-new-layout',case['profile']['allocations'],0,'S1');report(case,'r','repeat-layout-stable',case['profile']['stable'],True,'S1')
  if 'cloneRoles' in case['profile']:report(case,'r','clone-preserves-role',case['profile']['cloneRoles'],True,'S0')
  if case['id']=='K-reference-power':report(case,'r','scientific-descriptive-subscript-upright',[g['cp'] for g in base['glyphs']],[119898,101,50])
 return findings,refs,dict(nodeCoverage=sorted(coverage),missingNodeTypes=sorted(set(TYPES)-coverage),observedNodes=sum(counts.values()),nodeCounts=dict(counts),comparisons=comparisons)
def main():
 p=argparse.ArgumentParser();p.add_argument('--geometry',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--require-style-pass',action='store_true');p.add_argument('--require-all-pass',action='store_true');a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
 fontPath=ROOT/'assets/fonts/STIXTwoMath-Regular.ttf';font=TTFont(fontPath);constants={}
 for k,v in vars(font['MATH'].table.MathConstants).items():
  if k.startswith('_'):continue
  if isinstance(v,(float,int)):constants[k]=v
  elif hasattr(v,'Value'):constants[k]=v.Value
 cases,summary=load(a.geometry/'geometry.log');issues,refs,stats=analyze(cases,constants,font['head'].unitsPerEm,font)
 for sub in ['reference-geometry','numos-geometry','diffs','overlays']:(a.out/sub).mkdir(exist_ok=True)
 (a.out/'reference-oracle-version.json').write_text(json.dumps(dict(version='NumOS-primary-constraints-v4',source=SOURCE,font=dict(path='assets/fonts/STIXTwoMath-Regular.ttf',sha256=hashlib.sha256(fontPath.read_bytes()).hexdigest(),version=font['name'].getDebugName(5),upm=font['head'].unitsPerEm),constants=constants,policy=dict(styleTolerancePx=0,geometryReportingTolerancePx=0,reviewTolerancePx=1,systematicBias='all signed deviations are retained; no tolerance suppresses evidence',reference='constraint oracle using identical glyph boxes and input-font MATH values; not TeX raster equality',limits='TeX spacing literal re-audited; Power(Subscript) shares a nucleus, while reverse Subscript(Power) remains structural nesting. Vector radical/brace/bar policies are product choices; glyph outlines and bitmap advances differ.')),indent=2))
 (a.out/'reference-geometry/rules.json').write_text(json.dumps(refs,indent=2))
 (a.out/'numos-geometry/cases.json').write_text(json.dumps(cases,indent=2))
 (a.out/'diffs/findings.json').write_text(json.dumps(issues,indent=2))
 observedRules={'compound-ink-encloses-emitted','rule-excludes-descendant-ink','fraction-observed-numerator-gap','fraction-observed-denominator-gap','radical-observed-radicand-gap'}
 observedCases=[]
 observedByCase=collections.defaultdict(list)
 for row in refs:
  if row['rule'] in ('ObservedInk','ObservedGap'):observedByCase[row['case'],row['style']].append(row)
 issueByCase=collections.defaultdict(list)
 for row in issues:
  if row['rule'] in observedRules:issueByCase[row['case'],row['style']].append(row)
 for case in cases:
  key=case['id'],case['style'];checks=observedByCase[key]
  observedCases.append(dict(id=case['id'],style=case['style'],compoundChecks=[r for r in checks if r['rule']=='ObservedInk'],gapChecks=[r for r in checks if r['rule']=='ObservedGap'],findings=issueByCase[key],pass_=not issueByCase[key]))
 manifest=a.geometry/'source-object-manifest.json';executable=a.geometry/'probe.exe'
 (a.out/'observed-ink-gate.json').write_text(json.dumps(dict(version=1,geometry=str(a.geometry),geometrySha256=hashlib.sha256((a.geometry/'geometry.log').read_bytes()).hexdigest(),sourceObjectManifestSha256=hashlib.sha256(manifest.read_bytes()).hexdigest() if manifest.exists() else None,captureExecutableSha256=hashlib.sha256(executable.read_bytes()).hexdigest() if executable.exists() else None,oracleSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),policy='Actual submitted primitive unions; clipped assembly raster; no cached child ink used for gap measurements.',cases=observedCases),indent=2))
 signed=collections.defaultdict(list)
 for x in issues:
  if type(x['actual']) is int and type(x['expected']) is int:signed[x['rule']].append(x['actual']-x['expected'])
 bias={k:dict(n=len(v),mean=sum(v)/len(v),minimum=min(v),maximum=max(v)) for k,v in signed.items()}
 summary.update(stats);summary.update(cases=len(cases),rulesFailed=dict(collections.Counter(x['rule'] for x in issues)),signedBias=bias,issues=len(issues))
 # Negative control mutates observed data only. Real rule detector must reject
 # a style drift, baseline drift and cursor-origin drift independently.
 mutated=copy.deepcopy(cases[:1]);n=next(iter(mutated[0]['frames'][0]['nodes'].values()));n['em']+=1
 bad,_,_=analyze(mutated,constants,font['head'].unitsPerEm,font);summary['negativeControl']=dict(emMutationDetected=any(x['rule']=='style-physical-em' for x in bad))
 target=next((c for c in cases if c['id']=='B-square' and c['style']==1),None)
 if target:
  mutant=copy.deepcopy(target);mutant['frames'][0]['nodes']['r/0/1']['baseline']+=2
  bad,_,_=analyze([mutant],constants,font['head'].unitsPerEm,font);summary['negativeControl']['superscriptShiftMutationDetected']=any(x['rule']=='superscript-baseline' for x in bad)
 target=next((c for c in cases if any(f['cursors'] for f in c['frames'])),None)
 if target:
  mutant=copy.deepcopy(target);frame=next(f for f in mutant['frames'] if f['cursors']);frame['cursors'][-1]['origin']+=1
  bad,_,_=analyze([mutant],constants,font['head'].unitsPerEm,font);summary['negativeControl']['cursorOriginMutationDetected']=any(x['rule']=='cursor-draw-origin' for x in bad)
 target=next((c for c in cases if c['id']=='C-half' and c['style']==1 and c['frames'][0]['rects']),None)
 if target:
  mutant=copy.deepcopy(target);mutant['frames'][0]['rects'][0]['y']+=1
  bad,_,_=analyze([mutant],constants,font['head'].unitsPerEm,font);summary['negativeControl']['fractionBarMutationDetected']=any(x['rule']=='fraction-bar-rect' for x in bad)
 # Mutate the topmost piece whose top is not bounded by its own clip: moving a
 # clipped assembly extender (or a slack middle piece) proves nothing.
 unclippedTop=lambda d:'inkY' in d and d['clip'][1]<d['inkY']
 target=next((c for c in cases if c['id']=='E-paren-tall' and c['style']==1 and any(unclippedTop(d) for d in c['frames'][0]['delimiters'])),None)
 if target:
  mutant=copy.deepcopy(target);min((d for d in mutant['frames'][0]['delimiters'] if unclippedTop(d)),key=lambda d:d['inkY'])['inkY']-=1
  bad,_,_=analyze([mutant],constants,font['head'].unitsPerEm,font);summary['negativeControl']['delimiterInkMutationDetected']=any(x['rule']=='logical-encloses-delimiter-y' for x in bad)
 target=next((c for c in cases if c['id']=='E-brace-tall' and c['style']==1 and c['frames'][0].get('strokes')),None)
 if target:
  mutant=copy.deepcopy(target);f=mutant['frames'][0];s=f['strokes'][0];s['x1']=f['nodes'][s['path']]['x']-1
  bad,_,_=analyze([mutant],constants,font['head'].unitsPerEm,font);summary['negativeControl']['vectorBoundsMutationDetected']=any(x['rule']=='logical-encloses-vector-x' for x in bad)
 target=next((c for c in cases if c['id']=='E-bar-tall' and c['style']==1 and c['frames'][0].get('delimiterRequests')),None)
 if target:
  mutant=copy.deepcopy(target);mutant['frames'][0]['delimiterRequests'][0]['top']-=3
  bad,_,_=analyze([mutant],constants,font['head'].unitsPerEm,font);summary['negativeControl']['delimiterHeightMutationDetected']=any(x['rule']=='synthetic-delimiter-target-height' for x in bad)
 target=next((c for c in cases if c['id']=='K-measurement-root' and c['style']==1),None)
 if target:
  mutant=copy.deepcopy(target);frame=mutant['frames'][0];node=next(n for n in frame['nodes'].values() if TYPES[n['type']]=='Measurement');node['inkAscent']-=3
  bad,_,_=analyze([mutant],constants,font['head'].unitsPerEm,font);summary['negativeControl']['reportedInkUnderclaimMutationDetected']=any(x['rule']=='compound-ink-encloses-emitted' and x['path']==node['path'] for x in bad)
  mutant=copy.deepcopy(target);frame=mutant['frames'][0];node=next(n for n in frame['nodes'].values() if TYPES[n['type']]=='Measurement');glyph=next(g for g in frame['delimiters'] if g['path']==node['path']);glyph['inkY']-=node['em']
  bad,_,_=analyze([mutant],constants,font['head'].unitsPerEm,font);summary['negativeControl']['emittedInkGapMutationDetected']=any(x['rule']=='radical-observed-radicand-gap' for x in bad)
 (a.out/'summary.json').write_text(json.dumps(summary,indent=2))
 from PIL import Image,ImageDraw
 chosen=['B-square','B-negative','B-subscript','C-half','C-nested','C-powered','D-indexed','D-fraction','E-bracket-tall','I-integral','K-temperature','L-fraction','N-fraction-base-script']
 for c in cases:
  if c['id'] not in chosen or c['style']!=1:continue
  src=a.geometry/(c['id']+'-s1.png')
  if not src.exists():continue
  im=Image.open(src).convert('RGB');draw=ImageDraw.Draw(im)
  for node in c['frames'][0]['nodes'].values():
   if node['type'] in (0,):continue
   x,y,w,asc,desc=[node[k] for k in ['x','baseline','width','ascent','descent']];draw.rectangle((x,y-asc,x+w,y+desc),outline='#4291bb',width=1);draw.line((x,y,x+w,y),fill='#ed5c66',width=1)
  im.save(a.out/'overlays'/(c['id']+'.png'))
 print(json.dumps(summary,indent=2))
 if a.require_style_pass and any(x['rule'].startswith('style-') for x in issues):raise SystemExit(1)
 if a.require_all_pass and issues:raise SystemExit(1)
if __name__=='__main__':main()
