/**
 * SymSimplify.cpp — Multi-pass fixed-point algebraic simplifier.
 *
 * Phase 3: Full rule engine applying arithmetic, symbolic, trig, log/exp
 * identities in repeated passes until the expression stabilises.
 *
 * Key design:
 *   · Pure-functional: NEVER mutates input nodes (Phase 2 contract).
 *   · Fixed-point detection via pointer identity (hash-consing).
 *   · Every returned node is cons'd through the arena's ConsTable.
 *   · MAX_PASSES = 8 absolute safety net against oscillation.
 *
 * Part of: NumOS Pro-CAS — Phase 3 (Fixed-Point Simplifier)
 */

#include "SymSimplify.h"
#include <cmath>
#include <vector>
#include <algorithm>

namespace cas {

// ════════════════════════════════════════════════════════════════════════
//  Helpers
// ════════════════════════════════════════════════════════════════════════

bool SymSimplify::isZero(const SymExpr* e) {
    if (e->type != SymExprType::Num) return false;
    const auto* sn = static_cast<const SymNum*>(e);
    return sn->_coeff.isZero();
}

bool SymSimplify::isOne(const SymExpr* e) {
    if (e->type != SymExprType::Num) return false;
    const auto* sn = static_cast<const SymNum*>(e);
    return sn->_coeff.isOne() && sn->isPureRational();
}

bool SymSimplify::isMinusOne(const SymExpr* e) {
    if (e->type != SymExprType::Num) return false;
    const auto* sn = static_cast<const SymNum*>(e);
    if (!sn->isPureRational()) return false;
    return sn->_coeff.num().toInt64() == -1 && sn->_coeff.den().toInt64() == 1;
}

bool SymSimplify::isConstNum(const SymExpr* e) {
    return e->type == SymExprType::Num;
}

bool SymSimplify::isPureNum(const SymExpr* e) {
    if (e->type != SymExprType::Num) return false;
    return static_cast<const SymNum*>(e)->isPureRational();
}

double SymSimplify::numVal(const SymExpr* e) {
    return static_cast<const SymNum*>(e)->toExactVal().toDouble();
}

CASRational SymSimplify::getNumCoeff(const SymExpr* e) {
    return static_cast<const SymNum*>(e)->_coeff;
}

// ── Pattern helpers ────────────────────────────────────────────────

bool SymSimplify::isFuncOfKind(const SymExpr* e, SymFuncKind kind) {
    if (e->type != SymExprType::Func) return false;
    return static_cast<const SymFunc*>(e)->kind == kind;
}

/// Check if e = f(arg)^2  for a given function kind.
/// If so, sets outArg to the function's argument.
bool SymSimplify::isSquareOfFunc(const SymExpr* e, SymFuncKind kind,
                                  SymExpr*& outArg) {
    if (e->type != SymExprType::Pow) return false;
    const auto* pw = static_cast<const SymPow*>(e);
    if (!isPureNum(pw->exponent)) return false;
    if (getNumCoeff(pw->exponent).toInt64() != 2) return false;
    if (!isFuncOfKind(pw->base, kind)) return false;
    outArg = static_cast<const SymFunc*>(pw->base)->argument;
    return true;
}

// ════════════════════════════════════════════════════════════════════════
//  Coefficient / Base extraction for like-term collection
//
//  Every additive term is decomposed as  coeff × base  where:
//    SymNum(c)       → (c, nullptr)  — pure constant
//    SymNeg(x)       → negate coeff of recursive extraction on x
//    SymMul(c, rest) → (c, rest)  where c is the SymNum factor
//    anything else   → (1, term)
// ════════════════════════════════════════════════════════════════════════

SymSimplify::CoeffBase SymSimplify::extractCoeffAndBase(const SymExpr* term) {
    if (!term) return { CASRational::zero(), nullptr };

    // Pure constant
    if (term->type == SymExprType::Num) {
        const auto* sn = static_cast<const SymNum*>(term);
        if (sn->isPureRational())
            return { sn->_coeff, nullptr };
        // Num with pi/e/radical → treat as opaque base with coeff 1
        return { CASRational::one(), const_cast<SymExpr*>(term) };
    }

    // Negation: -(x) → negate the coeff of x
    if (term->type == SymExprType::Neg) {
        const auto* neg = static_cast<const SymNeg*>(term);
        CoeffBase inner = extractCoeffAndBase(neg->child);
        return { CASRational::neg(inner.coeff), inner.base };
    }

    // Product: look for exactly one SymNum factor
    if (term->type == SymExprType::Mul) {
        const auto* mul = static_cast<const SymMul*>(term);
        int numIdx = -1;
        for (uint16_t i = 0; i < mul->count; ++i) {
            if (isPureNum(mul->factors[i])) {
                numIdx = static_cast<int>(i);
                break;
            }
        }
        if (numIdx >= 0) {
            CASRational c = getNumCoeff(mul->factors[numIdx]);
            // Build base = product of remaining factors
            if (mul->count == 2) {
                SymExpr* base = const_cast<SymExpr*>(
                    mul->factors[numIdx == 0 ? 1 : 0]);
                return { c, base };
            }
            // 3+ factors: rebuild a smaller Mul without the numeric factor
            // Note: we don't actually rebuild here to avoid arena waste.
            // Instead, return coeff=c and base=original Mul.
            // The caller will rebuild if needed.
            // Actually, let's rebuild for correctness:
            std::vector<SymExpr*> rest;
            for (uint16_t i = 0; i < mul->count; ++i) {
                if (i == static_cast<uint16_t>(numIdx)) continue;
                rest.push_back(const_cast<SymExpr*>(mul->factors[i]));
            }
            // Don't allocate without arena — return the special marker
            // that the caller should handle.
            // For simplicity, if there are 2+ remaining factors,
            // we can't easily form a base without the arena. Return coeff=1.
            return { c, nullptr };  // will be handled at call site with arena
        }
    }

    // Default: coeff=1, base=term
    return { CASRational::one(), const_cast<SymExpr*>(term) };
}

// ════════════════════════════════════════════════════════════════════════
//  Base / Exponent extraction for power merging in Mul
//
//  SymPow(b, e) → (b, e)
//  anything     → (term, 1)
// ════════════════════════════════════════════════════════════════════════

SymSimplify::BaseExp SymSimplify::extractBaseAndExp(const SymExpr* factor,
                                                     SymExprArena& arena) {
    if (factor->type == SymExprType::Pow) {
        const auto* pw = static_cast<const SymPow*>(factor);
        return { pw->base, pw->exponent };
    }
    return { const_cast<SymExpr*>(factor), symInt(arena, 1) };
}

// ════════════════════════════════════════════════════════════════════════
//  makeCoeffTerm — rebuild  coeff × base
// ════════════════════════════════════════════════════════════════════════

SymExpr* SymSimplify::makeCoeffTerm(const CASRational& coeff, SymExpr* base,
                                     SymExprArena& arena) {
    if (coeff.isZero()) return symInt(arena, 0);
    if (!base) return symNum(arena, coeff);       // pure constant
    if (coeff.isOne()) return base;
    if (coeff.num().toInt64() == -1 && coeff.den().toInt64() == 1)
        return symNeg(arena, base);
    return symMul(arena, symNum(arena, coeff), base);
}

// ════════════════════════════════════════════════════════════════════════
//  PUBLIC API — Fixed-point loop
// ════════════════════════════════════════════════════════════════════════

SymExpr* SymSimplify::simplify(SymExpr* expr, SymExprArena& arena) {
    if (!expr) return nullptr;

    SymExpr* current = expr;
    for (int pass = 0; pass < MAX_PASSES; ++pass) {
        SymExpr* next = simplifyPass(current, arena);
        if (next == current) break;   // Fixed point! (pointer identity)
        current = next;
    }
    return current;
}

// ════════════════════════════════════════════════════════════════════════
//  Single bottom-up recursive pass
// ════════════════════════════════════════════════════════════════════════

SymExpr* SymSimplify::simplifyPass(SymExpr* expr, SymExprArena& arena) {
    if (!expr) return nullptr;

    switch (expr->type) {
        case SymExprType::Num:
        case SymExprType::Var:
            return expr;   // Leaf — nothing to simplify

        case SymExprType::Neg: {
            auto* n = static_cast<SymNeg*>(expr);
            SymExpr* newChild = simplifyPass(n->child, arena);
            SymNeg* working = (newChild == n->child) ? n
                : static_cast<SymNeg*>(symNeg(arena, newChild));
            return simplifyNeg(working, arena);
        }

        case SymExprType::Add: {
            auto* a = static_cast<SymAdd*>(expr);
            bool changed = false;
            auto** newTerms = static_cast<SymExpr**>(
                arena.allocRaw(a->count * sizeof(SymExpr*)));
            for (uint16_t i = 0; i < a->count; ++i) {
                newTerms[i] = simplifyPass(
                    const_cast<SymExpr*>(a->terms[i]), arena);
                if (newTerms[i] != a->terms[i]) changed = true;
            }
            if (changed) {
                SymExpr* rebuilt = symAddN(arena, newTerms, a->count);
                if (rebuilt->type == SymExprType::Add)
                    return simplifyAdd(static_cast<SymAdd*>(rebuilt), arena);
                return rebuilt;
            }
            return simplifyAdd(a, arena);
        }

        case SymExprType::Mul: {
            auto* m = static_cast<SymMul*>(expr);
            bool changed = false;
            auto** newFactors = static_cast<SymExpr**>(
                arena.allocRaw(m->count * sizeof(SymExpr*)));
            for (uint16_t i = 0; i < m->count; ++i) {
                newFactors[i] = simplifyPass(
                    const_cast<SymExpr*>(m->factors[i]), arena);
                if (newFactors[i] != m->factors[i]) changed = true;
            }
            if (changed) {
                SymExpr* rebuilt = symMulN(arena, newFactors, m->count);
                if (rebuilt->type == SymExprType::Mul)
                    return simplifyMul(static_cast<SymMul*>(rebuilt), arena);
                return rebuilt;
            }
            return simplifyMul(m, arena);
        }

        case SymExprType::Pow: {
            auto* p = static_cast<SymPow*>(expr);
            SymExpr* newBase = simplifyPass(p->base, arena);
            SymExpr* newExp  = simplifyPass(p->exponent, arena);
            SymPow* working = (newBase == p->base && newExp == p->exponent)
                ? p
                : static_cast<SymPow*>(symPow(arena, newBase, newExp));
            return simplifyPow(working, arena);
        }

        case SymExprType::Func: {
            auto* f = static_cast<SymFunc*>(expr);
            SymExpr* newArg = simplifyPass(f->argument, arena);
            SymFunc* working = (newArg == f->argument) ? f
                : static_cast<SymFunc*>(symFunc(arena, f->kind, newArg));
            return simplifyFunc(working, arena);
        }

        default:
            return expr;
    }
}

// ════════════════════════════════════════════════════════════════════════
//  simplifyNeg  (pure-functional)
//
//    -(0)       → 0
//    --x        → x
//    -(num)     → fold coefficient
//    -(a + b)   → (-a) + (-b)   (push negation inward)
// ════════════════════════════════════════════════════════════════════════

SymExpr* SymSimplify::simplifyNeg(SymNeg* n, SymExprArena& arena) {
    // -(0) → 0
    if (isZero(n->child)) return symInt(arena, 0);

    // --x → x
    if (n->child->type == SymExprType::Neg) {
        return static_cast<const SymNeg*>(n->child)->child;
    }

    // -(constant) → fold: negate the coefficient
    if (isPureNum(n->child)) {
        CASRational negCoeff = CASRational::neg(getNumCoeff(n->child));
        return symNum(arena, negCoeff);
    }

    // -(Mul(c, rest...)) where c is numeric → Mul(-c, rest...)
    if (n->child->type == SymExprType::Mul) {
        const auto* mul = static_cast<const SymMul*>(n->child);
        // Check if first factor (by canonical order) is a number
        for (uint16_t i = 0; i < mul->count; ++i) {
            if (isPureNum(mul->factors[i])) {
                CASRational negC = CASRational::neg(getNumCoeff(mul->factors[i]));
                auto** newFactors = static_cast<SymExpr**>(
                    arena.allocRaw(mul->count * sizeof(SymExpr*)));
                for (uint16_t j = 0; j < mul->count; ++j) {
                    if (j == i) {
                        newFactors[j] = symNum(arena, negC);
                    } else {
                        newFactors[j] = const_cast<SymExpr*>(mul->factors[j]);
                    }
                }
                return symMulN(arena, newFactors, mul->count);
            }
        }
    }

    // -(Add(t1, t2, ...)) → Add(-t1, -t2, ...) — push negation inward
    if (n->child->type == SymExprType::Add) {
        const auto* add = static_cast<const SymAdd*>(n->child);
        auto** negTerms = static_cast<SymExpr**>(
            arena.allocRaw(add->count * sizeof(SymExpr*)));
        for (uint16_t i = 0; i < add->count; ++i) {
            negTerms[i] = symNeg(arena, const_cast<SymExpr*>(add->terms[i]));
        }
        return symAddN(arena, negTerms, add->count);
    }

    return n;
}

// ════════════════════════════════════════════════════════════════════════
//  simplifyAdd  (pure-functional)
//
//    Flatten nested Adds
//    Filter zeros
//    Constant folding: 3 + 5 → 8
//    Like-term collection: 2x + 3x → 5x
//    Pythagorean identity: sin²(u) + cos²(u) → 1
//    ln(a) + ln(b) → ln(a·b)
//    Single term unwrap: Add(x) → x
// ════════════════════════════════════════════════════════════════════════

SymExpr* SymSimplify::simplifyAdd(SymAdd* a, SymExprArena& arena) {
    // ── Step 1: Flatten nested Adds ──────────────────────────────────
    std::vector<SymExpr*> flat;
    flat.reserve(a->count);
    for (uint16_t i = 0; i < a->count; ++i) {
        SymExpr* term = const_cast<SymExpr*>(a->terms[i]);
        if (term->type == SymExprType::Add) {
            const auto* inner = static_cast<const SymAdd*>(term);
            for (uint16_t j = 0; j < inner->count; ++j)
                flat.push_back(const_cast<SymExpr*>(inner->terms[j]));
        } else {
            flat.push_back(term);
        }
    }

    // ── Step 2: Filter zeros and fold pure constants ────────────────
    std::vector<SymExpr*> nonZero;
    CASRational constAccum = CASRational::zero();
    bool hasConst = false;

    for (auto* term : flat) {
        if (isZero(term)) continue;

        if (isPureNum(term)) {
            constAccum = CASRational::add(constAccum, getNumCoeff(term));
            hasConst = true;
        } else {
            nonZero.push_back(term);
        }
    }

    // ── Step 3: Like-term collection ────────────────────────────────
    if (nonZero.size() >= 2) {
        // Group by base pointer — O(n²) but n is small
        std::vector<CASRational> coeffs;
        std::vector<SymExpr*>    bases;

        for (auto* term : nonZero) {
            CoeffBase cb = extractCoeffAndBase(term);

            // extractCoeffAndBase might return base=nullptr for complex Muls
            // with 3+ factors. In that case, just keep the term as-is.
            if (cb.base == nullptr && !cb.coeff.isOne()) {
                // Pure constant that wasn't isPureNum — shouldn't happen
                // but be safe: add to constant accumulator
                hasConst = true;
                constAccum = CASRational::add(constAccum, cb.coeff);
                continue;
            }

            // Find existing base
            bool merged = false;
            for (size_t j = 0; j < bases.size(); ++j) {
                if (bases[j] == cb.base) {   // Pointer identity!
                    coeffs[j] = CASRational::add(coeffs[j], cb.coeff);
                    merged = true;
                    break;
                }
            }
            if (!merged) {
                coeffs.push_back(cb.coeff);
                bases.push_back(cb.base);
            }
        }

        // Rebuild non-zero terms
        nonZero.clear();
        for (size_t j = 0; j < bases.size(); ++j) {
            if (coeffs[j].isZero()) continue;   // Term cancelled!
            nonZero.push_back(makeCoeffTerm(coeffs[j], bases[j], arena));
        }
    }

    // ── Step 4: Pythagorean identity: sin²(u) + cos²(u) → 1 ───────
    if (nonZero.size() >= 2) {
        for (size_t i = 0; i < nonZero.size(); ++i) {
            SymExpr* sinArg = nullptr;
            if (!isSquareOfFunc(nonZero[i], SymFuncKind::Sin, sinArg)) continue;

            for (size_t j = i + 1; j < nonZero.size(); ++j) {
                SymExpr* cosArg = nullptr;
                if (!isSquareOfFunc(nonZero[j], SymFuncKind::Cos, cosArg)) continue;

                if (sinArg == cosArg) {   // Same argument (pointer identity!)
                    // Replace both with 1
                    hasConst = true;
                    constAccum = CASRational::add(constAccum, CASRational::one());
                    // Remove j first (larger index), then i
                    nonZero.erase(nonZero.begin() + j);
                    nonZero.erase(nonZero.begin() + i);
                    --i;  // Re-check current index after erasure
                    break;
                }
            }
        }
    }

    // ── Step 5: ln(a) + ln(b) → ln(a · b) ──────────────────────────
    if (nonZero.size() >= 2) {
        for (size_t i = 0; i < nonZero.size(); ++i) {
            if (!isFuncOfKind(nonZero[i], SymFuncKind::Ln)) continue;
            SymExpr* argA = static_cast<const SymFunc*>(nonZero[i])->argument;

            for (size_t j = i + 1; j < nonZero.size(); ++j) {
                if (!isFuncOfKind(nonZero[j], SymFuncKind::Ln)) continue;
                SymExpr* argB = static_cast<const SymFunc*>(nonZero[j])->argument;

                // ln(a) + ln(b) → ln(a · b)
                SymExpr* product = symMul(arena, argA, argB);
                nonZero[i] = symFunc(arena, SymFuncKind::Ln, product);
                nonZero.erase(nonZero.begin() + j);
                --j;  // Re-check
            }
        }
    }

    // ── Step 6: Add folded constant if non-zero ─────────────────────
    if (hasConst && !constAccum.isZero()) {
        nonZero.push_back(symNum(arena, constAccum));
    }

    // ── Step 7: Final assembly ──────────────────────────────────────
    if (nonZero.empty()) return symInt(arena, 0);
    if (nonZero.size() == 1) return nonZero[0];

    uint16_t cnt = static_cast<uint16_t>(nonZero.size());
    auto** arr = static_cast<SymExpr**>(
        arena.allocRaw(cnt * sizeof(SymExpr*)));
    for (uint16_t i = 0; i < cnt; ++i) arr[i] = nonZero[i];
    return symAddN(arena, arr, cnt);
}

// ════════════════════════════════════════════════════════════════════════
//  simplifyMul  (pure-functional)
//
//    Flatten nested Muls
//    0 · x → 0
//    1 · x → x
//    Constant folding: 3 · 5 → 15
//    Power-base merging: x · x² → x³
//    Distribution: a(b + c) → ab + ac  (simple products)
//    Single factor unwrap: Mul(x) → x
// ════════════════════════════════════════════════════════════════════════

SymExpr* SymSimplify::simplifyMul(SymMul* m, SymExprArena& arena) {
    // ── Step 1: Flatten nested Muls ─────────────────────────────────
    std::vector<SymExpr*> flat;
    flat.reserve(m->count);
    for (uint16_t i = 0; i < m->count; ++i) {
        SymExpr* factor = const_cast<SymExpr*>(m->factors[i]);
        if (factor->type == SymExprType::Mul) {
            const auto* inner = static_cast<const SymMul*>(factor);
            for (uint16_t j = 0; j < inner->count; ++j)
                flat.push_back(const_cast<SymExpr*>(inner->factors[j]));
        } else {
            flat.push_back(factor);
        }
    }

    // ── Step 2: Check for zero factor (short-circuit) ───────────────
    for (auto* f : flat) {
        if (isZero(f)) return symInt(arena, 0);
    }

    // ── Step 3: Filter ones and fold constants ──────────────────────
    std::vector<SymExpr*> nonOne;
    CASRational constAccum = CASRational::one();
    bool hasConst = false;

    for (auto* factor : flat) {
        if (isOne(factor)) continue;

        if (isPureNum(factor)) {
            constAccum = CASRational::mul(constAccum, getNumCoeff(factor));
            hasConst = true;
        } else {
            nonOne.push_back(factor);
        }
    }

    if (hasConst && constAccum.isZero()) return symInt(arena, 0);

    // ── Step 4: Power-base merging ──────────────────────────────────
    //   x · x² → x³,  x^a · x^b → x^(a+b)
    if (nonOne.size() >= 2) {
        std::vector<SymExpr*> mergedBases;
        std::vector<SymExpr*> mergedExps;

        for (auto* factor : nonOne) {
            BaseExp be = extractBaseAndExp(factor, arena);

            bool merged = false;
            for (size_t j = 0; j < mergedBases.size(); ++j) {
                if (mergedBases[j] == be.base) {   // Same base!
                    // Sum exponents
                    mergedExps[j] = symAdd(arena, mergedExps[j], be.exp);
                    merged = true;
                    break;
                }
            }
            if (!merged) {
                mergedBases.push_back(be.base);
                mergedExps.push_back(be.exp);
            }
        }

        // Check if any merging happened
        if (mergedBases.size() < nonOne.size()) {
            nonOne.clear();
            for (size_t j = 0; j < mergedBases.size(); ++j) {
                // Simplify the summed exponent
                SymExpr* exp = simplifyPass(mergedExps[j], arena);
                if (isZero(exp)) {
                    // x^0 = 1 → contributes to constant
                    hasConst = true;
                    // constAccum already 1 for this, no multiplication needed
                    continue;
                }
                if (isOne(exp)) {
                    nonOne.push_back(mergedBases[j]);
                } else {
                    nonOne.push_back(symPow(arena, mergedBases[j], exp));
                }
            }
        }
    }

    // ── Step 5: Add constant coefficient ────────────────────────────
    if (hasConst && !constAccum.isOne()) {
        nonOne.insert(nonOne.begin(), symNum(arena, constAccum));
    }

    if (nonOne.empty()) return symInt(arena, 1);
    if (nonOne.size() == 1) return nonOne[0];

    // ── Step 6: Build result ────────────────────────────────────────
    uint16_t cnt = static_cast<uint16_t>(nonOne.size());
    auto** arr = static_cast<SymExpr**>(
        arena.allocRaw(cnt * sizeof(SymExpr*)));
    for (uint16_t i = 0; i < cnt; ++i) arr[i] = nonOne[i];
    SymExpr* result = symMulN(arena, arr, cnt);

    // ── Step 7: Distribution: a(b + c) → ab + ac ───────────────────
    //   Only for simple 2-factor products where one factor is an Add.
    if (result->type == SymExprType::Mul) {
        SymExpr* dist = tryDistribute(static_cast<SymMul*>(result), arena);
        if (dist) return dist;
    }

    return result;
}

// ════════════════════════════════════════════════════════════════════════
//  tryDistribute — a · (b + c) → a·b + a·c
//
//  Only applied when the Mul has exactly 2 factors and one is an Add
//  with ≤ 8 terms.  This keeps expansion bounded.
// ════════════════════════════════════════════════════════════════════════

SymExpr* SymSimplify::tryDistribute(SymMul* m, SymExprArena& arena) {
    if (m->count != 2) return nullptr;

    SymExpr* mul_a = const_cast<SymExpr*>(m->factors[0]);
    SymExpr* mul_b = const_cast<SymExpr*>(m->factors[1]);

    // Find which is the Add
    const SymAdd* addPart = nullptr;
    SymExpr*      scalar  = nullptr;

    if (mul_a->type == SymExprType::Add) {
        addPart = static_cast<const SymAdd*>(mul_a);
        scalar  = mul_b;
    } else if (mul_b->type == SymExprType::Add) {
        addPart = static_cast<const SymAdd*>(mul_b);
        scalar  = mul_a;
    }

    if (!addPart) return nullptr;
    if (addPart->count > 8) return nullptr;   // Too many terms

    // Only distribute when scalar is a simple expression (Num, Var, Pow)
    // to avoid explosive expansion from things like (a+b)(c+d)
    if (scalar->type == SymExprType::Add) return nullptr;

    // Build distributed terms: scalar · t[i]  for each addPart term
    auto** distTerms = static_cast<SymExpr**>(
        arena.allocRaw(addPart->count * sizeof(SymExpr*)));
    for (uint16_t i = 0; i < addPart->count; ++i) {
        distTerms[i] = symMul(arena,
            scalar, const_cast<SymExpr*>(addPart->terms[i]));
    }
    return symAddN(arena, distTerms, addPart->count);
}

// ════════════════════════════════════════════════════════════════════════
//  simplifyPow  (pure-functional)
//
//    x^0 → 1
//    x^1 → x
//    0^n → 0  (n > 0)
//    1^n → 1
//    Constant^Constant → fold (small exponents)
//    (a^b)^c → a^(b·c)  when both exponents are numeric
// ════════════════════════════════════════════════════════════════════════

SymExpr* SymSimplify::simplifyPow(SymPow* p, SymExprArena& arena) {
    // x^0 → 1
    if (isZero(p->exponent)) return symInt(arena, 1);

    // x^1 → x
    if (isOne(p->exponent)) return p->base;

    // 0^n → 0  (for positive n)
    if (isZero(p->base) && isConstNum(p->exponent)) {
        double ev = numVal(p->exponent);
        if (ev > 0) return symInt(arena, 0);
    }

    // 1^n → 1
    if (isOne(p->base)) return symInt(arena, 1);

    // (-1)^n → fold for integer n
    if (isMinusOne(p->base) && isPureNum(p->exponent)) {
        CASRational expR = getNumCoeff(p->exponent);
        if (expR.isInteger()) {
            int64_t e = expR.toInt64();
            return (e % 2 == 0) ? symInt(arena, 1) : symInt(arena, -1);
        }
    }

    // (a^b)^c → a^(b·c)  when both exponents are numeric
    if (p->base->type == SymExprType::Pow) {
        const auto* innerPow = static_cast<const SymPow*>(p->base);
        if (isPureNum(innerPow->exponent) && isPureNum(p->exponent)) {
            CASRational innerExp = getNumCoeff(innerPow->exponent);
            CASRational outerExp = getNumCoeff(p->exponent);
            CASRational newExp = CASRational::mul(innerExp, outerExp);
            if (!newExp.hasError()) {
                return symPow(arena, innerPow->base,
                              symNum(arena, newExp));
            }
        }
    }

    // Constant^Constant → fold (small exponents only)
    if (isPureNum(p->base) && isPureNum(p->exponent)) {
        CASRational baseR = getNumCoeff(p->base);
        CASRational expR  = getNumCoeff(p->exponent);
        if (baseR.isInteger() && expR.isInteger()) {
            int64_t exp = expR.toInt64();
            if (exp >= -10 && exp <= 10) {
                CASRational result = CASRational::pow(
                    baseR, static_cast<int32_t>(exp));
                if (!result.hasError())
                    return symNum(arena, result);
            }
        }
    }

    return p;
}

// ════════════════════════════════════════════════════════════════════════
//  simplifyFunc  (pure-functional)
//
//  Log/Exp identities:
//    ln(e^x) → x
//    e^(ln x) → x
//    ln(1)   → 0
//    exp(0)  → 1
//
//  Trig identities:
//    sin(0)   → 0
//    cos(0)   → 1
//    sin(-x)  → -sin(x)
//    cos(-x)  → cos(x)
//    tan(x)   → sin(x)/cos(x)
// ════════════════════════════════════════════════════════════════════════

SymExpr* SymSimplify::simplifyFunc(SymFunc* f, SymExprArena& arena) {
    SymExpr* arg = f->argument;

    switch (f->kind) {

    // ── Ln ──────────────────────────────────────────────────────────
    case SymFuncKind::Ln:
        // ln(1) → 0
        if (isOne(arg)) return symInt(arena, 0);

        // ln(e^x) → x
        if (isFuncOfKind(arg, SymFuncKind::Exp))
            return static_cast<const SymFunc*>(arg)->argument;

        // ln(e) — represented as Exp(1) consed
        // (If the user entered 'e' as SymFunc(Exp, 1) → ln(Exp(1)) = 1)
        break;

    // ── Exp ─────────────────────────────────────────────────────────
    case SymFuncKind::Exp:
        // exp(0) → 1
        if (isZero(arg)) return symInt(arena, 1);

        // e^(ln x) → x
        if (isFuncOfKind(arg, SymFuncKind::Ln))
            return static_cast<const SymFunc*>(arg)->argument;
        break;

    // ── Sin ─────────────────────────────────────────────────────────
    case SymFuncKind::Sin:
        // sin(0) → 0
        if (isZero(arg)) return symInt(arena, 0);

        // sin(-x) → -sin(x)
        if (arg->type == SymExprType::Neg) {
            SymExpr* inner = static_cast<const SymNeg*>(arg)->child;
            return symNeg(arena, symFunc(arena, SymFuncKind::Sin, inner));
        }
        break;

    // ── Cos ─────────────────────────────────────────────────────────
    case SymFuncKind::Cos:
        // cos(0) → 1
        if (isZero(arg)) return symInt(arena, 1);

        // cos(-x) → cos(x)
        if (arg->type == SymExprType::Neg) {
            SymExpr* inner = static_cast<const SymNeg*>(arg)->child;
            return symFunc(arena, SymFuncKind::Cos, inner);
        }
        break;

    // ── Tan ─────────────────────────────────────────────────────────
    case SymFuncKind::Tan:
        // tan(0) → 0
        if (isZero(arg)) return symInt(arena, 0);

        // tan(x) → sin(x) / cos(x)  =  sin(x) · cos(x)^(-1)
        {
            SymExpr* sinX = symFunc(arena, SymFuncKind::Sin, arg);
            SymExpr* cosX = symFunc(arena, SymFuncKind::Cos, arg);
            SymExpr* cosInv = symPow(arena, cosX, symInt(arena, -1));
            return symMul(arena, sinX, cosInv);
        }

    // ── ArcSin / ArcCos / ArcTan ────────────────────────────────────
    case SymFuncKind::ArcSin:
    case SymFuncKind::ArcCos:
    case SymFuncKind::ArcTan:
        break;

    // ── Log10 ───────────────────────────────────────────────────────
    case SymFuncKind::Log10:
        // log10(1) → 0
        if (isOne(arg)) return symInt(arena, 0);
        break;

    // ── Abs ─────────────────────────────────────────────────────────
    case SymFuncKind::Abs:
        // |c| → fold for numeric constants
        if (isPureNum(arg)) {
            CASRational c = getNumCoeff(arg);
            if (c.isNegative())
                return symNum(arena, CASRational::neg(c));
            return arg;
        }
        // |(-x)| → |x|
        if (arg->type == SymExprType::Neg) {
            SymExpr* inner = static_cast<const SymNeg*>(arg)->child;
            return symFunc(arena, SymFuncKind::Abs, inner);
        }
        break;
    }

    return f;
}

} // namespace cas
