#include "ScientificUncertainty.h"
#include "scientific/PhysicalConstants.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#if defined(ARDUINO)
#include <esp_heap_caps.h>
#endif

namespace numos::scientific {
namespace {
using E=UncertaintyError;
constexpr double kPi=3.141592653589793238462643383279502884;
// GUM-6 Annex F.2: screen the tangent model locally at +/- one standard
// uncertainty. This is not a coverage interval or a global distribution test.
// Fidelity target: an omitted effect must fit within HALF the last digit of
// a two-significant-digit standard uncertainty. It is fixed model policy;
// switching FORMAT never changes acceptance or canonical sensitivities.
constexpr double kNeighbourhood=1.0;
constexpr int kPropagationReportingDigits=2;
#if defined(ARDUINO)
// Small, bounded covariance payloads are read repeatedly during evaluation.
// Put the combined object/control block in capability-explicit internal RAM;
// no allocation is performed by layout, draw or cursor paths. The existing
// production Giac profile enables C++ exceptions; publish() catches failure.
template<class T> struct InternalAllocator {
    using value_type=T;
    InternalAllocator() noexcept=default;
    template<class U>InternalAllocator(const InternalAllocator<U>&) noexcept{}
    T* allocate(std::size_t n) {
        if(n>std::numeric_limits<std::size_t>::max()/sizeof(T))throw std::bad_alloc();
        void* memory=heap_caps_malloc(n*sizeof(T),MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
        if(!memory)throw std::bad_alloc();
        return static_cast<T*>(memory);
    }
    void deallocate(T* p,std::size_t) noexcept {heap_caps_free(p);}
    template<class U>bool operator==(const InternalAllocator<U>&)const noexcept{return true;}
    template<class U>bool operator!=(const InternalAllocator<U>&)const noexcept{return false;}
};
#endif
bool finite(double x) noexcept {return std::isfinite(x) && (x==0 || std::fpclassify(x)!=FP_SUBNORMAL);}
bool validSource(SourceId id) noexcept {
    if(isUserSource(id))return true;
    if((id&UINT32_C(0xffff0000))!=(UINT32_C(0x80000000)|(uint32_t(kCurrentDataset)<<16)))return false;
    const auto* value=constant(ConstantId(id&0xffff));return value && !value->exact;
}
E checkedProduct(double a,double b,double& out) noexcept {
    if(!finite(a) || !finite(b))return E::Numeric;
    out=a*b;
    return finite(out) && (out!=0 || a==0 || b==0)?E::None:E::Numeric;
}
E publish(const Uncertainty& next,Owned& out) {
    if(!next.count){out.reset();return E::None;}
#if defined(__cpp_exceptions)
    try {
#endif
#if defined(ARDUINO)
        auto candidate=std::allocate_shared<const Uncertainty>(InternalAllocator<Uncertainty>{},next);
#else
        auto candidate=std::make_shared<const Uncertainty>(next);
#endif
        out=std::move(candidate);return E::None;
#if defined(__cpp_exceptions)
    }catch(const std::bad_alloc&){return E::Allocation;}
#endif
}
E validate(const Owned& value) noexcept {
    if(!value)return E::None;
    if(value->count>kMaximumSources)return E::Limit;
    SourceId previous=0;
    for(unsigned i=0;i<value->count;++i) {
        const auto& c=value->contributions[i];
        if(!validSource(c.source) || c.source<=previous)return E::Invalid;
        if(!finite(c.coefficient) || c.coefficient==0)return E::Numeric;
        previous=c.source;
    }
    return E::None;
}
struct Compensated {
    double sum=0,correction=0;
    void add(double value) noexcept {
        const double t=sum+value;
        correction+=std::abs(sum)>=std::abs(value)?(sum-t)+value:(value-t)+sum;
        sum=t;
    }
    double result() const noexcept{return sum+correction;}
};
E curvature(double secondOrder,double linear) noexcept {
    if(!finite(secondOrder) || !finite(linear))return E::Numeric;
    if(secondOrder==0)return E::None;
    if(linear==0)return E::Nonlinear;
    const double digit=std::pow(10.0,std::floor(std::log10(linear))-kPropagationReportingDigits+1);
    const double fidelity=0.5*digit;
    if(!finite(fidelity) || fidelity==0)return E::Numeric;
    if(secondOrder>fidelity)return E::Nonlinear;
    return E::None;
}
E independent(const Owned& a,const Owned& b,bool& result) noexcept {
    result=true;if(!a || !b)return E::None;
    unsigned i=0,j=0;
    while(i<a->count && j<b->count) {
        const auto left=a->contributions[i].source,right=b->contributions[j].source;
        if(left==right){result=false;return E::None;}
        if(left<right)++i;else ++j;
    }
    // Disjoint user sources are independent by construction. CODATA only
    // supplies covariance, so zero rho is NOT proof of statistical independence.
    for(unsigned n=0;n<a->count;++n)if(!isUserSource(a->contributions[n].source))result=false;
    for(unsigned n=0;n<b->count;++n)if(!isUserSource(b->contributions[n].source))result=false;
    return E::None;
}
double maximumSine(double lo,double hi) noexcept {
    if(hi-lo>=kPi || std::floor((lo-kPi/2)/kPi)!=std::floor((hi-kPi/2)/kPi))return 1;
    return std::max(std::abs(std::sin(lo)),std::abs(std::sin(hi)));
}
double realPower(double x,int p,int q) noexcept {
    const double magnitude=std::pow(std::abs(x),double(p)/q);
    return x<0 && (p%2)!=0?-magnitude:magnitude;
}
}

bool sourceCorrelation(SourceId a,SourceId b,double& rho) noexcept {
    if(!validSource(a) || !validSource(b))return false;
    if(a==b){rho=1;return true;}
    // Independently entered measurements and measurements vs CODATA are
    // independent by the explicit product contract, not by physical inference.
    if(isUserSource(a) || isUserSource(b)){rho=0;return true;}
    return correlation(ConstantId(a&0xffff),ConstantId(b&0xffff),rho);
}
E makeSource(SourceId source,double u,Owned& out) {
    if(!source)return E::SourceExhausted;
    if(!validSource(source))return E::Invalid;
    if(!finite(u) || u<0)return E::Invalid;
    if(u==0){out.reset();return E::None;}
    Uncertainty value;value.count=1;value.contributions[0]={source,u};return publish(value,out);
}
E combine(const Owned& a,double da,const Owned& b,double db,Owned& out) {
    if(!finite(da) || !finite(db))return E::Numeric;
    auto e=validate(a);if(e!=E::None)return e;e=validate(b);if(e!=E::None)return e;
    Uncertainty next;unsigned i=0,j=0;
    const unsigned na=a?a->count:0,nb=b?b->count:0;
    while(i<na || j<nb) {
        const SourceId ia=i<na?a->contributions[i].source:UINT32_MAX;
        const SourceId ib=j<nb?b->contributions[j].source:UINT32_MAX;
        const SourceId id=std::min(ia,ib);double ca=0,cb=0;
        if(ia==id){e=checkedProduct(a->contributions[i++].coefficient,da,ca);if(e!=E::None)return e;}
        if(ib==id){e=checkedProduct(b->contributions[j++].coefficient,db,cb);if(e!=E::None)return e;}
        const double c=ca+cb;
        if(!finite(c))return E::Numeric;
        // WHY: only exact arithmetic cancellation removes an entry. No epsilon
        // pruning, no discarded small sources to manufacture room at the limit.
        if(c==0)continue;
        if(next.count==kMaximumSources)return E::Limit;
        next.contributions[next.count++]={id,c};
    }
    return publish(next,out);
}
E scale(const Owned& a,double derivative,Owned& out) {
    if(derivative==1){const auto e=validate(a);if(e==E::None)out=a;return e;}
    return combine(a,derivative,{},0,out);
}
E standardDeviation(const Owned& value,double& result,CorrelationResolver resolver) noexcept {
    const auto e=validate(value);if(e!=E::None)return e;
    if(!value || !value->count){result=0;return E::None;}
    double maximum=0;
    for(unsigned i=0;i<value->count;++i)maximum=std::max(maximum,std::abs(value->contributions[i].coefficient));
    // WHY: scale before squares; Planck quantities and tiny laboratory units
    // must not underflow variance merely because their standard u is small.
    Compensated variance,absoluteSum;unsigned operations=0;
    for(unsigned i=0;i<value->count;++i) {
        const auto& a=value->contributions[i];const double ai=a.coefficient/maximum;
        if(!finite(ai) || ai==0)return E::Numeric;
        const double diagonal=ai*ai;if(diagonal==0 || !finite(diagonal))return E::Numeric;
        variance.add(diagonal);absoluteSum.add(diagonal);++operations;
        if(isUserSource(a.source))continue; // all user cross-terms are exactly 0
        for(unsigned j=i+1;j<value->count;++j) {
            const auto& b=value->contributions[j];if(isUserSource(b.source))continue;
            double rho=0;if(!resolver || !resolver(a.source,b.source,rho))return E::CorrelationUnavailable;
            if(!std::isfinite(rho) || std::abs(rho)>1)return E::Numeric;
            if(rho==0)continue;
            const double cross=2*ai*(b.coefficient/maximum)*rho;
            if(!finite(cross) || cross==0)return E::Numeric;
            variance.add(cross);absoluteSum.add(std::abs(cross));++operations;
        }
    }
    double total=variance.result();if(!std::isfinite(total))return E::Numeric;
    // Conservative forward-error bound for products and compensated addition;
    // only a negative consistent with binary64 rounding is clamped to zero.
    const double gamma=double(8*operations+4)*std::numeric_limits<double>::epsilon();
    const double tolerance=gamma/(1-gamma)*absoluteSum.result();
    if(total<0){if(total < -tolerance)return E::Numeric;total=0;}
    const double answer=maximum*std::sqrt(total);
    if(!finite(answer) || (answer==0 && total!=0))return E::Numeric;
    result=answer;return E::None;
}
E product(const Owned& a,double x,const Owned& b,double y,bool divide,Owned& out) {
    if(!a && !b){out.reset();return E::None;}
    if(!finite(x) || !finite(y))return E::Numeric;
    double ux=0,uy=0;auto e=standardDeviation(a,ux);if(e!=E::None)return e;
    e=standardDeviation(b,uy);if(e!=E::None)return e;
    if(divide && (y==0 || (uy>0 && std::abs(y)<=kNeighbourhood*uy)))return E::Nonlinear;
    Owned candidate;
    e=combine(a,divide?1/y:y,b,divide?-(x/y)/y:x,candidate);if(e!=E::None)return e;
    double linear=0;e=standardDeviation(candidate,linear);if(e!=E::None)return e;
    if(divide) {
        // The quadratic remainder after linearization is proportional to the
        // *combined* differential, preserving exact x/x source cancellation.
        e=curvature(uy==0?0:linear*(uy/(std::abs(y)-uy)),linear);
    } else {
        double second=0;e=checkedProduct(ux,uy,second);
        if(e==E::None) {
            bool independentInputs=false;e=independent(a,b,independentInputs);
            if(e!=E::None)return e;
            if(independentInputs && second>0) {
                // Exact finite-second-moment identity for independent inputs:
                // Var(XY)=linearVariance+Var(X)Var(Y). No normality assumption.
                // Evaluate the *change in u*, not a fabricated confidence band.
                const double corrected=std::hypot(linear,second);
                second=second*(second/(corrected+linear));
            }
            e=curvature(second,linear);
        }
    }
    if(e!=E::None)return e;
    out=std::move(candidate);return E::None;
}
E power(const Owned& a,double x,int p,int q,Owned& out) {
    if(!a){out.reset();return E::None;}
    if(q<=0 || !finite(x))return E::Invalid;
    double u=0;auto e=standardDeviation(a,u);if(e!=E::None)return e;
    if(p==0)return scale(a,0,out);
    if(p==q)return scale(a,1,out);
    const double lo=x-kNeighbourhood*u,hi=x+kNeighbourhood*u;
    if(!finite(lo) || !finite(hi))return E::Numeric;
    // The local Taylor screen requires two distinguishable endpoints. A
    // nominal rounded more coarsely than u cannot establish this contract.
    if(u>0 && (lo==x || hi==x))return E::Numeric;
    if((q%2==0 && lo<=0) || (p<0 && lo<=0 && hi>=0) ||
       (q!=1 && lo<=0 && hi>=0))return E::Nonlinear;
    // Every nonconstant power at the uncertain zero is outside the safe local
    // linear contract (including 0 +/- 1 squared).
    if(x==0 && u>0)return E::Nonlinear;
    const double exponent=double(p)/q,y=realPower(x,p,q);
    const double derivative=exponent*y/x;
    if(!finite(y) || !finite(derivative) || y==0 || derivative==0)return E::Numeric;
    const double secondLo=std::abs(exponent*(exponent-1)*realPower(lo,p-2*q,q));
    const double secondHi=std::abs(exponent*(exponent-1)*realPower(hi,p-2*q,q));
    const double secondBound=std::max(secondLo,secondHi);
    double linear=0,quadratic=0;
    e=checkedProduct(std::abs(derivative),u,linear);if(e!=E::None)return e;
    e=checkedProduct(secondBound*u/2,u,quadratic);if(e!=E::None)return e;
    e=curvature(quadratic,linear);if(e!=E::None)return e;
    return scale(a,derivative,out);
}
E unary(const Owned& a,double x,Unary function,Owned& out) {
    if(!a){out.reset();return E::None;}
    if(!finite(x))return E::Numeric;
    double u=0;auto e=standardDeviation(a,u);if(e!=E::None)return e;
    const double lo=x-kNeighbourhood*u,hi=x+kNeighbourhood*u;
    if(!finite(lo) || !finite(hi))return E::Numeric;
    // Abs is locally affine away from zero; all other supported functions
    // need a numerically resolved +/-u neighborhood for their curvature bound.
    if(function!=Unary::Abs && u>0 && (lo==x || hi==x))return E::Numeric;
    double derivative=0,secondBound=0;
    switch(function) {
        case Unary::Abs:
            if(lo<=0 && hi>=0)return E::Nonlinear;
            derivative=x<0?-1:1;break;
        case Unary::Exp:
            derivative=std::exp(x);secondBound=std::exp(hi);
            if(!finite(secondBound) || !finite(std::exp(lo)) || derivative==0)return E::Numeric;
            break;
        case Unary::Ln:case Unary::Log10:
            if(lo<=0)return E::Nonlinear;
            derivative=1/x;secondBound=(1/lo)/lo;
            if(function==Unary::Log10){derivative/=std::log(10.0);secondBound/=std::log(10.0);}break;
        case Unary::Sin:derivative=std::cos(x);secondBound=maximumSine(lo,hi);break;
        case Unary::Cos:derivative=-std::sin(x);secondBound=maximumSine(lo+kPi/2,hi+kPi/2);break;
        case Unary::Tan: {
            if(std::floor((lo+kPi/2)/kPi)!=std::floor((hi+kPi/2)/kPi))return E::Nonlinear;
            const double c=std::cos(x);derivative=1/(c*c);
            const double t=std::max(std::abs(std::tan(lo)),std::abs(std::tan(hi)));
            secondBound=2*t*(1+t*t);break;
        }
    }
    if(!finite(derivative) || !finite(secondBound))return E::Numeric;
    // Taylor's bounded second derivative covers inflections too: unlike a
    // Hessian sampled only at x, this cannot hide the cubic sin(x) term at 0.
    double linear=0,quadratic=0;
    e=checkedProduct(std::abs(derivative),u,linear);if(e!=E::None)return e;
    e=checkedProduct(secondBound*u/2,u,quadratic);if(e!=E::None)return e;
    e=curvature(quadratic,linear);if(e!=E::None)return e;
    return scale(a,derivative,out);
}
}
