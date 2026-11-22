#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Versioned factory recipes, not an expression parser or a production dependency."""
import json
from pathlib import Path

CASES=[]
def add(group,id,formula,cpp): CASES.append(dict(id=group+'-'+id,group=group,formula=formula,cpp=cpp))
def n(x): return 'makeNumber('+json.dumps(str(x))+')'
def v(x='x'): return 'makeSymbol('+json.dumps(x)+')'
def row(*x): return 'row('+','.join(x)+')'
def op(x): return 'makeOperator(OpKind::'+x+')'
def f(a,b): return 'makeFraction('+a+','+b+')'
def p(a,b): return 'makePower('+a+','+b+')'
def r(a,b=None): return 'makeRoot('+a+(','+b if b else '')+')'
def par(a,kind='Paren'): return 'makeParen('+a+',DelimKind::'+kind+')'
def sub(a,b): return 'makeSubscript('+a+','+b+')'
def unit(key,prefix='',role='None'): return 'unit('+json.dumps(key)+','+json.dumps(prefix)+',numos::units::TemperatureRole::'+role+')'
def ref(key): return 'reference('+json.dumps(key)+')'
def m(a,b): return 'makeMeasurement('+a+','+b+',17)'
def plus(a,b):return row(a,op('Add'),b)
def neg(a):return row(op('Sub'),a)
def fn(kind,a):return 'makeFunction(FuncKind::'+kind+','+a+')'

add('A','negative','-3',neg(n(3)))
add('A','relation-negative','x=-3',row(v(),op('Eq'),neg(n(3))))
add('A','sum','a+b-c',row(v('a'),op('Add'),v('b'),op('Sub'),v('c')))
add('A','product','a×b',row(v('a'),op('Mul'),v('b')))
add('A','implicit','2x',row(n(2),v()))
add('A','factors','(2x+5)(x-1)',row(par(plus(row(n(2),v()),n(5))),par(row(v(),op('Sub'),n(1)))))
add('A','scientific-fraction','66.63/(2×10^-3)',f(n('66.63'),row(n(2),op('Mul'),p(n(10),neg(n(3))))))
for id,form,tree in [('square','x²',p(v(),n(2))),('negative','x^-2',p(v(),neg(n(2)))),('sum','x^(a+b)',p(v(),plus(v('a'),v('b')))),('nested','x^(y²)',p(v(),p(v('y'),n(2)))),('subscript','x_(i+1)',sub(v(),plus(v('i'),n(1)))),('subsup','(x_i)^2',p(sub(v(),v('i')),n(2))),('supsub','(x²)_i',sub(p(v(),n(2)),v('i'))),('exp','e^(x²+1)',p('makeConstant(ConstKind::E)',plus(p(v(),n(2)),n(1)))),('small','10^-12',p(n(10),neg(n(12)))),('unit','s^-2',p(unit('second'),neg(n(2))))]:add('B',id,form,tree)
ab=plus(v('a'),v('b')); cd=plus(v('c'),v('d'))
for id,form,tree in [('half','1/2',f(n(1),n(2))),('sums','(a+b)/(c+d)',f(ab,cd)),('nested','(1/x)/(1/y)',f(f(n(1),v()),f(n(1),v('y')))),('continued','1/(1+1/x)',f(n(1),plus(n(1),f(n(1),v())))),('exponent','x^(1/2)',p(v(),f(n(1),n(2)))),('powered','((a+b)/(c+d))^2',p(f(ab,cd),n(2))),('root','sqrt((a+b)/(c+d))',r(f(ab,cd)))]:add('C',id,form,tree)
for id,form,tree in [('simple','sqrt(x)',r(v())),('square','sqrt(x²+1)',r(plus(p(v(),n(2)),n(1)))),('fraction','sqrt((a+b)/(c+d))',r(f(ab,cd))),('indexed','cuberoot(x)',r(v(),n(3))),('nested','sqrt(2+sqrt(2+sqrt(2)))',r(plus(n(2),r(plus(n(2),r(n(2))))))),('index-expression','(n+1)th-root(x)',r(v(),plus(v('n'),n(1))))]:add('D',id,form,tree)
for kind in ['Paren','Bracket','Brace','Bar']:
 add('E',kind.lower(),kind+' around x',par(v(),kind))
 add('E',kind.lower()+'-tall',kind+' around nested fraction',par(f(f(ab,cd),f(n(1),v())),kind))
