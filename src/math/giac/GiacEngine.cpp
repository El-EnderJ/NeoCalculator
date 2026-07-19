/*
 * NeoCalculator - NumOS
 * Copyright (C) 2026 Juan Ramon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

// GiacEngine.cpp — GIAC-A01 production seam implementation.
// Owns THE giac::context (see GiacEngine.h contract). Compiled for firmware,
// emulator_pc and the host harness; everything Giac-typed stays in this TU.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <exception>
#include <new>
#include <sstream>
#include <iostream>

#include "config.h"
#include "gen.h"
#include "global.h"
#include "identificateur.h"
#include "prog.h"    // cas_setup
#include "subst.h"   // subst, _simplify
#include "unary.h"   // unary_function_ptr/_eval full types (result tree walk)
#include "solve.h"   // has_num_coeff
#include "sym2poly.h"
#include "usual.h"

#include "math/giac/GiacEngine.h"
#include "math/giac/GiacEngineInternal.h"
#include "math/AngleModeRuntime.h"

namespace giac {
// Not exposed by this snapshot's headers; defined in the lexer TU (same
// forward declarations GiacBridge.cpp has always used).
void check_browser_functions();
void lexer_localization(int lang, const context* contextptr);
}

namespace numos {

// ---------------------------------------------------------------------------
// Engine state
// ---------------------------------------------------------------------------

struct GiacEngine::State {
    giac::context* ctx = nullptr;
    std::string lastInitError;
};

namespace {

// Mirrors the historical GiacBridge initGiac() configuration so the shared
// context behaves identically for the UART path during the transition.
void configureContext(giac::context* ctx) {
    giac::xcas_mode(0, ctx);
    giac::approx_mode(false, ctx);
    giac::complex_mode(false, ctx);
    giac::complex_variables(false, ctx);
    giac::i_sqrt_minus1(1, ctx);
    giac::withsqrt(true, ctx);
    giac::eval_level(ctx) = 1;
    giac::step_infolevel(ctx) = 0;
    giac::language(0, ctx);
    giac::check_browser_functions();
    giac::lexer_localization(0, ctx);
    giac::cas_setup(giac::makevecteur(0, 0, 0, 1, 0), ctx);
}

// RAII reentrancy gate: engine calls made while another call is on the
// stack observe entered()==false and are rejected.
struct CallGuard {
    bool& flag;
    bool ok;
    explicit CallGuard(bool& f) : flag(f), ok(!f) { if (ok) flag = true; }
    ~CallGuard() { if (ok) flag = false; }
    bool entered() const { return ok; }
};

// Captures cout/cerr/logptr during parse+eval so parser and engine
// messages become MathEngineResult::diagnostic instead of leaking to the
// console (the legacy bridge does the same for [STEP_OUTPUT]).
struct DiagnosticCapture {
    giac::context* ctx;
    std::ostringstream buf;
    std::streambuf* oldCout;
    std::streambuf* oldCerr;
    std::ostream* oldLog;
    explicit DiagnosticCapture(giac::context* c)
        : ctx(c),
          oldCout(std::cout.rdbuf(buf.rdbuf())),
          oldCerr(std::cerr.rdbuf(buf.rdbuf())),
          oldLog(giac::logptr(c)) {
        giac::logptr(&buf, c);
    }
    ~DiagnosticCapture() {
        giac::logptr(oldLog, ctx);
        std::cout.rdbuf(oldCout);
        std::cerr.rdbuf(oldCerr);
    }
    std::string text() const { return buf.str(); }
};

MathEngineResult rejectedReentrant() {
    MathEngineResult r;
    r.status = MathEngineStatus::Unsupported;
    r.diagnostic = "GiacEngine is non-reentrant: nested call rejected";
    return r;
}

// ---------------------------------------------------------------------------
// GIAC-B01: gen -> EngineResultNode structural walk. Bounded (depth + node
// budget) so a pathological result degrades to Unsupported, never recurses
// away. Every giac type stays inside this TU.
// ---------------------------------------------------------------------------

constexpr int kTreeMaxDepth = 40;
constexpr int kTreeNodeBudget = 400;

void genToNode(const giac::gen& g, giac::context* ctx,
               EngineResultNode& out, int depth, int& budget);

void childrenFromFeuille(const giac::gen& f, giac::context* ctx,
                         EngineResultNode& out, int depth, int& budget) {
    if (f.type == giac::_VECT) {
        out.children.resize(f._VECTptr->size());
        for (size_t i = 0; i < f._VECTptr->size(); ++i)
            genToNode((*f._VECTptr)[i], ctx, out.children[i], depth, budget);
    } else {
        out.children.resize(1);
        genToNode(f, ctx, out.children[0], depth, budget);
    }
}

void genToNode(const giac::gen& g, giac::context* ctx,
               EngineResultNode& out, int depth, int& budget) {
    if (depth > kTreeMaxDepth || --budget < 0) {
        out.kind = EngineNodeKind::Unsupported;
        return;
    }
    if (giac::is_undef(g)) {  // shouldn't reach here (non-Ok upstream)
        out.kind = EngineNodeKind::Unsupported;
        return;
    }
    if (g == giac::unsigned_inf) { out.kind = EngineNodeKind::UnsignedInfinity; return; }
    if (g == giac::plus_inf)     { out.kind = EngineNodeKind::PlusInfinity; return; }
    if (g == giac::minus_inf)    { out.kind = EngineNodeKind::MinusInfinity; return; }

    switch (g.type) {
        case giac::_INT_:
        case giac::_ZINT:
            out.kind = EngineNodeKind::Integer;
            out.text = g.print(ctx);
            return;
        case giac::_DOUBLE_:
            out.kind = EngineNodeKind::Decimal;
            out.text = g.print(ctx);
            return;
        case giac::_FRAC:
            out.kind = EngineNodeKind::Rational;
            out.children.resize(2);
            genToNode(g._FRACptr->num, ctx, out.children[0], depth + 1, budget);
            genToNode(g._FRACptr->den, ctx, out.children[1], depth + 1, budget);
            return;
        case giac::_CPLX:
            out.kind = EngineNodeKind::Complex;
            out.children.resize(2);
            genToNode(*g._CPLXptr, ctx, out.children[0], depth + 1, budget);
            genToNode(*(g._CPLXptr + 1), ctx, out.children[1], depth + 1, budget);
            return;
        case giac::_IDNT: {
            if (g == giac::cst_pi) { out.kind = EngineNodeKind::Pi; return; }
            if (g == giac::cst_i)  { out.kind = EngineNodeKind::ImagUnit; return; }
            out.kind = EngineNodeKind::Symbol;
            out.text = g.print(ctx);
            return;
        }
        case giac::_VECT: {
            out.kind = EngineNodeKind::List;
            out.children.resize(g._VECTptr->size());
            for (size_t i = 0; i < g._VECTptr->size(); ++i)
                genToNode((*g._VECTptr)[i], ctx, out.children[i], depth + 1, budget);
            return;
        }
        case giac::_SYMB: {
            const giac::unary_function_ptr& s = g._SYMBptr->sommet;
            const giac::gen& f = g._SYMBptr->feuille;
            if (s == giac::at_plus) {
                out.kind = EngineNodeKind::Add;
                childrenFromFeuille(f, ctx, out, depth + 1, budget);
                return;
            }
            if (s == giac::at_neg) {
                out.kind = EngineNodeKind::Neg;
                out.children.resize(1);
                genToNode(f, ctx, out.children[0], depth + 1, budget);
                return;
            }
            if (s == giac::at_prod) {
                out.kind = EngineNodeKind::Mul;
                childrenFromFeuille(f, ctx, out, depth + 1, budget);
                return;
            }
            if (s == giac::at_inv) {
                out.kind = EngineNodeKind::Inv;
                out.children.resize(1);
                genToNode(f, ctx, out.children[0], depth + 1, budget);
                return;
            }
            if (s == giac::at_sqrt) {
                out.kind = EngineNodeKind::Sqrt;
                out.children.resize(1);
                genToNode(f, ctx, out.children[0], depth + 1, budget);
                return;
            }
            if (s == giac::at_exp && giac::is_one(f)) {
                out.kind = EngineNodeKind::EulerE;
                return;
            }
            if (s == giac::at_pow && f.type == giac::_VECT &&
                f._VECTptr->size() == 2) {
                const giac::gen& base = f._VECTptr->front();
                const giac::gen& expo = f._VECTptr->back();
                // x^(1/2) -> sqrt, x^(1/n) -> nth root (n small positive int)
                if (expo.type == giac::_FRAC &&
                    giac::is_one(expo._FRACptr->num) &&
                    expo._FRACptr->den.type == giac::_INT_ &&
                    expo._FRACptr->den.val >= 2 &&
                    expo._FRACptr->den.val <= 64) {
                    if (expo._FRACptr->den.val == 2) {
                        out.kind = EngineNodeKind::Sqrt;
                        out.children.resize(1);
                        genToNode(base, ctx, out.children[0], depth + 1, budget);
                    } else {
                        out.kind = EngineNodeKind::Root;
                        out.children.resize(2);
                        genToNode(base, ctx, out.children[0], depth + 1, budget);
                        out.children[1].kind = EngineNodeKind::Integer;
                        out.children[1].text = std::to_string(expo._FRACptr->den.val);
                    }
                    return;
                }
                out.kind = EngineNodeKind::Pow;
                out.children.resize(2);
                genToNode(base, ctx, out.children[0], depth + 1, budget);
                genToNode(expo, ctx, out.children[1], depth + 1, budget);
                return;
            }
            if (s == giac::at_equal && f.type == giac::_VECT &&
                f._VECTptr->size() == 2) {
                out.kind = EngineNodeKind::Equation;
                out.children.resize(2);
                genToNode(f._VECTptr->front(), ctx, out.children[0], depth + 1, budget);
                genToNode(f._VECTptr->back(), ctx, out.children[1], depth + 1, budget);
                return;
            }
            // Generic named function call (sin, cos, ln, log10, abs, ...).
            const char* name = s.ptr() ? s.ptr()->s : nullptr;
            if (name && *name) {
                out.kind = EngineNodeKind::Function;
                out.text = name;
                childrenFromFeuille(f, ctx, out, depth + 1, budget);
                return;
            }
            out.kind = EngineNodeKind::Unsupported;
            out.text = g.print(ctx);
            return;
        }
        default:
            out.kind = EngineNodeKind::Unsupported;
            out.text = g.print(ctx);
            return;
    }
}

// ---------------------------------------------------------------------------
// GIAC-C01: strict free-variable validation for Grapher-shaped handles.
// Bounded recursive walk over a parsed gen looking for the FIRST identifier
// that is neither an allowed sampling variable nor a mathematical constant
// (pi and the infinity/undef sentinels are _IDNT-typed in Giac but are not
// variables). Returns true and fills `name` when such an identifier exists.
// ---------------------------------------------------------------------------

bool findDisallowedIdent(const giac::gen& g, const giac::gen* allowed,
                         int nAllowed, int depth, int& budget,
                         std::string& name, giac::context* ctx) {
    if (depth > 64 || --budget < 0) return false;
    switch (g.type) {
        case giac::_IDNT: {
            if (g == giac::cst_pi || g == giac::cst_i) return false;
            if (g == giac::unsigned_inf || g == giac::plus_inf ||
                g == giac::minus_inf || giac::is_undef(g)) return false;
            for (int i = 0; i < nAllowed; ++i)
                if (g == allowed[i]) return false;
            name = g.print(ctx);
            return true;
        }
        case giac::_SYMB:
            return findDisallowedIdent(g._SYMBptr->feuille, allowed, nAllowed,
                                       depth + 1, budget, name, ctx);
        case giac::_VECT: {
            for (const giac::gen& child : *g._VECTptr)
                if (findDisallowedIdent(child, allowed, nAllowed, depth + 1,
                                        budget, name, ctx))
                    return true;
            return false;
        }
        case giac::_FRAC:
            return findDisallowedIdent(g._FRACptr->num, allowed, nAllowed,
                                       depth + 1, budget, name, ctx) ||
                   findDisallowedIdent(g._FRACptr->den, allowed, nAllowed,
                                       depth + 1, budget, name, ctx);
        default:
            return false;
    }
}

// File-scope mirrors of the singleton's context/generation. The context
// pointer feeds the giacinternal transition accessor; the generation lets
// CompiledExpression::valid() detect handles orphaned by reset() without
// widening the class API. Both are written only by begin()/reset().
giac::context* s_sharedCtx = nullptr;
uint32_t s_generationForHandles = 0;

} // namespace

// ---------------------------------------------------------------------------
// CompiledExpression
// ---------------------------------------------------------------------------

struct CompiledExpression::Impl {
    giac::gen expr;
    giac::gen var;
    giac::gen var2;         // second sampling variable (2D handles only)
    uint32_t generation = 0;
    bool parsedOk = false;
    bool twoVars = false;   // compiled via compileNumeric2D
    std::string diag;
};

CompiledExpression::CompiledExpression() : _impl(nullptr) {}
CompiledExpression::~CompiledExpression() { delete _impl; }

CompiledExpression::CompiledExpression(CompiledExpression&& other) noexcept
    : _impl(other._impl) {
    other._impl = nullptr;
}

CompiledExpression& CompiledExpression::operator=(
    CompiledExpression&& other) noexcept {
    if (this != &other) {
        delete _impl;
        _impl = other._impl;
        other._impl = nullptr;
    }
    return *this;
}

// Generation is checked against the engine's current one so a handle
// compiled before a reset() can never evaluate against the rebuilt context.
bool CompiledExpression::valid() const {
    return _impl && _impl->parsedOk &&
           _impl->generation == s_generationForHandles;
}

const std::string& CompiledExpression::diagnostic() const {
    static const std::string kNoImpl = "empty compiled-expression handle";
    return _impl ? _impl->diag : kNoImpl;
}

// ---------------------------------------------------------------------------
// GiacEngine
// ---------------------------------------------------------------------------

GiacEngine& GiacEngine::instance() {
    static GiacEngine engine;
    return engine;
}

bool GiacEngine::begin() {
    if (_state && _state->ctx) return true;
    try {
        if (!_state) _state = new State();
        _state->ctx = new giac::context;
        configureContext(_state->ctx);
        s_sharedCtx = _state->ctx;
        s_generationForHandles = _generation;
        return true;
    } catch (const std::exception& e) {
        if (_state) _state->lastInitError = e.what();
        return false;
    } catch (...) {
        return false;
    }
}

void GiacEngine::reset() {
    if (_inCall) return;  // contract: never yank the context mid-call
    if (_state) {
        delete _state->ctx;
        _state->ctx = nullptr;
        s_sharedCtx = nullptr;
    }
    ++_generation;
    s_generationForHandles = _generation;
    begin();
}

// Reads vpam::g_angleMode (AngleModeRuntime single source of truth) into the
// shared context. Giac contexts default to radians, matching the vpam boot
// default; this keeps them aligned when the user flips DEG.
static void syncAngleMode(giac::context* ctx) {
    giac::angle_radian(!numos::angleModeIsDeg(), ctx);
}

static MathEngineResult runTextual(giac::context* ctx, const char* expression,
                                   bool simplifyMode,
                                   EngineResultNode* outTree = nullptr,
                                   bool* outHasTree = nullptr) {
    MathEngineResult r;
    if (outHasTree) *outHasTree = false;
    if (!expression || !*expression) {
        r.status = MathEngineStatus::ParseError;
        r.diagnostic = "empty expression";
        return r;
    }
    try {
        DiagnosticCapture capture(ctx);

        giac::gen parsed(std::string(expression), ctx);
        if (giac::is_undef(parsed)) {
            r.status = MathEngineStatus::ParseError;
            r.diagnostic = capture.text();
            if (r.diagnostic.empty()) r.diagnostic = parsed.print(ctx);
            return r;
        }

        giac::gen out = giac::eval(parsed, giac::eval_level(ctx), ctx);
        if (simplifyMode && !giac::is_undef(out)) {
            out = giac::_simplify(out, ctx);
        }

        if (giac::is_undef(out)) {
            r.diagnostic = capture.text();
            std::string printed = out.print(ctx);
            if (!printed.empty() && printed != "undef") {
                if (!r.diagnostic.empty()) r.diagnostic += "\n";
                r.diagnostic += printed;
            }
            r.status = (r.diagnostic.find("rror") != std::string::npos)
                           ? MathEngineStatus::EvaluationError
                           : MathEngineStatus::Undefined;
            return r;
        }

        r.status = MathEngineStatus::Ok;
        r.exactText = out.print(ctx);
        r.diagnostic = capture.text();

        if (outTree && outHasTree) {
            int budget = kTreeNodeBudget;
            genToNode(out, ctx, *outTree, 0, budget);
            *outHasTree = true;
        }

        // Companion numeric form, only when it adds information.
        try {
            giac::gen approx = giac::evalf(out, 1, ctx);
            if (!giac::is_undef(approx)) {
                std::string printed = approx.print(ctx);
                if (printed != r.exactText) r.approximateText = printed;
            }
        } catch (...) {
            // exact result stands on its own
        }
        return r;
    } catch (const std::bad_alloc&) {
        r.status = MathEngineStatus::OutOfMemory;
        r.diagnostic = "allocation failure inside Giac";
        return r;
    } catch (const std::exception& e) {
        r.status = MathEngineStatus::EvaluationError;
        r.diagnostic = e.what();
        return r;
    } catch (...) {
        r.status = MathEngineStatus::EvaluationError;
        r.diagnostic = "unknown Giac exception";
        return r;
    }
}

MathEngineResult GiacEngine::evaluate(const char* expression) {
    CallGuard guard(_inCall);
    if (!guard.entered()) return rejectedReentrant();
    if (!begin()) {
        MathEngineResult r;
        r.status = MathEngineStatus::OutOfMemory;
        r.diagnostic = "Giac context initialization failed";
        return r;
    }
    syncAngleMode(_state->ctx);
    return runTextual(_state->ctx, expression, /*simplifyMode=*/false);
}

