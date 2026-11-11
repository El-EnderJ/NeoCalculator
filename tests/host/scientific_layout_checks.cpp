#include "math/MathAST.h"
#include "ui/MathRenderer.h"
#include <cstdlib>
#include <cstdio>
#include <new>
bool setting_complex_enabled=false;
static bool deny=false;
static unsigned attempts=0,checks=0;
void* operator new(std::size_t n){if(deny){++attempts;throw std::bad_alloc();}if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void*p)noexcept{std::free(p);}
void operator delete(void*p,std::size_t)noexcept{std::free(p);}
static void check(bool ok,const char*text){++checks;if(!ok){std::fprintf(stderr,"FAIL %s\n",text);std::exit(1);}}
int main(){
 using namespace vpam;lv_init();MathCanvas canvas;const auto normal=canvas.normalMetrics();
 auto measured=makeMeasurement(makeFraction(makeNumber("123"),makeNumber("7")),makeRoot(makeNumber("5")));
 auto*node=static_cast<NodeMeasurement*>(measured.get());const auto id=node->sourceId();auto cloned=cloneNode(node);
 check(static_cast<NodeMeasurement*>(cloned.get())->sourceId()==id,"AST clone preserves measurement source");
 for(const auto&fm:{normal,normal.superscript(),normal.superscript().superscript()}){
  node->calculateLayout(fm);const auto before=node->layout();
  check(node->mathClass()==MathClass::INNER,"measurement is grouped inner atom");
  check(node->nominalX()>=node->delimiterWidth()&&node->delimiterWidth()>0,"opening delimiter before nominal");
  check(node->signX()>=node->nominalX()+node->nominal()->layout().width,"plusminus follows nominal");
  check(node->uncertaintyX()>node->signX(),"uncertainty follows plusminus");
  check(node->uncertaintyX()+node->uncertainty()->layout().width+node->delimiterWidth()<=before.width,"closing delimiter and uncertainty inside bounds");
  check(before.ascent>=node->nominal()->layout().ascent&&before.ascent>=node->uncertainty()->layout().ascent,"nested ascents enclosed");
  check(before.descent>=node->nominal()->layout().descent&&before.descent>=node->uncertainty()->layout().descent,"nested descents enclosed");
  deny=true;try{for(unsigned i=0;i<1000;++i)node->calculateLayout(fm);}catch(...){deny=false;check(false,"measurement hot layout attempted dynamic allocation");}deny=false;
  check(attempts==0,"zero C++ allocations in repeated layout");check(node->layout().width==before.width&&node->layout().ascent==before.ascent&&node->layout().descent==before.descent,"repeat layout has stable box");check(node->sourceId()==id,"layout never changes mathematical source");
 }
 std::printf("PASS scientific layout %u assertions, 3000 repeated layouts, zero C++ new attempts; excludes LVGL draw queue/C malloc\n",checks);
}
