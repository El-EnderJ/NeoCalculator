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
    uint32_t generation = 0;
    bool parsedOk = false;
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
                                              const char* variable) {
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
        // One symbolic normalization up front; samples reuse the DAG.
        handle._impl->expr =
            giac::eval(parsed, giac::eval_level(_state->ctx), _state->ctx);
        if (giac::is_undef(handle._impl->expr)) {
            handle._impl->diag = capture.text();
            return handle;
        }
        handle._impl->parsedOk = true;
    } catch (const std::exception& e) {
        handle._impl->diag = e.what();
    } catch (...) {
        handle._impl->diag = "unknown Giac exception";
    }
    return handle;
}

bool GiacEngine::evaluateNumeric(const CompiledExpression& expr, double x,
                                 double& out) {
    out = NAN;
    if (!expr.valid()) return false;

    CallGuard guard(_inCall);
    if (!guard.entered()) return false;
    if (!begin()) return false;
    syncAngleMode(_state->ctx);
    try {
        giac::gen sub = giac::subst(expr._impl->expr, expr._impl->var,
                                    giac::gen(x), false, _state->ctx);
        giac::gen num = giac::evalf_double(sub, 1, _state->ctx);
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