MathEngineResult GiacEngine::simplify(const char* expression) {
    CallGuard guard(_inCall);
    if (!guard.entered()) return rejectedReentrant();
    if (!begin()) {
        MathEngineResult r;
        r.status = MathEngineStatus::OutOfMemory;
        r.diagnostic = "Giac context initialization failed";
        return r;
    }
    syncAngleMode(_state->ctx);
    return runTextual(_state->ctx, expression, /*simplifyMode=*/true);
}

StructuredEngineResult GiacEngine::evaluateStructured(const char* expression) {
    StructuredEngineResult sr;
    CallGuard guard(_inCall);
    if (!guard.entered()) {
        sr.base = rejectedReentrant();
        return sr;
    }
    if (!begin()) {
        sr.base.status = MathEngineStatus::OutOfMemory;
        sr.base.diagnostic = "Giac context initialization failed";
        return sr;
    }
    syncAngleMode(_state->ctx);
    sr.base = runTextual(_state->ctx, expression, /*simplifyMode=*/false,
                         &sr.tree, &sr.hasTree);
    return sr;
}

namespace {

constexpr int kSolveWalkBudget = 800;

bool isValidSolveIdentifier(const std::string& name) {
    if (name.empty() || name.size() > 31) return false;
    const auto alphaOrUnderscore = [](unsigned char c) {
        return std::isalpha(c) != 0 || c == '_';
    };
    const auto alnumOrUnderscore = [](unsigned char c) {
        return std::isalnum(c) != 0 || c == '_';
    };
    if (!alphaOrUnderscore(static_cast<unsigned char>(name.front())))
        return false;
    for (char c : name)
        if (!alnumOrUnderscore(static_cast<unsigned char>(c))) return false;

    // WHY: these names are parsed as constants/commands by Giac and therefore
    // cannot safely designate an authored equation variable.
    static const char* const kReserved[] = {
        "pi", "e", "i", "oo", "undef", "solve", "csolve", "fsolve",
        "sin", "cos", "tan", "asin", "acos", "atan", "ln", "log",
        "log10", "sqrt", "surd", "exp", "abs", "diff", "integrate"
    };
    for (const char* reserved : kReserved)
        if (name == reserved) return false;
    return true;
}

bool containsUndef(const giac::gen& g, int depth, int& budget) {
    if (depth > 64 || --budget < 0) return true;
    if (giac::is_undef(g)) return true;
    switch (g.type) {
        case giac::_VECT:
            for (const auto& child : *g._VECTptr)
                if (containsUndef(child, depth + 1, budget)) return true;
            return false;
        case giac::_SYMB:
            return containsUndef(g._SYMBptr->feuille, depth + 1, budget);
        case giac::_FRAC:
            return containsUndef(g._FRACptr->num, depth + 1, budget) ||
                   containsUndef(g._FRACptr->den, depth + 1, budget);
        default:
            return false;
    }
}

bool containsSolveCall(const giac::gen& g, int depth, int& budget) {
    if (depth > 64 || --budget < 0) return true;
    if (g.type == giac::_SYMB) {
        if (g._SYMBptr->sommet == giac::at_solve) return true;
        return containsSolveCall(g._SYMBptr->feuille, depth + 1, budget);
    }
    if (g.type == giac::_VECT) {
        for (const auto& child : *g._VECTptr)
            if (containsSolveCall(child, depth + 1, budget)) return true;
    } else if (g.type == giac::_FRAC) {
        return containsSolveCall(g._FRACptr->num, depth + 1, budget) ||
               containsSolveCall(g._FRACptr->den, depth + 1, budget);
    }
    return false;
}

bool containsAnyVariable(const giac::gen& g,
                         const std::vector<giac::gen>& variables,
                         int depth, int& budget) {
    if (depth > 64 || --budget < 0) return false;
    if (g.type == giac::_IDNT) {
        for (const auto& variable : variables)
            if (g == variable) return true;
        return false;
    }
    if (g.type == giac::_VECT) {
        for (const auto& child : *g._VECTptr)
            if (containsAnyVariable(child, variables, depth + 1, budget))
                return true;
    } else if (g.type == giac::_SYMB) {
        return containsAnyVariable(g._SYMBptr->feuille, variables,
                                   depth + 1, budget);
    } else if (g.type == giac::_FRAC) {
        return containsAnyVariable(g._FRACptr->num, variables,
                                   depth + 1, budget) ||
               containsAnyVariable(g._FRACptr->den, variables,
                                   depth + 1, budget);
    }
    return false;
}

giac::gen explicitDegreeTrig(const giac::gen& value, int depth, int& budget) {
    if (depth > 64 || --budget < 0) return value;
    if (value.type == giac::_VECT) {
        giac::vecteur children;
        children.reserve(value._VECTptr->size());
        for (const auto& child : *value._VECTptr) {
            children.push_back(
                explicitDegreeTrig(child, depth + 1, budget));
        }
        return giac::gen(children, value.subtype);
    }
    if (value.type == giac::_FRAC) {
        return explicitDegreeTrig(value._FRACptr->num, depth + 1, budget) /
               explicitDegreeTrig(value._FRACptr->den, depth + 1, budget);
    }
    if (value.type != giac::_SYMB) return value;

    const auto function = value._SYMBptr->sommet;
    giac::gen leaf =
        explicitDegreeTrig(value._SYMBptr->feuille, depth + 1, budget);
    if (function == giac::at_sin || function == giac::at_cos ||
        function == giac::at_tan) {
        // WHY: embedded Giac's symbolic solver is unstable with a DEG
        // context. Making the unit conversion explicit preserves the authored
        // equation while the solver itself remains in its stable RAD mode.
        leaf = leaf * giac::cst_pi / giac::gen(180);
        return giac::symbolic(function, leaf);
    }
    if (function == giac::at_asin || function == giac::at_acos ||
        function == giac::at_atan) {
        return giac::gen(180) / giac::cst_pi *
               giac::symbolic(function, leaf);
    }
    return giac::symbolic(function, leaf);
}

bool splitEquality(const giac::gen& g, giac::gen& lhs, giac::gen& rhs) {
    if (g.type != giac::_SYMB || g._SYMBptr->sommet != giac::at_equal)
        return false;
    const giac::gen& leaf = g._SYMBptr->feuille;
    if (leaf.type != giac::_VECT || leaf._VECTptr->size() != 2) return false;
    lhs = leaf._VECTptr->front();
    rhs = leaf._VECTptr->back();
    return true;
}

bool exactEquivalent(const giac::gen& a, const giac::gen& b,
                     giac::context* ctx) {
    if (a == b) return true;
    try {
        return giac::is_zero(giac::ratnormal(a - b, ctx), ctx);
    } catch (...) {
        return false;
    }
}

bool realApproximation(const giac::gen& exact, giac::context* ctx,
                       double& value, std::string& printed) {
    try {
        giac::gen approx = giac::evalf_double(exact, 1, ctx);
        printed = approx.print(ctx);
        if (approx.type == giac::_DOUBLE_) {
            value = approx._DOUBLE_val;
            return std::isfinite(value);
        }
        if (approx.type == giac::_INT_) {
            value = static_cast<double>(approx.val);
            return true;
        }
        if (approx.type == giac::_CPLX) {
            const giac::gen& re = *approx._CPLXptr;
            const giac::gen& im = *(approx._CPLXptr + 1);
            if (giac::is_zero(im, ctx) && re.type == giac::_DOUBLE_) {
                value = re._DOUBLE_val;
                return std::isfinite(value);
            }
        }
    } catch (...) {
        // The exact solution remains valid without a numeric companion.
    }
    return false;
}

struct RawSolutionGroup {
    std::vector<giac::gen> values;
    std::vector<double> realOrder;
    std::string exactOrder;
    bool allReal = true;
};

bool unwrapSingleValue(const giac::gen& raw, const giac::gen& variable,
                       giac::gen& value) {
    value = raw;
    for (int depth = 0;
         depth < 8 && value.type == giac::_VECT &&
         value._VECTptr->size() == 1;
         ++depth) {
        value = value._VECTptr->front();
    }
    giac::gen lhs, rhs;
    if (!splitEquality(value, lhs, rhs)) return true;
    if (lhs == variable) { value = rhs; return true; }
    if (rhs == variable) { value = lhs; return true; }
    return false;
}

bool adaptSystemGroup(const giac::gen& raw,
                      const std::vector<giac::gen>& variables,
                      RawSolutionGroup& group) {
    giac::gen branch = raw;
    for (int depth = 0;
         depth < 8 && branch.type == giac::_VECT &&
         branch._VECTptr->size() == 1 &&
         branch._VECTptr->front().type == giac::_VECT;
         ++depth) {
        branch = branch._VECTptr->front();
    }
    if (branch.type != giac::_VECT) return false;
    const giac::vecteur& elems = *branch._VECTptr;
    if (elems.size() != variables.size()) return false;

    bool anyEquality = false;
    bool allEquality = true;
    for (const auto& elem : elems) {
        giac::gen lhs, rhs;
        const bool equality = splitEquality(elem, lhs, rhs);
        anyEquality = anyEquality || equality;
        allEquality = allEquality && equality;
    }
    if (anyEquality && !allEquality) return false;

    group.values.resize(variables.size());
    if (!allEquality) {
        for (size_t i = 0; i < elems.size(); ++i) group.values[i] = elems[i];
        return true;
    }

    std::vector<bool> assigned(variables.size(), false);
    for (const auto& elem : elems) {
        giac::gen lhs, rhs;
        splitEquality(elem, lhs, rhs);
        bool matched = false;
        for (size_t i = 0; i < variables.size(); ++i) {
            if (lhs == variables[i] || rhs == variables[i]) {
                if (assigned[i]) return false;
                group.values[i] = (lhs == variables[i]) ? rhs : lhs;
                assigned[i] = true;
                matched = true;
                break;
            }
        }
        if (!matched) return false;
    }
    for (bool present : assigned)
        if (!present) return false;
    return true;
}

std::string rawGroupKey(const RawSolutionGroup& group, giac::context* ctx) {
    std::string key;
    for (const auto& value : group.values) {
        key += value.print(ctx);
        key.push_back('\x1f');
    }
    return key;
}

bool sameRawGroup(const RawSolutionGroup& a, const RawSolutionGroup& b,
                  giac::context* ctx) {
    if (a.values.size() != b.values.size()) return false;
    for (size_t i = 0; i < a.values.size(); ++i)
        if (!exactEquivalent(a.values[i], b.values[i], ctx)) return false;
    return true;
}

StructuredSolveResult rejectedSolveReentrant() {
    StructuredSolveResult result;
    result.status = MathEngineStatus::Unsupported;
    result.setKind = SolutionSetKind::Unsupported;
    result.diagnostic = "GiacEngine is non-reentrant: nested call rejected";
    return result;
}

StructuredSolveResult runStructuredSolve(
    giac::context* ctx,
    const std::vector<SolveEquation>& equations,
    const std::vector<std::string>& variableNames,
    SolveDomainPolicy policy) {
    StructuredSolveResult result;
    if (equations.empty() || variableNames.empty() ||
        (equations.size() == 1 && variableNames.size() != 1)) {
        result.status = MathEngineStatus::ParseError;
        result.diagnostic = "invalid equation/variable arity";
        return result;
    }
    for (const auto& name : variableNames) {
        if (!isValidSolveIdentifier(name)) {
            result.status = MathEngineStatus::ParseError;
            result.diagnostic = "invalid or reserved solve variable: " + name;
            return result;
        }
    }

    try {
        DiagnosticCapture capture(ctx);
        const bool authoredInDegrees = numos::angleModeIsDeg();
        std::vector<giac::gen> variables;
        variables.reserve(variableNames.size());
        for (const auto& name : variableNames) {
            giac::gen variable(name, ctx);
            if (variable.type != giac::_IDNT ||
                variable.print(ctx) != name) {
                result.status = MathEngineStatus::ParseError;
                result.diagnostic = "solve variable is not a free identifier: " + name;
                return result;
            }
            variables.push_back(variable);
        }

        giac::vecteur equationGens;
        equationGens.reserve(equations.size());
        for (const auto& equation : equations) {
            if (equation.lhs.empty() || equation.rhs.empty()) {
                result.status = MathEngineStatus::ParseError;
                result.diagnostic = "empty equation side";
                return result;
            }
            giac::gen lhs(equation.lhs, ctx);
            giac::gen rhs(equation.rhs, ctx);
            if (giac::is_undef(lhs) || giac::is_undef(rhs)) {
                result.status = MathEngineStatus::ParseError;
                result.diagnostic = capture.text();
                if (result.diagnostic.empty())
                    result.diagnostic = "unable to parse equation side";
                return result;
            }
            if (authoredInDegrees) {
                int angleBudget = kSolveWalkBudget;
                lhs = explicitDegreeTrig(lhs, 0, angleBudget);
                angleBudget = kSolveWalkBudget;
                rhs = explicitDegreeTrig(rhs, 0, angleBudget);
            }
            equationGens.push_back(giac::symb_equal(lhs, rhs));
        }

        const bool oldComplex = giac::complex_mode(ctx);
        const bool oldAngleRadians = giac::angle_radian(ctx);
        giac::complex_mode(policy == SolveDomainPolicy::RealAndComplex, ctx);
        giac::angle_radian(true, ctx);
        giac::vecteur raw;
        try {
            if (equationGens.size() == 1 && variables.size() == 1) {
                raw = giac::solve(equationGens.front(), variables.front(),
                                  policy == SolveDomainPolicy::RealAndComplex ? 1 : 0,
                                  ctx);
            } else {
                raw = giac::gsolve(
                    equationGens, giac::vecteur(variables.begin(), variables.end()),
                    policy == SolveDomainPolicy::RealAndComplex,
                    /*evalf_after=*/0, ctx);
            }
        } catch (...) {
            giac::complex_mode(oldComplex, ctx);
            giac::angle_radian(oldAngleRadians, ctx);
            throw;
        }
        giac::complex_mode(oldComplex, ctx);
        giac::angle_radian(oldAngleRadians, ctx);

        result.rawExactText = giac::gen(raw, giac::_LIST__VECT).print(ctx);
        result.diagnostic = capture.text();

        int undefBudget = kSolveWalkBudget;
        if (containsUndef(giac::gen(raw, giac::_LIST__VECT), 0, undefBudget)) {
            result.status = MathEngineStatus::Undefined;
            result.setKind = SolutionSetKind::Unsupported;
            if (result.diagnostic.empty())
                result.diagnostic = "Giac returned an undefined solution";
            return result;
        }
        if (raw.empty()) {
            result.status = MathEngineStatus::Ok;
            result.setKind = SolutionSetKind::NoSolution;
            return result;
        }

        std::vector<RawSolutionGroup> normalized;
        normalized.reserve(raw.size());
        for (const auto& rawItem : raw) {
            RawSolutionGroup group;
            if (variables.size() == 1) {
                giac::gen value;
                if (!unwrapSingleValue(rawItem, variables.front(), value)) {
                    result.status = MathEngineStatus::Unsupported;
                    result.setKind = SolutionSetKind::Unsupported;
                    result.diagnostic = "unsupported Giac equality solution shape";
                    return result;
                }
                group.values.push_back(value);
            } else if (!adaptSystemGroup(rawItem, variables, group)) {
                result.status = MathEngineStatus::Unsupported;
                result.setKind = SolutionSetKind::Unsupported;
                result.diagnostic = "unsupported Giac system solution shape";
                return result;
            }

            for (auto& value : group.values) {
                try {
                    value = giac::ratnormal(value, ctx);
                } catch (...) {
                    // Keep Giac's exact solve value if normalization declines.
                }
                int solveBudget = kSolveWalkBudget;
                if (containsSolveCall(value, 0, solveBudget)) {
                    result.status = MathEngineStatus::Unsupported;
                    result.setKind = SolutionSetKind::Unsupported;
                    result.diagnostic = "Giac left the solve operation unevaluated";
                    return result;
                }
                int variableBudget = kSolveWalkBudget;
                if (containsAnyVariable(value, variables, 0, variableBudget)) {
                    result.status = MathEngineStatus::Ok;
                    result.setKind = SolutionSetKind::AllValues;
                    result.groups.clear();
                    return result;
                }
                double realValue = 0.0;
                std::string approximateText;
                if (realApproximation(value, ctx, realValue,
                                      approximateText)) {
                    group.realOrder.push_back(realValue);
                } else {
                    group.allReal = false;
                }
            }
            group.exactOrder = rawGroupKey(group, ctx);
            normalized.push_back(std::move(group));
        }

        std::stable_sort(normalized.begin(), normalized.end(),
                         [](const RawSolutionGroup& a,
                            const RawSolutionGroup& b) {
                             // WHY: numeric approximations are used only as
                             // ordering keys; the retained/displayed values
                             // remain Giac's exact gens. This gives natural,
                             // stable order for real roots such as 30,150
                             // without reconstructing exact data from doubles.
                             if (a.allReal != b.allReal)
                                 return a.allReal;  // real groups first
                             if (a.allReal) {
                                 return std::lexicographical_compare(
                                     a.realOrder.begin(), a.realOrder.end(),
                                     b.realOrder.begin(), b.realOrder.end());
                             }
                             return a.exactOrder < b.exactOrder;
                         });
        std::vector<RawSolutionGroup> unique;
        unique.reserve(normalized.size());
        for (auto& group : normalized) {
            bool duplicate = false;
            for (const auto& accepted : unique) {
                if (sameRawGroup(group, accepted, ctx)) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) unique.push_back(std::move(group));
        }

        result.groups.reserve(unique.size());
        for (const auto& rawGroup : unique) {
            StructuredSolutionGroup group;
            group.values.reserve(rawGroup.values.size());
            for (size_t i = 0; i < rawGroup.values.size(); ++i) {
                StructuredSolution solution;
                solution.variable = variableNames[i];
                solution.exactText = rawGroup.values[i].print(ctx);
                int treeBudget = kTreeNodeBudget;
                genToNode(rawGroup.values[i], ctx, solution.exactValue, 0,
                          treeBudget);
                solution.hasApproximateReal = realApproximation(
                    rawGroup.values[i], ctx, solution.approximateReal,
                    solution.approximateText);
                group.values.push_back(std::move(solution));
            }
            result.groups.push_back(std::move(group));
        }
        result.status = MathEngineStatus::Ok;
        result.setKind = SolutionSetKind::Solutions;
        return result;
    } catch (const std::bad_alloc&) {
        result.status = MathEngineStatus::OutOfMemory;
        result.setKind = SolutionSetKind::Unsupported;
        result.diagnostic = "allocation failure inside Giac solve";
    } catch (const std::exception& e) {
        result.status = MathEngineStatus::EvaluationError;
        result.setKind = SolutionSetKind::Unsupported;
        result.diagnostic = e.what();
    } catch (...) {
        result.status = MathEngineStatus::EvaluationError;
        result.setKind = SolutionSetKind::Unsupported;
        result.diagnostic = "unknown Giac solve exception";
    }
    return result;
}

} // namespace

