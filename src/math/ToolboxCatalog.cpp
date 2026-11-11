#include "ToolboxCatalog.h"
#include "InputSymbols.h"
#include "i18n/Locale.h"
#include "units/ReferenceRegistry.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <cstdio>

namespace numos::toolbox {
static Prepared buildMath(const Entry&);
namespace {
constexpr Entry kEntries[] = {
#include "ToolboxEntries.inc"
};
vpam::NodePtr slot() {
    auto row=vpam::makeRow();
    static_cast<vpam::NodeRow*>(row.get())->appendChild(vpam::makeEmpty());
    return row;
}
void fold(const char* source, char* out, size_t capacity) {
    size_t n=0;
    for (auto* p=reinterpret_cast<const unsigned char*>(source); *p && n+1<capacity;) {
        unsigned c=*p++;
        if (c==0xc3 && *p) {
            const unsigned next=*p++;
            switch(next) {
                case 0x81: case 0xa1: c='a'; break;
                case 0x89: case 0xa9: c='e'; break;
                case 0x8d: case 0xad: c='i'; break;
                case 0x93: case 0xb3: c='o'; break;
                case 0x9a: case 0xba: case 0x9c: case 0xbc: c='u'; break;
                case 0x91: case 0xb1: c='n'; break;
                default: if(n+2<capacity) {out[n++]=char(c); c=next;} else continue;
            }
        }
        if(c>='A' && c<='Z') c+='a'-'A';
        out[n++]=char(c);
    }
    out[n]=0;
}
unsigned rankText(const char* text, const char* query,bool sensitive=false) {
    char normalized[192];
    if(sensitive){std::strncpy(normalized,text,sizeof(normalized)-1);normalized[sizeof(normalized)-1]=0;}
    else fold(text,normalized,sizeof(normalized));
    if(!std::strcmp(normalized,query)) return 0;
    const size_t length=std::strlen(query);
    if(!std::strncmp(normalized,query,length)) return 1;
    const char* match=std::strstr(normalized,query);
    if(!match) return 99;
    return match==normalized || match[-1]==' ' || match[-1]=='|' ? 2 : 3;
}
bool inGroup(const Entry& e,uint16_t group) {
    if(group==100)return e.identity.id==120;
    if(group==101)return e.identity.id==410 && e.identity.variant!=0;
    if(group>=1000)return e.options==group;
    return !e.keyboardShortcut && e.category==group && !e.identity.variant;
}
size_t count(const void*, uint16_t group) {
    // Disjoint built-in namespaces: no math rows in unit categories/prefixes.
    if((group>=200 && group<1000) || (group&0xc000))return 0;
    if(group==18)return std::size(kAlphabets);
    size_t n=group?0:std::size(kCategories);
    for(const auto& e:kEntries)
        if(inGroup(e,group)) ++n;
    return n;
}
Row at(const void*, uint16_t group, size_t index) {
    if(group==18) {
        if(index>=std::size(kAlphabets))return {};
        const auto& c=kAlphabets[index];return {nullptr,c.id,c.en,c.es};
    }
    for(const auto& e:kEntries) {
        if(inGroup(e,group)) {
            if(index--==0) return {&e,group==100 || group==101 || group>=1000 ? uint16_t(0):e.options,e.en,e.es};
        }
    }
    if(!group && index<std::size(kCategories)) {
        const auto& c=kCategories[index];return {nullptr,c.id,c.en,c.es};
    }
    return {};
}
size_t initial(const void*, uint16_t group) { return group==100 || group>=1000 ? 1 : 0; }
size_t mathCount(const void*) {return std::size(kEntries);}
const Entry* mathAt(const void*,size_t i) {return i<std::size(kEntries)?&kEntries[i]:nullptr;}
const Provider mathProvider{nullptr,count,at,initial,mathCount,mathAt,buildMath};
#include "units/UnitToolboxProvider.inc"
const Provider* providers[4]={&mathProvider,&unitProvider};
unsigned providerCount=2;
size_t combinedCount(const void*,uint16_t group) {
    size_t total=0;for(unsigned i=0;i<providerCount;++i)total+=providers[i]->count(providers[i]->context,group);return total;
}
Row combinedAt(const void*,uint16_t group,size_t index) {
    for(unsigned i=0;i<providerCount;++i){const auto& p=*providers[i];auto n=p.count(p.context,group);if(index<n)return p.at(p.context,group,index);index-=n;}return {};
}
size_t combinedInitial(const void*,uint16_t group) {
    if(!group)return 0;
    for(unsigned i=0;i<providerCount;++i){const auto& p=*providers[i];if(p.count(p.context,group))return p.initial(p.context,group);}return 0;
}
size_t combinedInitialFrom(const void*,uint16_t group,Identity origin) {
    for(unsigned i=0;i<providerCount;++i){const auto& p=*providers[i];if(p.count(p.context,group))return p.initialFrom?p.initialFrom(p.context,group,origin):p.initial(p.context,group);}return 0;
}
size_t flatCount(const void*) {size_t n=0;for(unsigned i=0;i<providerCount;++i)n+=providers[i]->entryCount(providers[i]->context);return n;}
const Entry* flatAt(const void*,size_t index) {
    for(unsigned i=0;i<providerCount;++i){const auto& p=*providers[i];auto n=p.entryCount(p.context);if(index<n)return p.entryAt(p.context,index);index-=n;}return nullptr;
}

}
const Entry* entries() { return kEntries; }
size_t entryCount() { return flatCount(nullptr); }
const Entry* entryAt(size_t index) {return flatAt(nullptr,index);}
size_t entryIndex(const Entry& entry) {
    if(entry.recipe==Recipe::Unit && unitEntry(uint16_t(entry.argument),entry.identity.variant)==&entry)
        return std::size(kEntries)+size_t(&entry-unitEntries);
    if(entry.recipe==Recipe::QuantityReference && referenceEntry(uint16_t(entry.argument),entry.identity.variant)==&entry)
        return std::size(kEntries)+std::size(unitEntries)+size_t(&entry-referenceEntries);
    for(size_t i=0;i<std::size(kEntries);++i)if(&kEntries[i]==&entry)return i;
    for(size_t i=std::size(kEntries)+unitEntryCount(nullptr);i<entryCount();++i)if(entryAt(i)==&entry)return i;
    return entryCount();
}
const Entry* find(Identity id) {
    if((id.id&0xc000)==0xc000)return nullptr;
    if(id.id&0x8000)return unitEntry(id.id&0x3fff,id.variant);
    if(id.id&0x4000)return referenceEntry(id.id&0x3fff,id.variant);
    for(size_t i=0;i<entryCount();++i){const auto* e=entryAt(i);if(e->identity==id)return e;}return nullptr;
}
const char* displayName(const Entry& e) {
    const auto* meta=variantMetadata(e.identity);const auto locale=i18n::productLocale;
    if(i18n::isSpanish(locale))return meta && locale==i18n::Locale::SpanishLatinAmerica && *meta->es419?meta->es419:e.es;
    return meta && !i18n::isEnglishUK(locale) && *meta->enUS?meta->enUS:e.en;
}
bool available(const Entry& e,uint8_t caps) { return (e.capabilities&caps)!=0; }
bool discoverable(const Entry& e) { return !e.keyboardShortcut; }
unsigned searchRank(const Entry& e,const char* query) {
    if(e.recipe==Recipe::Unit || e.recipe==Recipe::QuantityReference)return unitSearchRank(e,query);
    if(unitSymbolQuery(query)) {
        // WHY: a complete symbol is case-sensitive across providers too. Keep
        // exact letter aliases (m versus metre) without dozens of folded name
        // substring matches such as MA -> "maximo comun divisor".
        if(!std::strcmp(e.en,query) || !std::strcmp(e.es,query))return 0;
        const size_t length=std::strlen(query);
        for(const char* start=e.aliases,*p=e.aliases;;++p)if(*p=='|' || !*p) {
            if(size_t(p-start)==length && !std::strncmp(start,query,length))return 0;
            if(!*p)break;start=p+1;
        }
        return 99;
    }
    char q[65]; fold(query,q,sizeof(q));
    if(!q[0]) return 3;
    unsigned rank=std::min(rankText(e.en,q),rankText(e.es,q));
    char aliases[192];
    if(e.caseSensitiveAliases){std::strncpy(aliases,e.aliases,sizeof(aliases)-1);aliases[sizeof(aliases)-1]=0;}
    else fold(e.aliases,aliases,sizeof(aliases));
    char* begin=aliases;
    for(char* p=aliases;;++p) {
        if(*p!='|' && *p) continue;
        const bool end=*p==0;*p=0;rank=std::min(rank,rankText(begin,e.caseSensitiveAliases?query:q,e.caseSensitiveAliases));
        if(end)break;
        begin=p+1;
    }
    return rank;
}
const Provider& catalogProvider() { static const Provider p{nullptr,combinedCount,combinedAt,combinedInitial,flatCount,flatAt,prepare,combinedInitialFrom}; return p; }
bool registerProvider(const Provider& provider) {
    if(providerCount==4 || !provider.count || !provider.at || !provider.initial || !provider.entryCount || !provider.entryAt || !provider.build)return false;
    const size_t n=provider.entryCount(provider.context);
    if(n>4096 || !n)return false;
    const size_t groups=provider.count(provider.context,0);
    if(groups>32)return false;
    for(size_t i=0;i<groups;++i) {
        const auto group=provider.at(provider.context,0,i).children;
        if(group<10 || combinedCount(nullptr,group))return false;
        for(size_t j=0;j<i;++j)if(provider.at(provider.context,0,j).children==group)return false;
    }
    for(size_t i=0;i<n;++i){const auto* e=provider.entryAt(provider.context,i);if(!e||!e->identity.id||find(e->identity))return false;
        for(size_t j=0;j<i;++j)if(provider.entryAt(provider.context,j)->identity==e->identity)return false;}
    providers[providerCount++]=&provider;return true;
}
Prepared prepare(const Entry& entry) {
    for(unsigned i=0;i<providerCount;++i){const auto& p=*providers[i];for(size_t j=0;j<p.entryCount(p.context);++j)
        if(p.entryAt(p.context,j)==&entry)return p.build(entry);}
    return {};
}
vpam::NodePtr preview(const Entry& entry) {
    auto prepared=prepare(entry);
    if(!prepared.node || !entry.previewArgs)return std::move(prepared.node);
    // WHY: the menu illustrates a function; an editor insertion still receives
    // the original empty slots. This walk never touches the receiver's AST.
    std::array<vpam::MathNode*,82> pending{};
    size_t size=1,letter=0;pending[0]=prepared.node.get();
    while(size) {
        auto* node=pending[--size];
        if(node->type()==vpam::NodeType::Row) {
            auto* row=static_cast<vpam::NodeRow*>(node);
            for(int i=0;i<row->childCount();++i) {
                if(row->child(i)->type()==vpam::NodeType::Empty && entry.previewArgs[letter])
                    row->replaceChild(i,vpam::makeVariable(entry.previewArgs[letter++]));
            }
        }
        for(int i=node->childCount()-1;i>=0;--i) {
            if(size==pending.size())return {};
            pending[size++]=node->child(i);
        }
    }
    return std::move(prepared.node);
}
static Prepared buildMath(const Entry& entry) {
    using namespace vpam;
    Prepared p;
    switch(entry.recipe) {
        case Recipe::Measurement: p.node=makeMeasurement(); p.slot=static_cast<NodeRow*>(p.node->child(0)); break;
        case Recipe::Unit: case Recipe::QuantityReference: return {};
        case Recipe::Fraction: p.node=makeFraction(); p.slot=static_cast<NodeRow*>(p.node->child(0)); break;
        case Recipe::Power: p.node=makePower(); p.slot=static_cast<NodeRow*>(p.node->child(0)); break;
        case Recipe::PowerTen: {
            auto base=makeRow(); static_cast<NodeRow*>(base.get())->appendChild(makeNumber("10"));
            p.node=makePower(std::move(base),nullptr); p.slot=static_cast<NodeRow*>(p.node->child(1)); break;
        }
        case Recipe::Root: p.node=makeRoot(); p.slot=static_cast<NodeRow*>(static_cast<NodeRoot*>(p.node.get())->radicand()); break;
        case Recipe::NthRoot: p.node=makeRoot(nullptr,slot()); p.slot=static_cast<NodeRow*>(static_cast<NodeRoot*>(p.node.get())->degree()); break;
        case Recipe::Paren: p.node=makeParen(nullptr,static_cast<DelimKind>(entry.argument)); p.slot=static_cast<NodeRow*>(p.node->child(0)); break;
        case Recipe::Abs: p.node=makeBar(); p.slot=static_cast<NodeRow*>(p.node->child(0)); break;
        case Recipe::LogBase: p.node=makeLogBase(); p.slot=static_cast<NodeRow*>(p.node->child(0)); break;
        case Recipe::Function: p.node=makeFunction(static_cast<FuncKind>(entry.argument)); p.slot=static_cast<NodeRow*>(p.node->child(0)); break;
        case Recipe::Constant: p.node=makeConstant(static_cast<ConstKind>(entry.argument)); break;
        case Recipe::Variable: p.node=makeVariable(char(entry.argument)); break;
        case Recipe::Symbol: {
            if(!numos::inputsymbol::greek(entry.argument))return {};
            char text[5]{};numos::inputsymbol::utf8(entry.argument,text);
            p.node=makeSymbol(text);break;
        }
        case Recipe::Infinity:
            switch(static_cast<SpecialValueKind>(entry.argument)) {
                case SpecialValueKind::BarePositiveInfinity:
                case SpecialValueKind::PositiveInfinity:
                case SpecialValueKind::NegativeInfinity:
                case SpecialValueKind::BothInfinities:
                    p.node=makeSpecialValue(static_cast<SpecialValueKind>(entry.argument));break;
                default: return {};
            }
            break;
        case Recipe::Call: {
            const auto& spec=kInputCalls[entry.argument];
            p.node=makeCall(spec.name);
            for(unsigned i=0;i<spec.arity;++i) static_cast<NodeCall*>(p.node.get())->appendArgument(slot());
            p.slot=static_cast<NodeRow*>(p.node->child(0)); break;
        }
        case Recipe::Integral: p.node=makeDefIntegral(); p.slot=static_cast<NodeRow*>(p.node->child(0)); break;
    }
    return p;
}
}
