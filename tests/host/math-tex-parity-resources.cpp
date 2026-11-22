// SPDX-License-Identifier: GPL-3.0-or-later
// Private host probe: construction, snapshots and reporting are outside scopes.
#include "ui/MathRenderer.h"
#include "resource-profile.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <new>
#include <malloc.h>
#include <string>
#include <utility>
using namespace vpam;
bool setting_complex_enabled=false;

namespace {
void cppAttempt() { if(resource_profile::active) { ++resource_profile::counts.cppAllocations; if(resource_profile::denyCpp) throw std::bad_alloc(); } }
void* cppAllocate(std::size_t n) { cppAttempt(); if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc(); }
}
void* operator new(std::size_t n){return cppAllocate(n);}
void* operator new[](std::size_t n){return cppAllocate(n);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
void* operator new(std::size_t n,const std::nothrow_t&)noexcept{try{return cppAllocate(n);}catch(...){return nullptr;}}
void* operator new[](std::size_t n,const std::nothrow_t&)noexcept{try{return cppAllocate(n);}catch(...){return nullptr;}}
void operator delete(void* p,const std::nothrow_t&)noexcept{std::free(p);}
void operator delete[](void* p,const std::nothrow_t&)noexcept{std::free(p);}
void* operator new(std::size_t n,std::align_val_t a){cppAttempt();if(void* p=_aligned_malloc(n?n:1,static_cast<std::size_t>(a)))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n,std::align_val_t a){return ::operator new(n,a);}
void* operator new(std::size_t n,std::align_val_t a,const std::nothrow_t&)noexcept{try{return ::operator new(n,a);}catch(...){return nullptr;}}
void* operator new[](std::size_t n,std::align_val_t a,const std::nothrow_t&)noexcept{try{return ::operator new(n,a);}catch(...){return nullptr;}}
void operator delete(void* p,std::align_val_t)noexcept{_aligned_free(p);}
void operator delete[](void* p,std::align_val_t)noexcept{_aligned_free(p);}
void operator delete(void* p,std::size_t,std::align_val_t)noexcept{_aligned_free(p);}
void operator delete[](void* p,std::size_t,std::align_val_t)noexcept{_aligned_free(p);}
void operator delete(void* p,std::align_val_t,const std::nothrow_t&)noexcept{_aligned_free(p);}
void operator delete[](void* p,std::align_val_t,const std::nothrow_t&)noexcept{_aligned_free(p);}

extern "C" void* __real_malloc(std::size_t);
extern "C" void* __real_calloc(std::size_t,std::size_t);
extern "C" void* __real_realloc(void*,std::size_t);
static bool denyCAllocation(){if(!resource_profile::active)return false;++resource_profile::counts.cAllocations;if(resource_profile::cursorDepth)++resource_profile::counts.cursorCAllocations;return resource_profile::denyC;}
extern "C" void* __wrap_malloc(std::size_t n){return denyCAllocation()?nullptr:__real_malloc(n);}
extern "C" void* __wrap_calloc(std::size_t n,std::size_t s){return denyCAllocation()?nullptr:__real_calloc(n,s);}
extern "C" void* __wrap_realloc(void* p,std::size_t n){return denyCAllocation()?nullptr:__real_realloc(p,n);}
extern "C" bool __real_lv_font_get_glyph_dsc(const lv_font_t*,lv_font_glyph_dsc_t*,uint32_t,uint32_t);
extern "C" bool __wrap_lv_font_get_glyph_dsc(const lv_font_t* f,lv_font_glyph_dsc_t* d,uint32_t c,uint32_t n){if(resource_profile::active)++resource_profile::counts.glyphLookups;return __real_lv_font_get_glyph_dsc(f,d,c,n);}
namespace vpam {
void* testNodeAllocate(std::size_t n){if(resource_profile::active){++resource_profile::counts.nodeAllocations;if(resource_profile::denyCpp)throw std::bad_alloc();}if(auto* p=std::malloc(n))return p;throw std::bad_alloc();}
void testNodeRelease(void* p){std::free(p);}
}

template<class... T> static NodePtr row(T... nodes){auto r=std::make_unique<NodeRow>();(r->appendChild(std::move(nodes)),...);return r;}
static NodePtr var(){return makeVariable('x');}
static NodePtr number(const char* s){return makeNumber(s);}
static NodePtr fraction(NodePtr a,NodePtr b){return makeFraction(row(std::move(a)),row(std::move(b)));}
static NodePtr wide(unsigned count){auto r=std::make_unique<NodeRow>();for(unsigned i=0;i<count;++i){if(i)r->appendChild(makeOperator(OpKind::Add));r->appendChild(var());}return r;}
static NodePtr fracTower(unsigned n){NodePtr t=number("1");for(unsigned i=0;i<n;++i)t=fraction(number("1"),std::move(t));return t;}
static NodePtr unit(uint16_t id,uint8_t prefix=0){using namespace numos::units;return makeUnit({UnitId(id),PrefixId(prefix)});}
static NodePtr reference(uint16_t id){using namespace numos::units;return makeQuantityReference({ReferenceId(id),PrefixId(0)});}
static NodePtr fixture(unsigned which){
 switch(which){
 case 0:return row(number("2"),makeOperator(OpKind::Add),number("3"));
 case 1:return wide(150);
 case 2:{NodePtr n=var();for(unsigned i=0;i<6;++i)n=makePower(row(var()),row(std::move(n)));return row(std::move(n));}
 case 3:return row(fracTower(6));
 case 4:return row(makeRoot(row(fraction(row(number("1"),makeOperator(OpKind::Add),makePower(var(),number("2"))),row(number("1"),makeOperator(OpKind::Sub),var())))));
 case 5:return row(makeParen(row(fracTower(22)),DelimKind::Bracket));
 case 6:return row(var(),makeRelation(OpKind::Eq),fraction(row(makeOperator(OpKind::Sub),makeVariable('b'),makeOperator(OpKind::PlusMinus),makeRoot(row(makePower(makeVariable('b'),number("2")),makeOperator(OpKind::Sub),number("4"),makeVariable('a'),makeVariable('c')))),row(number("2"),makeVariable('a'))));
 case 7:return row(makeMeasurement(row(number("10")),row(number("1")),31),unit(1));
 case 8:return row(fraction(row(unit(2,10),makeOperator(OpKind::Mul),unit(1)),makePower(unit(3),number("2"))));
 case 9:return row(makeMeasurement(row(number("6.67430")),row(number("0.00015")),31),makeOperator(OpKind::Mul),makePower(row(number("10")),row(makeOperator(OpKind::Sub),number("11"))));
 case 10:return wide(150);
 case 11:return row(makePower(row(reference(3)),row(number("2"))),makeOperator(OpKind::Mul),reference(2));
 default:return nullptr;
 }
}
static const char* names[]={"short-scalar","long-row","nested-scripts","nested-fractions","radical","delimiter-assembly","tutor-quadratic-page","measurement","units","scientific-result","panning-unchanged","constant-scripts"};
static uint16_t buffer[320*240];
static void flush(lv_display_t* d,const lv_area_t*,uint8_t*){lv_display_flush_ready(d);}
static unsigned failures=0;
static void check(bool good,const char* label){if(!good){++failures;std::fprintf(stderr,"FAIL %s\n",label);}}
static std::string semanticDump(const MathNode* node){
 // dumpTree includes cached [width x height a/d] suffixes. Refusal may reset
 // the root box, so compare its semantic text without those geometry suffixes.
 const auto dump=dumpTree(node);std::string result;
 for(size_t start=0;start<dump.size();){const auto end=dump.find('\n',start);const auto stop=end==std::string::npos?dump.size():end;const auto metrics=dump.rfind("  [",stop);const auto textEnd=metrics!=std::string::npos&&metrics>=start?metrics:stop;result.append(dump,start,textEnd-start);result+='\n';start=stop+1;}
 return result;
}
static void report(const char* name,unsigned style,const char* phase,unsigned repeats,uint64_t ns){
 const auto c=resource_profile::counts;
 std::printf("{\"case\":\"%s\",\"style\":%u,\"phase\":\"%s\",\"repeats\":%u,\"elapsedNs\":%llu,"
 "\"rootLayouts\":%llu,\"nodeLayouts\":%llu,\"rowMeasures\":%llu,\"admissionCalls\":%llu,\"maxLayoutDepth\":%llu,\"glyphLookups\":%llu,"
 "\"mathConstantLookups\":%llu,\"variantSearches\":%llu,\"mathKernLookups\":%llu,\"draws\":%llu,\"paintNodes\":%llu,"
 "\"cursorCalls\":%llu,\"cursorNs\":%llu,\"cursorLayoutCalls\":%llu,\"cursorCAllocations\":%llu,\"cppAllocations\":%llu,\"nodeAllocations\":%llu,\"cAllocations\":%llu}\n",
 name,style,phase,repeats,ns,c.rootLayouts,c.nodeLayouts,c.rowMeasures,c.admissionCalls,c.maxLayoutDepth,c.glyphLookups,c.mathConstants,c.variants,c.mathKerns,c.draws,c.paintNodes,c.cursorCalls,c.cursorNs,c.cursorLayouts,c.cursorCAllocations,c.cppAllocations,c.nodeAllocations,c.cAllocations);
}
static void layoutPublic(MathNode& node,const FontMetrics& fm){
#ifdef NUMOS_RESOURCE_GUARD
 check(calculateLayoutChecked(node,fm)==LayoutStatus::Ok,"valid fixture admission");
#else
 node.calculateLayout(fm);
#endif
}
int main(int argc,char** argv){
 const unsigned layouts=argc>1?unsigned(std::strtoul(argv[1],nullptr,10)):1000;
 lv_init();auto* display=lv_display_create(320,240);lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(display,buffer,nullptr,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(display,flush);
 MathCanvas canvas;canvas.create(lv_screen_active());canvas.setAutoHeightEnabled(false);lv_obj_set_size(canvas.obj(),320,240);canvas.stopCursorBlink();
 // C allocator wrappers must actually intercept linked calls, or zero is vacuous.
 void* resizeInput=std::malloc(17);
 resource_profile::begin(false,true);
 void* (*volatile allocate)(size_t)=std::malloc;void* denied=allocate(17);
 void* (*volatile zeroAllocate)(size_t,size_t)=std::calloc;void* zeroDenied=zeroAllocate(3,17);
 void* (*volatile resize)(void*,size_t)=std::realloc;void* resizeDenied=resize(resizeInput,37);
 resource_profile::end();
 check(!denied&&!zeroDenied&&!resizeDenied&&resource_profile::counts.cAllocations==3,"C allocation denial causal controls");
 if(denied)std::free(denied);if(zeroDenied)std::free(zeroDenied);std::free(resizeDenied?resizeDenied:resizeInput);
 resource_profile::begin(true,false);
 unsigned caught=0;try{(void)::operator new(17);}catch(const std::bad_alloc&){++caught;}
 try{(void)::operator new[](17);}catch(const std::bad_alloc&){++caught;}
 try{(void)::operator new(17,std::align_val_t(32));}catch(const std::bad_alloc&){++caught;}
 try{(void)vpam::testNodeAllocate(17);}catch(const std::bad_alloc&){++caught;}
 void* nothrowDenied=::operator new(17,std::nothrow);
 resource_profile::end();
 check(caught==4&&!nothrowDenied&&resource_profile::counts.cppAllocations==4&&resource_profile::counts.nodeAllocations==1,"C++ and node allocation denial causal controls");
#ifdef NUMOS_RESOURCE_GUARD
 {
  NodePtr tooDeep=number("1");for(int i=0;i<kLayoutMaxDepth+1;++i)tooDeep=row(std::move(tooDeep));
  const auto semantic=semanticDump(tooDeep.get());
  resource_profile::begin(true,true);const auto status=calculateLayoutChecked(*tooDeep,canvas.normalMetrics());resource_profile::end();
  check(status==LayoutStatus::DepthLimit,"oversized tree rejected");
  check(resource_profile::counts.admissionCalls==1&&resource_profile::counts.nodeLayouts==0,"structural refusal precedes recursive layout");
  check(resource_profile::counts.cppAllocations==0&&resource_profile::counts.nodeAllocations==0&&resource_profile::counts.cAllocations==0,"structural admission allocates nothing");
  check(semantic==semanticDump(tooDeep.get()),"refusal preserves semantic tree");
 }
#endif
 for(unsigned i=0;i<sizeof(names)/sizeof(*names);++i){
  auto tree=fixture(i);check(bool(tree),"fixture construction");if(!tree)continue;auto* r=static_cast<NodeRow*>(tree.get());CursorController cursor;cursor.init(r);
  for(unsigned style: {0U,1U,2U,3U}){
   cursor.init(r);cursor.moveLeft(); // Begin at the end, then visit real nested editor slots.
   canvas.setMathStyle(MathStyle(style));canvas.setExpression(r,&cursor);canvas.resetScroll();lv_obj_invalidate(canvas.obj());lv_refr_now(display);auto fm=canvas.normalMetrics();
   const auto before=semanticDump(r);const auto beforeBox=r->layout();
   resource_profile::begin(true,true);const auto start=resource_profile::now();try{for(unsigned j=0;j<layouts;++j)layoutPublic(*r,fm);}catch(...){check(false,"layout allocation denied");}const auto elapsed=resource_profile::now()-start;resource_profile::end();
   report(names[i],style,"layout-denied",layouts,elapsed);check(resource_profile::counts.cppAllocations==0&&resource_profile::counts.nodeAllocations==0&&resource_profile::counts.cAllocations==0,"zero hot geometry allocations");
   check(before==semanticDump(r),"layout semantic invariance");check(beforeBox.width==r->layout().width&&beforeBox.ascent==r->layout().ascent&&beforeBox.descent==r->layout().descent,"layout idempotence");
   // LVGL allocates draw tasks through C allocators. Count these, but deny own
   // C++/AST allocations. The separate cursor scope forbids C allocation too.
   constexpr unsigned frames=20;resource_profile::begin(true,false);const auto drawStart=resource_profile::now();try{for(unsigned j=0;j<frames;++j){if(i==10)canvas.scrollBy(j%2?37:-37);else if(j)cursor.moveRight();lv_obj_invalidate(canvas.obj());lv_refr_now(display);}}catch(...){check(false,"draw or cursor C++ allocation denied");}const auto drawNs=resource_profile::now()-drawStart;resource_profile::end();
   report(names[i],style,"draw-cursor-denied",frames,drawNs);check(resource_profile::counts.cppAllocations==0&&resource_profile::counts.nodeAllocations==0,"zero own draw cursor allocations");check(resource_profile::counts.cursorCalls>0,"real cursor computation covered");check(resource_profile::counts.cursorLayouts==0,"cursor does not relayout");check(resource_profile::counts.cursorCAllocations==0,"cursor does not allocate C heap");
   check(before==semanticDump(r),"painting semantic invariance");
  }
  canvas.setExpression(nullptr,nullptr);
 }
 canvas.destroy();lv_display_delete(display);lv_deinit();
 std::printf("{\"summary\":true,\"failures\":%u,\"layoutRepeatsPerFixtureStyle\":%u,\"styles\":4,\"cases\":12,\"cache\":\"none\",\"limitations\":\"host timings include observer overhead; linked C wrappers exclude CRT/DLL internal allocations; LVGL C draw tasks counted but not denied; no device high-water or PSRAM latency\"}\n",failures,layouts);
 return failures?1:0;
}
