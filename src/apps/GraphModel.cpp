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

/**
 * GraphModel.cpp — Grapher MVC: Model implementation
 */
#include "GraphModel.h"
#include "../math/AngleModeRuntime.h"
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <string>

namespace grapher {

// ── Helpers ────────────────────────────────────────────────────────────────

// The numeric Parser (Parser.cpp) is binary-only — it has no unary-minus rule,
// so a function whose rhs starts with a sign ("-x", "-2x", "sin(-x)") fails to
// compile and never plots. Rewrite such a leading/parenthesised unary +/- into
// a parser-safe binary form by inserting a '0' operand ("-2x" -> "0-2x"). Only
// positions where "0<sign>" provably preserves both value AND precedence are
// rewritten: expression start, right after '(' , or right after '=' (after '='
// the rhs sign is effectively leading). Signs following a value or a *, /, ^ or
// another sign are left untouched, so this can never change the value of an
// already well-formed expression — it only rescues ones that were invalid.
static std::string normalizeLeadingUnary(const char* rhs) {
    std::string out;
    char prevSig = '\0';
    for (const char* p = rhs; *p; ++p) {
        char c = *p;
        if (c == ' ' || c == '\t') { out += c; continue; }
        if ((c == '-' || c == '+') &&
            (prevSig == '\0' || prevSig == '(' || prevSig == '=')) {
            out += '0';
        }
        out += c;
        prevSig = c;
    }
    return out;
}

// Trim leading/trailing ASCII whitespace from a std::string in place.
static std::string trimSpaces(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t')) --b;
    return s.substr(a, b - a);
}

// True when the trimmed string is exactly the single variable y/Y — i.e. the
// side is the plain dependent variable, marking an explicit "y = f(x)" form.
static bool isJustY(const std::string& s) {
    std::string t = trimSpaces(s);
    return t.size() == 1 && (t[0] == 'y' || t[0] == 'Y');
}

// True when the trimmed string is exactly the single variable x/X — marks the
// "x = f(y)" orientation (explicit in y) when the other side has no x.
static bool isJustX(const std::string& s) {
    std::string t = trimSpaces(s);
    return t.size() == 1 && (t[0] == 'x' || t[0] == 'X');
}

const char* GraphModel::getExprRHS(const char* text) {
    if (!text) return nullptr;
    // An explicit "lhs = rhs" plots the rhs; a bare expression (no '=') is
    // treated as the implicit function y = <expr>, so "x" plots as y=x and
    // "2x" as y=2x. The whole (leading-trimmed) string becomes the rhs.
    const char* eq = strchr(text, '=');
    const char* rhs = eq ? eq + 1 : text;
    while (*rhs == ' ' || *rhs == '\t') ++rhs;
    if (!*rhs) return nullptr;
    return rhs;
}

// ── Implicit variable multiplication ───────────────────────────────────────
//
// The shared Tokenizer reads a run of consecutive letters as ONE identifier, so
// "xx" tokenizes to the single unknown identifier "xx" and never plots — even
// though a student means x·x. On this calculator every variable is a single
// letter; a multi-letter identifier is only ever a function name (which keeps
// its '(' and tokenizes as TokenType::Function, untouched here) or a reserved
// constant word. So a bare multi-letter *Identifier* run is juxtaposed
// single-letter variables: expand it into a product, greedily preserving the
// reserved multi-char words (pi, ans, preans; single 'e'/'E' resolves to Euler
// on its own). This is Grapher-local (GraphModel only) — the Tokenizer, and
// therefore CalculationApp, is left exactly as it was.
//
// Examples:  "xx" → x*x   "xxx" → x*x*x   "xy" → x*y   "xpi" → x*pi   "pi" → pi
//            "x2" → x*2   "pi2" → pi*2    "x(x+1)" → x*(x+1)

// Case-insensitive prefix compare of the first n chars.
static bool ciPrefixEq(const std::string& s, size_t pos, const char* w) {
    size_t n = std::strlen(w);
    if (pos + n > s.size()) return false;
    for (size_t i = 0; i < n; ++i) {
        char a = s[pos + i]; if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
        char b = w[i];       if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
        if (a != b) return false;
    }
    return true;
}

// The functions the shared Evaluator actually dispatches (Evaluator.cpp Function
// case, case-insensitive). A Tokenizer "Function" token with any OTHER name is
// not a call at all — it is juxtaposed variables in front of a parenthesised
// group ("x(x+1)"), which the lookahead misclassified.
static bool isSupportedFunctionName(const Token& t) {
    static const char* kFuncs[] = { "sin", "cos", "tan", "sqrt", "log", "ln", "abs" };
    std::string lc(t.text.c_str());
    for (char& c : lc) if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
    for (const char* f : kFuncs) if (lc == f) return true;
    return false;
}

static std::vector<Token> expandImplicitVarMul(const std::vector<Token>& in) {
    // Reserved multi-letter words that must NOT be split into single vars.
    // Longer words first so a prefix match is unambiguous.
    static const char* kReserved[] = { "preans", "ans", "pi" };

    std::vector<Token> out;
    out.reserve(in.size());

    auto pushMul = [&out]() {
        Token mul;
        mul.type = TokenType::Operator;
        mul.op   = OperatorKind::Mul;
        mul.text = "*";
        out.push_back(mul);
    };

    // Split one merged run into '*'-joined pieces: reserved words, single-letter
    // variables, and DIGIT RUNS as Number literals ("x2" → x*2, the mirror of the
    // already-valid "2x"; a bare digit piece must never surface as an unknown
    // *identifier* — the old expansion produced the nonsense reason "unknown: 2").
    auto expandRun = [&](const std::string& s) {
        size_t i = 0;
        bool firstPiece = true;
        while (i < s.size()) {
            size_t matchLen = 0;
            bool   isNum    = false;
            if (s[i] >= '0' && s[i] <= '9') {          // whole digit run → Number
                while (i + matchLen < s.size() &&
                       s[i + matchLen] >= '0' && s[i + matchLen] <= '9') ++matchLen;
                isNum = true;
            } else {
                for (const char* w : kReserved) {
                    if (ciPrefixEq(s, i, w)) { matchLen = std::strlen(w); break; }
                }
                if (matchLen == 0) matchLen = 1;       // single-letter variable
            }

            if (!firstPiece) pushMul();                // implicit '*' between pieces
            firstPiece = false;

            Token id;
            id.text = s.substr(i, matchLen).c_str();
            if (isNum) {
                id.type  = TokenType::Number;
                id.value = std::strtod(s.substr(i, matchLen).c_str(), nullptr);
            } else {
                id.type = TokenType::Identifier;
            }
            out.push_back(id);
            i += matchLen;
        }
    };

    for (size_t k = 0; k < in.size(); ++k) {
        const Token& t = in[k];

        // "x(x+1)": the Tokenizer lookahead marks the identifier before '(' as a
        // Function, so its implicit-mul pass deliberately skipped it and the
        // Evaluator died with "Función no soportada: x" (classifier: syntax).
        // A Function token that is NOT a real evaluator function is juxtaposed
        // variables times a parenthesised group: expand the name and multiply
        // into the following '('. sin(/cos(/…​ pass through untouched.
        if (t.type == TokenType::Function && !isSupportedFunctionName(t)) {
            expandRun(std::string(t.text.c_str()));
            pushMul();                                 // …* ( — LParen is the next token
            continue;
        }

        if (t.type != TokenType::Identifier || t.text.length() <= 1) {
            out.push_back(t);
            continue;
        }

        // "x2.5" splits at the Tokenizer into Identifier "x2" · '*' · Number
        // "0.5" (readNumber rewrites ".5" → "0.5" and the Tokenizer's own
        // implicit-mul pass inserts the '*'). Expanding the digit tail would
        // silently evaluate x*2*0.5 — a wrong answer, worse than a refusal.
        // When a digit-TAILED identifier is followed (directly or through that
        // '*') by a decimal-containing Number, keep the identifier intact so it
        // stays an honest unknown-identifier refusal, exactly as before.
        std::string s(t.text.c_str());
        const bool digitTail = (s.back() >= '0' && s.back() <= '9');
        auto isDecimalNumber = [&](size_t idx) {
            return idx < in.size() && in[idx].type == TokenType::Number &&
                   std::strchr(in[idx].text.c_str(), '.') != nullptr;
        };
        const bool mulNext = (k + 1 < in.size() &&
                              in[k + 1].type == TokenType::Operator &&
                              in[k + 1].op == OperatorKind::Mul);
        if (digitTail && (isDecimalNumber(k + 1) || (mulNext && isDecimalNumber(k + 2)))) {
            out.push_back(t);
            continue;
        }

        expandRun(s);
    }
    return out;
}

// Tokenize `expr` and expand juxtaposed single-letter variables (see above).
// Returns false when the tokenizer itself rejects the input.
bool GraphModel::tokenizeExpanded(const char* expr, std::vector<Token>& outTokens) {
    TokenizeResult tr = _tok.tokenize(expr);
    if (!tr.ok) return false;
    outTokens = expandImplicitVarMul(tr.tokens);
    return true;
}

// ── compileExpr / referencesY ──────────────────────────────────────────────

bool GraphModel::compileExpr(const char* expr, std::vector<Token>& outRpn) {
    outRpn.clear();
    if (!expr || !*expr) return false;
    std::string norm = normalizeLeadingUnary(expr);
    std::vector<Token> toks;
    if (!tokenizeExpanded(norm.c_str(), toks)) return false;
    ParseResult pr = _par.toRPN(toks);
    if (!pr.ok) return false;
    outRpn = pr.outputRPN;
    return true;
}

bool GraphModel::referencesY(const char* expr) {
    if (!expr) return false;
    std::vector<Token> toks;
    if (!tokenizeExpanded(expr, toks)) {
        // Tokenize failed — fall back to a literal scan so we still classify.
        for (const char* p = expr; *p; ++p)
            if (*p == 'y' || *p == 'Y') return true;
        return false;
    }
    for (const Token& t : toks) {
        if (t.type == TokenType::Identifier &&
            (t.text == "y" || t.text == "Y")) return true;
    }
    return false;
}

bool GraphModel::referencesX(const char* expr) {
    if (!expr) return false;
    std::vector<Token> toks;
    if (!tokenizeExpanded(expr, toks)) {
        // Tokenize failed — fall back to a literal scan so we still classify.
        for (const char* p = expr; *p; ++p)
            if (*p == 'x' || *p == 'X') return true;
        return false;
    }
    for (const Token& t : toks) {
        if (t.type == TokenType::Identifier &&
            (t.text == "x" || t.text == "X")) return true;
    }
    return false;
}

// ── dryRunStructural ───────────────────────────────────────────────────────

bool GraphModel::dryRunStructural(const std::vector<Token>& rpn, CartesianFunction& fn) {
    if (rpn.empty()) return true;
    _vars.setVar('x', 1.0);
    _vars.setVar('y', 1.0);
    EvalResult er = _eval.evaluateRPN(rpn, _vars);
    if (er.ok) return true;

    // Value-dependent failures at the probe point do not invalidate: the slot
    // may be perfectly fine elsewhere in its domain (contract §A.3).
    if (er.errorMessage == "Error: Dominio" ||
        er.errorMessage == "Error: Div por 0" ||
        er.errorMessage == "Error: Resultado no finito") {
        return true;
    }

    // "Variable no definida: <ident>" → unknown identifier; keep the name so the
    // user sees exactly what the pipeline could not resolve ("unknown: xy").
    const char* kUndef = "Variable no definida: ";
    const char* msg = er.errorMessage.c_str();
    if (strncmp(msg, kUndef, strlen(kUndef)) == 0) {
        fn.invalidReason = InvalidReason::UnknownIdent;
        snprintf(fn.reasonDetail, sizeof(fn.reasonDetail), "%s", msg + strlen(kUndef));
    } else {
        fn.invalidReason = InvalidReason::Syntax;
    }
    fn.valid = false;
    return false;
}

// ── GIAC-C01: classifier RPN → Giac text → retained compiled handles ───────

bool GraphModel::rpnToGiacText(const std::vector<Token>& rpn, std::string& out) {
    std::vector<std::string> st;
    st.reserve(rpn.size());
    for (const Token& t : rpn) {
        switch (t.type) {
            case TokenType::End:
                break;
            case TokenType::Number: {
                char buf[40];
                snprintf(buf, sizeof(buf), "(%.17g)", t.value);
                st.push_back(buf);
                break;
            }
            case TokenType::Identifier: {
                String lc = t.text;
                lc.toLowerCase();
                if (lc == "x")       st.push_back("x");
                else if (lc == "y")  st.push_back("y");
                else if (lc == "pi") st.push_back("pi");
                else if (lc == "e")  st.push_back("exp(1)");
                // The Grapher's private VariableContext never stores an Ans:
                // the legacy Evaluator resolved it to 0.0 for every sample.
                // Preserve that observable value verbatim (dryRunStructural
                // has already rejected every OTHER identifier).
                else if (lc == "ans") st.push_back("(0)");
                else return false;
                break;
            }
            case TokenType::Function: {
                if (st.empty()) return false;
                String lc = t.text;
                lc.toLowerCase();
                const char* name = nullptr;
                if      (lc == "sin")  name = "sin";
                else if (lc == "cos")  name = "cos";
                else if (lc == "tan")  name = "tan";
                else if (lc == "sqrt") name = "sqrt";
                else if (lc == "abs")  name = "abs";
                else if (lc == "ln")   name = "ln";
                // NumOS log is base 10; Giac's log() is the NATURAL log.
                else if (lc == "log")  name = "log10";
                else return false;
                st.back() = std::string(name) + "(" + st.back() + ")";
                break;
            }
            case TokenType::Operator: {
                if (st.size() < 2) return false;
                char op;
                switch (t.op) {
                    case OperatorKind::Plus:  op = '+'; break;
                    case OperatorKind::Minus: op = '-'; break;
                    case OperatorKind::Mul:   op = '*'; break;
                    case OperatorKind::Div:   op = '/'; break;
                    case OperatorKind::Pow:   op = '^'; break;
                    default: return false;
                }
                std::string b = std::move(st.back()); st.pop_back();
                std::string a = std::move(st.back()); st.pop_back();
                st.push_back("(" + a + op + b + ")");
                break;
            }
            default:
                return false;   // LParen/RParen/Comma/Equal never reach RPN
        }
    }
    if (st.size() != 1) return false;
    out = std::move(st.back());
    return true;
}

bool GraphModel::compileGiacHandles(CartesianFunction& fn) {
    numos::GiacEngine& eng = numos::GiacEngine::instance();
    fn.giacF  = numos::CompiledExpression();
    fn.giacG  = numos::CompiledExpression();
    fn.giacFy = numos::CompiledExpression();
    fn.giacFValid = fn.giacGValid = fn.giacFyValid = false;
    if (!fn.valid) return false;
    ++fn.giacCompileCount;   // one compilation pass (test-hook counter)

    // Turn a Giac rejection of classifier-accepted text into a VISIBLE
    // invalid slot. An engine-reported unknown variable keeps its name so
    // the user sees "unknown: <ident>", everything else reads as syntax.
    auto rejectSlot = [&fn](const std::string& diag) {
        const char* kUnknown = "unknown variable: ";
        if (diag.compare(0, strlen(kUnknown), kUnknown) == 0) {
            fn.invalidReason = InvalidReason::UnknownIdent;
            snprintf(fn.reasonDetail, sizeof(fn.reasonDetail), "%s",
                     diag.c_str() + strlen(kUnknown));
        } else {
            fn.invalidReason = InvalidReason::Syntax;
        }
        fn.valid = false;
        return false;
    };

    if (!fn.implicit) {
        // Explicit y = f(x): fn.rpn holds f(x).
        std::string fx;
        if (!fn.rpnValid || fn.rpn.empty() || !rpnToGiacText(fn.rpn, fx))
            return rejectSlot("");
        fn.giacF = eng.compileNumeric(fx.c_str(), "x", /*strictVariables=*/true);
        if (!fn.giacF.valid()) return rejectSlot(fn.giacF.diagnostic());
        fn.giacFValid = true;
        return true;
    }

    // Implicit / inequality: residual G(x,y) = (lhs) - (rhs); an absent side
    // contributes exactly 0, mirroring the legacy evaluator.
    std::string l = "0", r = "0";
    if (fn.rpnLValid && !fn.rpnL.empty() && !rpnToGiacText(fn.rpnL, l))
        return rejectSlot("");
    if (fn.rpnValid && !fn.rpn.empty() && !rpnToGiacText(fn.rpn, r))
        return rejectSlot("");
    const std::string g = "(" + l + ")-(" + r + ")";
    fn.giacG = eng.compileNumeric2D(g.c_str(), "x", "y");
    if (!fn.giacG.valid()) return rejectSlot(fn.giacG.diagnostic());
    fn.giacGValid = true;

    // x = f(y) trace refinement: optional exactly like the legacy rpnY —
    // a failure downgrades the refinement (slot stays a plain contour).
    if (fn.explicitY && fn.rpnYValid && !fn.rpnY.empty()) {
        std::string fy;
        if (rpnToGiacText(fn.rpnY, fy)) {
            fn.giacFy = eng.compileNumeric(fy.c_str(), "y",
                                           /*strictVariables=*/true);
            fn.giacFyValid = fn.giacFy.valid();
        }
        if (!fn.giacFyValid) fn.explicitY = false;
    }
    return true;
}

bool GraphModel::ensureGiacFresh(CartesianFunction& fn) {
    if (!fn.valid) return false;
    const bool fFresh  = !fn.giacFValid  || fn.giacF.valid();
    const bool gFresh  = !fn.giacGValid  || fn.giacG.valid();
    const bool fyFresh = !fn.giacFyValid || fn.giacFy.valid();
    if (fFresh && gFresh && fyFresh &&
        (fn.implicit ? fn.giacGValid : fn.giacFValid))
        return true;
    // Handles orphaned by a GiacEngine reset (generation mismatch): one
    // deterministic recompile from the retained classifier RPNs.
    return compileGiacHandles(fn);
}
// ── preCacheRPN ────────────────────────────────────────────────────────────
//
// Classifies the committed expression per the classifier contract
// (NUMOS_GRAPHER_CLASSIFIER_CONTRACT.md, rules R0-R14; earlier rule wins):
//
//   pending serialiser verdict  → invalid (TooLong / Unsupported)     [R1]
//   ≥2 relation operators       → invalid "multiple relations"        [R9]
//   one '<' '>' '<=' '>='       → inequality region (both sides must
//                                 be present, compile and dry-run)    [R3,R7,R8,R11]
//   no operator                 → explicit y=<expr>, or implicit G=<expr>
//                                 when <expr> references y            [R4]
//   "y" = f(x) (either side)    → explicit y=f(x)                     [R5]
//   "x" = f(y) (either side)    → implicit contour + explicit-in-y
//                                 trace refinement (kind: explicitX)  [R6]
//   any other single '='        → implicit G = lhs - rhs              [R12]
//   relation with no x and no y → invalid "constant relation"         [R10]
//
// Every side that compiles is dry-run at (1,1); structural failures (unknown
// identifier such as "xy", malformed RPN) invalidate the slot VISIBLY instead
// of leaving a valid-but-blank plot. One-sided relations ("x=", "=x", "y<")
// are invalid instead of silently becoming "= 0".
void GraphModel::preCacheRPN(CartesianFunction& fn) {
    // Sync the numeric evaluator to the runtime angle mode before the
    // structural dry-run, so classification and plotting share DEG/RAD truth.
    _eval.setAngleMode(numos::legacyAngleMode());
    fn.rpnValid  = false;
    fn.rpnLValid = false;
    fn.implicit  = false;
    fn.relation  = Relation::Equal;
    fn.explicitY = false;
    fn.rpnYValid = false;
    fn.rpn.clear();
    fn.rpnL.clear();
    fn.rpnY.clear();
    fn.valid = false;
    fn.invalidReason  = InvalidReason::None;
    fn.reasonDetail[0] = '\0';
    // GIAC-C01: drop any previously compiled handles up front so an edited
    // slot can never sample a stale expression (rejected commits included).
    fn.giacF  = numos::CompiledExpression();
    fn.giacG  = numos::CompiledExpression();
    fn.giacFy = numos::CompiledExpression();
    fn.giacFValid = fn.giacGValid = fn.giacFyValid = false;

    if (fn.len <= 0) return;   // R0: empty — not classified, not invalid

    // R1: the serialiser already rejected this AST (overflow / unsupported node);
    // the text buffer may be truncated garbage, so never classify it.
    if (fn.serializeVerdict != InvalidReason::None) {
        fn.invalidReason = fn.serializeVerdict;
        return;
    }

    // R2: relation-operator census. '<=' / '>=' count as ONE operator; the
    // position and shape of the first one is kept for the split below.
    int         opCount   = 0;
    const char* opPos     = nullptr;
    char        opChar    = '\0';
    bool        opOrEqual = false;
    for (const char* p = fn.text; *p; ++p) {
        if (*p == '<' || *p == '>') {
            if (opCount++ == 0) {
                opPos = p; opChar = *p; opOrEqual = (p[1] == '=');
            }
            if (p[1] == '=') ++p;   // consume the '=' of '<=' / '>='
        } else if (*p == '=') {
            if (opCount++ == 0) {
                opPos = p; opChar = '='; opOrEqual = false;
            }
        }
    }

    // R9: chained/multiple relations have no defined semantics — reject loudly
    // ("y=x=2", "0<x<1") instead of evaluating to something surprising.
    if (opCount >= 2) {
        fn.invalidReason = InvalidReason::MultiRelation;
        return;
    }

    // R4: bare expression (no relation operator).
    if (opCount == 0) {
        const char* rhs = getExprRHS(fn.text);
        if (!rhs) { fn.invalidReason = InvalidReason::Syntax; return; }
        if (!referencesY(rhs)) {
            // Plain y = f(x).
            if (!compileExpr(rhs, fn.rpn)) { fn.invalidReason = InvalidReason::Syntax; return; }
            if (!dryRunStructural(fn.rpn, fn)) return;
            fn.rpnValid = true;
            fn.valid    = true;
            compileGiacHandles(fn);
            return;
        }
        // Implicit contour with an implicit "= 0": G = <expr>.
        if (!compileExpr(rhs, fn.rpnL)) { fn.invalidReason = InvalidReason::Syntax; return; }
        if (!dryRunStructural(fn.rpnL, fn)) return;
        fn.rpnLValid = true;
        fn.implicit  = true;
        fn.valid     = true;
        compileGiacHandles(fn);
        return;
    }

    // Single relation operator: split into lhs / rhs around it.
    std::string lhs = trimSpaces(std::string(fn.text, opPos - fn.text));
    std::string rhs = trimSpaces(std::string(opPos + (opOrEqual ? 2 : 1)));

    // R8: one-sided relations ("x=", "=x", "y<", "<x") are invalid. The old
    // behaviour compiled the present side and treated the missing one as 0,
    // silently plotting x=0 / shading y<0 — the flagship silent-wrong bug.
    if (lhs.empty() || rhs.empty()) {
        fn.invalidReason = InvalidReason::OneSided;
        return;
    }

    // R10 helper: a relation where NEITHER side references x or y ("1=2",
    // "pi<e") is a constant truth value, not a graph. Checked after compile
    // (uncompilable input stays "syntax") AND after the dry-run (an unknown
    // identifier like "xy" reports "unknown: xy", not "constant relation").
    auto isConstantRelation = [&]() {
        return !referencesX(lhs.c_str()) && !referencesY(lhs.c_str()) &&
               !referencesX(rhs.c_str()) && !referencesY(rhs.c_str());
    };

    // ── Inequality path (R3/R7/R11) ───────────────────────────────────────
    // Compile G(x,y)=lhs-rhs exactly like an implicit equation and remember the
    // comparison direction; the renderer shades the half-plane where the
    // relation holds and draws the G=0 boundary. `implicit` is set so the
    // y=f(x) consumers (trace/table) treat the slot as multi-valued.
    if (opChar == '<' || opChar == '>') {
        if (opChar == '<') fn.relation = opOrEqual ? Relation::LessEqual    : Relation::Less;
        else               fn.relation = opOrEqual ? Relation::GreaterEqual : Relation::Greater;
        if (!compileExpr(lhs.c_str(), fn.rpnL) || !compileExpr(rhs.c_str(), fn.rpn)) {
            fn.relation = Relation::Equal;
            fn.invalidReason = InvalidReason::Syntax;
            return;
        }
        if (!dryRunStructural(fn.rpnL, fn) || !dryRunStructural(fn.rpn, fn)) {
            fn.relation = Relation::Equal;
            return;
        }
        if (isConstantRelation()) {
            fn.relation = Relation::Equal;
            fn.invalidReason = InvalidReason::ConstantRelation;
            return;
        }
        fn.rpnLValid = true;
        fn.rpnValid  = true;
        fn.implicit  = true;
        fn.valid     = true;
        compileGiacHandles(fn);
        return;
    }

    // ── Equality path ──────────────────────────────────────────────────────
    const bool lhsHasY = referencesY(lhs.c_str());
    const bool rhsHasY = referencesY(rhs.c_str());

    // R5: explicit "y = f(x)" (either orientation), when the function side has
    // no y. Tested BEFORE the x=f(y) form so "x=y" classifies explicitY.
    if (isJustY(lhs) && !rhsHasY) {
        if (!compileExpr(rhs.c_str(), fn.rpn)) { fn.invalidReason = InvalidReason::Syntax; return; }
        if (!dryRunStructural(fn.rpn, fn)) return;
        fn.rpnValid = true;
        fn.valid    = true;
        compileGiacHandles(fn);
        return;
    }
    if (isJustY(rhs) && !lhsHasY) {
        if (!compileExpr(lhs.c_str(), fn.rpn)) { fn.invalidReason = InvalidReason::Syntax; return; }
        if (!dryRunStructural(fn.rpn, fn)) return;
        fn.rpnValid = true;
        fn.valid    = true;
        compileGiacHandles(fn);
        return;
    }

    // R12: general implicit relation G(x,y) = lhs - rhs. BOTH sides must
    // compile (a failed side is no longer silently treated as 0) and pass the
    // structural dry-run.
    if (!compileExpr(lhs.c_str(), fn.rpnL) || !compileExpr(rhs.c_str(), fn.rpn)) {
        fn.invalidReason = InvalidReason::Syntax;
        return;
    }
    if (!dryRunStructural(fn.rpnL, fn) || !dryRunStructural(fn.rpn, fn)) return;
    if (isConstantRelation()) {
        fn.invalidReason = InvalidReason::ConstantRelation;
        return;
    }
    fn.rpnLValid = true;
    fn.rpnValid  = true;
    fn.implicit  = true;
    fn.valid     = true;

    // R6: explicit-in-y refinement — "x = f(y)" (either orientation), when one
    // side is the bare variable x and the other has no x. The contour above
    // already plots it (pixel-identical to the pre-classifier renderer); here we
    // additionally compile the single-valued f(y) so the tracer can parametrise
    // the slot by y. The lhs==y / rhs==y explicit forms returned earlier, so x
    // can no longer be the dependent variable of a y=f(x) slot. Script-visible
    // kind for this shape: "explicitX".
    if (isJustX(lhs) && !referencesX(rhs.c_str())) {
        fn.explicitY = compileExpr(rhs.c_str(), fn.rpnY);
        fn.rpnYValid = fn.explicitY;
    } else if (isJustX(rhs) && !referencesX(lhs.c_str())) {
        fn.explicitY = compileExpr(lhs.c_str(), fn.rpnY);
        fn.rpnYValid = fn.explicitY;
    }
    compileGiacHandles(fn);   // giacG residual + optional giacFy refinement
}

// ── evalAt ─────────────────────────────────────────────────────────────────

float GraphModel::evalAt(CartesianFunction& fn, float x) {
    // The classifier Evaluator's mode doubles as the
    // emulator's debugAngleMode mirror (last-sync semantics, AM-01 smoke).
    _eval.setAngleMode(numos::legacyAngleMode());
    if (!fn.valid) return NAN;
    // Implicit relations are multi-valued in y; they have no single y=f(x).
    // Trace/table/auto-fit call evalAt and gracefully skip a NAN result.
    if (fn.implicit) return NAN;

    // GIAC-C01: retained-handle sampling; no reparse, no context writes.
    // Non-finite (pole) and non-numeric (domain/symbolic) outcomes are gaps
    // — never converted to 0.
    if (!ensureGiacFresh(fn) || !fn.giacFValid) return NAN;
    double y = NAN;
    if (!numos::GiacEngine::instance().evaluateNumeric(fn.giacF, (double)x, y))
        return NAN;
    if (std::isnan(y) || std::isinf(y)) return NAN;
    return (float)y;
}

// ── evalImplicit: G(x,y) = lhs(x,y) - rhs(x,y) ─────────────────────────────

float GraphModel::evalImplicit(CartesianFunction& fn, float x, float y) {
    _eval.setAngleMode(numos::legacyAngleMode());
    if (!fn.implicit) return NAN;

    // GIAC-C01: the retained residual handle covers contour drawing,
    // inequality shading, implicit trace, gradients and 2-D intersections —
    // one evaluator for every consumer. Simultaneous x,y substitution; the
    // shared context is never written.
    if (!ensureGiacFresh(fn) || !fn.giacGValid) return NAN;
    double g = NAN;
    if (!numos::GiacEngine::instance().evaluateNumeric2D(fn.giacG, (double)x,
                                                         (double)y, g))
        return NAN;
    if (std::isnan(g) || std::isinf(g)) return NAN;
    return (float)g;
}

// ── evalAtY: x = f(y) for an explicit-in-y slot ────────────────────────────

float GraphModel::evalAtY(CartesianFunction& fn, float y) {
    _eval.setAngleMode(numos::legacyAngleMode());
    if (!fn.explicitY || !fn.rpnYValid || fn.rpnY.empty()) return NAN;
    if (!ensureGiacFresh(fn) || !fn.giacFyValid) return NAN;
    double xv = NAN;
    if (!numos::GiacEngine::instance().evaluateNumeric(fn.giacFy, (double)y,
                                                       xv))
        return NAN;
    if (std::isnan(xv) || std::isinf(xv)) return NAN;
    return (float)xv;
}

// ── Difference function: f1(x) - f2(x) ────────────────────────────────────

float GraphModel::diffAt(CartesianFunction& fn1, CartesianFunction& fn2, float x) {
    float y1 = evalAt(fn1, x);
    float y2 = evalAt(fn2, x);
    if (std::isnan(y1) || std::isnan(y2)) return NAN;
    return y1 - y2;
}

// ── Bisection root finder ──────────────────────────────────────────────────

float GraphModel::bisect(CartesianFunction& fn1, CartesianFunction& fn2,
                         float a, float b, int maxIter) {
    float fa = diffAt(fn1, fn2, a);
    float fb = diffAt(fn1, fn2, b);

    // Check if signs differ and are non-nan
    if (std::isnan(fa) || std::isnan(fb)) return NAN;
    if ((fa > 0.0f && fb > 0.0f) || (fa < 0.0f && fb < 0.0f)) return NAN;

    float tolerance = 1e-5f;

    for (int i = 0; i < maxIter; ++i) {
        float c = (a + b) / 2.0f;
        float fc = diffAt(fn1, fn2, c);

        if (std::isnan(fc)) return NAN;
        if (std::abs(fc) < tolerance) return c;

        if ((fa > 0.0f && fc < 0.0f) || (fa < 0.0f && fc > 0.0f)) {
            b = c;
            fb = fc;
        } else {
            a = c;
            fa = fc;
        }

        if (std::abs(b - a) < tolerance) return c;
    }

    return (a + b) / 2.0f;
}

// ── Find intersections ─────────────────────────────────────────────────────

std::vector<PointOfInterest> GraphModel::findIntersections(
    CartesianFunction& fn1,
    CartesianFunction& fn2,
    float xMin, float xMax,
    int maxPoints,
    int maxIterations)
{
    std::vector<PointOfInterest> results;

    if (!fn1.valid || !fn2.valid) return results;
    if (xMin >= xMax) return results;

    // Adaptive sampling: look for sign changes in f1(x) - f2(x)
    int samples = 100;  // Initial grid for sign change detection
    float step = (xMax - xMin) / samples;

    float prevX = xMin;
    float prevDiff = diffAt(fn1, fn2, prevX);

    for (int i = 1; i <= samples && (int)results.size() < maxPoints; ++i) {
        float currX = xMin + i * step;
        float currDiff = diffAt(fn1, fn2, currX);

        // Skip NaN values
        if (std::isnan(prevDiff) || std::isnan(currDiff)) {
            prevX = currX;
            prevDiff = currDiff;
            continue;
        }

        // Check for sign change (intersection point exists here)
        if ((prevDiff > 0.0f && currDiff < 0.0f) || (prevDiff < 0.0f && currDiff > 0.0f)) {
            // Refine with bisection
            float xIntersect = bisect(fn1, fn2, prevX, currX, maxIterations);

            if (!std::isnan(xIntersect)) {
                float yIntersect = evalAt(fn1, xIntersect);

                // Avoid duplicates: check if this root is far from previous ones
                bool isDuplicate = false;
                for (const auto& poi : results) {
                    if (std::abs(poi.x - xIntersect) < 0.01f) {
                        isDuplicate = true;
                        break;
                    }
                }

                if (!isDuplicate) {
                    PointOfInterest poi;
                    poi.x = xIntersect;
                    poi.y = yIntersect;
                    snprintf(poi.label, sizeof(poi.label), "Intersection");
                    results.push_back(poi);
                }
            }
        }

        prevX = currX;
        prevDiff = currDiff;
    }

    return results;
}

} // namespace grapher

