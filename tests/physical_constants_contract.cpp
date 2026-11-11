// Independent facts: NIST reversed query and analytic Planck/G dependencies.
// Deliberately does not include generated JSON or expected values from the table.
#include "math/scientific/PhysicalConstants.h"
#include <cmath>
#include <cstdio>
#include <cstring>
using namespace numos::scientific;
static bool near(double a,double b) { return std::abs(a-b)<=std::abs(b)*2e-14; }
int main() {
    unsigned checks=0;
#define REQUIRE(condition) do { ++checks; if (!(condition)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#condition); return 1; } } while(0)
    REQUIRE(constantCount()==15);
    REQUIRE(constant(ConstantId::Invalid)==nullptr);
    REQUIRE(constant(ConstantId(16))==nullptr);
    REQUIRE(byReference(15)==nullptr);
    REQUIRE(byReference(58)==nullptr);
    REQUIRE(byReference(59)==constant(ConstantId::Gravitational));
    for (unsigned i=1;i<=15;++i) {
        const auto* c=constant(ConstantId(i));
        REQUIRE(c && unsigned(c->id)==i);
        REQUIRE(byReference(c->referenceId)==c);
        REQUIRE(c->nominal && *c->nominal && c->unit && *c->unit);
        REQUIRE(std::isfinite(c->standardUncertainty));
        REQUIRE(c->exact ? c->standardUncertainty==0 : c->standardUncertainty>0);
        REQUIRE(datasetSource(c->id)==(UINT32_C(0x80010000)|i));
    }
    const auto* light=constant(ConstantId::SpeedOfLight);
    REQUIRE(light->exact && light->standardUncertainty==0);
    REQUIRE(std::strcmp(light->nominal,"299792458")==0);
    REQUIRE(light->dimension[0]==1 && light->dimension[2]==-1);
    REQUIRE(constant(ConstantId::ReducedPlanck)->exact);
    REQUIRE(std::strstr(constant(ConstantId::ReducedPlanck)->nominal,"pi")!=nullptr);
    REQUIRE(std::strstr(constant(ConstantId::PlanckForce)->nominal,"299792458^4")!=nullptr);
    REQUIRE(std::strstr(constant(ConstantId::PlanckEnergy)->nominal,"sqrt")!=nullptr);
    const auto* gravity=constant(ConstantId::Gravitational);
    REQUIRE(!gravity->exact && near(gravity->standardUncertainty,1.5e-15));
    REQUIRE(gravity->dimension[0]==3 && gravity->dimension[1]==-1 && gravity->dimension[2]==-2);
    REQUIRE(std::strcmp(constant(ConstantId::BohrRadius)->symbol,"a₀")==0);
    REQUIRE(std::strcmp(constant(ConstantId::ElectronMass)->symbol,"mₑ")==0);
    double rho=42;
    REQUIRE(!correlation(ConstantId::Invalid,ConstantId::ElectronMass,rho) && rho==42);
    REQUIRE(!correlation(ConstantId::SpeedOfLight,ConstantId::SpeedOfLight,rho) && rho==42);
    REQUIRE(correlation(ConstantId::BohrRadius,ConstantId::ElectronMass,rho));
    REQUIRE(near(rho,-0.99999));
    const double ua=8.2e-21,ub=2.8e-40,a=5.29177210544e-11,b=9.1093837139e-31;
    const double u=std::sqrt((b*ua)*(b*ua)+(a*ub)*(a*ub)+2*rho*a*b*ua*ub);
    REQUIRE(near(u,7.3474178868567063004e-51));
    REQUIRE(correlation(ConstantId::PlanckLength,ConstantId::PlanckTime,rho) && rho==1);
    REQUIRE(correlation(ConstantId::PlanckLength,ConstantId::PlanckMass,rho) && rho==-1);
    REQUIRE(correlation(ConstantId::PlanckForce,ConstantId::Gravitational,rho) && rho==-1);
    REQUIRE(correlation(ConstantId::BohrRadius,ConstantId::Gravitational,rho) && rho==0);
    unsigned known=0;
    for (unsigned i=1;i<=15;++i) for (unsigned j=1;j<=15;++j) {
        const auto aId=ConstantId(i), bId=ConstantId(j);
        if (constant(aId)->exact || constant(bId)->exact) continue;
        double reverse;
        REQUIRE(correlation(aId,bId,rho));
        REQUIRE(correlation(bId,aId,reverse));
        REQUIRE(rho==reverse && rho>=-1 && rho<=1);
        if (i==j) REQUIRE(rho==1);
        ++known;
    }
    REQUIRE(known==121);
    std::printf("PASS %u assertions, 15 constants, 55 pairs; sizeof ConstantRecord=%zu\n",checks,sizeof(ConstantRecord));
    return 0;
}
