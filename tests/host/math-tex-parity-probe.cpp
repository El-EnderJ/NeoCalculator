// SPDX-License-Identifier: GPL-3.0-or-later
// Private native harness. The runner instruments a copy of the renderer;
// observations come from actual LVGL submission, not reconstructed placement.
#include "ui/MathRenderer.h"
#include "ui/MathTypography.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <new>
#include <string>
#include <vector>
using namespace vpam;
bool setting_complex_enabled=false;
static bool counting=false, observing=false;
static size_t allocations=0;
void* operator new(size_t n){if(counting)++allocations;if(void*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}
template<class... T> static NodePtr row(T... children){auto n=std::make_unique<NodeRow>();(n->appendChild(std::move(children)),...);return n;}
template<class... T> static NodePtr collection(CollectionKind k,T... children){auto n=std::make_unique<NodeCollection>(k);(n->appendElement(std::move(children)),...);return n;}
static NodePtr call(const char*s,NodePtr arg){auto n=std::make_unique<NodeCall>(s);n->appendArgument(std::move(arg));return n;}
// Actual Tutor uses this explicit REL presentation role for integer membership.
// The native fixture preserves that contract without changing Symbol identity.
struct MembershipSymbol final : NodeSymbol {MembershipSymbol():NodeSymbol("\xE2\x88\x88"){} MathClass mathClass()const override{return MathClass::REL;}};
static NodePtr membership(){return std::make_unique<MembershipSymbol>();}
static NodePtr matrix(){auto n=std::make_unique<NodeMatrix>(2,2);n->setCell(0,0,makeNumber("1"));n->setCell(0,1,makeSymbol("x"));n->setCell(1,0,makeFraction(makeNumber("1"),makeNumber("2")));n->setCell(1,1,makeRoot(makeNumber("2")));return n;}
static NodePtr piecewise(){auto n=std::make_unique<NodePiecewise>();n->appendBranch(makePower(makeSymbol("x"),makeNumber("2")),row(makeSymbol("x"),makeOperator(OpKind::Ge),makeNumber("0")),false);n->appendBranch(row(makeOperator(OpKind::Sub),makeSymbol("x")),nullptr,true);return n;}
static NodePtr wide(unsigned count){auto n=std::make_unique<NodeRow>();for(unsigned i=0;i<count;++i){if(i)n->appendChild(makeOperator(OpKind::Add));n->appendChild(makeSymbol("x"));}return n;}
static NodePtr unit(const char*key,const char*prefix,numos::units::TemperatureRole role){
 using namespace numos::units;const Definition*d=nullptr;const Prefix*p=nullptr;
 for(const auto&x:kDefinitions)if(!std::strcmp(x.key,key))d=&x;
 for(const auto&x:kPrefixes)if(!std::strcmp(x.symbol,prefix))p=&x;
 if(!d||!p){std::cerr<<"Unknown unit "<<key<<'/'<<prefix;std::exit(2);}return makeUnit({d->id,p->id,role});
}
static NodePtr reference(const char*key){using namespace numos::units;for(const auto&x:kReferences)if(!std::strcmp(x.key,key)||!std::strcmp(x.symbol,key))return makeQuantityReference({x.id,PrefixId(0)});std::cerr<<"Unknown reference "<<key;std::exit(2);}
#include "corpus.inc"
static std::vector<std::pair<const MathNode*,std::string>> paths;
static std::string activePath;
static std::vector<std::string> activePaths;
static std::string q(const std::string&s){std::string o="\"";for(char c:s){if(c=='"'||c=='\\')o+='\\';if(c=='\n')o+="\\n";else if(c=='\r')o+="\\r";else o+=c;}return o+'"';}
static std::string path(const MathNode*n){for(const auto&p:paths)if(p.first==n)return p.second;return "?";}
static void index(const MathNode*n,const std::string&p){if(!n)return;paths.emplace_back(n,p);for(int i=0;i<n->childCount();++i)index(n->child(i),p+'/'+std::to_string(i));}
static bool cloneRoles(const MathNode*a,const MathNode*b){if(!a||!b)return a==b;if(a->type()!=b->type()||a->childCount()!=b->childCount()||a->mathClass()!=b->mathClass())return false;
#ifdef PARITY_UPRIGHT
 if(a->type()==NodeType::Symbol && static_cast<const NodeSymbol*>(a)->upright()!=static_cast<const NodeSymbol*>(b)->upright())return false;
#endif
 for(int i=0;i<a->childCount();++i)if(!cloneRoles(a->child(i),b->child(i)))return false;return true;}
namespace vpam {
void parityScopeEnter(){if(observing)activePaths.push_back(activePath);}
void parityScopeExit(){if(observing){activePath=std::move(activePaths.back());activePaths.pop_back();}}
void parityNode(const MathNode*n,int x,int y,const FontMetrics&fm){if(!observing)return;activePath=path(n);const auto&l=n->layout();
 std::cout<<"{\"event\":\"node\",\"path\":"<<q(activePath)<<",\"type\":"<<int(n->type())<<",\"class\":"<<int(n->mathClass())<<",\"left\":"<<int(l.effectiveLeft)<<",\"right\":"<<int(l.effectiveRight)<<",\"style\":"<<int(fm.style)<<",\"level\":"<<int(fm.scriptLevel)<<",\"em\":"<<fm.emSize<<",\"axis\":"<<fm.axisHeight()<<",\"x\":"<<x<<",\"baseline\":"<<y<<",\"width\":"<<l.width<<",\"ascent\":"<<l.ascent<<",\"descent\":"<<l.descent<<",\"inkAscent\":"<<l.inkAscent<<",\"inkDescent\":"<<l.inkDescent<<",\"rowX\":"<<l.rowX<<",\"spaceBefore\":"<<l.spaceBefore<<",\"repeatCount\":"<<(n->type()==NodeType::PeriodicDecimal?static_cast<const NodePeriodicDecimal*>(n)->repeat().size():0)<<",\"functionKind\":"<<(n->type()==NodeType::Function?int(static_cast<const NodeFunction*>(n)->funcKind()):-1)<<",\"operatorCp\":"<<(n->type()==NodeType::BigOp?static_cast<const NodeBigOp*>(n)->operatorCodepoint():0)<<",\"children\":"<<n->childCount()<<"}\n";
}
void parityGlyph(int x,int y,unsigned level,uint32_t cp,const lv_font_glyph_dsc_t&g){if(observing)std::cout<<"{\"event\":\"glyph\",\"path\":"<<q(activePath)<<",\"cp\":"<<cp<<",\"x\":"<<x<<",\"baseline\":"<<y<<",\"level\":"<<level<<",\"advance\":"<<g.adv_w<<",\"inkX\":"<<x+g.ofs_x<<",\"inkY\":"<<y-g.box_h-g.ofs_y<<",\"inkW\":"<<g.box_w<<",\"inkH\":"<<g.box_h<<"}\n";}
void parityLine(int x1,int y1,int x2,int y2,int w){if(observing)std::cout<<"{\"event\":\"line\",\"path\":"<<q(activePath)<<",\"x1\":"<<x1<<",\"y1\":"<<y1<<",\"x2\":"<<x2<<",\"y2\":"<<y2<<",\"thickness\":"<<w<<"}\n";}
void parityStroke(int x1,int y1,int x2,int y2,int w){if(observing)std::cout<<"{\"event\":\"stroke\",\"path\":"<<q(activePath)<<",\"x1\":"<<x1<<",\"y1\":"<<y1<<",\"x2\":"<<x2<<",\"y2\":"<<y2<<",\"thickness\":"<<w<<"}\n";}
void parityDelimiterRequest(int x,int top,int bottom,uint32_t cp){if(observing)std::cout<<"{\"event\":\"delimiterRequest\",\"path\":"<<q(activePath)<<",\"x\":"<<x<<",\"top\":"<<top<<",\"bottom\":"<<bottom<<",\"cp\":"<<cp<<"}\n";}
void parityRect(int x,int y,int w,int h){if(observing)std::cout<<"{\"event\":\"rect\",\"path\":"<<q(activePath)<<",\"x\":"<<x<<",\"y\":"<<y<<",\"width\":"<<w<<",\"height\":"<<h<<"}\n";}
void parityMissing(uint32_t cp,unsigned level){if(observing)std::cout<<"{\"event\":\"missing\",\"path\":"<<q(activePath)<<",\"cp\":"<<cp<<",\"level\":"<<level<<"}\n";}
void parityCursor(const NodeRow*r,int i,int x,int y,int off,const FontMetrics&fm){if(observing)std::cout<<"{\"event\":\"cursor\",\"path\":"<<q(path(r))<<",\"index\":"<<i<<",\"origin\":"<<x<<",\"baseline\":"<<y<<",\"offset\":"<<off<<",\"style\":"<<int(fm.style)<<",\"em\":"<<fm.emSize<<"}\n";}
void parityDelimiter(int x,int y,int em,uint32_t cp,const lv_font_t*f,const lv_area_t& clip){if(!observing)return;lv_font_glyph_dsc_t g{};lv_font_get_glyph_dsc(f,&g,cp,0);int ix=x-g.adv_w/2+g.ofs_x,iy=y-g.box_h-g.ofs_y;
 std::cout<<"{\"event\":\"delimiter\",\"path\":"<<q(activePath)<<",\"cp\":"<<cp<<",\"x\":"<<x<<",\"baseline\":"<<y<<",\"em\":"<<em<<",\"inkX\":"<<ix<<",\"inkY\":"<<iy<<",\"inkW\":"<<g.box_w<<",\"inkH\":"<<g.box_h<<",\"offsetX\":"<<g.ofs_x<<",\"offsetY\":"<<g.ofs_y<<",\"advance\":"<<g.adv_w<<",\"clip\":["<<clip.x1<<','<<clip.y1<<','<<clip.x2<<','<<clip.y2<<"]}\n";}

}
static uint16_t framebuffer[320*240],drawbuffer[320*240];
static void flush(lv_display_t*d,const lv_area_t*a,uint8_t*data){auto*s=reinterpret_cast<uint16_t*>(data);for(int y=a->y1;y<=a->y2;++y)for(int x=a->x1;x<=a->x2;++x)framebuffer[y*320+x]=*s++;lv_display_flush_ready(d);}
static void save(const std::string&p){std::ofstream f(p,std::ios::binary);f<<"P6\n320 240\n255\n";for(auto x:framebuffer){unsigned char rgb[]={static_cast<unsigned char>(((x>>11)&31)*255/31),static_cast<unsigned char>(((x>>5)&63)*255/63),static_cast<unsigned char>((x&31)*255/31)};f.write(reinterpret_cast<const char*>(rgb),3);}}
static uint32_t seed=0x50415232;
static uint32_t rnd(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static NodePtr randomTree(unsigned depth){if(!depth||rnd()%4==0)return rnd()%5?makeSymbol("x"):makeEmpty();switch(rnd()%8){case 0:return makePower(randomTree(depth-1),randomTree(depth-1));case 1:return makeFraction(randomTree(depth-1),randomTree(depth-1));case 2:return makeRoot(randomTree(depth-1));case 3:return makeParen(randomTree(depth-1),DelimKind(rnd()%4));case 4:return makeSubscript(randomTree(depth-1),randomTree(depth-1));case 5:return makeFunction(FuncKind::Sin,randomTree(depth-1));case 6:return makeMeasurement(randomTree(depth-1),randomTree(depth-1),31);default:return row(randomTree(depth-1),makeOperator(OpKind::Add),randomTree(depth-1));}}
int main(int argc,char**argv){if(argc<2)return 2;const std::string out=argv[1];unsigned randomCount=argc>2?unsigned(std::strtoul(argv[2],nullptr,10)):2000;const uint32_t initialSeed=argc>3?uint32_t(std::strtoul(argv[3],nullptr,10)):seed;seed=initialSeed;const bool randomDraw=argc>4&&std::strtoul(argv[4],nullptr,10);lv_init();auto*d=lv_display_create(320,240);lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,drawbuffer,nullptr,sizeof(drawbuffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
 MathCanvas canvas;canvas.create(lv_screen_active());canvas.setAutoHeightEnabled(false);lv_obj_set_pos(canvas.obj(),0,0);lv_obj_set_size(canvas.obj(),320,240);paths.reserve(4096);
 for(unsigned ci=0;ci<sizeof(caseNames)/sizeof(*caseNames);++ci){
  auto tree=fixture(ci);auto*root=static_cast<NodeRow*>(tree.get());CursorController ctrl;ctrl.init(root);paths.clear();index(root,"r");
  for(unsigned style=0;style<PARITY_STYLE_COUNT;++style){
   canvas.setMathStyle(MathStyle(style));canvas.setExpression(root,&ctrl);canvas.resetScroll();canvas.stopCursorBlink();
   std::cout<<"{\"event\":\"case\",\"id\":"<<q(caseNames[ci])<<",\"style\":"<<style<<"}\n";
   observing=true;lv_obj_invalidate(lv_screen_active());lv_refr_now(d);observing=false;
   save(out+'/'+caseNames[ci]+"-s"+std::to_string(style)+".ppm");
   const std::string before=dumpTree(root);auto fm=canvas.normalMetrics();allocations=0;counting=true;auto start=std::chrono::steady_clock::now();for(unsigned k=0;k<100;++k)root->calculateLayout(fm);auto stop=std::chrono::steady_clock::now();counting=false;
   const bool stable=before==dumpTree(root);auto cloned=cloneNode(root);const bool roles=cloneRoles(root,cloned.get());std::cout<<"{\"event\":\"profile\",\"cloneRoles\":"<<(roles?"true":"false")<<",\"allocations\":"<<allocations<<",\"stable\":"<<(stable?"true":"false")<<",\"layoutUs\":"<<std::chrono::duration<double,std::micro>(stop-start).count()/100<<"}\n";
   {
    for(const auto&entry:paths)if(entry.first->type()==NodeType::Row){auto*slot=const_cast<NodeRow*>(static_cast<const NodeRow*>(entry.first));if(!ctrl.focusSlot(slot))continue;
     for(int j=0;j<=slot->childCount();++j){if(j)ctrl.moveRight();if(ctrl.cursor().row!=slot)break;if(style!=1 && j!=0 && j!=slot->childCount())continue;
      std::cout<<"{\"event\":\"frame\",\"row\":"<<q(entry.second)<<",\"index\":"<<ctrl.cursor().index<<"}\n";observing=true;canvas.resetCursorBlink();lv_obj_invalidate(lv_screen_active());lv_refr_now(d);observing=false;
      lv_area_t b{};if(canvas.cursorBounds(b))std::cout<<"{\"event\":\"cursorBounds\",\"x\":"<<b.x1<<",\"y\":"<<b.y1<<",\"bottom\":"<<b.y2<<"}\n";
     }
    }
   }
  }
  canvas.setExpression(nullptr,nullptr);
 }
 size_t totalAlloc=0,unstable=0,badBox=0;double totalUs=0;unsigned maxNodes=0,maxDepth=0,styleCounts[PARITY_STYLE_COUNT]{};
 for(unsigned i=0;i<randomCount;++i){
  const unsigned style=i%PARITY_STYLE_COUNT;++styleCounts[style];canvas.setMathStyle(MathStyle(style));const auto fm=canvas.normalMetrics();
  const unsigned depth=2+rnd()%5;auto t=row(randomTree(depth));t->calculateLayout(fm);const auto before=dumpTree(t.get());paths.clear();index(t.get(),"r");maxNodes=std::max<unsigned>(maxNodes,paths.size());maxDepth=std::max(maxDepth,depth);
  allocations=0;counting=true;auto start=std::chrono::steady_clock::now();t->calculateLayout(fm);auto stop=std::chrono::steady_clock::now();counting=false;const bool stable=before==dumpTree(t.get());totalAlloc+=allocations;const double micros=std::chrono::duration<double,std::micro>(stop-start).count();totalUs+=micros;unstable+=!stable;for(auto&p:paths)badBox+=p.first->layout().width<0||p.first->layout().ascent<0||p.first->layout().descent<0;
  if(randomDraw){
   CursorController ctrl;ctrl.init(static_cast<NodeRow*>(t.get()));canvas.setExpression(static_cast<NodeRow*>(t.get()),&ctrl);canvas.resetScroll();canvas.stopCursorBlink();
   std::cout<<"{\"event\":\"case\",\"id\":\"R-"<<i<<"\",\"style\":"<<style<<"}\n";observing=true;lv_obj_invalidate(lv_screen_active());lv_refr_now(d);observing=false;
   std::cout<<"{\"event\":\"profile\",\"allocations\":"<<allocations<<",\"stable\":"<<(stable?"true":"false")<<",\"layoutUs\":"<<micros<<"}\n";canvas.setExpression(nullptr,nullptr);
  }
 }
 std::cout<<"{\"event\":\"random\",\"count\":"<<randomCount<<",\"seed\":"<<initialSeed<<",\"rendered\":"<<(randomDraw?"true":"false")<<",\"styleCounts\":[";for(unsigned i=0;i<PARITY_STYLE_COUNT;++i){if(i)std::cout<<',';std::cout<<styleCounts[i];}std::cout<<"],\"maxFactoryDepth\":"<<maxDepth<<",\"maxNodes\":"<<maxNodes<<",\"allocations\":"<<totalAlloc<<",\"unstable\":"<<unstable<<",\"badBox\":"<<badBox<<",\"meanLayoutUs\":"<<(randomCount?totalUs/randomCount:0)<<"}\n";
 std::cout<<"{\"event\":\"sizes\",\"node\":"<<sizeof(MathNode)<<",\"layout\":"<<sizeof(LayoutResult)<<",\"metrics\":"<<sizeof(FontMetrics)<<",\"power\":"<<sizeof(NodePower)<<",\"subscript\":"<<sizeof(NodeSubscript)<<",\"symbol\":"<<sizeof(NodeSymbol)<<"}\n";canvas.destroy();lv_display_delete(d);lv_deinit();return totalAlloc||unstable||badBox?1:0;
}
