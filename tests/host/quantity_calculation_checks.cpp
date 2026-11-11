// Independent rational oracles through the real typed constructor and engine.
#include "math/CalculationEngine.h"
#include "math/ToolboxCatalog.h"
#include "math/AngleModeRuntime.h"
#include "math/VariableManager.h"
#include "lvgl.h"
#include "hal/FileSystem.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include "../fixtures/quantity_factor_oracles.inc"
using namespace vpam;
using namespace numos;
bool setting_complex_enabled=false;
unsigned cases=0,checks=0;
void check(bool b,const char* m){++checks;if(!b){std::fprintf(stderr,"FAIL %s\n",m);std::exit(1);}}
NodePtr number(const char* s){return makeNumber(s);}
NodePtr unit(unsigned id,unsigned prefix=0){return makeUnit({units::UnitId(id),units::PrefixId(prefix)});}
NodePtr amount(const char* value,unsigned id,unsigned prefix=0){auto r=makeRow();auto* row=static_cast<NodeRow*>(r.get());row->appendChild(number(value));row->appendChild(makeOperator(OpKind::UnitAttach));row->appendChild(unit(id,prefix));return r;}
NodePtr binary(NodePtr a,OpKind op,NodePtr b){auto r=makeRow();auto* row=static_cast<NodeRow*>(r.get());row->appendChild(makeParen(std::move(a)));row->appendChild(makeOperator(op));row->appendChild(makeParen(std::move(b)));return r;}
CalculationEvaluation expect(NodePtr input,const char* exact,int length=0,int mass=0,int time=0,int current=0){
 ++cases;auto result=CalculationEngine::instance().evaluate(input.get());
 if(!result.ok() || result.exactText!=exact){std::fprintf(stderr,"case %u expected=%s actual=%s status=%u reason=%s\n",cases,exact,result.exactText.c_str(),unsigned(result.status),result.diagnostic.c_str());check(false,"canonical coefficient");}
 check(result.exactAST!=nullptr,"typed result");quantity::Dimension d;d.powers[0]=length;d.powers[1]=mass;d.powers[2]=time;d.powers[3]=current;
 check(d.scalar()?!result.quantity:bool(result.quantity) && result.quantity->dimension==d,"independent dimensions");return result;
}
void error(NodePtr input,quantity::Error expected){++cases;auto r=CalculationEngine::instance().evaluate(input.get());if(r.quantityError!=expected)std::fprintf(stderr,"error expected=%s got=%s\n",quantity::errorName(expected),r.diagnostic.c_str());check(!r.ok() && r.quantityError==expected,"typed error");}
int main(){
 lv_init();check(GiacEngine::instance().begin(),"Giac start");
 expect(binary(amount("2",1),OpKind::Add,amount("30",1,14)),"23/10",1);
 expect(binary(amount("5",1,15),OpKind::Add,amount("2",1,14)),"1/40",1);
 expect(binary(amount("2",1,14),OpKind::Mul,amount("3",1,14)),"3/5000",2);
 expect(makePower(amount("2",1,14),number("3")),"1/125000",3);
 expect(makeRoot(binary(number("9"),OpKind::Mul,makePower(unit(1,14),number("2")))),"3/100",1);
 expect(binary(amount("1",2,10),OpKind::Add,amount("500",2)),"3/2",0,1);
 expect(amount("250",2,15),"1/4000",0,1);
 expect(makeFraction(amount("6",1,10),amount("300",3)),"20",1,0,-1);
 expect(makeFraction(amount("12",16),amount("3",4)),"4",2,1,-3,-2);
 expect(binary(amount("2",4),OpKind::Mul,amount("3",18)),"6",2,1,-3,-1);
 expect(binary(amount("100",14),OpKind::Mul,amount("2",48)),"720000",2,1,-2);
 expect(makeFraction(amount("1",1),amount("1",1,14)),"100");
 auto squared=makePower(unit(3),number("2"));
 expect(binary(makeFraction(amount("2",3),amount("5",1)),OpKind::Mul,
   makeFraction(amount("10",1,10),binary(number("40"),OpKind::Mul,std::move(squared)))),"100",0,0,-1);
 expect(makeFunction(FuncKind::Ln,makeFraction(amount("2",1),amount("1",1))),"ln(2)");
 for(auto mode:{vpam::AngleMode::DEG,vpam::AngleMode::RAD}){setAngleMode(mode);expect(makeFunction(FuncKind::Sin,amount("90",57)),"1");
  expect(makeFunction(FuncKind::Sin,binary(makeFraction(makeConstant(ConstKind::Pi),number("2")),OpKind::Mul,unit(8))),"1");check(angleMode()==mode,"angle mode restored");}
 error(binary(amount("2",1),OpKind::Add,amount("3",3)),quantity::Error::DimensionMismatch);
 error(binary(number("0"),OpKind::Mul,binary(amount("2",1),OpKind::Add,amount("3",3))),quantity::Error::DimensionMismatch);
 error(makePower(binary(amount("2",1),OpKind::Add,amount("3",3)),number("0")),quantity::Error::DimensionMismatch);
 error(makeFraction(number("1"),amount("0",3)),quantity::Error::DivisionByZero);
 error(makeFunction(FuncKind::Ln,amount("2",1)),quantity::Error::DimensionMismatch);
 error(binary(amount("0",1),OpKind::Add,amount("3",3)),quantity::Error::DimensionMismatch);
 error(makeRoot(unit(1)),quantity::Error::ExponentLimit);
 error(binary(makeVariable('m'),OpKind::Mul,unit(1)),quantity::Error::SymbolicPending);
 auto q=CalculationEngine::instance().evaluate(amount("2",1).get());
 quantity::Descriptor cm;check(quantity::descriptor(1,14,cm),"cm descriptor");quantity::Display view;
 check(quantity::convert(*q.quantity,cm,view)==quantity::Error::None && view.exact=="200","conversion from canonical");
 check(q.quantity->coefficient=="2","conversion preserves canonical");
 error(binary(amount("1",23),OpKind::Add,amount("1",23)),quantity::Error::AffineOperation);
 error(binary(number("0"),OpKind::Mul,amount("1",60)),quantity::Error::AffineOperation);
 error(amount("1",1103),quantity::Error::UncertaintyPending);
 error(binary(number("0"),OpKind::Mul,makeQuantityReference({units::ReferenceId(126),units::PrefixId(0)})),quantity::Error::ContextPending);
 // Session values own their mathematics independently of a display descriptor.
 // A clean checkout has no ignored parent directories from earlier runs.
 std::error_code filesystemError;
 std::filesystem::create_directories("out/calc-units-01/host-session-fs",filesystemError);
 check(!filesystemError,"create isolated filesystem parents");
 LittleFSClass::setRoot("out/calc-units-01/host-session-fs");check(LittleFS.begin(false),"isolated filesystem");
 auto& engine=CalculationEngine::instance();auto& vm=VariableManager::instance();
 vm.resetAll();vm.setVariable('A',ExactVal::fromInt(77));check(vm.saveToFlash(),"seed durable scalar");
 auto first=engine.evaluate(amount("2",1).get());check(engine.commitResultAns(first,nullptr),"commit quantity Ans");
 quantity::Display changed;check(quantity::convert(*first.quantity,cm,changed)==quantity::Error::None && changed.exact=="200","output centimetres");
 expect(makeFraction(makeVariable(VAR_ANS),amount("2",3)),"1",1,0,-1);
 check(engine.storeAns('A'),"quantity STO session");expect(makeVariable('A'),"2",1);
 expect(unit(4),"1",0,0,0,1); // Unit A cannot read memory A.
 auto second=engine.evaluate(amount("3",3).get());check(engine.commitResultAns(second,nullptr),"rotate quantity");
 expect(makeVariable(VAR_PREANS),"2",1);expect(makeVariable(VAR_ANS),"3",0,0,1);
 auto failed=engine.evaluate(binary(amount("2",1),OpKind::Add,amount("3",3)).get());
 check(!engine.commitResultAns(failed,nullptr),"failed result cannot commit");expect(makeVariable(VAR_ANS),"3",0,0,1);
 check(vm.loadFromFlash(),"reload durable slots");check(vm.getVariable('A').ok && vm.getVariable('A').num==0,"old scalar not resurrected");
 auto cleared=engine.evaluate(makeVariable('A').get());check(cleared.ok() && !cleared.quantity && cleared.exactText=="0","restart invalidates session identity");
 // Independent component, factor-100 and prefix-power output oracles.
 auto converted=[&](NodePtr input,unsigned item,unsigned prefix,const char* expected){
   auto result=engine.evaluate(input.get());quantity::Descriptor d;quantity::Display view;
   check(result.ok() && result.quantity && quantity::descriptor(item,prefix,d),"conversion fixture");
   check(quantity::convert(*result.quantity,d,view)==quantity::Error::None && view.exact==expected,"independent output coefficient");
 };
 converted(binary(amount("100",14),OpKind::Mul,amount("2",48)),114,10,"1/5");
 converted(binary(amount("2",1,14),OpKind::Mul,amount("3",1,14)),101,14,"6");
 converted(makeFraction(amount("6",1,10),amount("300",3)),111,0,"72");
 converted(makeFraction(number("100"),unit(3)),10,0,"100");
 auto fuel=toolbox::prepare(*toolbox::find({uint16_t(32768|1298),0})).node;
 auto consumption=engine.evaluate(fuel.get());check(consumption.ok() && consumption.quantity && consumption.quantity->coefficient=="1/100000000","L/100km exact factor");
 error(binary(number("0"),OpKind::Mul,makeQuantityReference({units::ReferenceId(113),units::PrefixId(0)})),quantity::Error::ContextPending);
 error(binary(number("0"),OpKind::Add,unit(1)),quantity::Error::DimensionMismatch);
 expect(binary(amount("2",1),OpKind::Sub,amount("2",1)),"0",1);
 error(makePower(unit(1),number("65")),quantity::Error::ExponentLimit);
 error(binary(makeConstant(ConstKind::Imag),OpKind::Mul,unit(1)),quantity::Error::ComplexPending);
 auto photo=engine.evaluate(binary(unit(7),OpKind::Mul,unit(9)).get());quantity::Descriptor lumen;quantity::Display photoView;
 check(photo.ok() && photo.quantity && quantity::descriptor(24,0,lumen) && quantity::convert(*photo.quantity,lumen,photoView)==quantity::Error::None && photoView.exact=="1","cd sr to lumen domain");
 // Generated small rational sums and products: independent metre powers and
 // integer scale arithmetic, never expected factors read from the registry.
 unsigned generated=0;
 for(int a=1;a<=5;++a)for(int b=1;b<=4;++b)for(int c=1;c<=3;++c) {
   auto x=makeFraction(amount(std::to_string(a).c_str(),1,14),number(std::to_string(b).c_str()));
   auto y=amount(std::to_string(c).c_str(),1,15);
   auto result=engine.evaluate(binary(std::move(x),OpKind::Add,std::move(y)).get());
   const auto oracle=GiacEngine::instance().evaluateStructured((std::to_string(10*a+b*c)+"/"+std::to_string(1000*b)).c_str(),true);
   check(result.ok() && result.quantity && result.quantity->coefficient==oracle.base.exactText,"generated independent rational sum");
   quantity::Dimension length;length.powers[0]=1;check(result.quantity->dimension==length,"generated dimension");
   auto left=[&]{return makeFraction(amount(std::to_string(a).c_str(),1,14),number(std::to_string(b).c_str()));};
   auto right=[&]{return amount(std::to_string(c).c_str(),1,15);};
   auto ab_c=engine.evaluate(binary(binary(left(),OpKind::Mul,right()),OpKind::Mul,amount("2",3)).get());
   auto a_bc=engine.evaluate(binary(left(),OpKind::Mul,binary(right(),OpKind::Mul,amount("2",3))).get());
   const auto product=GiacEngine::instance().evaluateStructured((std::to_string(a*c)+"/"+std::to_string(50000*b)).c_str(),true);
   quantity::Dimension areaTime;areaTime.powers[0]=2;areaTime.powers[2]=1;
   check(ab_c.ok() && a_bc.ok() && ab_c.quantity && a_bc.quantity &&
       ab_c.quantity->coefficient==product.base.exactText && a_bc.quantity->coefficient==product.base.exactText &&
       ab_c.quantity->dimension==areaTime && a_bc.quantity->dimension==areaTime,"generated exact associative product");
   for(unsigned prefix:{14u,15u}) {
     quantity::Descriptor destination;quantity::Display display;
     check(quantity::descriptor(1,prefix,destination) && quantity::convert(*result.quantity,destination,display)==quantity::Error::None,"generated output conversion");
     const auto expected=GiacEngine::instance().evaluateStructured((std::to_string(10*a+b*c)+"/"+std::to_string((prefix==14?10:1)*b)).c_str(),true);
     check(display.exact==expected.base.exactText && result.quantity->coefficient==oracle.base.exactText,"generated independent display factor and canonical preservation");
     auto restored=engine.evaluate(binary(std::move(display.exactCoefficient),OpKind::Mul,unit(1,prefix)).get());
     check(restored.ok() && restored.quantity && restored.quantity->coefficient==oracle.base.exactText && restored.quantity->dimension==length,"generated typed recovery from output coefficient and unit");
   }
   ++generated;
 }
 // SCIENTIFIC-QUANTITIES-01 promotes audited physical references to exact or
 // uncertain quantities; contextual references above remain explicitly pending.
 expect(makeQuantityReference({units::ReferenceId(1),units::PrefixId(0)}),"299792458",1,0,-1);
 unsigned admitted=0,atomicVariants=0;
 for(const auto& definition:units::kDefinitions) {
   if(quantity::admissible({definition.id,units::PrefixId(0)})!=quantity::Error::None)continue;
   ++admitted;
   for(unsigned p=0;p<25;++p)if(units::offered({definition.id,units::PrefixId(p)})) {
     ++atomicVariants;auto node=unit(unsigned(definition.id),p);auto result=CalculationEngine::instance().evaluate(node.get());
     if(!result.ok())std::fprintf(stderr,"unit=%u prefix=%u error=%s\n",unsigned(definition.id),p,result.diagnostic.c_str());
     check(result.ok() && result.quantity && result.quantity->coefficient!="0","every admitted atomic variant computes without underflow");
   }
 }
 check(admitted==193 && atomicVariants==1921,"190 previous definitions and three affine scales");
 unsigned factorCases=0;
 for(const auto& oracle:factors) {
   if(quantity::admissible({units::UnitId(oracle.id),units::PrefixId(0)})!=quantity::Error::None)continue;
   if(units::definition(units::UnitId(oracle.id))->conversion==units::Conversion::Affine)continue; // point oracles live in temperature_checks
   auto input=unit(oracle.id);auto result=CalculationEngine::instance().evaluate(input.get());
   auto expected=GiacEngine::instance().evaluateStructured((std::string(oracle.numerator)+"/"+oracle.denominator).c_str(),true);
   if(!result.ok() || !result.quantity || result.quantity->coefficient!=expected.base.exactText)std::fprintf(stderr,"oracle unit=%u expected=%s got=%s\n",oracle.id,expected.base.exactText.c_str(),result.quantity?result.quantity->coefficient.c_str():result.diagnostic.c_str());
   check(result.ok() && result.quantity && result.quantity->coefficient==expected.base.exactText,"independent exact factor oracle");++factorCases;
 }
 unsigned catalogueVariants=0,deferredVariants=0;
 for(size_t i=0;i<toolbox::entryCount();++i) {
   const auto& entry=*toolbox::entryAt(i);if(entry.recipe!=toolbox::Recipe::Unit)continue;
   quantity::Descriptor d;const bool admittedItem=quantity::descriptor(uint16_t(entry.argument),uint8_t(entry.identity.variant),d);
   auto prepared=toolbox::prepare(entry);auto evaluated=engine.evaluate(prepared.node.get());
   if(admittedItem) {check(evaluated.ok() && evaluated.quantity && quantity::compatible(*evaluated.quantity,d),"every admitted catalogue composition evaluates with its typed domain");++catalogueVariants;}
   else {check(!evaluated.ok() && evaluated.status==MathEngineStatus::UnitsUnavailable,"every deferred catalogue composition explains its pending capability");++deferredVariants;}
 }
 std::printf("CATALOGUE %u admitted variants %u deferred variants\n",catalogueVariants,deferredVariants);
 std::printf("GENERATED %u rational scenarios: sum, associative product, two independent conversions and typed recovery\n",generated);
 std::printf("COVERAGE %u definitions %u atomic variants %u independent factors\n",admitted,atomicVariants,factorCases);
 std::printf("PASS %u cases, %u assertions\n",cases,checks);
}
