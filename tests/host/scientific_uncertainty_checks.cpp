// Independent analytical oracles: the expected sensitivities below do not call
// production propagation, covariance or constant metadata to derive expected u.
#include "math/ScientificUncertainty.h"
#include "math/scientific/PhysicalConstants.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <string>
using namespace numos::scientific;
using E=UncertaintyError;
static unsigned checks=0,allocations=0;
static bool failAlloc=false;
void* operator new(std::size_t n){if(failAlloc)throw std::bad_alloc();++allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
static void check(bool value,const char* label){++checks;if(!value){std::fprintf(stderr,"FAIL %s\n",label);std::exit(1);}}
static void near(double actual,double expected,const char* label){if(!(std::isfinite(actual)&&std::abs(actual-expected)<=std::abs(expected)*3e-14+(expected==0?0:1e-300)))std::fprintf(stderr,"actual=%.17g expected=%.17g\n",actual,expected);check(std::isfinite(actual)&&std::abs(actual-expected)<=std::abs(expected)*3e-14+(expected==0?0:1e-300),label);}
static Owned source(SourceId id,double u){Owned x;check(makeSource(id,u,x)==E::None,"makeSource valid");return x;}
static double sd(const Owned& x){double u=-1;check(standardDeviation(x,u)==E::None,"standardDeviation succeeds");return u;}
static bool rhoHalf(SourceId,SourceId,double& rho) noexcept{rho=.5;return true;}
static bool rhoMinusOne(SourceId,SourceId,double& rho) noexcept{rho=-1;return true;}
static bool rhoTooLarge(SourceId,SourceId,double& rho) noexcept{rho=1.01;return true;}
static bool rhoMissing(SourceId,SourceId,double&) noexcept{return false;}
static bool rhoNaN(SourceId,SourceId,double& rho) noexcept{rho=std::numeric_limits<double>::quiet_NaN();return true;}
static Owned constantPair(){auto a=source(datasetSource(ConstantId::BohrRadius),1),b=source(datasetSource(ConstantId::ElectronMass),2);Owned pair;check(combine(a,1,b,1,pair)==E::None,"two measured constant sources");return pair;}
static void identity(){
 SourceSequence sequence;auto x=source(sequence.allocate(),1),y=source(sequence.allocate(),1);Owned result;
 check(x->contributions[0].source!=y->contributions[0].source,"new measures independent IDs");
 check(combine(x,1,x,-1,result)==E::None,"same source subtraction");check(!result,"exact source cancellation removes payload");near(sd(result),0,"X-X u=0");
 check(combine(x,1,x,1,result)==E::None,"same source addition");check(result->count==1,"X+X preserves one source");near(sd(result),2,"X+X u=2");
 check(combine(x,1,y,-1,result)==E::None,"distinct source subtraction");check(result->count==2,"independent difference has two sources");near(sd(result),std::sqrt(2.),"X-Y u=sqrt(2)");
 auto copied=x;check(copied->contributions[0].source==x->contributions[0].source,"shared copy identity");
}
static void arithmetic(){
 auto x=source(10,1),y=source(11,2);Owned result;
 check(combine(x,1,y,1,result)==E::None,"independent addition");near(sd(result),std::sqrt(5.),"u=sum sqrt5");
 auto a=source(12,.1),b=source(13,.2);check(product(a,2,b,3,false,result)==E::None,"product admissible");near(sd(result),.5,"independent product u=.5");
 check(scale(x,.01,result)==E::None,"centimetre scale");near(sd(result),.01,"100+/-1 cm has .01m u");check(result->contributions[0].source==10,"scale preserves ID");
 check(product(a,2,b,3,false,result)==E::None,"area product");check(scale(result,.0001,result)==E::None,"square centimetre scale");near(sd(result),.00005,"cm2 area u to m2");
 check(scale(source(14,.5),1.8,result)==E::None,"difference scale for Celsius Fahrenheit");near(sd(result),.9,"Celsius Fahrenheit u only scale");
 check(product(x,10,x,10,true,result)==E::None,"same source ratio admitted");near(sd(result),0,"X/X cancellation");
 check(product(source(15,.01),2,source(16,.02),4,true,result)==E::None,"division admitted");near(sd(result),std::sqrt(.0025*.0025+.0025*.0025),"independent quotient derivative oracle");
}
static void generated(){
 // 128 deterministic triples: compare a*(X+Y) with aX+aY and analytic norms.
 for(unsigned i=0;i<128;++i){double ux=(i%7+1)/10.,uy=(i%11+1)/20.,uz=(i%13+1)/30.,a=(int(i%9)-4)/3.;auto x=source(20,ux),y=source(21,uy),z=source(22,uz);Owned xy,left,ax,ay,right,tmp,reverse;
  check(combine(x,1,y,1,xy)==E::None && scale(xy,a,left)==E::None,"generated left");check(scale(x,a,ax)==E::None&&scale(y,a,ay)==E::None&&combine(ax,1,ay,1,right)==E::None,"generated right");near(sd(left),std::abs(a)*std::sqrt(ux*ux+uy*uy),"generated independent analytical variance");near(sd(right),sd(left),"linear distributivity");
  check(combine(xy,1,z,1,tmp)==E::None&&combine(z,1,y,1,reverse)==E::None&&combine(reverse,1,x,1,reverse)==E::None,"permuted sum");near(sd(tmp),std::sqrt(ux*ux+uy*uy+uz*uz),"three-source external norm");near(sd(tmp),sd(reverse),"order invariance");
 }
}
static void covariance(){
 auto pair=constantPair();double u=-1;check(standardDeviation(pair,u,rhoHalf)==E::None,"known rho accepted");near(u,std::sqrt(7.),"rho .5 contributes covariance");
 check(standardDeviation(pair,u,rhoTooLarge)==E::Numeric,"rho outside range rejected");check(standardDeviation(pair,u,rhoNaN)==E::Numeric,"NaN rho rejected");check(standardDeviation(pair,u,rhoMissing)==E::CorrelationUnavailable,"missing rho not zero");
 auto x=source(datasetSource(ConstantId::BohrRadius),1),y=source(datasetSource(ConstantId::ElectronMass),1),z=source(datasetSource(ConstantId::HartreeEnergy),1);Owned sum,tmp;
 check(combine(x,1,y,1,tmp)==E::None&&combine(tmp,1,z,1,sum)==E::None,"three correlated source construction");u=123;check(standardDeviation(sum,u,rhoMinusOne)==E::Numeric,"material negative variance rejected");near(u,123,"failed variance never publishes NaN");
 check(standardDeviation(tmp,u,rhoMinusOne)==E::None,"semidefinite covariance supported");near(u,0,"perfect anticorrelation cancels");
 double rho=9;check(sourceCorrelation(1,2,rho)&&rho==0,"user independence");check(sourceCorrelation(1,datasetSource(ConstantId::BohrRadius),rho)&&rho==0,"user constant independence");check(sourceCorrelation(datasetSource(ConstantId::BohrRadius),datasetSource(ConstantId::BohrRadius),rho)&&rho==1,"same CODATA ID correlation one");
}
static void nonlinear(){
 auto x=source(30,1),small=source(31,.1);Owned result=source(32,2);const auto previous=result;
 check(power(x,0,2,1,result)==E::Nonlinear,"0+/-1 squared cannot publish false certainty");check(result==previous,"nonlinear rejection transactional");
 check(power(small,10,2,1,result)==E::None,"healthy square admitted");near(sd(result),2,"healthy square derivative 20*.1");
 check(product({},1,x,2,true,result)==E::Nonlinear,"reciprocal neighbourhood zero rejected");check(power(x,1,1,2,result)==E::Nonlinear,"sqrt invalid neighbourhood rejected");check(unary(x,1.5707963267948966,Unary::Tan,result)==E::Nonlinear,"tan pole rejected");
 check(unary(small,0,Unary::Abs,result)==E::Nonlinear,"abs cusp rejected");check(unary(x,0,Unary::Cos,result)==E::Nonlinear,"cos stationary uncertainty not zero");
 check(unary(source(33,.001),2,Unary::Ln,result)==E::None,"safe log");near(sd(result),.0005,"log derivative");
 auto unresolved=source(34,1e-20),largeArgument=source(35,.01);result=previous;
 check(unary(unresolved,1.5707963267948966,Unary::Sin,result)==E::Numeric,"binary64 cannot resolve extremum neighbourhood");check(result==previous,"extremum range rejection transactional");
 check(unary(largeArgument,1e20,Unary::Sin,result)==E::Numeric,"binary64 cannot resolve huge angle neighbourhood");check(result==previous,"angle range rejection transactional");
 check(power(largeArgument,1e20,2,1,result)==E::Numeric,"binary64 cannot resolve power neighbourhood");check(result==previous,"power range rejection transactional");
}
static void codataProduct(){
 auto a=source(datasetSource(ConstantId::BohrRadius),8.2e-21),b=source(datasetSource(ConstantId::ElectronMass),2.8e-40);Owned productResult;
 check(product(a,5.29177210544e-11,b,9.1093837139e-31,false,productResult)==E::None,"a0 me product propagates");near(sd(productResult),7.347417886856706300435607832631308e-51,"independent high-precision correlated product oracle");
}
static void dataset(){
 // Values are independently transcribed CODATA 2022/NIST oracle values, not
 // obtained by traversing the generated production table for expectations.
 const auto* g=constant(ConstantId::Gravitational);check(g&&!g->exact,"G physically measured");near(g->standardUncertainty,1.5e-15,"G CODATA 2022 standard uncertainty");
 for(auto id:{ConstantId::SpeedOfLight,ConstantId::ReducedPlanck,ConstantId::ElementaryCharge,ConstantId::StandardGravity}){const auto*c=constant(id);check(c&&c->exact&&c->standardUncertainty==0,"definition exact constant no invented uncertainty");}
 double rho=0;check(correlation(ConstantId::BohrRadius,ConstantId::ElectronMass,rho)&&rho==-.99999,"independent NIST a0 electron-mass correlation");double reverse=0;check(correlation(ConstantId::ElectronMass,ConstantId::BohrRadius,reverse)&&reverse==rho,"CODATA symmetric pair");
 check(correlation(ConstantId::PlanckLength,ConstantId::PlanckMass,rho)&&rho==-1,"sqrt G inverse sqrt G anticorrelation");check(!correlation(ConstantId::Invalid,ConstantId::BohrRadius,rho),"invalid identity never zero covariance");
}
static void limits(){
 Owned all;for(unsigned i=1;i<=kMaximumSources;++i){Owned result;check(combine(all,1,source(i,1),1,result)==E::None,"fill bounded sources");all=std::move(result);}check(all->count==16,"explicit 16 source budget");near(sd(all),4,"16 independent norm");
 Owned sentinel=source(300,7),out=sentinel;check(combine(all,1,source(17,1e-10),1,out)==E::Limit,"17th source rejected, never prune smallest");check(out==sentinel,"limit does not partially publish");check(combine(source(1,1),1,source(1,1),-1,out)==E::None&&!out,"healthy operation after limit");
 SourceSequence wrap(kLastUserSource);check(wrap.allocate()==kLastUserSource,"last user ID valid");for(unsigned i=0;i<4;++i)check(wrap.allocate()==0,"exhaustion does not wrap or reuse");SourceSequence invalid(0);check(invalid.allocate()==0,"zero counter exhausted");
 out=sentinel;check(makeSource(0,1,out)==E::SourceExhausted&&out==sentinel,"source exhaustion transactional");
}
static void numeric(){
 Owned sentinel=source(400,1),out=sentinel;for(double u:{-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::denorm_min()})check(makeSource(401,u,out)!=E::None&&out==sentinel,"invalid u rejected transactionally");
 auto tiny=source(402,1e-200);near(sd(tiny),1e-200,"scaled variance prevents underflow");check(scale(tiny,1e-200,out)==E::Numeric&&out==sentinel,"relevant coefficient underflow rejected");check(scale(source(403,1e200),1e200,out)==E::Numeric,"coefficient overflow rejected");
 auto corrupt=std::make_shared<Uncertainty>();corrupt->count=2;corrupt->contributions[0]={500,1};corrupt->contributions[1]={500,2};double u=7;check(standardDeviation(corrupt,u)==E::Invalid&&u==7,"duplicate IDs corrupt payload rejected");
 corrupt->contributions[1]={501,0};check(standardDeviation(corrupt,u)==E::Numeric,"zero stored contribution rejected");corrupt->count=17;check(standardDeviation(corrupt,u)==E::Limit,"corrupt count bounded before access");
}
static void allocation(){
 auto previous=source(600,1);Owned out=previous;failAlloc=true;check(makeSource(601,2,out)==E::Allocation&&out==previous,"persistent allocation failure no partial publication");check(makeSource(602,2,out)==E::Allocation&&out==previous,"persistent allocation failure repeat");failAlloc=false;check(makeSource(603,2,out)==E::None,"allocation recovery without reset");
 unsigned before=allocations;Owned none,result;double u=3;check(scale(none,7,result)==E::None&&standardDeviation(result,u)==E::None&&u==0,"exact path empty payload");check(allocations==before,"exact uncertainty path performs zero allocations");
 std::weak_ptr<const Uncertainty> weak=out;out.reset();check(weak.expired(),"unreferenced payload released");
}
int main(int argc,char** argv){
 const std::string only=argc>1?argv[1]:"";
 struct Test{const char*name;void(*run)();};for(auto test:{Test{"identity",identity},{"arithmetic",arithmetic},{"generated",generated},{"covariance",covariance},{"dataset",dataset},{"codata-product",codataProduct},{"nonlinear",nonlinear},{"limits",limits},{"numeric",numeric},{"allocation",allocation}})if(only.empty()||only==test.name){test.run();std::printf("PASS %s\n",test.name);}
 std::printf("PASS scientific uncertainty independent assertions=%u sizeofContribution=%zu sizeofPayload=%zu sizeofHandle=%zu\n",checks,sizeof(Contribution),sizeof(Uncertainty),sizeof(Owned));return 0;
}