StructuredSolveResult GiacEngine::solveStructured(
    const SolveEquation& equation, const std::string& variable,
    SolveDomainPolicy policy) {
    CallGuard guard(_inCall);
    if (!guard.entered()) return rejectedSolveReentrant();
    if (!begin()) {
        StructuredSolveResult result;
        result.status = MathEngineStatus::OutOfMemory;
        result.diagnostic = "Giac context initialization failed";
        return result;
    }
    syncAngleMode(_state->ctx);
    return runStructuredSolve(_state->ctx, {equation}, {variable}, policy);
}

StructuredSolveResult GiacEngine::solveSystemStructured(
    const std::vector<SolveEquation>& equations,
    const std::vector<std::string>& solveVariables,
    SolveDomainPolicy policy) {
    CallGuard guard(_inCall);
    if (!guard.entered()) return rejectedSolveReentrant();
    if (!begin()) {
        StructuredSolveResult result;
        result.status = MathEngineStatus::OutOfMemory;
        result.diagnostic = "Giac context initialization failed";
        return result;
    }
    syncAngleMode(_state->ctx);
    return runStructuredSolve(_state->ctx, equations, solveVariables, policy);
}

#ifdef NUMOS_GIAC_HOST_HARNESS
bool GiacEngine::debugStructuredSolveAdapterForms() {
    CallGuard guard(_inCall);
    if (!guard.entered() || !begin()) return false;

    giac::context* ctx = _state->ctx;
    const giac::gen x(giac::identificateur("x"));
    const giac::gen y(giac::identificateur("y"));

    // Single-value adapter: [[x=2]] -> 2.
    const giac::gen xEquality = giac::symb_equal(x, giac::gen(2));
    giac::vecteur nestedOnceItems;
    nestedOnceItems.push_back(xEquality);
    const giac::gen nestedOnce(nestedOnceItems, giac::_LIST__VECT);
    giac::vecteur nestedTwiceItems;
    nestedTwiceItems.push_back(nestedOnce);
    const giac::gen nestedTwice(nestedTwiceItems, giac::_LIST__VECT);
    giac::gen singleValue;
    if (!unwrapSingleValue(nestedTwice, x, singleValue) ||
        !exactEquivalent(singleValue, giac::gen(2), ctx)) {
        return false;
    }

    // System adapter: equality order is deliberately y,x; output must follow
    // the requested UI order x,y.
    giac::vecteur equalitySystemItems;
    equalitySystemItems.push_back(giac::symb_equal(y, giac::gen(1)));
    equalitySystemItems.push_back(giac::symb_equal(x, giac::gen(2)));
    const giac::gen equalitySystem(equalitySystemItems, giac::_LIST__VECT);
    RawSolutionGroup group;
    const std::vector<giac::gen> variables{x, y};
    return adaptSystemGroup(equalitySystem, variables, group) &&
           group.values.size() == 2 &&
           exactEquivalent(group.values[0], giac::gen(2), ctx) &&
           exactEquivalent(group.values[1], giac::gen(1), ctx);
}
#endif

