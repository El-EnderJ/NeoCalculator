#pragma once
#include "MathAST.h"
#include "MathInputCalls.h"
#include <cstddef>
#include <cstdint>

namespace numos::toolbox {
enum Capability : uint8_t { Calculation=1, Equations=2, Calculus=4, Grapher=8, Cas=7, All=15 };
enum class Recipe : uint8_t { Fraction, Power, Root, NthRoot, Paren, Abs, LogBase, Function, Constant, Variable, Call, Integral, PowerTen, Symbol, Infinity, Unit, QuantityReference, Measurement };
struct Identity {
    uint16_t id=0;
    uint16_t variant=0;
    constexpr bool operator==(Identity b) const { return id==b.id && variant==b.variant; }
    constexpr bool operator!=(Identity b) const { return !(*this==b); }
};
struct Entry {
    Identity identity;
    uint16_t category;
    Recipe recipe;
    uint32_t argument;
    uint8_t capabilities;
    const char* en;
    const char* es;
    const char* helpEn;
    const char* helpEs;
    const char* aliases;
    uint16_t options;
    // Search-only text policy. Typed identities/variants never undergo folding.
    // A future provider can distinguish m/M while retaining bilingual names.
    bool caseSensitiveAliases=false;
    bool keyboardShortcut=false;
    // Display-only parameter letters in AST child order; never inserted.
    const char* previewArgs=nullptr;
};
struct Category { uint16_t id; const char* en; const char* es; };
inline constexpr Category kCategories[] = {
    {13,"Calculus","Cálculo"}, {14,"Complex numbers","Complejos"},
    {15,"Arithmetic and combinatorics","Aritmética y combinatoria"},
    {11,"Trigonometry","Trigonometría"}, {17,"Rounding","Redondeo"},
    {18,"Variables","Variables"}, {22,"Measurements","Medidas"}
};
inline constexpr Category kAlphabets[] = {
    {19,"Latin Alphabet","Alfabeto latino"},
    {20,"Greek Alphabet","Alfabeto griego"},
    {21,"Special Characters","Caracteres especiales"}
};
const Entry* find(Identity identity);
const Entry* entries();
size_t entryCount();
const Entry* entryAt(size_t index);
// Transient catalogue position for session indexes; never persisted as identity.
size_t entryIndex(const Entry&);
bool available(const Entry& entry, uint8_t capabilities);
// Stable ranking, with accent folding only in search text, never in identities.
unsigned searchRank(const Entry&, const char* query);
// Regional presentation never changes the stored identity or canonical symbol.
const char* displayName(const Entry&);
struct Prepared { vpam::NodePtr node; vpam::NodeRow* slot=nullptr; int index=0; };
Prepared prepare(const Entry&);  // no evaluation; may fail through existing AST allocator
vpam::NodePtr preview(const Entry&); // same structure, illustrative letters only
bool discoverable(const Entry&);

// Providers enumerate rows lazily; action and children are independent.
struct Row { const Entry* entry=nullptr; uint16_t children=0; const char* en=nullptr; const char* es=nullptr; uint8_t capabilities=All; };
struct Provider {
    const void* context;
    size_t (*count)(const void*, uint16_t);
    Row (*at)(const void*, uint16_t, size_t);
    size_t (*initial)(const void*, uint16_t);
    size_t (*entryCount)(const void*);
    const Entry* (*entryAt)(const void*,size_t);
    Prepared (*build)(const Entry&);
    size_t (*initialFrom)(const void*,uint16_t,Identity)=nullptr;
};
// Static-lifetime providers are registered during startup, before loading
// favorites/opening a session. No dynamic plugins or owning catalogue trees.
// A future quantity provider's builder delegates its typed identity to the
// quantity engine; this API imposes no representation of numeric quantities.
bool registerProvider(const Provider& provider);
const Provider& catalogProvider();
}
