// SPDX-License-Identifier: GPL-3.0-or-later
#include "math_notation_fixtures.h"
#include "fonts/StixMathFont.h"
#include "ui/MathTypography.h"
#include <iostream>
#include <sstream>
#include <cstdlib>
using namespace notationtest;
using namespace vpam;
using namespace numos;
static void check(bool ok,const char* message) { if(!ok){std::cerr<<"FAIL "<<message<<'\n';std::exit(1);} }
static unsigned multiplications(const MathNode* n) {
    unsigned count=n->type()==NodeType::Operator && static_cast<const NodeOperator*>(n)->op()==OpKind::Mul;
    for(int i=0;i<n->childCount();++i)count+=multiplications(n->child(i));return count;
}
static std::string signature(const EngineResultNode& n) {
    std::string s=std::to_string(unsigned(n.kind))+":"+std::to_string(n.text.size())+":"+n.text+"[";
    for(const auto& c:n.children)s+=signature(c);return s+"]";
}
static void rowGeometry(const MathNode* n, const FontMetrics& fm) {
    if(n->type()==NodeType::Row) {
        int width=0;
        for(int i=0;i<n->childCount();++i) {
            if(i)width+=interAtomSpacingPx(n->child(i-1)->rightMathClass(),n->child(i)->leftMathClass(),fm.style,fm.emSize);
            width+=n->child(i)->layout().width;
        }
        check(width==n->layout().width,"row width differs from chosen atom spacing");
    }
}
int main() {
    lv_init();
    unsigned count=0;
    for(const auto& c:cases()) {
        const auto before=signature(c.tree);
        auto explicitAst=CalculationEngine::resultTreeToAST(c.tree);
        auto naturalAst=CalculationEngine::resultTreeToAST(c.tree,ProductNotation::ScalarNatural);
        check(explicitAst && naturalAst,c.id);check(signature(c.tree)==before,"mutated authoritative structured product");
        check(multiplications(naturalAst.get())==c.visible,c.id);
        auto fm=defaultFontMetrics();explicitAst->calculateLayout(fm);naturalAst->calculateLayout(fm);rowGeometry(naturalAst.get(),fm);
        check(naturalAst->layout().width<=explicitAst->layout().width,"natural width grew unexpectedly");
        auto copy=cloneNode(naturalAst.get());copy->calculateLayout(fm);
        check(dumpTree(copy.get())==dumpTree(naturalAst.get()),"clone lost representation/spacing");
        std::string e,n,ee,ne;
        bool es=CalculationEngine::serializeForGiac(explicitAst.get(),e,ee),ns=CalculationEngine::serializeForGiac(naturalAst.get(),n,ne);
        check(es==ns,"notation changed serializer availability");
        if(es && std::string(c.id)!="negative_coefficient")check(e==n,"canonical multiplication changed");
        if(std::string(c.id)=="negative_coefficient")check(n=="(-1)*3*x","signed coefficient lost sign or precedence");
        if(std::string(c.id)=="sin")check(naturalAst->child(1)->leftMathClass()==MathClass::OP,"missing function operator spacing");
        if(std::string(c.id)=="user_D")check(n.find('D')!=std::string::npos && n.find("\xCE\x94")==std::string::npos,"user D was aliased");
        auto again=cloneNode(naturalAst.get());applyGeneratedProductNotation(again.get(),ProductNotation::ScalarNatural);again->calculateLayout(fm);
        check(dumpTree(again.get())==dumpTree(naturalAst.get()),"notation is not idempotent");
        std::cout<<"CASE|"<<c.id<<"|explicit="<<multiplications(explicitAst.get())<<"|natural="<<c.visible<<"|width="<<explicitAst->layout().width<<"/"<<naturalAst->layout().width<<"|source="<<(ns?n:"unsupported (unchanged)")<<"|engine="<<before<<'\n';++count;
    }
    auto instruction=CalculationEngine::resultTreeToAST(mul(integer("2"),integer("3")),ProductNotation::Explicit);
    check(multiplications(instruction.get())==1,"instructional multiplication hidden");
    const lv_font_t* fonts[]={&stix_math_18,&stix_math_12,&stix_math_8};
    for(auto* font:fonts) {
        lv_font_glyph_dsc_t stix,tex;
        check(lv_font_get_glyph_dsc(font,&stix,0x0394,0) && !stix.is_placeholder,"missing compiled STIX Delta");
        const auto* selected=ui::mathGlyphFont(font,0x0394);
        check(lv_font_get_glyph_dsc(selected,&tex,0x0394,0) && !tex.is_placeholder,"missing compiled TeX Delta");
        check(ui::mathGlyphFont(font,'D')==font && ui::mathGlyphFont(font,0x2206)==font,"wrong glyph scope");
        check(tex.box_w>0 && tex.box_h>0 && tex.adv_w>=tex.box_w,"Delta ink outside width");
        std::cout<<"GLYPH|U+0394|em="<<ui::nominalMathEmSizeForFont(font)<<"|STIX="<<stix.box_w<<"x"<<stix.box_h<<"|TeX="<<tex.box_w<<"x"<<tex.box_h<<"|advance="<<tex.adv_w<<"|offset="<<tex.ofs_x<<","<<tex.ofs_y<<'\n';
    }
    std::cout<<"PASS|cases="<<count<<"|NodeFunction="<<sizeof(NodeFunction)<<"|FontMetrics="<<sizeof(FontMetrics)<<'\n';
}