MathEngineResult GiacEngine::assign(const char* name,
                                    const char* valueExpression) {
    MathEngineResult bad;
    bad.status = MathEngineStatus::ParseError;
    if (!name || !*name || !valueExpression || !*valueExpression) {
        bad.diagnostic = "assign: empty name or value";
        return bad;
    }
    for (const char* p = name; *p; ++p) {
        const bool okChar = (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
                            *p == '_' || (p != name && *p >= '0' && *p <= '9');
        if (!okChar) {
            bad.diagnostic = "assign: name is not a plain identifier";
            return bad;
        }
    }
    std::string stmt;
    stmt.reserve(std::strlen(name) + std::strlen(valueExpression) + 8);
    stmt += name;
    stmt += ":=(";
    stmt += valueExpression;
    stmt += ")";

    CallGuard guard(_inCall);
    if (!guard.entered()) return rejectedReentrant();
    if (!begin()) {
        MathEngineResult r;
        r.status = MathEngineStatus::OutOfMemory;
        r.diagnostic = "Giac context initialization failed";
        return r;
    }
    syncAngleMode(_state->ctx);
    return runTextual(_state->ctx, stmt.c_str(), /*simplifyMode=*/false);
}

CompiledExpression GiacEngine::compileNumeric(const char* expression,
                                              const char* variable,
                                              bool strictVariables) {
    CompiledExpression handle;
    handle._impl = new CompiledExpression::Impl();
    handle._impl->generation = _generation;

    CallGuard guard(_inCall);
    if (!guard.entered()) {
        handle._impl->diag = "GiacEngine is non-reentrant: nested call rejected";
        return handle;
    }
    if (!begin() || !expression || !*expression || !variable || !*variable) {
        handle._impl->diag = "invalid input or Giac context unavailable";
        return handle;
    }
    syncAngleMode(_state->ctx);
    try {
        DiagnosticCapture capture(_state->ctx);
        handle._impl->var = giac::gen(giac::identificateur(variable));
        giac::gen parsed(std::string(expression), _state->ctx);
        if (giac::is_undef(parsed)) {
            handle._impl->diag = capture.text();
            if (handle._impl->diag.empty())
                handle._impl->diag = parsed.print(_state->ctx);
            return handle;
        }
        if (strictVariables) {
            // Grapher contract: no free identifier beyond the sampling
            // variable, and NO context eval() — retaining the raw parse means
            // a context assignment (x:=5) or a DEG-folded constant can never
            // be baked into the handle. Angle mode therefore only matters at
            // evaluateNumeric() time.
            std::string offender;
            int budget = 4096;
            if (findDisallowedIdent(parsed, &handle._impl->var, 1, 0, budget,
                                    offender, _state->ctx)) {
                handle._impl->diag = "unknown variable: " + offender;
                return handle;
            }
            handle._impl->expr = parsed;
        } else {
            // One symbolic normalization up front; samples reuse the DAG.
            handle._impl->expr =
                giac::eval(parsed, giac::eval_level(_state->ctx), _state->ctx);
            if (giac::is_undef(handle._impl->expr)) {
                handle._impl->diag = capture.text();
                return handle;
            }
        }
        handle._impl->parsedOk = true;
    } catch (const std::exception& e) {
        handle._impl->diag = e.what();
    } catch (...) {
        handle._impl->diag = "unknown Giac exception";
    }
    return handle;
}

CompiledExpression GiacEngine::compileNumeric2D(const char* expression,
                                                const char* variableA,
                                                const char* variableB) {
    CompiledExpression handle;
    handle._impl = new CompiledExpression::Impl();
    handle._impl->generation = _generation;

    CallGuard guard(_inCall);
    if (!guard.entered()) {
        handle._impl->diag = "GiacEngine is non-reentrant: nested call rejected";
        return handle;
    }
    if (!begin() || !expression || !*expression ||
        !variableA || !*variableA || !variableB || !*variableB ||
        std::strcmp(variableA, variableB) == 0) {
        handle._impl->diag = "invalid input or Giac context unavailable";
        return handle;
    }
    syncAngleMode(_state->ctx);
    try {
        DiagnosticCapture capture(_state->ctx);
        handle._impl->var  = giac::gen(giac::identificateur(variableA));
        handle._impl->var2 = giac::gen(giac::identificateur(variableB));
        handle._impl->twoVars = true;
        giac::gen parsed(std::string(expression), _state->ctx);
        if (giac::is_undef(parsed)) {
            handle._impl->diag = capture.text();
            if (handle._impl->diag.empty())
                handle._impl->diag = parsed.print(_state->ctx);
            return handle;
        }
        // Always strict (see compileNumeric strictVariables): raw parse
        // retained, only the two sampling variables may appear free.
        const giac::gen allowed[2] = { handle._impl->var, handle._impl->var2 };
        std::string offender;
        int budget = 4096;
        if (findDisallowedIdent(parsed, allowed, 2, 0, budget, offender,
                                _state->ctx)) {
            handle._impl->diag = "unknown variable: " + offender;
            return handle;
        }
        handle._impl->expr = parsed;
        handle._impl->parsedOk = true;
    } catch (const std::exception& e) {
        handle._impl->diag = e.what();
    } catch (...) {
        handle._impl->diag = "unknown Giac exception";
    }
    return handle;
}

// Shared result classification for the retained-sample evaluators: real
// doubles and signed infinities are samples; anything else (undef, unsigned
// infinity, complex, symbolic residue) is an honest rejection.
static bool classifyNumericResult(const giac::gen& num, double& out) {
    if (num.type == giac::_DOUBLE_) {
        out = num._DOUBLE_val;
        return true;
    }
    // Preserve pole behavior: signed infinities are legitimate samples.
    if (num == giac::plus_inf) {
        out = INFINITY;
        return true;
    }
    if (num == giac::minus_inf) {
        out = -INFINITY;
        return true;
    }
    return false;  // symbolic residue / undef / unsigned infinity
}

bool GiacEngine::evaluateNumeric(const CompiledExpression& expr, double x,
                                 double& out) {
    out = NAN;
    if (!expr.valid() || expr._impl->twoVars) return false;

    CallGuard guard(_inCall);
    if (!guard.entered()) return false;
    if (!begin()) return false;
    syncAngleMode(_state->ctx);
    try {
        giac::gen sub = giac::subst(expr._impl->expr, expr._impl->var,
                                    giac::gen(x), false, _state->ctx);
        giac::gen num = giac::evalf_double(sub, 1, _state->ctx);
        return classifyNumericResult(num, out);
    } catch (...) {
        return false;
    }
}

bool GiacEngine::evaluateNumeric2D(const CompiledExpression& expr, double a,
                                   double b, double& out) {
    out = NAN;
    if (!expr.valid() || !expr._impl->twoVars) return false;

    CallGuard guard(_inCall);
    if (!guard.entered()) return false;
    if (!begin()) return false;
    syncAngleMode(_state->ctx);
    try {
        // Simultaneous substitution of both sampling variables — the shared
        // context is never written, so a stored x/y value cannot capture the
        // graph variables and no per-sample assignment state accumulates.
        giac::vecteur vars(2), vals(2);
        vars[0] = expr._impl->var;  vars[1] = expr._impl->var2;
        vals[0] = giac::gen(a);     vals[1] = giac::gen(b);
        giac::gen sub = giac::subst(expr._impl->expr, vars, vals, false,
                                    _state->ctx);
        giac::gen num = giac::evalf_double(sub, 1, _state->ctx);
        return classifyNumericResult(num, out);
    } catch (...) {
        return false;
    }
}

// ---------------------------------------------------------------------------
// Transition access for the legacy GiacBridge UART path
// ---------------------------------------------------------------------------

namespace giacinternal {

giac::context* sharedContext() {
    if (!GiacEngine::instance().begin()) return nullptr;
    return s_sharedCtx;
}

} // namespace giacinternal

} // namespace numos
