// SPDX-License-Identifier: GPL-3.0-or-later
// Bounded layout integration gate. All synthetic nodes are test-only.
#include "ui/MathRenderer.h"
#include "math/CalculationEngine.h"
#include "math/Quantity.h"
#include "i18n/Locale.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
using namespace vpam;
bool setting_complex_enabled=false;
static unsigned checks=0,failures=0,frames=0;
static std::string active;
static void check(bool value,const char*why){++checks;if(!value){++failures;std::cout<<"FAIL "<<active<<": "<<why<<'\n';}}
static std::string submittedAscii;static bool observing=false;
namespace vpam {void productReferenceGlyph(int,int,unsigned,uint32_t cp,const lv_font_glyph_dsc_t&) {if(!observing||cp==' ')return;if(cp<128)submittedAscii+=char(cp);else if(cp<2048){submittedAscii+=char(0xC0|(cp>>6));submittedAscii+=char(0x80|(cp&63));}else if(cp<65536){submittedAscii+=char(0xE0|(cp>>12));submittedAscii+=char(0x80|((cp>>6)&63));submittedAscii+=char(0x80|(cp&63));}else{submittedAscii+=char(0xF0|(cp>>18));submittedAscii+=char(0x80|((cp>>12)&63));submittedAscii+=char(0x80|((cp>>6)&63));submittedAscii+=char(0x80|(cp&63));}}}
static uint16_t framebuffer[320*240],drawbuffer[320*240];
static void flush(lv_display_t*d,const lv_area_t*a,uint8_t*data){auto*s=reinterpret_cast<uint16_t*>(data);for(int y=a->y1;y<=a->y2;++y)for(int x=a->x1;x<=a->x2;++x)framebuffer[y*320+x]=*s++;lv_display_flush_ready(d);}
static void save(const std::string&p){std::ofstream f(p,std::ios::binary);f<<"P6\n320 240\n255\n";for(auto c:framebuffer){const unsigned char rgb[]={uint8_t(((c>>11)&31)*255/31),uint8_t(((c>>5)&63)*255/63),uint8_t((c&31)*255/31)};f.write(reinterpret_cast<const char*>(rgb),3);}++frames;}
static NodePtr row(NodePtr n){auto r=std::make_unique<NodeRow>();r->appendChild(std::move(n));return r;}
static NodePtr depth(unsigned edges){NodePtr n=makeNumber("1");while(edges--)n=row(std::move(n));return n;}
static NodePtr flat(unsigned children,bool empty){auto r=std::make_unique<NodeRow>();r->reserveChildren(children);for(unsigned i=0;i<children;++i)r->appendChild(empty?NodePtr(std::make_unique<NodeRow>()):makeNumber("1"));return r;}
static std::string authored(const MathNode*root){std::string out;std::vector<const MathNode*> todo{root};while(!todo.empty()){const auto*n=todo.back();todo.pop_back();if(!n){out+="null;";continue;}out+=std::to_string(unsigned(n->type()))+":"+std::to_string(n->childCount())+";";if(n->type()==NodeType::Number)out+=static_cast<const NodeNumber*>(n)->value();if(n->type()==NodeType::QuantityReference)out+=std::to_string(unsigned(static_cast<const NodeQuantityReference*>(n)->atom().reference));for(int i=n->childCount()-1;i>=0;--i)todo.push_back(n->child(i));}return out;}
static void status(MathNode&n,const FontMetrics&fm,LayoutStatus expected){const auto before=authored(&n);const auto actual=calculateLayoutChecked(n,fm);check(actual==expected,"exact checked layout status");check(authored(&n)==before,"authored tree preserved");std::cout<<"STATUS "<<active<<" expected="<<unsigned(expected)<<" actual="<<unsigned(actual)<<" width="<<n.layout().width<<" ascent="<<n.layout().ascent<<" descent="<<n.layout().descent<<'\n';}
// WHY: probe exact signed narrowing boundaries without relying on font advances.
// Real glyph/row overflow cases below independently exercise production layout.
struct CoordinateProbe final:MathNode {
 int32_t width,ascent,descent;
 CoordinateProbe(int32_t w,int32_t a=1,int32_t d=0):MathNode(NodeType::Empty),width(w),ascent(a),descent(d){}
 void calculateLayout(const FontMetrics&fm)override{_layout={};_layout.width=fm.coordinate(width);_layout.ascent=fm.coordinate(ascent);_layout.descent=fm.coordinate(descent);}
};
struct MissingChild final:MathNode {
 MissingChild():MathNode(NodeType::Empty){}
 int childCount()const override{return 1;}
 MathNode*child(int)const override{return nullptr;}
 void calculateLayout(const FontMetrics&)override{check(false,"invalid child reached layout");}
};
struct SelfCycle final:MathNode {
 SelfCycle():MathNode(NodeType::Empty){setParent(this);}
 int childCount()const override{return 1;}
 MathNode*child(int)const override{return const_cast<SelfCycle*>(this);}
 void calculateLayout(const FontMetrics&)override{check(false,"cycle reached layout");}
};
static void render(MathCanvas&canvas,lv_display_t*d,NodeRow*tree,CursorController&ctrl,const std::string&out,bool refuse){
 const auto signature=authored(tree);canvas.setExpression(tree,&ctrl);canvas.resetScroll();canvas.resetCursorBlink();submittedAscii.clear();observing=true;lv_obj_invalidate(lv_screen_active());lv_refr_now(d);observing=false;
 check((canvas.layoutStatus()!=LayoutStatus::Ok)==refuse,"canvas exposes the checked refusal status");
 lv_area_t bounds{};check(canvas.cursorBounds(bounds)!=refuse,"refusal suppresses cursor; successful recovery restores it");
 const auto ink=std::count_if(std::begin(framebuffer),std::end(framebuffer),[](uint16_t c){return c!=0xFFFF;});
 check(ink>0,"visible canvas output");check(authored(tree)==signature,"render/refusal retains authored tree");
 if(refuse)check(submittedAscii==(numos::i18n::isSpanish(numos::i18n::productLocale)?u8"Expresióndemasiadogrande":"Expressiontoolarge"),"refusal renders the actual localized bounded message");
 std::cout<<"FRAME "<<active<<" refusal="<<refuse<<" nonwhite="<<ink<<'\n';save(out+'/'+active+".ppm");canvas.setExpression(nullptr,nullptr);
}
int main(int argc,char**argv){if(argc!=2)return 2;const std::string out=argv[1];lv_init();auto*d=lv_display_create(320,240);lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,drawbuffer,nullptr,sizeof(drawbuffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
 MathCanvas canvas;canvas.create(lv_screen_active());canvas.setAutoHeightEnabled(false);lv_obj_set_pos(canvas.obj(),0,0);lv_obj_set_size(canvas.obj(),320,240);auto normal=canvas.normalMetrics();submittedAscii.reserve(40000);
 active="contract";check(kLayoutMaxDepth==192,"documented depth cap");check(kLayoutMaxNodes==4096,"documented node cap");check(kLayoutMaxExtent==30000,"documented coordinate cap");
 for(unsigned style=0;style<8;++style){canvas.setMathStyle(MathStyle(style));auto fm=canvas.normalMetrics();
  for(unsigned edges:{191U,192U,193U}){active="depth-"+std::to_string(edges)+"-s"+std::to_string(style);auto n=depth(edges);status(*n,fm,edges<=192?LayoutStatus::Ok:LayoutStatus::DepthLimit);}
  for(unsigned count:{4095U,4096U,4097U}){active="nodes-"+std::to_string(count)+"-s"+std::to_string(style);auto n=flat(count-1,true);status(*n,fm,count<=4096?LayoutStatus::Ok:LayoutStatus::NodeLimit);}
  for(int32_t extent:{29999,30000,30001,2147483647}){active="coordinate-"+std::to_string(extent)+"-s"+std::to_string(style);CoordinateProbe n(extent);status(n,fm,extent<=30000?LayoutStatus::Ok:LayoutStatus::CoordinateLimit);}
  active="signed-narrowing-s"+std::to_string(style);bool overflow=false;auto signedMetrics=fm;signedMetrics.coordinateOverflow=&overflow;check(signedMetrics.coordinate(-30000)==-30000&&!overflow,"negative exact boundary preserved");check(signedMetrics.coordinate(-30001)==-30000&&overflow,"negative overflow signaled before narrowing");
  active="height-combined-s"+std::to_string(style);CoordinateProbe tall(1,20000,20000);status(tall,fm,LayoutStatus::CoordinateLimit);
  active="real-number-overflow-s"+std::to_string(style);auto wide=makeNumber(std::string(10000,'1'));status(*wide,fm,LayoutStatus::CoordinateLimit);
  active="script-overflow-s"+std::to_string(style);auto script=makePower(makeSymbol("x"),makeNumber(std::string(10000,'1')));status(*script,fm,LayoutStatus::CoordinateLimit);
  active="fraction-overflow-s"+std::to_string(style);auto fraction=makeFraction(makeNumber(std::string(10000,'1')),makeNumber("2"));status(*fraction,fm,LayoutStatus::CoordinateLimit);
  active="radical-overflow-s"+std::to_string(style);auto radical=makeRoot(makeNumber(std::string(10000,'1')));status(*radical,fm,LayoutStatus::CoordinateLimit);
  active="recovery-s"+std::to_string(style);auto valid=row(makeNumber("123"));status(*valid,fm,LayoutStatus::Ok);
 }
 active="invalid-missing-child";MissingChild broken;check(calculateLayoutChecked(broken,normal)==LayoutStatus::InvalidTree,"missing child rejected before recursive layout");
 active="invalid-self-cycle";SelfCycle cycle;check(calculateLayoutChecked(cycle,normal)==LayoutStatus::InvalidTree,"self cycle rejected before layout");
 active="matrix-vacant-cells";NodeMatrix vacant(2,2);status(vacant,normal,LayoutStatus::Ok);
 active="invalid-parent";auto orphan=row(makeNumber("7"));orphan->child(0)->setParent(nullptr);check(calculateLayoutChecked(*orphan,normal)==LayoutStatus::InvalidTree,"wrong parent rejected");orphan->child(0)->setParent(orphan.get());status(*orphan,normal,LayoutStatus::Ok);
 active="ninety-roots";NodePtr roots=makeNumber("2");for(unsigned i=0;i<90;++i)roots=makeRoot(row(std::move(roots)));roots=row(std::move(roots));status(*roots,normal,LayoutStatus::Ok);CursorController rootCursor;rootCursor.init(static_cast<NodeRow*>(roots.get()));canvas.setMathStyle(MathStyle::DISPLAY_STYLE);render(canvas,d,static_cast<NodeRow*>(roots.get()),rootCursor,out,false);
 active="flat-two-thousand";auto many=flat(2000,false);status(*many,normal,LayoutStatus::Ok);CursorController manyCursor;manyCursor.init(static_cast<NodeRow*>(many.get()));render(canvas,d,static_cast<NodeRow*>(many.get()),manyCursor,out,false);
 // Actual overflow, depth and node refusal must each show visible bounded output.
 for(const std::string kind:{"coordinate","depth","nodes"}){active="canvas-refusal-"+kind;NodePtr bad=kind=="coordinate"?row(makeNumber(std::string(10000,'1'))):kind=="depth"?depth(193):flat(4096,true);CursorController badCursor;badCursor.init(static_cast<NodeRow*>(bad.get()));render(canvas,d,static_cast<NodeRow*>(bad.get()),badCursor,out,true);}
 active="canvas-refusal-es";numos::i18n::productLocale=numos::i18n::Locale::SpanishSpain;auto spanish=row(makeNumber(std::string(10000,'1')));CursorController spanishCursor;spanishCursor.init(static_cast<NodeRow*>(spanish.get()));render(canvas,d,static_cast<NodeRow*>(spanish.get()),spanishCursor,out,true);numos::i18n::productLocale=numos::i18n::Locale::EnglishUS;
 active="semantic-recovery";auto recovery=std::make_unique<NodeRow>();recovery->appendChild(makeNumber("2"));recovery->appendChild(makeOperator(OpKind::Add));recovery->appendChild(makeNumber("3"));std::string before,error;check(numos::CalculationEngine::serializeForGiac(recovery.get(),before,error),"ordinary semantics serialize before layout");
 for(unsigned i=0;i<50;++i){auto bad=makeNumber(std::string(10000,'1'));check(calculateLayoutChecked(*bad,normal)==LayoutStatus::CoordinateLimit,"repeat overflow rejected");check(calculateLayoutChecked(*recovery,normal)==LayoutStatus::Ok,"per-invocation overflow state resets");}
 std::string after;check(numos::CalculationEngine::serializeForGiac(recovery.get(),after,error)&&before==after,"recovery retains scalar serialization");CursorController validCursor;validCursor.init(recovery.get());render(canvas,d,recovery.get(),validCursor,out,false);
 active="quantity-source-preservation";auto quantity=row(makeQuantityReference({numos::units::ReferenceId(3),numos::units::PrefixId(0)}));const auto prior=numos::quantity::evaluate(quantity.get(),nullptr,nullptr);status(*quantity,normal,LayoutStatus::Ok);const auto next=numos::quantity::evaluate(quantity.get(),nullptr,nullptr);check(prior.error==next.error&&prior.value.coefficient==next.value.coefficient&&prior.value.dimension==next.value.dimension,"typed quantity value preserved");check(prior.value.uncertainty&&next.value.uncertainty&&prior.value.uncertainty->contributions[0].source==next.value.uncertainty->contributions[0].source,"scientific uncertainty source retained");
 std::cout<<"SUMMARY checks="<<checks<<" failures="<<failures<<" frames="<<frames<<" styles=8 sizeofSymbol="<<sizeof(NodeSymbol)<<'\n';canvas.destroy();lv_display_delete(d);lv_deinit();return failures?1:0;
}