add('E','nested','([((a+b)/(c+d))])',par(par(par(f(ab,cd)),'Bracket')))
for kind in ['Sin','Cos','Tan','ArcSin','ArcCos','ArcTan','Ln','Log','Factorial']:add('F',kind.lower(),kind+'(x²)',fn(kind,p(v(),n(2))))
add('F','log-base','log_2(x²+1)','makeLogBase('+n(2)+','+plus(p(v(),n(2)),n(1))+')')
add('F','hyperbolic','sinh(x)','call("sinh",'+v()+')')
add('G','quadratic','(-b±sqrt(b²-4ac))/(2a)',f(row(op('Sub'),v('b'),op('PlusMinus'),r(row(p(v('b'),n(2)),op('Sub'),n(4),v('a'),v('c')))),row(n(2),v('a'))))
add('G','discriminant','b²-4ac',row(p(v('b'),n(2)),op('Sub'),n(4),v('a'),v('c')))
add('G','matrix','[[1,x],[1/2,sqrt(2)]]','matrix()')
add('G','piecewise','{x² if x≥0; -x otherwise}','piecewise()')
add('G','list','[1/2,sqrt(2),x²]','collection(CollectionKind::List,'+f(n(1),n(2))+','+r(n(2))+','+p(v(),n(2))+')')
add('G','set','{1,2,x}','collection(CollectionKind::Set,'+n(1)+','+n(2)+','+v()+')')
add('G','interval','[0,+∞)','makeInterval('+n(0)+',makeSpecialValue(SpecialValueKind::PositiveInfinity),true,false)')
add('G','equation','x=1/2','makeEquation('+v()+','+f(n(1),n(2))+')')
add('G','unevaluated','unevaluated(x²+1)','makeUnevaluated('+plus(p(v(),n(2)),n(1))+')')
add('H','periodic','x=π/6+2πk',row(v(),op('Eq'),f('makeConstant(ConstKind::Pi)',n(6)),op('Add'),n(2),'makeConstant(ConstKind::Pi)',v('k')))
add('H','domain','k∈ℤ',row(v('k'),'membership()',v('ℤ')))
add('H','decimal','0.1(6)','makePeriodicDecimal("0","1","6")')
add('H','degrees','sin(30°)',fn('Sin',row(n(30),op('UnitAttachTight'),unit('degree'))))
add('I','integral','∫_0^1 x² dx','makeDefIntegral('+n(0)+','+n(1)+','+p(v(),n(2))+','+v()+')')
add('I','sum','Σ_(k=1)^n k²','makeSummation('+row(v('k'),op('Eq'),n(1))+','+v('n')+','+p(v('k'),n(2))+')')
for kind in ['Product','CoProduct','Intersection','Union','LogicalAnd','LogicalOr']:add('I',kind.lower(),kind+'_(i=1)^n x_i','makeBigOp(BigOpKind::'+kind+','+row(v('i'),op('Eq'),n(1))+','+v('n')+','+sub(v(),v('i'))+')')
for id,form,tree in [('acceleration','9.81 m/s²',row(n('9.81'),op('UnitAttach'),f(unit('metre'),p(unit('second'),n(2))))),('force','kg·m/s²',f(row(unit('gram','k'),op('UnitProduct'),unit('metre')),p(unit('second'),n(2)))),('kohm','kΩ',unit('ohm','k')),('microfarad','µF',unit('farad','µ')),('square-cm','cm²',p(unit('metre','c'),n(2))),('cube-m','m³',p(unit('metre'),n(3))),('celsius','°C',unit('celsius')),('fahrenheit-difference','Δ°F',unit('fahrenheit','','Difference'))]:add('J',id,form,tree)
add('K','measurement','10±1 m',row(m(n(10),n(1)),op('UnitAttach'),unit('metre')))
add('K','temperature','20.0 °C ± 0.5 Δ°C',m(row(n('20.0'),op('UnitAttach'),unit('celsius')),row(n('0.5'),op('UnitAttach'),unit('celsius','','Difference'))))
add('K','long','(6.67430±0.00015)×10^-11',row(m(n('6.67430'),n('0.00015')),op('Mul'),p(n(10),neg(n(11)))))
add('K','relative','u_r',sub(v('u'),v('r')))
add('K','expanded','U, k=2',row(v('U'),v(','),v('k'),op('Eq'),n(2)))
CASES[-1]['contract']='Synthetic Symbol comma remains explicit ORD; full expanded uncertainty report is in product gate.'
add('K','codata','G',ref('G'))
add('K','correlated','a_0 × m_e',row(ref('bohr_radius'),op('Mul'),ref('electron_mass')))
for id,form,tree in [('empty','□','makeEmpty()'),('fraction','□/□','makeFraction()'),('power','□^□','makePower()'),('subscript','□_□','makeSubscript()'),('root','sqrt(□)','makeRoot()'),('indexed-root','□th-root(□)','makeRoot(nullptr,row(makeEmpty()))'),('function','sin(□)','makeFunction(FuncKind::Sin)'),('logbase','log_□(□)','makeLogBase()'),('paren','(□)','makeParen()'),('integral','∫_□^□ □ d□','makeDefIntegral()'),('sum','Σ_□^□ □','makeSummation()'),('bigop','Π_□^□ □','makeBigOp(BigOpKind::Product)'),('measurement','□±□','makeMeasurement(nullptr,nullptr,18)')]:add('L',id,form,tree)
add('M','wide','sum of 70 x terms','wide(70)')
add('M','wide-tall','long rational expression',f('wide(35)','wide(35)'))
for depth in range(2,7):
 t=v()
 for _ in range(depth):t=p(v(),t)
 add('N','powers-'+str(depth),'power nesting '+str(depth),t)
