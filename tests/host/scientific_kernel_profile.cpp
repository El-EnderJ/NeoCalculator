#include "math/ScientificUncertainty.h"
#include "math/scientific/PhysicalConstants.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <cmath>

static bool measuring=false;
static unsigned long long allocations=0,bytes=0;
void* operator new(std::size_t size) {
    if(measuring){++allocations;bytes+=size;}
    if(void* p=std::malloc(size))return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}

using namespace numos::scientific;
using Clock=std::chrono::steady_clock;
static volatile double observed=0;
template<class F>void benchmark(const char* name,unsigned sources,F run,UncertaintyError expected=UncertaintyError::None) {
    constexpr unsigned iterations=20000;
    for(unsigned i=0;i<300;++i)if(run()!=expected)std::abort();
    allocations=bytes=0;measuring=true;
    const auto start=Clock::now();
    for(unsigned i=0;i<iterations;++i)if(run()!=expected)std::abort();
    const auto stop=Clock::now();measuring=false;
    const double ns=std::chrono::duration<double,std::nano>(stop-start).count()/iterations;
    std::printf("{\"case\":\"%s\",\"sources\":%u,\"iterations\":%u,\"nsPerOperation\":%.3f,\"allocationsPerOperation\":%.3f,\"requestedCppBytesPerOperation\":%.3f}\n",name,sources,iterations,ns,double(allocations)/iterations,double(bytes)/iterations);
}
int main() {
    Owned one,two,eight,sixteen,extra;
    makeSource(1,1,one);makeSource(17,1,extra);
    for(unsigned n=1;n<=16;++n) {
        Owned next;makeSource(n,1.0/double(n),next);combine(sixteen,1,next,1,sixteen);
        if(n==2)two=sixteen;if(n==8)eight=sixteen;
    }
    benchmark("exact-no-payload",0,[]{Owned out;return combine({},1,{},1,out);});
    benchmark("one-source-copy",1,[&]{Owned out;return scale(one,1,out);});
    benchmark("one-source-unit-slope",1,[&]{Owned out;return scale(one,0.01,out);});
    benchmark("one-source-temperature-slope",1,[&]{Owned out;return scale(one,5.0/9.0,out);});
    benchmark("two-sources-sum",2,[&]{Owned out;return combine(one,1,extra,1,out);});
    benchmark("eight-sources-merge",8,[&]{Owned out;return combine(eight,1,eight,1,out);});
    benchmark("sixteen-sources-merge",16,[&]{Owned out;return combine(sixteen,1,sixteen,1,out);});
    benchmark("seventeenth-source-rejection",17,[&]{Owned out;return combine(sixteen,1,extra,1,out);},UncertaintyError::Limit);
    for(const auto& pair: {std::pair<const char*,Owned>{"variance-exact",{}},{"variance-one",one},{"variance-two",two},{"variance-eight",eight},{"variance-sixteen",sixteen}}) {
        benchmark(pair.first,pair.second?pair.second->count:0,[&]{double u=0;const auto e=standardDeviation(pair.second,u);observed=u;return e;});
    }
    Owned g,a0,me,cross;
    makeSource(datasetSource(ConstantId::Gravitational),1.5e-15,g);
    makeSource(datasetSource(ConstantId::BohrRadius),8.2e-21,a0);
    makeSource(datasetSource(ConstantId::ElectronMass),2.8e-40,me);
    combine(a0,9.1093837139e-31,me,5.29177210544e-11,cross);
    benchmark("constant-exact",0,[]{Owned out;const auto* c=constant(ConstantId::SpeedOfLight);return c&&c->exact?UncertaintyError::None:UncertaintyError::Invalid;});
    benchmark("constant-measured-source",1,[]{Owned out;return makeSource(datasetSource(ConstantId::Gravitational),1.5e-15,out);});
    benchmark("variance-codata-correlated",2,[&]{double u=0;const auto e=standardDeviation(cross,u);observed=u;return e;});
    benchmark("product-codata-correlated",2,[&]{Owned out;return product(a0,5.29177210544e-11,me,9.1093837139e-31,false,out);});
    benchmark("same-source-cancellation",0,[&]{Owned out;return combine(one,1,one,-1,out);});
    return observed<0?1:0;
}
