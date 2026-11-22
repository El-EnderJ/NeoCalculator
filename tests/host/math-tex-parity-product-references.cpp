// SPDX-License-Identifier: GPL-3.0-or-later
// Private product gate: observe the real renderer's submitted glyphs. No firmware hooks.
#include "ui/MathRenderer.h"
#include "math/CalculationEngine.h"
#include "math/Quantity.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
using namespace vpam;
using namespace numos;
bool setting_complex_enabled=false;
struct Glyph { uint32_t cp; int x, baseline, level; };
static std::vector<Glyph> glyphs;
static bool observing=false;
static unsigned checks=0, failures=0;
static std::string active;
static void check(bool value,const char* why) { ++checks; if(!value){++failures;std::cout<<"FAIL "<<active<<": "<<why<<'\n';} }
namespace vpam { void productReferenceGlyph(int x,int y,unsigned level,uint32_t cp,const lv_font_glyph_dsc_t&) {
 if(observing)glyphs.push_back({cp,x,y,int(level)});
} }
static uint16_t framebuffer[320*240], drawbuffer[320*240];
static void flush(lv_display_t*d,const lv_area_t*a,uint8_t*data) {
 auto*s=reinterpret_cast<uint16_t*>(data);
 for(int y=a->y1;y<=a->y2;++y)for(int x=a->x1;x<=a->x2;++x)framebuffer[y*320+x]=*s++;
 lv_display_flush_ready(d);
}
static void save(const std::string&p) {
 std::ofstream f(p,std::ios::binary);f<<"P6\n320 240\n255\n";
 for(auto c:framebuffer){const unsigned char rgb[]={uint8_t(((c>>11)&31)*255/31),uint8_t(((c>>5)&63)*255/63),uint8_t((c&31)*255/31)};f.write(reinterpret_cast<const char*>(rgb),3);}
}
static NodePtr row(NodePtr n){auto r=std::make_unique<NodeRow>();r->appendChild(std::move(n));return r;}
static const units::Reference* ref(const char*key){for(const auto&r:units::kReferences)if(!std::strcmp(r.key,key))return &r;return nullptr;}
static bool equalValue(const quantity::Analysis&a,const quantity::Analysis&b) {
 if(a.error!=b.error || a.value.coefficient!=b.value.coefficient || a.value.dimension!=b.value.dimension || a.value.role!=b.value.role || a.value.provenance!=b.value.provenance)return false;
 const auto&x=a.value.uncertainty;const auto&y=b.value.uncertainty;
 if(bool(x)!=bool(y))return false;if(!x)return true;if(x->count!=y->count)return false;
 for(unsigned i=0;i<x->count;++i)if(x->contributions[i].source!=y->contributions[i].source || x->contributions[i].coefficient!=y->contributions[i].coefficient)return false;
 return true;
}
static void symbolRoles(const MathNode*n) {
 if(n->type()==NodeType::Subscript){auto*sub=static_cast<const NodeSubscript*>(n);check(sub->subscript()->type()==NodeType::Symbol,"descriptor is a semantic symbol");
  if(sub->subscript()->type()==NodeType::Symbol)check(static_cast<const NodeSymbol*>(sub->subscript())->upright(),"descriptive subscript has upright role");}
 for(int i=0;i<n->childCount();++i)symbolRoles(n->child(i));
}
static void paint(MathCanvas&canvas,lv_display_t*d,NodeRow*root,unsigned style,const std::string&out) {
 canvas.setMathStyle(MathStyle(style));canvas.setExpression(root,nullptr);canvas.resetScroll();canvas.stopCursorBlink();
 glyphs.clear();observing=true;lv_obj_invalidate(lv_screen_active());lv_refr_now(d);observing=false;
 std::cout<<"GLYPHS "<<active;
 for(const auto&g:glyphs)std::cout<<" "<<std::hex<<g.cp<<std::dec<<"@"<<g.x<<","<<g.baseline<<","<<g.level;
 std::cout<<'\n';save(out+'/'+active+".ppm");
}
static void expected(std::initializer_list<uint32_t> cps) {
 std::vector<uint32_t> actual;for(const auto&g:glyphs)actual.push_back(g.cp);
 check(actual==std::vector<uint32_t>(cps),"submitted glyph sequence matches independent Unicode expectation");
}
// Same virtual presentation classification used by TutorPresentation.inc.
// WHY: clone must retain the relation role without guessing it from Unicode.
class ProductIntegerMembership final:public NodeSymbol {
public:ProductIntegerMembership():NodeSymbol(u8"∈"){}
 MathClass mathClass()const override{return MathClass::REL;}
};
int main(int argc,char**argv) {
 if(argc!=2)return 2;const std::string out=argv[1];lv_init();auto*d=lv_display_create(320,240);
 lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,drawbuffer,nullptr,sizeof(drawbuffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
 MathCanvas canvas;canvas.create(lv_screen_active());canvas.setAutoHeightEnabled(false);lv_obj_set_pos(canvas.obj(),0,0);lv_obj_set_size(canvas.obj(),320,240);glyphs.reserve(64);
 // WHY: literals below are independent of registry presentation fields and renderer helpers.
 struct RefCase {const char*key;unsigned id;uint32_t base,descriptor;};
 const RefCase cases[]={{"electron_mass",3,0x1D45A,'e'},{"hartree_energy",4,0x1D438,'h'},
  {"planck_length",7,0x2113,'p'},{"planck_time",8,0x1D461,'p'},{"planck_mass",9,0x1D45A,'p'},
  {"planck_force",10,0x1D439,'p'},{"planck_energy",11,0x1D438,'p'},{"planck_temperature",12,0x1D447,'p'},
  {"bohr_radius",2,0x1D44E,'0'},{"standard_gravity",13,0x1D454,'0'}};
 for(const auto&c:cases) {
  const auto*r=ref(c.key);check(r&&unsigned(r->id)==c.id,"independent reference identity");if(!r)continue;
  const units::ReferenceAtom atom{r->id,units::PrefixId(0)};auto tree=row(makeQuantityReference(atom));auto cloned=cloneNode(tree.get());
  active=c.key;symbolRoles(tree.get());symbolRoles(cloned.get());
  uint8_t encoded[5]{};const uint8_t wanted[]={0x52,1,uint8_t(c.id),uint8_t(c.id>>8),0};check(units::encode(atom,encoded)&&!std::memcmp(encoded,wanted,5),"closed semantic reference bytes unchanged");
  auto*copy=static_cast<const NodeQuantityReference*>(cloned->child(0));check(copy->atom()==atom,"clone retains reference identity");
  const auto before=quantity::evaluate(tree.get(),nullptr,nullptr);check(before.error==quantity::Error::None,"typed reference evaluation succeeds");
  check(equalValue(before,quantity::evaluate(cloned.get(),nullptr,nullptr)),"clone retains canonical value, dimensions, uncertainty source and sensitivity");
  for(unsigned style=0;style<8;++style){active=std::string(c.key)+"-s"+std::to_string(style);paint(canvas,d,static_cast<NodeRow*>(tree.get()),style,out);expected({c.base,c.descriptor});
   check(equalValue(before,quantity::evaluate(tree.get(),nullptr,nullptr)),"presentation leaves typed value and source unchanged");}
  std::string serialized,error;check(!CalculationEngine::serializeForGiac(tree.get(),serialized,error)&&error=="typed quantity references require the quantity engine","scalar boundary still rejects reference semantics");
  canvas.setExpression(nullptr,nullptr);
 }
 // A descriptor in the denominator uses the same presentation role.
 auto atomic=row(makeQuantityReference({units::ReferenceId(5),units::PrefixId(0)}));
 active="atomic-time-denominator";symbolRoles(atomic.get());paint(canvas,d,static_cast<NodeRow*>(atomic.get()),0,out);expected({0x210F,0x1D438,'h'});canvas.setExpression(nullptr,nullptr);
 // Clone the symbol itself, not only a QRef whose clone rebuilds its notation.
 for(const char*name:{"e","h","p","SPL",u8"α"})for(bool upright:{false,true}){
  active=std::string("clone-symbol-")+name+(upright?"-upright":"-italic");auto n=std::make_unique<NodeSymbol>(name,upright);auto copy=cloneNode(n.get());
  auto*c=static_cast<const NodeSymbol*>(copy.get());check(c->name()==name&&c->upright()==upright,"clone retains name and explicit presentation role");
  std::string a,b,ea,eb;const bool sa=CalculationEngine::serializeForGiac(n.get(),a,ea),sb=CalculationEngine::serializeForGiac(copy.get(),b,eb);
  auto other=std::make_unique<NodeSymbol>(name,!upright);std::string v,ev;const bool sv=CalculationEngine::serializeForGiac(other.get(),v,ev);
  check(sa==sb&&a==b&&ea==eb&&sa==sv&&a==v&&ea==ev,"presentation role does not alter serializer text or typed rejection");
 }
 auto member=std::make_unique<NodeRow>();member->appendChild(makeSymbol("k"));member->appendChild(std::make_unique<ProductIntegerMembership>());member->appendChild(makeSymbol(u8"ℤ"));auto memberCopy=cloneNode(member.get());
 for(unsigned style=0;style<8;++style){active="membership-clone-s"+std::to_string(style);check(member->child(1)->mathClass()==MathClass::REL&&memberCopy->child(1)->mathClass()==MathClass::REL,"clone retains virtual relation presentation class");
  paint(canvas,d,member.get(),style,out);active+="-copy";paint(canvas,d,static_cast<NodeRow*>(memberCopy.get()),style,out);
  check(member->layout().width==memberCopy->layout().width,"membership clone retains row width");
  for(int i=0;i<3;++i)check(member->child(i)->layout().rowX==memberCopy->child(i)->layout().rowX,"membership clone retains atom row origins");
 }
 canvas.setExpression(nullptr,nullptr);
 // The actual insertion controller captures a QRef atom into the power's base row.
 auto edit=std::make_unique<NodeRow>();CursorController ctrl;ctrl.init(edit.get());
 active="reference-square-capture";check(ctrl.insertPrepared(makeQuantityReference({units::ReferenceId(3),units::PrefixId(0)}),nullptr),"reference selection insertion");ctrl.insertPower();ctrl.insertDigit('2');
 check(edit->childCount()==1&&edit->child(0)->type()==NodeType::Power,"square captures preceding reference");
 auto*p=static_cast<NodePower*>(edit->child(0));check(p->base()->type()==NodeType::Row&&p->base()->childCount()==1&&p->base()->child(0)->type()==NodeType::QuantityReference,"Power(Row(QRef)) product topology");
 auto*qref=static_cast<NodeQuantityReference*>(p->base()->child(0));auto*sub=static_cast<const NodeSubscript*>(qref->notation());
 const auto before=quantity::evaluate(edit.get(),nullptr,nullptr);check(before.error==quantity::Error::None&&before.value.dimension.powers[1]==2,"square keeps typed mass-squared semantics");
 for(unsigned style=0;style<8;++style){active="reference-square-s"+std::to_string(style);paint(canvas,d,edit.get(),style,out);expected({0x1D45A,'e','2'});
  if(glyphs.size()==3){check(glyphs[2].x==glyphs[1].x,"electron mass scripts share the same nucleus edge");check(glyphs[2].baseline<glyphs[0].baseline&&glyphs[1].baseline>glyphs[0].baseline,"scripts straddle shared nucleus baseline");}
  check(p->base()->layout().width==qref->layout().width&&qref->layout().width==sub->layout().width,"transparent wrappers refreshed after paired script adjustment");
  check(equalValue(before,quantity::evaluate(edit.get(),nullptr,nullptr)),"paired script placement preserves typed source");}
 canvas.setExpression(nullptr,nullptr);
 // Unit labels have upright Latin glyphs; delta is part of the typed temperature role.
 struct UnitCase{unsigned id;units::TemperatureRole role;std::vector<uint32_t> cps;const char*name;};
 const UnitCase unitsToTest[]={{1,units::TemperatureRole::None,{'m'},"metre"},{5,units::TemperatureRole::Absolute,{'K'},"kelvin"},{23,units::TemperatureRole::Absolute,{0xB0,'C'},"celsius"},{60,units::TemperatureRole::Absolute,{0xB0,'F'},"fahrenheit"},{5,units::TemperatureRole::Difference,{0x394,'K'},"delta-kelvin"},{23,units::TemperatureRole::Difference,{0x394,0xB0,'C'},"delta-celsius"},{60,units::TemperatureRole::Difference,{0x394,0xB0,'F'},"delta-fahrenheit"}};
 for(const auto&u:unitsToTest){const units::Atom atom{units::UnitId(u.id),units::PrefixId(0),u.role};auto t=row(makeUnit(atom));auto copy=cloneNode(t.get());
  active=u.name;check(static_cast<const NodeUnit*>(copy->child(0))->atom()==atom,"clone retains temperature role");uint8_t bytes[5]{};units::Atom decoded{};check(units::encode(atom,bytes)&&units::decode(bytes,5,decoded)&&decoded==atom,"unit role serialization round trip");
  for(unsigned style=0;style<8;++style){active=std::string(u.name)+"-s"+std::to_string(style);paint(canvas,d,static_cast<NodeRow*>(t.get()),style,out);std::vector<uint32_t> actual;for(const auto&g:glyphs)actual.push_back(g.cp);check(actual==u.cps,"unit upright glyphs and temperature delta survive all styles");}canvas.setExpression(nullptr,nullptr);
 }
 std::cout<<"SUMMARY checks="<<checks<<" failures="<<failures<<" frames=161 styles=8 sizeofSymbol="<<sizeof(NodeSymbol)<<'\n';
 canvas.destroy();lv_display_delete(d);lv_deinit();return failures?1:0;
}