for depth in range(2,6):
 t=v()
 for _ in range(depth):t=f(n(1),plus(n(1),t))
 add('N','fractions-'+str(depth),'continued fraction depth '+str(depth),t)
t=f(p(v(),n(2)),r(plus(n(1),f(n(1),p(v('y'),neg(n(2)))))))
add('N','alternating','(x²)/(sqrt(1+1/y^-2))',t)
add('N','units-fraction','(m/s²)/(kg·m/s²)',f(f(unit('metre'),p(unit('second'),n(2))),f(row(unit('gram','k'),op('UnitProduct'),unit('metre')),p(unit('second'),n(2)))))
add('N','fraction-base-script','((1/x)/(1/y))^2',p(f(f(n(1),v()),f(n(1),v('y'))),n(2)))
add('N','large-base-subscript','((1/x)/(1/y))_i',sub(f(f(n(1),v()),f(n(1),v('y'))),v('i')))
add('N','native-variable','x² (NodeVariable)',p("makeVariable('x')",n(2)))
for base,script in [('f','x'),('V','A'),('A','V'),('f','f'),('x','j'),('T','i')]:
 add('B','kern-'+base+'-'+script,base+'^'+script,p(v(base),v(script)))
 add('B','kern-sub-'+base+'-'+script,base+'_'+script,sub(v(base),v(script)))
add('K','reference-power','m_e^2',p(ref('electron_mass'),n(2)))
add('B','combined-tall','x_(1/2)^(3/4)',p(sub(v(),f(n(1),n(2))),f(n(3),n(4))))
add('A','unscripted-italic','fx',row(v('f'),v('x')))
add('F','factorial-adjacency','2x!y',row(n(2),fn('Factorial',v()),v('y')))

# Compound ink regressions: measurements add physical fences; ancestors must
# clear those emitted fences, including through transparent reference/row nodes.
add('K','measurement-ratio','(10±1)/(2±0.1)',f(m(n(10),n(1)),m(n(2),n('0.1'))))
add('K','measurement-root','sqrt(100±4) m',row(r(m(n(100),n(4))),op('UnitAttach'),unit('metre')))
add('K','measurement-numerator','(6.67430±0.00015)/r²',f(m(n('6.67430'),n('0.00015')),p(v('r'),n(2))))
add('K','measurement-denominator','m_e/(2.0±0.1)',f(ref('electron_mass'),m(n('2.0'),n('0.1'))))
add('K','measurement-reference','(m_e±u)/(a_0±v)',f(m(ref('electron_mass'),v('u')),m(ref('bohr_radius'),v('v'))))
add('K','measurement-root-ratio','sqrt((10±1)/(2±0.1))',r(f(m(n(10),n(1)),m(n(2),n('0.1')))))
add('K','measurement-frac-root','1/sqrt(100±4)',f(n(1),r(m(n(100),n(4)))))
add('K','measurement-tall','(1/x±sqrt(2))/(3±1/y)',f(m(f(n(1),v()),r(n(2))),m(n(3),f(n(1),v('y')))))
add('K','measurement-script','x^((10±1)/(2±0.1))',p(v(),f(m(n(10),n(1)),m(n(2),n('0.1')))))
add('L','measurement-root-empty','sqrt(□±1)',r(row(m(row('makeEmpty()'),row(n(1))))))
add('L','measurement-ratio-empty','(10±□)/(□±1)',f(row(m(row(n(10)),row('makeEmpty()'))),row(m(row('makeEmpty()'),row(n(1))))))
add('K','measurement-script-root','2^(sqrt(100±4))',p(row(n(2)),row(r(row(m(row(n(100)),row(n(4))))))))
add('L','measurement-script-root-empty','2^(sqrt(□±4))',p(row(n(2)),row(r(row(m(row('makeEmpty()'),row(n(4))))))))
add('K','measurement-reciprocal-ratio','1/(1/(2±0.1))',f(row(n(1)),row(f(row(n(1)),row(m(row(n(2)),row(n('0.1'))))))))

def write(out):
 out=Path(out);out.mkdir(parents=True,exist_ok=True)
 (out/'master-corpus.json').write_text(json.dumps({'version':4,'unsupported':['dedicated derivative/limit nodes','combined-scripts single node','parenthetical uncertainty mode (separate product gate)'],'cases':CASES},ensure_ascii=False,indent=2),encoding='utf-8')
 body='\n'.join('case '+str(i)+': return row('+c['cpp']+');' for i,c in enumerate(CASES))
 names=','.join(json.dumps(c['id']) for c in CASES)
 (out/'corpus.inc').write_text('static const char* caseNames[]={'+names+'};\nstatic NodePtr fixture(unsigned index){switch(index){\n'+body+'\ndefault:return nullptr;}}\n',encoding='utf-8')
 return CASES

if __name__=='__main__':
 import sys
 write(sys.argv[1])
