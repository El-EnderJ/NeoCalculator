#include "Quantity.h"
#include "CalculationEngine.h"
#include "InputRowSyntax.h"
#include "AngleModeRuntime.h"
#include "scientific/PhysicalConstants.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cerrno>
#include <limits>

namespace numos::quantity {
namespace {
using namespace vpam;
using units::UnitId;
using units::PrefixId;
using Role=Value::Role;
using TemperatureRole=units::TemperatureRole;
constexpr unsigned kNodes=192,kDepth=16,kCalls=64,kText=2000;
bool temperature(const Dimension& d) {
    for(unsigned i=0;i<kDimensions;++i)if(d.powers[i]!=(i==4?1:0))return false;
    return true;
}
Role linearRole(const Dimension& d){return temperature(d)?Role::TemperatureDifference:Role::Linear;}
// Rows/columns: linear, point, difference. 3 is an undefined affine operation.
constexpr uint8_t kAddRole[3][3]={{0,3,3},{3,3,1},{3,1,2}};
constexpr uint8_t kSubtractRole[3][3]={{0,3,3},{3,2,1},{3,3,2}};
bool same(const char* a,const char* b){return a && b && !std::strcmp(a,b);}
const char* meaning(UnitId id){const auto* d=units::definition(id);return d?d->quantity:"";}
std::string group(const std::string& s){return "("+s+")";}
std::string exact(const units::Exact& e,int prefix=0) {
    std::string s=std::to_string(e.numerator);
    if(e.denominator!=1)s+="/"+std::to_string(e.denominator);
    if(e.decimal+prefix)s+="*10^("+std::to_string(e.decimal+prefix)+")";
    if(e.piPower)s+="*pi^("+std::to_string(e.piPower)+")";
    return group(s);
}
bool combine(Dimension& a,const Dimension& b,int sign) {
    auto next=a;
    for(unsigned i=0;i<kDimensions;++i) {
        int n=int(a.powers[i])+sign*int(b.powers[i]);
        if(n < -kExponentLimit || n > kExponentLimit)return false;
        next.powers[i]=int8_t(n);
    }
    a=next;return true;
}
bool power(Dimension& a,int numerator,int denominator) {
    auto next=a;
    if(denominator<=0 || denominator>16 || numerator < -kExponentLimit || numerator>kExponentLimit)return false;
    for(unsigned i=0;i<kDimensions;++i) {
        const int n=int(a.powers[i])*numerator;
        if(n%denominator || n/denominator < -kExponentLimit || n/denominator>kExponentLimit)return false;
        next.powers[i]=int8_t(n/denominator);
    }
    a=next;return true;
}
bool scalarTree(const EngineResultNode& n,unsigned& budget,unsigned depth=0) {
    if(!budget-- || depth>40)return false;
    switch(n.kind) {
        case EngineNodeKind::Integer:case EngineNodeKind::Decimal:case EngineNodeKind::Rational:
        case EngineNodeKind::Pi:case EngineNodeKind::EulerE:case EngineNodeKind::Add:
        case EngineNodeKind::Neg:case EngineNodeKind::Mul:case EngineNodeKind::Inv:
        case EngineNodeKind::Pow:case EngineNodeKind::Sqrt:case EngineNodeKind::Root:
        case EngineNodeKind::Function:break;
        default:return false;
    }
    for(const auto& c:n.children)if(!scalarTree(c,budget,depth+1))return false;
    return true;
}
bool imaginaryTree(const EngineResultNode& n) {
    if(n.kind==EngineNodeKind::ImagUnit || n.kind==EngineNodeKind::Complex)return true;
    for(const auto& c:n.children)if(imaginaryTree(c))return true;
    return false;
}
Error scalar(const std::string& source,StructuredEngineResult& result,bool radians=true) {
    if(source.size()>kText)return Error::ExpressionLimit;
    result=GiacEngine::instance().evaluateStructured(source.c_str(),true,
        radians?GiacEngine::EvaluationAngle::Radians:GiacEngine::EvaluationAngle::Current);
    if(result.base.status==MathEngineStatus::OutOfMemory)return Error::Allocation;
    if(!result.base.ok() || !result.hasTree)return Error::ScalarDomain;
    unsigned budget=400;
    if(!scalarTree(result.tree,budget))return imaginaryTree(result.tree)?Error::ComplexPending:Error::SymbolicPending;
    return Error::None;
}
bool directTrig(const std::string& name) {return name=="sin" || name=="cos" || name=="tan";}
bool inverseTrig(const std::string& name) {return name=="asin" || name=="acos" || name=="atan" || name=="atan2";}
bool radianFunctions(const EngineResultNode& n) {
    // Called only after scalarTree has enforced 400 nodes / depth 40.
    if(n.kind==EngineNodeKind::Function && (directTrig(n.text) || inverseTrig(n.text)))return true;
    for(const auto& c:n.children)if(radianFunctions(c))return true;
    return false;
}
EngineResultNode literal(const char* digits) {
    EngineResultNode n;n.kind=EngineNodeKind::Integer;n.text=digits;return n;
}
void normalizeDegreeTree(EngineResultNode& n) {
    // The input is prevalidated by scalarTree; inserted wrappers are validated
    // again before AST conversion, so transformation never expands unchecked.
    for(auto& c:n.children)normalizeDegreeTree(c);
    if(n.kind!=EngineNodeKind::Function)return;
    const bool direct=directTrig(n.text),inverse=inverseTrig(n.text);
    if(!direct && !inverse)return;
    EngineResultNode pi;pi.kind=EngineNodeKind::Pi;
    EngineResultNode factor;factor.kind=EngineNodeKind::Inv;
    factor.children.push_back(direct?literal("180"):std::move(pi));
    EngineResultNode product;product.kind=EngineNodeKind::Mul;
    if(direct && n.children.size()==1) {
        product.children.push_back(std::move(n.children[0]));
        product.children.push_back(std::move(pi));
        product.children.push_back(std::move(factor));
        n.children[0]=std::move(product);
    } else if(inverse) {
        product.children.push_back(std::move(n));
        product.children.push_back(literal("180"));
        product.children.push_back(std::move(factor));n=std::move(product);
    }
}
Error uncertaintyError(scientific::UncertaintyError e) {
    using U=scientific::UncertaintyError;
    switch(e) {
        case U::None:return Error::None;case U::Invalid:return Error::UncertaintyInvalid;
        case U::Limit:return Error::UncertaintyLimit;case U::Numeric:return Error::UncertaintyNumeric;
        case U::CorrelationUnavailable:return Error::CorrelationUnavailable;
        case U::Nonlinear:return Error::UncertaintyNonlinear;case U::SourceExhausted:return Error::SourceExhausted;
        case U::Allocation:return Error::Allocation;
    }
    return Error::UncertaintyInvalid;
}
// The existing exact nominal tree remains the mathematical authority. This
// bounded numeric companion is consumed only by local sensitivities/reporting.
bool numericTree(const EngineResultNode& n,double& out,unsigned depth=0) {
    if(depth>40)return false;
    auto child=[&](unsigned i,double& v){return i<n.children.size() && numericTree(n.children[i],v,depth+1);};
    double a=0,b=0;
    switch(n.kind) {
        case EngineNodeKind::Integer:case EngineNodeKind::Decimal: {
            if(n.text.empty())return false;
            char* end=nullptr;errno=0;
            out=std::strtod(n.text.c_str(),&end);
            if(errno || end!=n.text.c_str()+n.text.size())return false;
            break;
        }
        case EngineNodeKind::Pi:out=3.141592653589793238462643383279502884;break;
        case EngineNodeKind::EulerE:out=2.718281828459045235360287471352662498;break;
        case EngineNodeKind::Rational:
            if(!child(0,a) || !child(1,b) || b==0)return false;
            out=a/b;break;
        case EngineNodeKind::Neg:if(!child(0,a))return false;out=-a;break;
        case EngineNodeKind::Inv:if(!child(0,a) || a==0)return false;out=1/a;break;
        case EngineNodeKind::Add:case EngineNodeKind::Mul:
            out=n.kind==EngineNodeKind::Add?0:1;
            for(const auto& c:n.children){if(!numericTree(c,a,depth+1))return false;out=n.kind==EngineNodeKind::Add?out+a:out*a;}break;
        case EngineNodeKind::Pow:
            if(!child(0,a) || !child(1,b))return false;
            out=std::pow(a,b);break;
        case EngineNodeKind::Sqrt:if(!child(0,a))return false;out=std::sqrt(a);break;
        case EngineNodeKind::Root:
            if(!child(0,a) || !child(1,b) || b==0)return false;
            out=a<0 && std::fmod(b,2)==1?-std::pow(-a,1/b):std::pow(a,1/b);break;
        case EngineNodeKind::Function:
            // Canonical quantity functions always use radians. Preserve the
            // binary64 companion precision instead of reading Giac's rounded
            // (display-oriented) evalf text for local derivatives and u_r.
            if(n.text=="atan2") {
                if(n.children.size()!=2 || !child(0,a) || !child(1,b))return false;
                out=std::atan2(a,b);break;
            }
            if(n.children.size()!=1 || !child(0,a))return false;
            if(n.text=="sin")out=std::sin(a);
            else if(n.text=="cos")out=std::cos(a);
            else if(n.text=="tan")out=std::tan(a);
            else if(n.text=="asin")out=std::asin(a);
            else if(n.text=="acos")out=std::acos(a);
            else if(n.text=="atan")out=std::atan(a);
            else if(n.text=="exp")out=std::exp(a);
            else if(n.text=="ln" || n.text=="log")out=std::log(a);
            else if(n.text=="log10")out=std::log10(a);
            else if(n.text=="abs")out=std::abs(a);
            else return false;
            break;
        default:return false;
    }
    return std::isfinite(out) && (out==0 || std::fpclassify(out)!=FP_SUBNORMAL);
}
Error numeric(const std::string& source,double& out,bool radians=true) {
    StructuredEngineResult result;const auto e=scalar(source,result,radians);if(e!=Error::None)return e;
    // Prefer the exact arithmetic/function tree where supported; evalf is a
    // fallback only for other closed functions, never nominal comparison.
    if(!numericTree(result.tree,out) &&
       (!result.hasApproximateTree || !numericTree(result.approximateTree,out)))return Error::UncertaintyNumeric;
    if(out==0 && result.base.exactText!="0")return Error::UncertaintyNumeric;
    return Error::None;
}
Value::Provenance derived(const Value& a,const Value& b) {
    return a.provenance==Value::Provenance::Exact && b.provenance==Value::Provenance::Exact?
        Value::Provenance::Exact:Value::Provenance::DerivedUncertain;
}
UnitId propagated(const Value& a,const Value& b,bool divide) {
    if(b.dimension.scalar())return a.meaning;
    if(a.dimension.scalar() && !divide)return b.meaning;
    const char* x=meaning(a.meaning);const char* y=meaning(b.meaning);
    // Local physical relations with known operands, not inference from a vector.
    if(divide && same(x,"voltage") && same(y,"current"))return UnitId(18);
    if(!divide && ((same(x,"current") && same(y,"resistance")) || (same(y,"current") && same(x,"resistance"))))return UnitId(16);
    if(!divide && ((same(x,"power") && same(y,"time")) || (same(y,"power") && same(x,"time"))))return UnitId(13);
    if(!divide && ((same(x,"current") && same(y,"time")) || (same(y,"current") && same(x,"time"))))return UnitId(15);
    if(!divide && ((same(x,"voltage") && same(y,"current")) || (same(y,"voltage") && same(x,"current"))))return UnitId(14);
    return UnitId{};
}
struct Walker {
    const void* owner;Resolve resolve;
    Error error=Error::None;unsigned nodes=0,calls=0;
    bool fail(Error e){if(error==Error::None)error=e;return false;}
    bool number(const Value& value,double& out,bool radians=true) {
        if(++calls>kCalls)return fail(Error::ExpressionLimit);
        const auto e=numeric(value.coefficient,out,radians);return e==Error::None || fail(e);
    }
    bool propagate(scientific::UncertaintyError e){return e==scientific::UncertaintyError::None || fail(uncertaintyError(e));}
    bool unary(Value& value,scientific::Unary function,double argumentScale=1) {
        if(!value.uncertainty)return true;
        double x=0;if(!number(value,x))return false;
        scientific::Owned input,next;
        if(!propagate(scientific::scale(value.uncertainty,argumentScale,input)) ||
           !propagate(scientific::unary(input,x*argumentScale,function,next)))return false;
        value.uncertainty=std::move(next);value.provenance=Value::Provenance::DerivedUncertain;return true;
    }
    bool checked(Value& value,bool radians=true) {
        if(++calls>kCalls)return fail(Error::ExpressionLimit);
        StructuredEngineResult r;const auto e=scalar(value.coefficient,r,radians);
        if(e!=Error::None)return fail(e);
        value.radianFunctions=radianFunctions(r.tree);
        value.coefficient=std::move(r.base.exactText);return true;
    }
    bool apply(Value& a,Value b,OpKind op) {
        scientific::Owned payload;
        const auto provenance=derived(a,b);
        if(op==OpKind::Add || op==OpKind::Sub) {
            if(a.dimension!=b.dimension)return fail(Error::DimensionMismatch);
            if(unsigned(a.role)>2 || unsigned(b.role)>2)return fail(Error::InvalidIdentity);
            const auto role=(op==OpKind::Add?kAddRole:kSubtractRole)[unsigned(a.role)][unsigned(b.role)];
            if(role==3)return fail(Error::AffineOperation);
            if((a.uncertainty || b.uncertainty) && !propagate(scientific::combine(a.uncertainty,1,b.uncertainty,
                op==OpKind::Add?1:-1,payload)))return false;
            a.role=Role(role);
            if(!same(meaning(a.meaning),meaning(b.meaning)))a.meaning=UnitId{};
        } else {
            if(a.role==Role::TemperaturePoint || b.role==Role::TemperaturePoint)return fail(Error::AffineOperation);
            if(op==OpKind::Div) {
                if(!checked(b))return false;
                if(b.coefficient=="0")return fail(Error::DivisionByZero);
            }
            if(a.uncertainty || b.uncertainty) {
                double x=0,y=0;if(!number(a,x) || !number(b,y))return false;
                if(!propagate(scientific::product(a.uncertainty,x,b.uncertainty,y,op==OpKind::Div,payload)))return false;
            }
            a.meaning=propagated(a,b,op==OpKind::Div);
            if(!combine(a.dimension,b.dimension,op==OpKind::Div?-1:1))return fail(Error::ExponentLimit);
            a.role=linearRole(a.dimension);
        }
        const char symbol=op==OpKind::Add?'+':op==OpKind::Sub?'-':op==OpKind::Mul?'*':'/';
        a.coefficient=group(a.coefficient)+symbol+group(b.coefficient);
        a.uncertainty=std::move(payload);a.provenance=provenance;
        return a.coefficient.size()<=kText || fail(Error::ExpressionLimit);
    }
    bool exponent(Value& a,Value b,bool root=false) {
        if(a.role==Role::TemperaturePoint || b.role==Role::TemperaturePoint)return fail(Error::AffineOperation);
        if(!b.dimension.scalar())return fail(Error::DimensionMismatch);
        if(b.uncertainty)return fail(Error::FunctionUnsupported);
        if(!checked(b))return false;
        // The closed scalar was evaluated by Giac. Read its typed rational;
        // never approximate an exponent or parse a user's formula as text.
        StructuredEngineResult r;
        if(++calls>kCalls)return fail(Error::ExpressionLimit);
        const auto e=scalar(b.coefficient,r);if(e!=Error::None)return fail(e);
        ExactVal rational;
        if(!CalculationEngine::resultTreeToExactVal(r.tree,rational) || !rational.isRational() ||
           rational.num < -64 || rational.num>64 || rational.den>16 || rational.den<=0)return fail(Error::ExponentLimit);
        int p=int(rational.num),q=int(rational.den);
        if(root){if(p<=0 || q!=1 || p>16)return fail(Error::ExponentLimit);q=p;p=1;}
        if(a.uncertainty) {
            double x=0;if(!number(a,x))return false;scientific::Owned next;
            if(!propagate(scientific::power(a.uncertainty,x,p,q,next)))return false;
            a.uncertainty=std::move(next);a.provenance=Value::Provenance::DerivedUncertain;
        }
        if(!power(a.dimension,p,q))return fail(Error::ExponentLimit);
        a.role=linearRole(a.dimension);
        if(!(p==1 && q==1))a.meaning=UnitId{};
        if(p<0){Value denominator=a;if(!checked(denominator))return false;if(denominator.coefficient=="0")return fail(Error::DivisionByZero);}
        a.coefficient=root?"surd("+a.coefficient+","+std::to_string(q)+")":group(a.coefficient)+"^("+std::to_string(p)+"/"+std::to_string(q)+")";
        return checked(a);
    }
    bool node(const MathNode* n,Value& out,unsigned depth=0) {
        if(!n)return fail(Error::Incomplete);
        if(++nodes>kNodes || depth>kDepth)return fail(Error::ExpressionLimit);
        auto child=[&](const MathNode* c,Value& v){return node(c,v,depth+1);};
        switch(n->type()) {
            case NodeType::Empty:return fail(Error::Incomplete);
            case NodeType::Row: {
                // WHY: syntax visitors consume operator atoms without entering
                // node(). Count them too, and reject oversized rows before the
                // visitor or Giac can process an arbitrarily long sign chain.
                const auto count=n->childCount();
                if(count>int(kNodes-nodes) || (count && depth==kDepth))return fail(Error::ExpressionLimit);
                for(int i=0;i<count;++i)
                    if(n->child(i) && n->child(i)->type()==NodeType::Operator)++nodes;
                struct Sink {
                    Walker& w;unsigned depth;Value sum,term;bool hasSum=false,hasTerm=false,negative=false;
                    OpKind pending=OpKind::Mul,addition=OpKind::Add;
                    bool fail(const char* m){return w.fail(std::strstr(m,"incomplete")?Error::Incomplete:Error::FunctionUnsupported);}
                    bool unary(OpKind op){if(op==OpKind::Sub)negative=!negative;return true;}
                    bool flush(){if(!hasTerm)return true;if(!hasSum){sum=std::move(term);hasSum=true;}else if(!w.apply(sum,std::move(term),addition))return false;hasTerm=false;return true;}
                    bool binary(OpKind op){if(inputrow::precedence(op)==1){if(!flush())return false;addition=op;}else pending=op;return true;}
                    bool operand(const MathNode* n){
                        // WHY: a scalar coordinate followed by an atomic point
                        // unit constructs a point. Multiplying an existing point
                        // (including Ans or a parenthesized point) never does.
                        if(n->type()==NodeType::Unit && hasTerm && pending==OpKind::Mul && !negative && term.dimension.scalar() &&
                           units::normalized(static_cast<const NodeUnit*>(n)->atom()).role==TemperatureRole::Absolute) {
                            if(++w.nodes>kNodes || depth+1>kDepth)return w.fail(Error::ExpressionLimit);
                            return w.attach(static_cast<const NodeUnit*>(n)->atom(),term);
                        }
                        Value v;if(!w.node(n,v,depth+1))return false;
                        if(negative){if(v.role==Role::TemperaturePoint)return w.fail(Error::AffineOperation);
                            if(v.uncertainty && !w.propagate(scientific::scale(v.uncertainty,-1,v.uncertainty)))return false;
                            v.coefficient="-("+v.coefficient+")";}
                        negative=false;
                        if(!hasTerm){term=std::move(v);hasTerm=true;}else if(!w.apply(term,std::move(v),pending))return false;return true;
                    }
                } sink{*this,depth};
                if(!inputrow::visit(*static_cast<const NodeRow*>(n),sink) || !sink.flush())return false;
                out=std::move(sink.sum);return true;
            }
            case NodeType::Unit: {
                out.coefficient="1";return attach(static_cast<const NodeUnit*>(n)->atom(),out);
            }
            case NodeType::QuantityReference: {
                const auto a=static_cast<const NodeQuantityReference*>(n)->atom();const auto* r=units::reference(a.reference);
                if(!units::valid(a))return fail(Error::InvalidIdentity);
                if(r->kind==units::ReferenceKind::PhysicalConstant) {
                    const auto* constant=scientific::byReference(uint16_t(a.reference));
                    if(!constant)return fail(Error::ReferencePending);
                    out.coefficient=constant->nominal;
                    std::copy(std::begin(constant->dimension),std::end(constant->dimension),out.dimension.powers.begin());
                    out.role=constant->id==scientific::ConstantId::PlanckTemperature?Role::TemperaturePoint:linearRole(out.dimension);
                    if(!constant->exact) {
                        out.provenance=constant->id==scientific::ConstantId::PlanckForce || constant->id==scientific::ConstantId::PlanckEnergy?
                            Value::Provenance::DerivedUncertain:Value::Provenance::Measured;
                        if(!propagate(scientific::makeSource(scientific::datasetSource(constant->id),constant->standardUncertainty,out.uncertainty)))return false;
                    }
                    if(uint8_t(a.prefix)) {
                        // Reference prefixes were already an AST contract in the
                        // registry (e.g. milli-g). Preserve the same source while
                        // scaling nominal exactly and sensitivities linearly.
                        const auto factor=exact(units::Exact{1,1,units::prefix(a.prefix)->exponent,0});
                        out.coefficient=group(out.coefficient)+"*"+factor;
                        if(out.uncertainty) {
                            if(++calls>kCalls)return fail(Error::ExpressionLimit);
                            double slope=0;const auto e=numeric(factor,slope);if(e!=Error::None)return fail(e);
                            if(!propagate(scientific::scale(out.uncertainty,slope,out.uncertainty)))return false;
                        }
                    }
                    return true;
                }
                return fail(r->exactness==units::Exactness::Measured?Error::UncertaintyPending:
                    r->kind==units::ReferenceKind::PhysicalConstant?Error::ReferencePending:Error::ContextPending);
            }
            case NodeType::Measurement: {
                const auto* m=static_cast<const NodeMeasurement*>(n);Value u;
                if(!child(m->nominal(),out) || !child(m->uncertainty(),u))return false;
                // measurement() validates both closed exact nominal trees while
                // obtaining their numeric companions. Do not evaluate twice:
                // sixteen simple sources must fit the existing CAS call cap.
                if((calls+=2)>kCalls)return fail(Error::ExpressionLimit);
                Value candidate;const auto e=measurement(std::move(out),u,m->sourceId(),candidate);
                if(e!=Error::None)return fail(e);
                out=std::move(candidate);return true;
            }
            case NodeType::Fraction: {
                const auto* f=static_cast<const NodeFraction*>(n);Value b;
                return child(f->numerator(),out) && child(f->denominator(),b) && apply(out,std::move(b),OpKind::Div);
            }
            case NodeType::Power: {
                const auto* p=static_cast<const NodePower*>(n);Value b;
                return child(p->base(),out) && child(p->exponent(),b) && exponent(out,std::move(b));
            }
            case NodeType::Root: {
                const auto* r=static_cast<const NodeRoot*>(n);Value degree;degree.coefficient="2";
                return child(r->radicand(),out) && (!r->hasDegree() || child(r->degree(),degree)) && exponent(out,std::move(degree),true);
            }
            case NodeType::Paren: {
                const auto* p=static_cast<const NodeParen*>(n);if(!child(p->content(),out))return false;
                if(p->delimKind()==DelimKind::Bar){if(out.role==Role::TemperaturePoint)return fail(Error::AffineOperation);
                    if(!unary(out,scientific::Unary::Abs))return false;
                    out.coefficient="abs("+out.coefficient+")";return checked(out);}return true;
            }
            case NodeType::Variable: {
                if(resolve && resolve(owner,static_cast<const NodeVariable*>(n)->name(),out))return true;
                std::string error;
                if(!CalculationEngine::serializeForGiac(n,out.coefficient,error,true))return fail(Error::SymbolicPending);
                if(angleModeIsDeg()) {
                    // Legacy scalar memories belong to the normal input-mode
                    // evaluator. Import their exact typed result once, and
                    // normalize forward/inverse trig structurally. Canonical
                    // Quantity values never depend on a future DEG/RAD change.
                    if(++calls>kCalls)return fail(Error::ExpressionLimit);
                    StructuredEngineResult imported;
                    const auto e=scalar(out.coefficient,imported,false);if(e!=Error::None)return fail(e);
                    if(radianFunctions(imported.tree)) {
                        normalizeDegreeTree(imported.tree);
                        unsigned budget=400;
                        if(!scalarTree(imported.tree,budget))return fail(Error::ExpressionLimit);
                        auto ast=CalculationEngine::resultTreeToAST(imported.tree);
                        if(!ast)return fail(Error::Allocation);
                        if(!CalculationEngine::serializeForGiac(ast.get(),out.coefficient,error,true))return fail(Error::FunctionUnsupported);
                    } else {
                        out.coefficient=std::move(imported.base.exactText);
                        out.radianFunctions=false;return true;
                    }
                }
                return checked(out);
            }
            case NodeType::Number:case NodeType::Constant: {
                if(n->type()==NodeType::Constant && static_cast<const NodeConstant*>(n)->constKind()==ConstKind::Imag)return fail(Error::ComplexPending);
                std::string error;
                return CalculationEngine::serializeForGiac(n,out.coefficient,error,true) || fail(Error::Incomplete);
            }
            case NodeType::Symbol:return fail(Error::SymbolicPending);
            case NodeType::Function:case NodeType::Call:case NodeType::LogBase: {
                const char* function=nullptr;const MathNode* arg=nullptr;
                if(n->type()==NodeType::Function) {
                    const auto* f=static_cast<const NodeFunction*>(n);arg=f->argument();
                    switch(f->funcKind()) {
                        case FuncKind::Sin:function="sin";break;case FuncKind::Cos:function="cos";break;case FuncKind::Tan:function="tan";break;
                        case FuncKind::Ln:function="ln";break;case FuncKind::Log:function="log10";break;
                        default:break;
                    }
                } else if(n->type()==NodeType::Call) {
                    const auto* f=static_cast<const NodeCall*>(n);function=f->name().c_str();if(f->childCount()==1)arg=f->child(0);
                } else {const auto* l=static_cast<const NodeLogBase*>(n);Value base;
                    if(!child(l->argument(),out) || !child(l->base(),base))return false;
                    if(!out.dimension.scalar() || !base.dimension.scalar())return fail(Error::DimensionMismatch);
                    if(out.uncertainty || base.uncertainty)return fail(Error::FunctionUnsupported);
                    if(!checked(base))return false;if(base.coefficient=="0" || base.coefficient=="1")return fail(Error::ScalarDomain);
                    out.coefficient="logb("+out.coefficient+","+base.coefficient+")";return checked(out);
                }
                // Validate children even for unsupported functions before any
                // algebraic simplification can erase an invalid operand.
                if(!arg || !function)return fail(Error::FunctionUnsupported);
                if(!child(arg,out))return false;
                if(out.role==Role::TemperaturePoint)return fail(Error::AffineOperation);
                const bool trig=same(function,"sin") || same(function,"cos") || same(function,"tan");
                bool radians=false;
                if(same(function,"abs")){if(!unary(out,scientific::Unary::Abs))return false;out.coefficient="abs("+out.coefficient+")";return checked(out);}
                if(same(function,"sqrt")){Value degree;degree.coefficient="2";return exponent(out,std::move(degree),true);}
                if(trig) {
                    Dimension angle;angle.powers[7]=1;
                    if(out.dimension==angle)radians=true;
                    else if(!out.dimension.scalar())return fail(Error::DimensionMismatch);
                } else if(same(function,"ln") || same(function,"exp") || same(function,"log10")) {
                    if(!out.dimension.scalar())return fail(Error::DimensionMismatch);
                } else return fail(Error::FunctionUnsupported);
                scientific::Unary kind=scientific::Unary::Exp;
                if(same(function,"sin"))kind=scientific::Unary::Sin;
                else if(same(function,"cos"))kind=scientific::Unary::Cos;
                else if(same(function,"tan"))kind=scientific::Unary::Tan;
                else if(same(function,"ln"))kind=scientific::Unary::Ln;
                else if(same(function,"log10"))kind=scientific::Unary::Log10;
                const double angleScale=trig && !radians && angleModeIsDeg()?3.141592653589793238462643383279502884/180:1;
                if(!unary(out,kind,angleScale))return false;
                // WHY: input-mode conversion occurs exactly once at the typed
                // function boundary. Stored expressions, later arithmetic,
                // FORMAT, and numeric sensitivity companions are all radians.
                if(trig && !radians && angleModeIsDeg())out.coefficient=group(out.coefficient)+"*pi/180";
                out.dimension={};out.meaning=UnitId{};out.coefficient=std::string(function)+"("+out.coefficient+")";
                return checked(out,true);
            }
            default:return fail(Error::FunctionUnsupported);
        }
    }
    bool attach(units::Atom a,Value& out) {
        const auto e=admissible(a);if(e!=Error::None)return fail(e);
        a=units::normalized(a);const auto* d=units::definition(a.unit);
        out.dimension=dimension(a);out.meaning=a.unit;
        out.role=a.role==TemperatureRole::Absolute?Role::TemperaturePoint:linearRole(out.dimension);
        const auto factor=exact(d->scale,units::prefix(a.prefix)->exponent);
        if(out.uncertainty) {
            // An affine offset moves the point only; its uncertainty always
            // transforms with the exact linear slope (delta temperature).
            double slope=0;const auto e=numeric(factor,slope);if(e!=Error::None)return fail(e);
            if(!propagate(scientific::scale(out.uncertainty,slope,out.uncertainty)))return false;
        }
        out.coefficient=out.coefficient=="1"?factor:group(out.coefficient)+"*"+factor;
        if(out.role==Role::TemperaturePoint && d->offset.numerator)out.coefficient+="+"+exact(d->offset);
        return true;
    }
};
std::string scale(const Descriptor& d) {
    std::string s="("+std::to_string(d.numerator)+"/"+std::to_string(d.denominator)+")";
    for(unsigned i=0;i<d.count;++i) {
        const auto& t=d.terms[i];const auto* def=units::definition(t.atom.unit);
        s+="*"+exact(def->scale,units::prefix(t.atom.prefix)->exponent)+"^("+std::to_string(t.power)+")";
    }
    return s;
}
bool pointDescriptor(const Descriptor& d) {
    return d.count==1 && d.terms[0].power==1 && d.numerator==1 && d.denominator==1 &&
        units::normalized(d.terms[0].atom).role==TemperatureRole::Absolute;
}
NodePtr termNode(units::Term term,bool absolute=false) {
    auto result=makeUnit(term.atom);const int p=absolute?std::abs(int(term.power)):int(term.power);
    if(p==1)return result;
    auto exponent=makeRow();auto* row=static_cast<NodeRow*>(exponent.get());
    if(p<0)row->appendChild(makeOperator(OpKind::Sub));row->appendChild(makeNumber(std::to_string(std::abs(p))));
    return makePower(std::move(result),std::move(exponent));
}
}

Error admissible(units::Atom a) {
    if(!units::valid(a))return Error::InvalidIdentity;
    a=units::normalized(a);
    const auto& d=*units::definition(a.unit);
    if(d.conversion==units::Conversion::Affine && d.temperature==TemperatureRole::None)return Error::AffinePending;
    if(d.conversion!=units::Conversion::Multiplicative && d.conversion!=units::Conversion::Affine)return Error::ContextPending;
    if(d.exactness==units::Exactness::Measured)return Error::UncertaintyPending;
    if(d.exactness!=units::Exactness::Exact)return Error::ApproximatePending;
    if(d.scale.numerator<=0 || !d.scale.denominator || !d.offset.denominator ||
       (d.conversion==units::Conversion::Multiplicative && d.offset.numerator))return Error::InvalidIdentity;
    return Error::None;
}
Dimension dimension(units::Atom a) {
    Dimension result;const auto* d=units::definition(a.unit);if(!d)return result;
    std::copy(std::begin(d->dimension),std::end(d->dimension),result.powers.begin());
    if(same(d->quantity,"angle") || same(d->quantity,"plane_angle"))result.powers[7]=1;
    // WHY: lm = cd·sr and lx = cd·sr/m². Retaining explicit solid angle
    // requires the same domain exponent in the named photometric units.
    if(same(d->quantity,"solid_angle") || same(d->quantity,"luminous_flux") || same(d->quantity,"illuminance"))result.powers[8]=1;
    if(same(d->domain,"information"))result.powers[9]=1;
    if(same(d->domain,"count")) {
        constexpr const char* names[]={"symbol_rate","sample_count","frame_count","event_count","flop_count","instruction_count","io_count"};
        for(unsigned i=0;i<7;++i)if(same(d->quantity,names[i]))result.powers[10+i]=1;
    }
    return result;
}
Analysis evaluate(const MathNode* root,const void* owner,Resolve resolve) {
    Analysis a;Walker w{owner,resolve};
    if(w.node(root,a.value) && w.checked(a.value))a.error=Error::None;else a.error=w.error;
    a.calls=w.calls;return a;
}
bool contains(const MathNode* root,const void* owner,Resolve resolve) {
    const MathNode* pending[400]{};unsigned size=0,visited=0;if(root)pending[size++]=root;
    while(size && visited++<400) {
        const auto* n=pending[--size];if(!n)continue;
        if(n->type()==NodeType::Unit || n->type()==NodeType::QuantityReference || n->type()==NodeType::Measurement)return true;
        if(n->type()==NodeType::Variable && resolve){Value v;if(resolve(owner,static_cast<const NodeVariable*>(n)->name(),v))return true;}
        for(int i=0;i<n->childCount();++i){if(size>=400)return true;pending[size++]=n->child(i);}
    }
    return size!=0;
}
bool descriptor(uint16_t id,uint8_t prefix,Descriptor& out) {
    const auto* item=units::item(id);if(!item || prefix>=25 || (!item->prefixSelector && prefix))return false;
    Descriptor next;next.count=item->termCount;next.numerator=item->coefficientNumerator;next.denominator=item->coefficientDenominator;
    for(unsigned i=0;i<next.count;++i){next.terms[i]=item->terms[i];if(item->prefixSelector && i==item->prefixComponent)next.terms[i].atom.prefix=PrefixId(prefix);}
    Dimension d;if(!descriptorDimension(next,d))return false;out=next;return true;
}
bool descriptorDimension(const Descriptor& descriptor,Dimension& out) {
    if(descriptor.count>kComponents || !descriptor.numerator || !descriptor.denominator)return false;
    Dimension next;
    for(unsigned i=0;i<descriptor.count;++i) {
        const auto& t=descriptor.terms[i];if(admissible(t.atom)!=Error::None || !t.power)return false;
        if(units::normalized(t.atom).role==TemperatureRole::Absolute && !pointDescriptor(descriptor))return false;
        auto d=dimension(t.atom);if(!power(d,t.power,1) || !combine(next,d,1))return false;
    }
    out=next;return true;
}
bool compatible(const Value& value,const Descriptor& descriptor) {
    Dimension d;return descriptorDimension(descriptor,d) && d==value.dimension &&
        value.role==(pointDescriptor(descriptor)?Role::TemperaturePoint:linearRole(d));
}
Descriptor coherent(const Value& value) {
    Descriptor out;
    if(value.dimension.scalar())return out;
    if(temperature(value.dimension)) {
        out.terms[0]={{UnitId(5),PrefixId(0),value.role==Role::TemperaturePoint?TemperatureRole::Absolute:TemperatureRole::Difference},1};
        out.count=1;return out;
    }
    // Preserve a known named magnitude; never identify Hz with Bq or Gy with Sv.
    if(uint16_t(value.meaning))for(const auto& d:units::kDefinitions) {
        units::Atom atom{d.id,PrefixId(uint16_t(d.id)==2?10:0)};
        const bool kg=uint16_t(d.id)==2;
        if(same(d.quantity,meaning(value.meaning)) && dimension(atom)==value.dimension && admissible(atom)==Error::None &&
           d.scale.numerator==1 && d.scale.denominator==1 && d.scale.piPower==0 && d.scale.decimal+(kg?3:0)==0) {
            out.terms[0]={atom,1};out.count=1;return out;
        }
    }
    auto remaining=value.dimension;
    constexpr uint16_t domains[]={8,9,1286,1291,1292,1293,1294,1295,1296,1297};
    for(unsigned i=7;i<kDimensions;++i)if(remaining.powers[i]) {
        const int8_t p=remaining.powers[i];units::Atom atom{UnitId(domains[i-7]),PrefixId(0)};
        out.terms[out.count++]={atom,p};auto d=dimension(atom);power(d,p,1);combine(remaining,d,-1);
    }
    for(unsigned i=0;i<7;++i)if(remaining.powers[i])out.terms[out.count++]={{UnitId(i+1),PrefixId(i==1?10:0),i==4?TemperatureRole::Difference:TemperatureRole::None},remaining.powers[i]};
    return out;
}
Error convert(const Value& value,const Descriptor& descriptor,Display& destination) {
    if(!compatible(value,descriptor))return Error::DimensionMismatch;
    std::string canonical=value.coefficient;
    if(value.role==Role::TemperaturePoint) {
        const auto a=units::normalized(descriptor.terms[0].atom);
        const auto& offset=units::definition(a.unit)->offset;
        if(offset.numerator)canonical=group(canonical)+"-"+exact(offset);
    }
    StructuredEngineResult result;const auto e=scalar(group(canonical)+"/"+group(scale(descriptor)),result);
    if(e!=Error::None)return e;
    Display next;next.exact=result.base.exactText;next.approximate=result.base.approximateText;
    next.hasUncertainty=value.provenance!=Value::Provenance::Exact;
    if(next.hasUncertainty) {
        double coherentU=0,slope=0,coherentNominal=0;
        auto uncertainty=standardUncertainty(value,coherentU);if(uncertainty!=Error::None)return uncertainty;
        uncertainty=numeric(scale(descriptor),slope);if(uncertainty!=Error::None)return uncertainty;
        if(slope<=0)return Error::UncertaintyNumeric;
        next.standardUncertainty=coherentU/slope;
        if(!std::isfinite(next.standardUncertainty) || (coherentU>0 &&
            (next.standardUncertainty==0 || std::fpclassify(next.standardUncertainty)==FP_SUBNORMAL)))return Error::UncertaintyNumeric;
        uncertainty=numeric(group(canonical)+"/"+group(scale(descriptor)),next.nominal);if(uncertainty!=Error::None)return uncertainty;
        uncertainty=numeric(value.coefficient,coherentNominal);if(uncertainty!=Error::None)return uncertainty;
        // Point-relative reporting uses the coherent thermodynamic origin;
        // changing Celsius/Fahrenheit cannot change relative uncertainty.
        next.relativeDefined=value.coefficient!="0" && coherentNominal!=0;
        if(next.relativeDefined) {
            next.relativeUncertainty=coherentU/std::abs(coherentNominal);
            if(!std::isfinite(next.relativeUncertainty) || (coherentU>0 && next.relativeUncertainty==0))return Error::UncertaintyNumeric;
        }
    }
    next.exactCoefficient=CalculationEngine::resultTreeToAST(result.tree,ProductNotation::ScalarNatural);
    if(!next.exactCoefficient)return Error::Allocation;
    if(result.hasApproximateTree) {
        unsigned budget=400;
        // A decimal underflow must never masquerade as the exact nonzero value.
        const bool falseZero=next.exact!="0" && (next.approximate=="0" || next.approximate=="0.0");
        if(!falseZero && scalarTree(result.approximateTree,budget))next.approximateCoefficient=CalculationEngine::resultTreeToAST(result.approximateTree);
    }
    if(!next.approximateCoefficient)next.approximate.clear();
    destination=std::move(next);return Error::None;
}
Error standardUncertainty(const Value& value,double& out) {
    return uncertaintyError(scientific::standardDeviation(value.uncertainty,out));
}
Error measurement(Value nominal,const Value& u,uint32_t sourceId,Value& destination) {
    if(!sourceId)return Error::SourceExhausted;
    if(!scientific::isUserSource(sourceId))return Error::UncertaintyInvalid;
    // An uncertainty magnitude cannot itself be another measurement. Explicit
    // new entries create new independent sources; nested uncertain input is not
    // silently collapsed to its nominal or combined standard uncertainty.
    if(nominal.provenance!=Value::Provenance::Exact || u.provenance!=Value::Provenance::Exact ||
       nominal.uncertainty || u.uncertainty)return Error::UncertaintyInvalid;
    if(nominal.dimension!=u.dimension)return Error::DimensionMismatch;
    if(u.role==Role::TemperaturePoint || (nominal.role==Role::TemperaturePoint && u.role!=Role::TemperatureDifference))
        return Error::UncertaintyInvalid;
    double standard=0,center=0;
    auto e=numeric(u.coefficient,standard);if(e!=Error::None)return e==Error::ComplexPending || e==Error::SymbolicPending?Error::UncertaintyInvalid:e;
    if(standard<0)return Error::UncertaintyInvalid;
    e=numeric(nominal.coefficient,center);if(e!=Error::None)return e;
    scientific::Owned next;e=uncertaintyError(scientific::makeSource(sourceId,standard,next));if(e!=Error::None)return e;
    nominal.uncertainty=std::move(next);nominal.provenance=Value::Provenance::Measured;
    destination=std::move(nominal);return Error::None;
}
NodePtr compose(NodePtr coefficient,const Descriptor& descriptor) {
    if(!coefficient)return {};
    Dimension valid;if(!descriptorDimension(descriptor,valid))return {};
    if(!descriptor.count)return coefficient;
    auto result=makeRow();auto* row=static_cast<NodeRow*>(result.get());
    bool parentheses=false;
    if(coefficient->type()==NodeType::Row)for(int i=1;i<coefficient->childCount();++i) {
        const auto* n=coefficient->child(i);if(n->type()==NodeType::Operator){const auto op=static_cast<const NodeOperator*>(n)->op();parentheses|=op==OpKind::Add || op==OpKind::Sub;}
    }
    row->appendChild(parentheses?makeParen(std::move(coefficient)):std::move(coefficient));
    const bool tight=descriptor.count==1 && descriptor.terms[0].power==1 && units::angularTight(descriptor.terms[0].atom);
    row->appendChild(makeOperator(tight?OpKind::UnitAttachTight:OpKind::UnitAttach));
    if(descriptor.count==1 && descriptor.numerator==1 && descriptor.denominator==1)row->appendChild(termNode(descriptor.terms[0]));
    else {
        auto numerator=makeRow(),denominator=makeRow();auto* num=static_cast<NodeRow*>(numerator.get());auto* den=static_cast<NodeRow*>(denominator.get());
        if(descriptor.numerator!=1)num->appendChild(makeNumber(std::to_string(descriptor.numerator)));
        if(descriptor.denominator!=1)den->appendChild(makeNumber(std::to_string(descriptor.denominator)));
        for(unsigned i=0;i<descriptor.count;++i){const auto& t=descriptor.terms[i];auto* target=t.power<0?den:num;if(target->childCount())target->appendChild(makeOperator(OpKind::UnitProduct));target->appendChild(termNode(t,true));}
        if(!num->childCount())num->appendChild(makeNumber("1"));
        row->appendChild(den->childCount()?makeFraction(std::move(numerator),std::move(denominator)):std::move(numerator));
    }
    return result;
}
bool symbol(const Descriptor& d,char* out,size_t capacity) {
    if(!capacity)return false;out[0]=0;size_t offset=0;
    for(unsigned i=0;i<d.count;++i) {
        char atom[48]{};if(!units::symbol(d.terms[i].atom,atom,sizeof(atom)))return false;
        const int n=d.terms[i].power==1?std::snprintf(out+offset,capacity-offset,"%s%s",i?" · ":"",atom):
            std::snprintf(out+offset,capacity-offset,"%s%s^%d",i?" · ":"",atom,int(d.terms[i].power));
        if(n<0 || size_t(n)>=capacity-offset)return false;offset+=size_t(n);
    }
    return true;
}
const char* errorName(Error e) {
    static constexpr const char* names[]={"none","incomplete","dimension_mismatch","division_by_zero","affine_pending","context_pending","uncertainty_pending","approximate_pending","symbolic_pending","complex_pending","function_unsupported","exponent_limit","expression_limit","invalid_identity","allocation","scalar_domain","reference_pending","affine_operation","uncertainty_invalid","uncertainty_limit","uncertainty_numeric","correlation_unavailable","uncertainty_nonlinear","source_exhausted"};
    return unsigned(e)<std::size(names)?names[unsigned(e)]:"invalid_identity";
}
const char* errorMessage(Error e,bool spanish) {
    static constexpr const char* messages[][2]={
        {"",""},{"Incomplete expression","Expresión incompleta"},
        {"Incompatible dimensions","Dimensiones incompatibles"},{"Division by zero","División por cero"},
        {"Affine temperatures not supported yet","Temperaturas afines aún no disponibles"},
        {"Contextual quantities not supported yet","Cantidades contextuales aún no disponibles"},
        {"Uncertainty not supported yet","Incertidumbre aún no disponible"},
        {"Approximate unit not supported yet","Unidad aproximada aún no disponible"},
        {"A closed real quantity is required","Se requiere una cantidad real cerrada"},
        {"Complex quantities not supported yet","Cantidades complejas aún no disponibles"},
        {"Function not supported for quantities","Función no disponible para cantidades"},
        {"Unit exponent outside supported range","Exponente de unidad fuera del límite"},
        {"Quantity expression limit","Límite de expresión con unidades"},
        {"Invalid unit identity","Identidad de unidad inválida"},
        {"Not enough memory","Memoria insuficiente"},{"Quantity outside function domain","Cantidad fuera del dominio"},
        {"Physical references not supported yet","Referencias físicas aún no disponibles"},
        {"Operation undefined for temperature points","Operación no definida para puntos de temperatura"},
        {"Invalid standard uncertainty","Incertidumbre estándar no válida"},
        {"Uncertainty source limit (16)","Límite de fuentes de incertidumbre (16)"},
        {"Uncertainty outside numeric range","Incertidumbre fuera del rango numérico"},
        {"Correlation data unavailable","Datos de correlación no disponibles"},
        {"Linear propagation is unreliable","Propagación lineal no fiable"},
        {"Measurement source IDs exhausted","Identidades de medida agotadas"}};
    return messages[unsigned(e)<std::size(messages)?unsigned(e):unsigned(Error::InvalidIdentity)][spanish?1:0];
}
}
