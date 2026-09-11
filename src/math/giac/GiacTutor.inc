// SPDX-License-Identifier: GPL-3.0-or-later
// Included only in GiacEngine.cpp: shared CallGuard/context/diagnostic capture.
// No second parser, arithmetic implementation, context, or CAS answer service.
namespace tutorkernel {
using namespace tutor;
using giac::gen;
struct LimitReached {
    int line;
};
struct UnsupportedInput {
    int line;
    std::string detail;
};
struct CheckFailure {};
struct Scope {
    giac::context *ctx;
    bool angle, complex, approx;
    int steps, lang;
    size_t quoted;
    Scope(giac::context *c, const Snapshot &input)
        : ctx(c), angle(giac::angle_radian(c)), complex(giac::complex_mode(c)),
          approx(giac::approx_mode(c)), steps(giac::step_infolevel(c)), lang(giac::language(c)),
          quoted(c->quoted_global_vars ? c->quoted_global_vars->size() : 0) {
        // WHY: reserve/parse can throw; finish those before changing context flags.
        giac::vecteur vars;
        for (const auto &v : input.variables)
            vars.push_back(gen(v, c));
        for (const auto &v : input.bindings)
            vars.push_back(gen(v.variable, c));
        if (c->quoted_global_vars)
            c->quoted_global_vars->insert(c->quoted_global_vars->end(), vars.begin(), vars.end());
        giac::angle_radian(true, c);
        giac::complex_mode(input.complex, c);
        giac::approx_mode(false, c);
        giac::step_infolevel(c) = 0;
        giac::language(0, c);
    }
    ~Scope() {
        giac::angle_radian(angle, ctx);
        giac::complex_mode(complex, ctx);
        giac::approx_mode(approx, ctx);
        giac::step_infolevel(ctx) = steps;
        giac::language(lang, ctx);
        if (ctx->quoted_global_vars)
            ctx->quoted_global_vars->resize(quoted);
    }
};
struct Algebra {
    giac::context *ctx;
    const Snapshot &input;
    unsigned calls = 0;
    int rejectedLine = 0;
    Verdict reject(int line) {
        rejectedLine = line;
        return Verdict::Rejected;
    }
    uint32_t *measuredCalls = nullptr;
    void tick() {
        if (measuredCalls)
            *measuredCalls = calls + 1;
        if (++calls > Limits::symbolicCalls)
            throw LimitReached{__LINE__};
    }
    gen parse(const std::string &s) {
        tick();
        if (s.empty() || s.size() > Limits::expressionBytes)
            throw LimitReached{__LINE__};
        return gen(s, ctx);
    }
    gen normal(const gen &g) {
        tick();
        return giac::_simplify(giac::ratnormal(g.eval(1, ctx), ctx), ctx);
    }
    gen expand(const gen &g) {
        tick();
        return giac::expand(g.eval(1, ctx), ctx);
    }
    gen sub(const gen &g, const gen &v, const gen &x) {
        tick();
        return giac::subst(g, v, x, false, ctx);
    }
    bool zero(const gen &g) { return giac::is_zero(normal(g), ctx); }
    bool same(const gen &a, const gen &b) { return a == b || zero(a - b); }
    bool rational(const gen &g) {
        return g.type == giac::_INT_ || g.type == giac::_ZINT ||
               (g.type == giac::_FRAC && rational(g._FRACptr->num) && rational(g._FRACptr->den) &&
                !giac::is_zero(g._FRACptr->den, ctx));
    }
    bool negative(const gen &g) {
        tick();
        return giac::is_strictly_positive(-g, ctx);
    }
    std::string print(const gen &g) {
        auto s = g.print(ctx);
        if (s.size() > Limits::expressionBytes)
            throw LimitReached{__LINE__};
        return s;
    }
    gen bound(const gen &g) {
        gen value = g;
        for (const auto &b : input.bindings)
            value = sub(value, parse(b.variable), parse(b.value));
        return value;
    }
    std::array<gen, 3> coefficients(const gen &expression, const gen &x) {
        const gen p = expand(expression);
        std::array<gen, 3> c;
        gen derivative = p;
        for (int i = 0; i < 3; ++i) {
            c[i] = normal(sub(derivative, x, gen(0)) / (i == 2 ? gen(2) : gen(1)));
            if (!rational(c[i]))
                throw UnsupportedInput{__LINE__};
            if (i < 2) {
                tick();
                derivative = giac::derive(derivative, x, ctx);
            }
        }
        if (!same(p, c[0] + c[1] * x + c[2] * x * x))
            throw UnsupportedInput{__LINE__, print(p) + " vs " + print(c[0]) + "," + print(c[1]) +
                                                 "," + print(c[2])};
        return c;
    }
};
bool samePath(const Path &a, const Path &b) {
    return a.equation == b.equation && a.side == b.side && a.children == b.children;
}
bool sameConditions(const Vector<Condition> &a, const Vector<Condition> &b) {
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].nonzero != b[i].nonzero || !samePath(a[i].source, b[i].source))
            return false;
    return true;
}
bool sameEquations(const Vector<Equation> &a, const Vector<Equation> &b) {
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].lhs != b[i].lhs || a[i].rhs != b[i].rhs)
            return false;
    return true;
}
bool sameBranches(const Vector<Branch> &a, const Vector<Branch> &b) {
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].id != b[i].id || a[i].status != b[i].status ||
            !sameEquations(a[i].equations, b[i].equations))
            return false;
    return true;
}
bool validIdentifier(const std::string &s) {
    return s.size() == 1 && (s[0] == 'x' || s[0] == 'y' || s[0] == 'z');
}

// Pure structural preflight precedes ALL expansion/evaluation. Nested powers
// cannot turn a short source into an unbounded polynomial expansion.
unsigned sourceShape(const gen &g, unsigned depth, unsigned &nodes) {
    if (depth > Limits::depth || ++nodes > Limits::sourceNodes)
        throw LimitReached{__LINE__};
    if (g.type == giac::_INT_ || g.type == giac::_ZINT)
        return 0;
    if (g.type == giac::_IDNT)
        return 1;
    if (g.type == giac::_FRAC)
        return std::max(sourceShape(g._FRACptr->num, depth + 1, nodes),
                        sourceShape(g._FRACptr->den, depth + 1, nodes));
    if (g.type != giac::_SYMB)
        throw UnsupportedInput{__LINE__};
    const auto op = g._SYMBptr->sommet;
    const auto &f = g._SYMBptr->feuille;
    if (op != giac::at_plus && op != giac::at_prod && op != giac::at_neg &&
        op != giac::at_binary_minus && op != giac::at_division && op != giac::at_inv &&
        op != giac::at_pow)
        throw UnsupportedInput{__LINE__};
    unsigned degree = 0;
    if (op == giac::at_pow) {
        if (f.type != giac::_VECT || f._VECTptr->size() != 2 ||
            f._VECTptr->back().type != giac::_INT_)
            throw UnsupportedInput{__LINE__};
        const int exponent = f._VECTptr->back().val;
        if (exponent < -2 || exponent > 4)
            throw UnsupportedInput{__LINE__};
        degree = sourceShape(f._VECTptr->front(), depth + 1, nodes) *
                 unsigned(std::max(1, std::abs(exponent)));
    } else if (f.type == giac::_VECT) {
        for (const auto &child : *f._VECTptr) {
            const auto d = sourceShape(child, depth + 1, nodes);
            degree = (op == giac::at_plus || op == giac::at_binary_minus) ? std::max(degree, d)
                                                                          : degree + d;
            if (degree > 8)
                throw LimitReached{__LINE__};
        }
    } else
        degree = sourceShape(f, depth + 1, nodes);
    if (degree > 8)
        throw LimitReached{__LINE__};
    return degree;
}

// A symbolic identity is only useful on a shared domain. Inspect AFTER syntax
// before reduction, rejecting newly introduced poles and variable radicals.
bool domainSafe(Algebra &a, const gen &g, const State &state, unsigned depth, unsigned &nodes) {
    if (depth > Limits::depth || ++nodes > Limits::sourceNodes)
        return false;
    if (a.rational(g))
        return true;
    if (g.type == giac::_CPLX)
        return a.rational(*g._CPLXptr) && a.rational(*(g._CPLXptr + 1));
    if (g.type == giac::_IDNT) {
        auto name = a.print(g);
        if (name == "i" && a.input.complex)
            return true;
        return std::find(a.input.variables.begin(), a.input.variables.end(), name) !=
               a.input.variables.end();
    }
    auto allowedDenominator = [&](const gen &raw) {
        const gen den = a.normal(raw);
        if (a.zero(den))
            return false;
        int budget = Limits::sourceNodes;
        std::vector<gen> vars;
        for (const auto &v : a.input.variables)
            vars.push_back(a.parse(v));
        if (!containsAnyVariable(den, vars, 0, budget))
            return !giac::is_undef(den);
        gen product(1);
        for (const auto &c : state.conditions)
            product = product * a.parse(c.nonzero);
        // If D divides a product known nonzero, D is nonzero. Each original factor
        // is retained, so normalization may safely combine denominator factors.
        const gen quotient = a.normal(product / den);
        a.tick();
        const gen remainderDen = a.normal(giac::_denom(quotient, a.ctx));
        return a.rational(remainderDen) && !a.zero(remainderDen);
    };
    if (g.type == giac::_FRAC)
        return domainSafe(a, g._FRACptr->num, state, depth + 1, nodes) &&
               domainSafe(a, g._FRACptr->den, state, depth + 1, nodes) &&
               allowedDenominator(g._FRACptr->den);
    if (g.type != giac::_SYMB)
        return false;
    const auto op = g._SYMBptr->sommet;
    const auto &f = g._SYMBptr->feuille;
    if (op != giac::at_plus && op != giac::at_prod && op != giac::at_neg &&
        op != giac::at_binary_minus && op != giac::at_division && op != giac::at_inv &&
        op != giac::at_pow && op != giac::at_sqrt)
        return false;
    if (f.type == giac::_VECT) {
        for (const auto &child : *f._VECTptr)
            if (!domainSafe(a, child, state, depth + 1, nodes))
                return false;
    } else if (!domainSafe(a, f, state, depth + 1, nodes))
        return false;
    if (op == giac::at_inv)
        return allowedDenominator(f);
    if (op == giac::at_division)
        return f.type == giac::_VECT && f._VECTptr->size() == 2 &&
               allowedDenominator(f._VECTptr->back());
    if (op == giac::at_sqrt) {
        int budget = Limits::sourceNodes;
        std::vector<gen> vars;
        for (const auto &v : a.input.variables)
            vars.push_back(a.parse(v));
        return !containsAnyVariable(f, vars, 0, budget);
    }
    if (op == giac::at_pow) {
        if (f.type != giac::_VECT || f._VECTptr->size() != 2)
            return false;
        const gen exponent = a.normal(f._VECTptr->back());
        if (exponent.type != giac::_INT_ || exponent.val < -2 || exponent.val > 4)
            return false;
        return exponent.val >= 0 || allowedDenominator(f._VECTptr->front());
    }
    return true;
}

// Capture domain-bearing syntax before normal/eval. Only a bounded rational
// expression grammar represented by Giac's existing AST is admitted here.
// An inverse of a non-polynomial expression is deliberately outside release 1.
void scan(Algebra &a, const gen &g, Path path, unsigned depth, unsigned &nodes,
          Vector<Condition> &conditions, std::vector<gen> &denominators,
          std::vector<std::string> *names = nullptr) {
    if (depth > Limits::depth || ++nodes > Limits::sourceNodes)
        throw LimitReached{__LINE__};
    auto denominator = [&](const gen &raw) {
        gen d = a.bound(raw);
        auto p = a.coefficients(d, a.parse(a.input.variables.front()));
        (void)p;
        if (a.zero(d))
            throw UnsupportedInput{__LINE__};
        bool exists = false;
        for (const auto &old : denominators)
            exists = exists || a.same(d, old);
        if (!exists) {
            if (denominators.size() >= Limits::conditions)
                throw LimitReached{__LINE__};
            denominators.push_back(d);
        }
        if (!a.rational(a.normal(d))) {
            bool found = false;
            for (const auto &c : conditions)
                found = found || a.same(d, a.parse(c.nonzero));
            if (!found) {
                if (conditions.size() >= Limits::conditions)
                    throw LimitReached{__LINE__};
                conditions.push_back({a.print(d), path});
            }
        }
    };
    if (g.type == giac::_INT_ || g.type == giac::_ZINT) {
        if (a.print(g).size() > 25)
            throw LimitReached{__LINE__};
        return;
    }
    if (g.type == giac::_IDNT) {
        auto n = a.print(g);
        if (std::find(a.input.variables.begin(), a.input.variables.end(), n) !=
            a.input.variables.end())
            return;
        if (n.size() != 1 || n[0] < 'A' || n[0] > 'F')
            throw UnsupportedInput{__LINE__};
        if (std::find_if(a.input.bindings.begin(), a.input.bindings.end(), [&](const Binding &b) {
                return b.variable == n;
            }) == a.input.bindings.end())
            throw UnsupportedInput{__LINE__};
        if (names && std::find(names->begin(), names->end(), n) == names->end())
            names->push_back(n);
        return;
    }
    if (g.type == giac::_FRAC) {
        denominator(g._FRACptr->den);
        scan(a, g._FRACptr->num, path, depth + 1, nodes, conditions, denominators, names);
        scan(a, g._FRACptr->den, path, depth + 1, nodes, conditions, denominators, names);
        return;
    }
    if (g.type == giac::_VECT) {
        for (size_t i = 0; i < g._VECTptr->size(); ++i) {
            auto p = path;
            p.children.push_back(uint8_t(i));
            scan(a, (*g._VECTptr)[i], std::move(p), depth + 1, nodes, conditions, denominators,
                 names);
        }
        return;
    }
    if (g.type != giac::_SYMB)
        throw UnsupportedInput{__LINE__};
    const auto op = g._SYMBptr->sommet;
    const auto &leaf = g._SYMBptr->feuille;
    if (op == giac::at_inv)
        denominator(leaf);
    else if (op == giac::at_pow) {
        if (leaf.type != giac::_VECT || leaf._VECTptr->size() != 2)
            throw UnsupportedInput{__LINE__};
        const gen &exponent = leaf._VECTptr->back();
        if (exponent.type != giac::_INT_ || exponent.val < -2 || exponent.val > 4)
            throw UnsupportedInput{__LINE__};
        if (exponent.val < 0)
            denominator(giac::symbolic(
                giac::at_pow, giac::makevecteur(leaf._VECTptr->front(), gen(-exponent.val))));
    } else if (op != giac::at_plus && op != giac::at_prod && op != giac::at_neg &&
               op != giac::at_binary_minus && op != giac::at_division)
        throw UnsupportedInput{__LINE__};
    if (op == giac::at_division) {
        if (leaf.type != giac::_VECT || leaf._VECTptr->size() != 2)
            throw UnsupportedInput{__LINE__};
        denominator(leaf._VECTptr->back());
    }
    scan(a, leaf, std::move(path), depth + 1, nodes, conditions, denominators, names);
}
State authoredState(const Snapshot &s) {
    State out;
    Branch b;
    b.equations = s.authored;
    out.branches.push_back(std::move(b));
    out.fingerprint = fingerprint(out);
    return out;
}
void capture(Algebra &a, Vector<Condition> &conditions, std::vector<gen> &denominators) {
    unsigned nodes = 0;
    for (const auto &e : a.input.authored) {
        sourceShape(a.parse(e.lhs), 0, nodes);
        sourceShape(a.parse(e.rhs), 0, nodes);
    }
    nodes = 0;
    for (size_t i = 0; i < a.input.authored.size(); ++i)
        for (int side = 0; side < 2; ++side) {
            Path p;
            p.equation = uint8_t(i);
            p.side = uint8_t(side);
            scan(a, a.parse(side ? a.input.authored[i].rhs : a.input.authored[i].lhs), p, 0, nodes,
                 conditions, denominators);
        }
}
Parameter expr(const std::string &s) { return {ParameterKind::Expression, s}; }
Parameter row(unsigned r) { return {ParameterKind::Row, std::to_string(r + 1)}; }
Parameter variable(const std::string &s) { return {ParameterKind::Variable, s}; }
bool shortInteger(const gen &value, const std::string &text) {
    // WHY: only short integer amounts belong in prose. Fractions, variable
    // terms and compound expressions use the verified operation's math view.
    return (value.type == giac::_INT_ || value.type == giac::_ZINT) && text.size() <= 12;
}
void message(Algebra &a, Step &s, const State &after) {
    s.parameters.clear();
    switch (s.rule) {
    case Rule::Domain:
        s.explanation = Message::Domain;
        break;
    case Rule::SubstituteValues:
        s.explanation = Message::SubstituteValues;
        break;
    case Rule::Expand:
        s.explanation = Message::Expand;
        break;
    case Rule::Collect:
        s.explanation = Message::Collect;
        break;
    case Rule::SwapSides:
        s.explanation = Message::SwapSides;
        s.parameters = {variable(a.input.variables.front())};
        break;
    case Rule::AddBoth: {
        const auto v = a.normal(a.parse(s.operand));
        const bool neg = a.print(v).front() == '-';
        const auto magnitude = a.normal(neg ? -v : v);
        const auto amount = a.print(magnitude);
        const bool numeric = shortInteger(magnitude, amount);
        const auto &target = after.branches[s.branch].equations[s.row];
        const auto &name = a.input.variables.front();
        bool singleTerm = false;
        if (a.input.variables.size() == 1 && a.rational(v)) {
            const auto c = a.coefficients(a.parse(target.lhs), a.parse(name));
            singleTerm = a.zero(c[0]) && (a.zero(c[1]) != a.zero(c[2]));
        }
        // These goals are reconstructed during replay from the checked output,
        // never supplied as an unchecked generator label.
        if (singleTerm) {
            s.explanation = numeric ? (neg ? Message::Subtract : Message::Add)
                                   : (neg ? Message::SubtractDisplayedIsolate : Message::AddDisplayedIsolate);
            if (numeric) s.parameters.push_back(expr(amount));
            s.parameters.push_back(variable(name));
        } else if (a.input.variables.size() == 1 && a.zero(a.parse(target.rhs))) {
            s.explanation = numeric ? (neg ? Message::SubtractZeroRight : Message::AddZeroRight)
                                   : (neg ? Message::SubtractDisplayedZeroRight : Message::AddDisplayedZeroRight);
            if (numeric) s.parameters.push_back(expr(amount));
        } else if (a.input.variables.size() == 1 && !a.rational(v)) {
            s.explanation = neg ? Message::SubtractCollect : Message::AddCollect;
            s.parameters = {variable(name)};
        } else {
            s.explanation = numeric ? (neg ? Message::SubtractSystem : Message::AddSystem)
                                   : (neg ? Message::SubtractDisplayedSystem : Message::AddDisplayedSystem);
            if (numeric) s.parameters.push_back(expr(amount));
        }
        break;
    }
    case Rule::DivideBoth: {
        const auto v = a.normal(a.parse(s.operand));
        const auto name = a.input.variables.front();
        const auto x = a.parse(name);
        const auto lhs = a.parse(after.branches[s.branch].equations[s.row].lhs);
        const bool isolated = a.same(lhs, x);
        if (!isolated && !a.same(lhs, x * x)) throw UnsupportedInput{__LINE__};
        const bool numeric = shortInteger(v, s.operand);
        s.explanation = numeric ? (isolated ? Message::Divide : Message::DivideSquare)
                               : (isolated ? Message::DivideDisplayed : Message::DivideDisplayedSquare);
        if (numeric) s.parameters.push_back(expr(s.operand));
        s.parameters.push_back(variable(name));
        break;
    }
    case Rule::ClearDenominators:
        s.explanation = Message::ClearDenominators;
        break;
    case Rule::Factor:
        s.explanation = Message::Factor;
        break;
    case Rule::SquareRoots:
        s.explanation = a.zero(a.parse(s.operand)) ? Message::SquareRootZero : Message::SquareRoots;
        break;
    case Rule::QuadraticFormula:
        s.explanation = after.conclusion == Conclusion::Empty ? Message::NoRealRoots
                                                              : Message::QuadraticFormula;
        if (after.conclusion != Conclusion::Empty)
            s.parameters = {variable(a.input.variables.front())};
        break;
    case Rule::ZeroProduct:
        if (s.auxiliaries.size() == 2 &&
            a.same(a.parse(s.auxiliaries[0]), a.parse(s.auxiliaries[1]))) {
            s.explanation = Message::RepeatedFactor;
        } else {
            s.explanation = Message::ZeroProduct;
        }
        break;
    case Rule::RejectCandidate:
        s.explanation = Message::RejectCandidate;
        break;
    case Rule::AlreadySolved:
        s.explanation = Message::AlreadySolved;
        s.parameters = {variable(a.input.variables.front())};
        break;
    case Rule::Identity:
        s.explanation = after.conditions.empty() ? Message::Identity : Message::ConditionalIdentity;
        break;
    case Rule::Contradiction:
        s.explanation = Message::Contradiction;
        break;
    case Rule::Finish:
        s.explanation =
            after.conclusion == Conclusion::Empty ? Message::EmptyAfterExclusions : Message::Finish;
        break;
    case Rule::RowSwap:
        s.explanation = Message::RowSwap;
        s.parameters = {row(s.row), row(s.otherRow)};
        break;
    case Rule::RowScale: {
        const bool numeric = shortInteger(a.normal(a.parse(s.operand)), s.operand);
        s.explanation = numeric ? Message::RowScale : Message::RowScaleDisplayed;
        s.parameters = {row(s.row)};
        if (numeric) s.parameters.push_back(expr(s.operand));
        s.parameters.push_back(variable(s.auxiliaries.at(0)));
        break;
    }
    case Rule::RowAdd: {
        const gen v = a.normal(a.parse(s.operand));
        const bool neg = a.negative(v);
        const gen magnitude = a.normal(neg ? -v : v);
        if (a.same(magnitude, gen(1))) {
            s.explanation = neg ? Message::RowSubtractUnit : Message::RowAddUnit;
            s.parameters = {row(s.otherRow), row(s.row), variable(s.auxiliaries.at(0))};
        } else {
            const auto amount = a.print(magnitude);
            const bool numeric = shortInteger(magnitude, amount);
            s.explanation = numeric ? (neg ? Message::RowSubtract : Message::RowAdd)
                                   : (neg ? Message::RowSubtractScaled : Message::RowAddScaled);
            if (numeric) s.parameters.push_back(expr(amount));
            s.parameters.push_back(row(s.otherRow));
            s.parameters.push_back(row(s.row));
            s.parameters.push_back(variable(s.auxiliaries.at(0)));
        }
        break;
    }
    case Rule::SystemFinish:
        s.explanation = after.conclusion == Conclusion::Empty    ? Message::InconsistentSystem
                        : after.conclusion == Conclusion::Family ? Message::DependentSystem
                                                                 : Message::UniqueSystem;
        break;
    }
}
bool checkMessage(Algebra &a, const Step &s, const State &after) {
    Step expected = s;
    message(a, expected, after);
    if (expected.explanation != s.explanation ||
        expected.parameters.size() != s.parameters.size() ||
        !validateMessage(s.explanation, s.parameters))
        return false;
    for (size_t i = 0; i < s.parameters.size(); ++i)
        if (s.parameters[i].kind != expected.parameters[i].kind ||
            s.parameters[i].value != expected.parameters[i].value)
            return false;
    return true;
}
bool equationSame(Algebra &a, const Equation &actual, const gen &lhs, const gen &rhs) {
    return a.same(a.parse(actual.lhs), lhs) && a.same(a.parse(actual.rhs), rhs);
}

// Small checking kernel. Each case replays the declared operation on BOTH
// sides, or invokes a theorem with explicitly checked hypotheses. In particular
// equation residual equality is NEVER used to justify balancing or row scaling.
Verdict verifyStep(Algebra &a, const Derivation &d, const Step &s) {
    if (s.before >= d.states.size() || s.after >= d.states.size() || s.after != s.before + 1)
        return a.reject(__LINE__);
    const State &before = d.states[s.before];
    const State &after = d.states[s.after];
    if (before.conclusion != Conclusion::None || s.branch >= before.branches.size() ||
        after.branches.size() > Limits::branches || after.conditions.size() > Limits::conditions)
        return a.reject(__LINE__);
    if (s.branch >= after.branches.size() || s.row >= after.branches[s.branch].equations.size())
        return a.reject(__LINE__);
    if (!s.discharged.empty())
        return a.reject(__LINE__);
    // Presentation grouping never changes a mathematical transition. Only
    // contiguous equivalent primitives in one branch can be folded together;
    // conditions, splits, rejections and classifications always stay visible.
    if (!s.substeps.empty()) {
        if (s.relation != Relation::Equivalent)
            return a.reject(__LINE__);
        for (size_t i = 0; i < s.substeps.size(); ++i) {
            const size_t index = s.before + 1 + i;
            if (s.substeps[i] != index || index >= d.steps.size())
                return a.reject(__LINE__);
            const auto &child = d.steps[index];
            if (child.relation != Relation::Equivalent || child.group != s.group ||
                child.branch != s.branch || !child.substeps.empty())
                return a.reject(__LINE__);
        }
    }
    const bool domain = s.rule == Rule::Domain;
    if (!domain && (!sameConditions(before.conditions, after.conditions) || !s.introduced.empty()))
        return a.reject(__LINE__);
    if (s.prerequisites.size() != before.conditions.size())
        return a.reject(__LINE__);
    for (size_t i = 0; i < s.prerequisites.size(); ++i)
        if (s.prerequisites[i] != i)
            return a.reject(__LINE__);
    Relation relation = Relation::Equivalent;
    if (domain || s.rule == Rule::AlreadySolved || s.rule == Rule::Identity ||
        s.rule == Rule::Contradiction || s.rule == Rule::Finish || s.rule == Rule::SystemFinish)
        relation = Relation::Terminal;
    if (s.rule == Rule::SquareRoots || s.rule == Rule::QuadraticFormula ||
        s.rule == Rule::ZeroProduct)
        relation = Relation::ExhaustiveSplit;
    if (s.rule == Rule::RejectCandidate)
        relation = Relation::Rejection;
    if (s.relation != relation)
        return a.reject(__LINE__);
    if (!domain) {
        for (const auto &b : after.branches) {
            if (b.equations.empty() || b.equations.size() > Limits::equations)
                return a.reject(__LINE__);
            for (const auto &e : b.equations) {
                unsigned nodes = 0;
                if (!domainSafe(a, a.parse(e.lhs), after, 0, nodes) ||
                    !domainSafe(a, a.parse(e.rhs), after, 0, nodes))
                    return a.reject(__LINE__);
            }
        }
        if (s.rule == Rule::AddBoth) {
            unsigned nodes = 0;
            if (!domainSafe(a, a.parse(s.operand), before, 0, nodes))
                return a.reject(__LINE__);
        }
    }
    // Every rule operand is untrusted on replay. Reject effectful syntax BEFORE
    // any formatter computes a sign or a symbolic primitive evaluates it.
    if (!s.operand.empty() && !domain) {
        unsigned nodes = 0;
        if (!domainSafe(a, a.parse(s.operand), before, 0, nodes))
            return a.reject(__LINE__);
    }
    for (const auto &operand : s.auxiliaries) {
        unsigned nodes = 0;
        if (!domainSafe(a, a.parse(operand), before, 0, nodes))
            return a.reject(__LINE__);
    }
    if (!checkMessage(a, s, after))
        return a.reject(__LINE__);
    if (s.affected.size() != 1 || s.affected[0].equation != s.row || s.affected[0].side != 2 ||
        !s.affected[0].children.empty())
        return a.reject(__LINE__);
    const auto &branch = before.branches[s.branch];
    if ((branch.status != BranchStatus::Active && s.rule != Rule::Finish) ||
        s.row >= branch.equations.size())
        return a.reject(__LINE__);
    bool ok = false;
    State expected = before;
    const auto &eq = branch.equations[s.row];
    const gen lhs = a.parse(eq.lhs), rhs = a.parse(eq.rhs);
    const gen x = a.parse(a.input.variables.front());
    auto transformed = [&](const gen &l, const gen &r) {
        if (after.branches.size() != before.branches.size() || s.branch >= after.branches.size() ||
            s.row >= after.branches[s.branch].equations.size())
            return false;
        auto &e = expected.branches[s.branch].equations[s.row];
        e = after.branches[s.branch].equations[s.row];
        if (s.rule == Rule::AddBoth || s.rule == Rule::DivideBoth || s.rule == Rule::RowAdd ||
            s.rule == Rule::RowScale || s.rule == Rule::ClearDenominators)
            return e.lhs == a.print(a.normal(l)) && e.rhs == a.print(a.normal(r));
        return equationSame(a, e, l, r);
    };
    switch (s.rule) {
    case Rule::Domain: {
        Vector<Condition> all;
        std::vector<gen> den;
        capture(a, all, den);
        const size_t i = before.conditions.size();
        ok = i < all.size() && s.operand == all[i].nonzero &&
             s.introduced == Vector<uint8_t>{uint8_t(i)};
        if (ok)
            expected.conditions.push_back(all[i]);
        break;
    }
    case Rule::SubstituteValues:
        ok = !a.input.bindings.empty();
        // Replay substitution structurally, preserving every branch identity/status.
        // Normalized equality alone could hide a newly injected removable pole.
        for (auto &b : expected.branches)
            for (auto &e : b.equations) {
                e.lhs = a.print(a.bound(a.parse(e.lhs)));
                e.rhs = a.print(a.bound(a.parse(e.rhs)));
            }
        break;
    case Rule::Expand: {
        const gen l = a.expand(lhs), r = a.expand(rhs);
        const auto &target = after.branches[s.branch].equations[s.row];
        ok = s.auxiliaries == Vector<std::string>{eq.lhs, eq.rhs} && target.lhs == a.print(l) &&
             target.rhs == a.print(r) && transformed(l, r);
        break;
    }
    case Rule::Collect: {
        const gen l = a.normal(lhs), r = a.normal(rhs);
        const auto &target = after.branches[s.branch].equations[s.row];
        ok = s.auxiliaries == Vector<std::string>{eq.lhs, eq.rhs} && target.lhs == a.print(l) &&
             target.rhs == a.print(r) && transformed(l, r);
        break;
    }
    case Rule::SwapSides:
        ok = transformed(rhs, lhs);
        break;
    case Rule::AddBoth: {
        const gen v = a.parse(s.operand);
        ok = !a.zero(v) && transformed(lhs + v, rhs + v);
        bool removes = false;
        gen leftConstant = lhs, rightConstant = rhs, afterLeft = lhs + v, afterRight = rhs + v;
        for (const auto &name : a.input.variables) {
            const gen var = a.parse(name);
            a.tick();
            const gen lc = giac::derive(a.expand(lhs), var, a.ctx);
            a.tick();
            const gen rc = giac::derive(a.expand(rhs), var, a.ctx);
            a.tick();
            const gen la = giac::derive(a.expand(lhs + v), var, a.ctx);
            a.tick();
            const gen ra = giac::derive(a.expand(rhs + v), var, a.ctx);
            removes = removes || (!a.zero(lc) && a.zero(la)) || (!a.zero(rc) && a.zero(ra));
            leftConstant = a.sub(leftConstant, var, gen(0));
            rightConstant = a.sub(rightConstant, var, gen(0));
            afterLeft = a.sub(afterLeft, var, gen(0));
            afterRight = a.sub(afterRight, var, gen(0));
        }
        ok = ok && (removes || (!a.zero(leftConstant) && a.zero(afterLeft)) ||
                    (!a.zero(rightConstant) && a.zero(afterRight)));
        break;
    }
    case Rule::DivideBoth: {
        const gen v = a.normal(a.parse(s.operand));
        const auto coefficients = a.coefficients(lhs, x);
        const gen leading = a.zero(coefficients[2]) ? coefficients[1] : coefficients[2];
        ok = a.rational(v) && !a.zero(v) && !a.same(v, gen(1)) && a.same(v, leading) &&
             transformed(lhs / v, rhs / v);
        break;
    }
    case Rule::ClearDenominators: {
        Vector<Condition> cs;
        std::vector<gen> ds;
        capture(a, cs, ds);
        gen multiplier(1);
        for (const auto &v : ds) {
            a.tick();
            multiplier = giac::_lcm(gen(giac::makevecteur(multiplier, v), giac::_SEQ__VECT), a.ctx);
        }
        multiplier = a.normal(multiplier);
        ok = sameConditions(before.conditions, cs) && a.same(multiplier, a.parse(s.operand)) &&
             !a.same(multiplier, gen(1)) && transformed(lhs * multiplier, rhs * multiplier);
        break;
    }
    case Rule::Factor: {
        ok = a.zero(rhs) && s.auxiliaries.size() == 2;
        gen product(1);
        for (const auto &f : s.auxiliaries) {
            const auto c = a.coefficients(a.parse(f), x);
            ok = ok && a.zero(c[2]) && !a.zero(c[1]);
            product = product * a.parse(f);
        }
        ok = ok && a.same(lhs, product) && transformed(product, rhs);
        break;
    }
    case Rule::RowSwap:
        ok = s.otherRow < branch.equations.size() && s.otherRow != s.row;
        if (ok)
            std::swap(expected.branches[s.branch].equations[s.row],
                      expected.branches[s.branch].equations[s.otherRow]);
        break;
    case Rule::RowScale: {
        const gen v = a.normal(a.parse(s.operand));
        ok = s.auxiliaries.size() == 1 &&
             std::find(a.input.variables.begin(), a.input.variables.end(), s.auxiliaries[0]) !=
                 a.input.variables.end() &&
             a.rational(v) && !a.zero(v) && !a.same(v, gen(1)) && transformed(lhs / v, rhs / v);
        if (ok) {
            a.tick();
            ok = a.same(giac::derive(a.expand(lhs), a.parse(s.auxiliaries[0]), a.ctx), v);
        }
        break;
    }
    case Rule::RowAdd: {
        const gen v = a.normal(a.parse(s.operand));
        ok = s.auxiliaries.size() == 1 &&
             std::find(a.input.variables.begin(), a.input.variables.end(), s.auxiliaries[0]) !=
                 a.input.variables.end() &&
             s.otherRow < branch.equations.size() && s.otherRow != s.row && a.rational(v) &&
             !a.zero(v);
        if (ok) {
            const auto &other = branch.equations[s.otherRow];
            const gen newLeft = lhs + v * a.parse(other.lhs);
            ok = transformed(newLeft, rhs + v * a.parse(other.rhs));
            a.tick();
            const gen prior = giac::derive(a.expand(lhs), a.parse(s.auxiliaries[0]), a.ctx);
            a.tick();
            const gen remaining = giac::derive(a.expand(newLeft), a.parse(s.auxiliaries[0]), a.ctx);
            ok = ok && !a.zero(prior) && a.zero(remaining);
        }
        break;
    }
    case Rule::ZeroProduct: {
        // WHY: the product theorem creates factor equations, not guessed roots.
        // Each branch is subsequently isolated by the ordinary balancing rules.
        ok = before.branches.size() == 1 && a.zero(rhs) && s.auxiliaries.size() == 2;
        if (!ok)
            break;
        std::array<gen, 2> factors{a.parse(s.auxiliaries[0]), a.parse(s.auxiliaries[1])};
        std::array<gen, 2> roots;
        for (unsigned i = 0; i < 2; ++i) {
            const auto c = a.coefficients(factors[i], x);
            ok = ok && a.zero(c[2]) && !a.zero(c[1]);
            if (!ok)
                break;
            roots[i] = a.normal(-c[0] / c[1]);
        }
        ok = ok && a.same(lhs, factors[0] * factors[1]);
        if (!ok)
            break;
        const unsigned count = a.same(roots[0], roots[1]) ? 1 : 2;
        expected.branches.clear();
        for (unsigned i = 0; i < count; ++i) {
            Branch b;
            b.id = uint8_t(i + 1);
            b.equations = {{s.auxiliaries[i], "0"}};
            expected.branches.push_back(std::move(b));
        }
        break;
    }
    case Rule::SquareRoots:
    case Rule::QuadraticFormula: {
        const auto c = a.coefficients(lhs - rhs, x);
        ok = !a.zero(c[2]) && before.branches.size() == 1;
        const gen discriminant = a.normal(c[1] * c[1] - gen(4) * c[2] * c[0]);
        if (s.rule == Rule::SquareRoots)
            ok = ok && a.same(lhs, x * x) && a.same(rhs, a.parse(s.operand));
        if (s.rule == Rule::QuadraticFormula)
            ok = ok && s.auxiliaries.size() == 4 && a.same(a.parse(s.auxiliaries[0]), c[2]) &&
                 a.same(a.parse(s.auxiliaries[1]), c[1]) &&
                 a.same(a.parse(s.auxiliaries[2]), c[0]) &&
                 a.same(a.parse(s.auxiliaries[3]), discriminant);
        if (!a.input.complex && a.negative(discriminant)) {
            ok = ok && after.conclusion == Conclusion::Empty &&
                 sameBranches(after.branches, before.branches);
            expected.conclusion = Conclusion::Empty;
            break;
        }
        const unsigned count = a.zero(discriminant) ? 1 : 2;
        ok = ok && after.branches.size() == count && after.conclusion == Conclusion::None;
        std::array<gen, 2> roots;
        for (unsigned i = 0; ok && i < count; ++i) {
            const auto &b = after.branches[i];
            ok = b.id == i + 1 && b.status == BranchStatus::Active && b.equations.size() == 1 &&
                 a.same(a.parse(b.equations[0].lhs), x);
            if (ok)
                roots[i] = a.parse(b.equations[0].rhs);
        }
        // WHY: Vieta reconstruction + degree 2 and nonzero leading coefficient
        // proves exhaustiveness, independently of the generator's formula choice.
        if (ok) {
            if (count == 1)
                roots[1] = roots[0];
            else
                ok = !a.same(roots[0], roots[1]);
            ok = ok && a.same(c[1], -c[2] * (roots[0] + roots[1])) &&
                 a.same(c[0], c[2] * roots[0] * roots[1]);
        }
        if (ok)
            expected.branches = after.branches;
        break;
    }
    case Rule::RejectCandidate: {
        ok = branch.equations.size() == 1 && a.same(lhs, x) && a.same(rhs, a.parse(s.operand)) &&
             s.auxiliaries.size() == 1;
        bool excluded = false;
        for (const auto &c : before.conditions)
            if (c.nonzero == s.auxiliaries[0])
                excluded = a.zero(a.sub(a.parse(c.nonzero), x, rhs));
        ok = ok && excluded;
        expected.branches[s.branch].status = BranchStatus::Rejected;
        break;
    }
    case Rule::AlreadySolved:
        ok = before.conditions.empty() && before.branches.size() == 1 &&
             branch.equations.size() == 1 && lhs == x && a.rational(a.normal(rhs));
        expected.conclusion = Conclusion::Finite;
        break;
    case Rule::Identity:
        ok = before.branches.size() == 1 && branch.equations.size() == 1 && a.same(lhs, rhs);
        expected.conclusion = Conclusion::Identity;
        break;
    case Rule::Contradiction: {
        const auto c = a.coefficients(lhs - rhs, x);
        ok = a.zero(c[1]) && a.zero(c[2]) && !a.zero(c[0]);
        expected.conclusion = Conclusion::Empty;
        break;
    }
    case Rule::Finish: {
        ok = true;
        unsigned active = 0;
        for (const auto &b : before.branches) {
            if (b.status == BranchStatus::Rejected)
                continue;
            ++active;
            if (b.equations.size() != 1 || !a.same(a.parse(b.equations[0].lhs), x)) {
                ok = false;
                break;
            }
            const gen v = a.parse(b.equations[0].rhs);
            for (const auto &c : before.conditions)
                if (a.zero(a.sub(a.parse(c.nonzero), x, v)))
                    ok = false;
            for (const auto &e : a.input.authored)
                if (!a.same(a.sub(a.bound(a.parse(e.lhs)), x, v),
                            a.sub(a.bound(a.parse(e.rhs)), x, v)))
                    ok = false;
        }
        expected.conclusion = active ? Conclusion::Finite : Conclusion::Empty;
        break;
    }
    case Rule::SystemFinish: {
        ok = before.branches.size() == 1 && a.input.variables.size() > 1 &&
             before.conditions.empty();
        std::array<bool, 3> pivot{};
        unsigned rank = 0;
        bool inconsistent = false;
        for (const auto &e : branch.equations) {
            const gen l = a.expand(a.parse(e.lhs)), r = a.parse(e.rhs);
            if (!a.rational(a.normal(r))) {
                ok = false;
                break;
            }
            gen reconstructed(0);
            int first = -1;
            for (size_t j = 0; j < a.input.variables.size(); ++j) {
                const gen v = a.parse(a.input.variables[j]);
                a.tick();
                gen coeff = a.normal(giac::derive(l, v, a.ctx));
                if (!a.rational(coeff)) {
                    ok = false;
                    break;
                }
                if (!a.zero(coeff)) {
                    if (first < 0) {
                        first = int(j);
                        if (!a.same(coeff, gen(1)) || pivot[j])
                            ok = false;
                        pivot[j] = true;
                        ++rank;
                    }
                    reconstructed += coeff * v;
                }
            }
            if (!a.same(l, reconstructed))
                ok = false;
            if (first < 0 && !a.zero(r))
                inconsistent = true;
        }
        expected.conclusion = inconsistent                      ? Conclusion::Empty
                              : rank < a.input.variables.size() ? Conclusion::Family
                                                                : Conclusion::Finite;
        if (expected.conclusion == Conclusion::Finite)
            for (size_t i = 0; i < a.input.variables.size(); ++i)
                ok = ok && a.parse(branch.equations[i].lhs) == a.parse(a.input.variables[i]);
        break;
    }
    }
    // Verify unchanged rows/branches/conditions exactly and reject accidental
    // no-ops. Terminal annotations and domain introduction are intentional.
    if (!ok || expected.conclusion != after.conclusion ||
        !sameConditions(expected.conditions, after.conditions) ||
        !sameBranches(expected.branches, after.branches))
        return a.reject(__LINE__);
    if (relation == Relation::Equivalent && sameBranches(before.branches, after.branches))
        return a.reject(__LINE__);
    if (after.fingerprint != fingerprint(after) || before.fingerprint != fingerprint(before))
        return a.reject(__LINE__);
    return Verdict::Verified;
}

struct Planner {
    Algebra &a;
    Derivation &d;
    uint16_t group = 0;
    const State &current() const { return d.states.back(); }
    bool add(Step s, State next) {
        if (d.steps.size() >= Limits::steps)
            throw LimitReached{__LINE__};
        s.before = uint16_t(d.states.size() - 1);
        s.after = s.before + 1;
        s.group = group;
        for (size_t i = 0; i < d.states.back().conditions.size(); ++i)
            s.prerequisites.push_back(uint8_t(i));
        if (s.rule == Rule::Domain) {
            s.relation = Relation::Terminal;
            s.introduced = {uint8_t(next.conditions.size() - 1)};
        }
        if (s.rule == Rule::AlreadySolved || s.rule == Rule::Identity ||
            s.rule == Rule::Contradiction || s.rule == Rule::Finish || s.rule == Rule::SystemFinish)
            s.relation = Relation::Terminal;
        if (s.rule == Rule::SquareRoots || s.rule == Rule::QuadraticFormula ||
            s.rule == Rule::ZeroProduct)
            s.relation = Relation::ExhaustiveSplit;
        if (s.rule == Rule::RejectCandidate)
            s.relation = Relation::Rejection;
        Path p;
        p.equation = s.row;
        p.side = 2;
        s.affected.push_back(p);
        next.fingerprint = fingerprint(next);
        if (s.relation == Relation::Equivalent && next.fingerprint == d.states.back().fingerprint)
            return false;
        for (const auto &state : d.states)
            if (state.fingerprint == next.fingerprint && s.relation == Relation::Equivalent)
                throw CheckFailure{};
        if (s.rule == Rule::Expand || s.rule == Rule::Collect) {
            const auto &e = d.states.back().branches[s.branch].equations[s.row];
            s.auxiliaries = {e.lhs, e.rhs};
        }
        message(a, s, next);
        struct Rollback {
            Derivation &d;
            size_t states, steps;
            bool committed = false;
            ~Rollback() {
                if (!committed) {
                    d.states.resize(states);
                    d.steps.resize(steps);
                }
            }
        } transaction{d, d.states.size(), d.steps.size()};
        d.states.push_back(std::move(next));
        if (retainedBytes(d) > Limits::retainedBytes) {
            d.states.pop_back();
            throw LimitReached{__LINE__};
        }
        s.verification = verifyStep(a, d, s);
        if (s.verification != Verdict::Verified) {
            d.diagnostic = std::string("checker rejected ") + ruleId(s.rule) + " at " +
                           std::to_string(a.rejectedLine);
            d.states.pop_back();
            throw CheckFailure{};
        }
        d.steps.push_back(std::move(s));
        if (retainedBytes(d) > Limits::retainedBytes)
            throw LimitReached{__LINE__};
        transaction.committed = true;
#if defined(ARDUINO) && !defined(NATIVE_SIM)
        // Cooperate only between committed primitives. A synchronous Giac
        // operation itself is neither preemptible by this planner nor cancellable.
        taskYIELD();
#endif
        return true;
    }
    void equation(Rule rule, const gen &l, const gen &r, const gen &operand = gen(0),
                  unsigned rowIndex = 0, unsigned branchIndex = 0, unsigned other = 0,
                  const std::string &focus = "") {
        State next = current();
        Step s;
        s.rule = rule;
        s.row = uint8_t(rowIndex);
        s.branch = uint8_t(branchIndex);
        s.otherRow = uint8_t(other);
        if (!focus.empty())
            s.auxiliaries = {focus};
        s.operand = a.print(operand);
        next.branches[branchIndex].equations[rowIndex] = {a.print(l), a.print(r)};
        add(s, std::move(next));
    }
    void terminal(Rule rule, Conclusion conclusion) {
        auto next = current();
        next.conclusion = conclusion;
        Step s;
        s.rule = rule;
        add(s, std::move(next));
    }
    void normalizeRows() {
        auto terms = [](const gen &g, auto &&self) -> unsigned {
            if (g.type == giac::_INT_ && g.val == 0)
                return 0;
            if (g.type == giac::_SYMB) {
                const auto op = g._SYMBptr->sommet;
                const auto &f = g._SYMBptr->feuille;
                if ((op == giac::at_plus || op == giac::at_binary_minus) && f.type == giac::_VECT) {
                    unsigned n = 0;
                    for (const auto &c : *f._VECTptr)
                        n += self(c, self);
                    return n;
                }
                if (op == giac::at_neg)
                    return self(f, self);
            }
            return 1;
        };
        auto distribution = [](const gen &g, auto &&self) -> bool {
            if (g.type != giac::_SYMB)
                return false;
            const auto op = g._SYMBptr->sommet;
            const auto &f = g._SYMBptr->feuille;
            if (f.type == giac::_VECT) {
                for (const auto &child : *f._VECTptr) {
                    if ((op == giac::at_prod || op == giac::at_pow) && child.type == giac::_SYMB &&
                        (child._SYMBptr->sommet == giac::at_plus ||
                         child._SYMBptr->sommet == giac::at_binary_minus))
                        return true;
                    if (self(child, self))
                        return true;
                }
            } else
                return self(f, self);
            return false;
        };
        for (size_t i = 0; i < current().branches[0].equations.size(); ++i) {
            auto e = current().branches[0].equations[i];
            auto l = a.parse(e.lhs), r = a.parse(e.rhs);
            const auto el = a.expand(l), er = a.expand(r);
            if ((distribution(l, distribution) || distribution(r, distribution)) &&
                (el != l || er != r))
                equation(Rule::Expand, el, er, gen(0), i);
            e = current().branches[0].equations[i];
            l = a.parse(e.lhs);
            r = a.parse(e.rhs);
            const auto nl = a.normal(l), nr = a.normal(r);
            if (terms(nl, terms) != terms(l, terms) || terms(nr, terms) != terms(r, terms))
                equation(Rule::Collect, nl, nr, gen(0), i);
        }
    }
    void finishCandidates() {
        ++group;
        const gen x = a.parse(a.input.variables[0]);
        for (size_t i = 0; i < current().branches.size(); ++i) {
            const gen v = a.parse(current().branches[i].equations[0].rhs);
            for (const auto &c : current().conditions)
                if (a.zero(a.sub(a.parse(c.nonzero), x, v))) {
                    State next = current();
                    next.branches[i].status = BranchStatus::Rejected;
                    Step s;
                    s.rule = Rule::RejectCandidate;
                    s.branch = uint8_t(i);
                    s.operand = a.print(v);
                    s.auxiliaries = {c.nonzero};
                    add(s, std::move(next));
                    break;
                }
        }
        unsigned count = 0;
        for (const auto &b : current().branches)
            count += b.status == BranchStatus::Active;
        terminal(Rule::Finish, count ? Conclusion::Finite : Conclusion::Empty);
    }
    void productBranches(const Vector<std::string> &factors) {
        const gen x = a.parse(a.input.variables[0]);
        const auto c0 = a.coefficients(a.parse(factors[0]), x);
        const auto c1 = a.coefficients(a.parse(factors[1]), x);
        const unsigned count = a.same(-c0[0] / c0[1], -c1[0] / c1[1]) ? 1 : 2;
        State next = current();
        next.branches.clear();
        Step split;
        split.rule = Rule::ZeroProduct;
        split.auxiliaries = factors;
        for (unsigned i = 0; i < count; ++i) {
            Branch b;
            b.id = uint8_t(i + 1);
            b.equations = {{factors[i], "0"}};
            next.branches.push_back(std::move(b));
        }
        add(split, std::move(next));
        for (unsigned i = 0; i < count; ++i) {
            ++group;
            const auto e = current().branches[i].equations[0];
            gen l = a.parse(e.lhs), r = a.parse(e.rhs);
            const auto c = a.coefficients(l, x);
            if (!a.zero(c[0])) {
                equation(Rule::AddBoth, a.normal(l - c[0]), a.normal(r - c[0]), -c[0], 0, i);
                l = a.normal(l - c[0]);
                r = a.normal(r - c[0]);
            }
            if (!a.same(c[1], gen(1)))
                equation(Rule::DivideBoth, a.normal(l / c[1]), a.normal(r / c[1]), c[1], 0, i);
        }
        finishCandidates();
    }
    void single() {
        auto e = current().branches[0].equations[0];
        const gen x = a.parse(a.input.variables[0]);
        gen l = a.parse(e.lhs), r = a.parse(e.rhs);
        if (l == x && a.rational(a.normal(r))) {
            if (current().conditions.empty())
                terminal(Rule::AlreadySolved, Conclusion::Finite);
            else
                finishCandidates();
            return;
        }
        if (r == x && a.rational(a.normal(l))) {
            equation(Rule::SwapSides, r, l);
            if (current().conditions.empty())
                terminal(Rule::AlreadySolved, Conclusion::Finite);
            else
                finishCandidates();
            return;
        }
        // Prefer the authored factor structure; no expand/refactor detour.
        if (a.zero(r) && l.type == giac::_SYMB) {
            std::vector<gen> factors;
            const auto op = l._SYMBptr->sommet;
            const auto &f = l._SYMBptr->feuille;
            if (op == giac::at_prod && f.type == giac::_VECT && f._VECTptr->size() == 2)
                factors.assign(f._VECTptr->begin(), f._VECTptr->end());
            if (op == giac::at_pow && f.type == giac::_VECT && f._VECTptr->size() == 2 &&
                f._VECTptr->back() == gen(2))
                factors = {f._VECTptr->front(), f._VECTptr->front()};
            if (factors.size() == 2) {
                const auto c0 = a.coefficients(factors[0], x), c1 = a.coefficients(factors[1], x);
                if (a.zero(c0[2]) && a.zero(c1[2]) && !a.zero(c0[1]) && !a.zero(c1[1])) {
                    productBranches({a.print(factors[0]), a.print(factors[1])});
                    return;
                }
            }
        }
        normalizeRows();
        ++group;
        e = current().branches[0].equations[0];
        l = a.parse(e.lhs);
        r = a.parse(e.rhs);
        auto lc = a.coefficients(l, x), rc = a.coefficients(r, x);
        if (a.zero(lc[2] - rc[2]) && a.zero(lc[1] - rc[1])) {
            terminal(a.same(l, r) ? Rule::Identity : Rule::Contradiction,
                     a.same(l, r) ? Conclusion::Identity : Conclusion::Empty);
            return;
        }
        if (a.zero(lc[2] - rc[2])) {
            const gen term = a.normal(-(rc[1] * x + rc[2] * x * x));
            if (!a.zero(term)) {
                equation(Rule::AddBoth, a.normal(l + term), a.normal(r + term), term);
                l = a.normal(l + term);
                r = a.normal(r + term);
            }
            const gen constant = a.normal(-lc[0]);
            if (!a.zero(constant)) {
                equation(Rule::AddBoth, a.normal(l + constant), a.normal(r + constant), constant);
                l = a.normal(l + constant);
                r = a.normal(r + constant);
            }
            const gen coeff = a.normal(lc[1] - rc[1]);
            if (!a.same(coeff, gen(1)))
                equation(Rule::DivideBoth, a.normal(l / coeff), a.normal(r / coeff), coeff);
            finishCandidates();
            return;
        }
        // Put quadratic in standard form without guessing from ordinary CAS roots.
        if (!a.zero(r) && (!a.zero(rc[2]) || !a.zero(lc[1] - rc[1]))) {
            equation(Rule::AddBoth, a.normal(l - r), gen(0), -r);
            l = a.normal(l - r);
            r = gen(0);
        }
        auto c = a.coefficients(l - r, x);
        const gen discriminant = a.normal(c[1] * c[1] - gen(4) * c[2] * c[0]);
        Rule method = Rule::QuadraticFormula;
        Step split;
        split.rule = method;
        if (a.zero(c[1])) {
            const gen leftConstant = a.coefficients(l, x)[0];
            if (!a.zero(leftConstant)) {
                equation(Rule::AddBoth, a.normal(l - leftConstant), a.normal(r - leftConstant),
                         -leftConstant);
                l = a.normal(l - leftConstant);
                r = a.normal(r - leftConstant);
            }
            if (!a.same(c[2], gen(1))) {
                equation(Rule::DivideBoth, a.normal(l / c[2]), a.normal(r / c[2]), c[2]);
                l = a.normal(l / c[2]);
                r = a.normal(r / c[2]);
            }
            split.rule = Rule::SquareRoots;
            split.operand = a.print(r);
        }
        a.tick();
        gen radical = giac::sqrt(discriminant, a.ctx);
        std::array<gen, 2> roots = {a.normal((-c[1] - radical) / (gen(2) * c[2])),
                                    a.normal((-c[1] + radical) / (gen(2) * c[2]))};
        if (split.rule == Rule::QuadraticFormula && a.rational(roots[0]) && a.rational(roots[1])) {
            // Useful exact rational factoring: product theorem, including c=0.
            const gen f1 = a.normal(c[2] * (x - roots[0])), f2 = a.normal(x - roots[1]);
            State fact = current();
            Step f;
            f.rule = Rule::Factor;
            f.auxiliaries = {a.print(f1), a.print(f2)};
            fact.branches[0].equations[0] = {"(" + a.print(f1) + ")*(" + a.print(f2) + ")", "0"};
            add(f, std::move(fact));
            productBranches(f.auxiliaries);
            return;
        } else if (split.rule == Rule::QuadraticFormula)
            split.auxiliaries = {a.print(c[2]), a.print(c[1]), a.print(c[0]),
                                 a.print(discriminant)};
        State next = current();
        if (!a.input.complex && a.negative(discriminant)) {
            next.conclusion = Conclusion::Empty;
            if (split.rule == Rule::SquareRoots) {
                split.rule = Rule::QuadraticFormula;
                const auto actual = a.coefficients(l - r, x);
                split.auxiliaries = {
                    a.print(actual[2]), a.print(actual[1]), a.print(actual[0]),
                    a.print(a.normal(actual[1] * actual[1] - gen(4) * actual[2] * actual[0]))};
            }
        } else {
            next.branches.clear();
            const unsigned count = a.zero(discriminant) ? 1 : 2;
            for (unsigned i = 0; i < count; ++i) {
                Branch b;
                b.id = uint8_t(i + 1);
                b.equations = {{a.print(x), a.print(roots[i])}};
                next.branches.push_back(std::move(b));
            }
        }
        add(split, std::move(next));
        if (current().conclusion == Conclusion::None)
            finishCandidates();
    }
    void system() {
        normalizeRows();
        const unsigned n = unsigned(a.input.variables.size());
        std::array<std::array<gen, 4>, 3> matrix;
        for (unsigned i = 0; i < n; ++i) {
            auto e = current().branches[0].equations[i];
            gen l = a.parse(e.lhs), r = a.parse(e.rhs);
            gen residual = a.expand(l - r), reconstructed(0), constant = residual;
            for (unsigned j = 0; j < n; ++j) {
                const gen v = a.parse(a.input.variables[j]);
                a.tick();
                matrix[i][j] = a.normal(giac::derive(residual, v, a.ctx));
                if (!a.rational(matrix[i][j]))
                    throw UnsupportedInput{__LINE__};
                reconstructed += matrix[i][j] * v;
                constant = a.sub(constant, v, gen(0));
            }
            constant = a.normal(constant);
            if (!a.rational(constant) || !a.same(residual, reconstructed + constant))
                throw UnsupportedInput{__LINE__};
            matrix[i][n] = -constant;
            // Both-side balancing, not an unexplained replacement by a residual row.
            const gen operand = a.normal(reconstructed - l);
            if (!a.zero(operand))
                equation(Rule::AddBoth, reconstructed, -constant, operand, i);
        }
        auto rowLhs = [&](unsigned i) {
            gen l(0);
            for (unsigned j = 0; j < n; ++j)
                l += matrix[i][j] * a.parse(a.input.variables[j]);
            return a.normal(l);
        };
        unsigned pivot = 0;
        for (unsigned col = 0; col < n && pivot < n; ++col) {
            ++group;
            unsigned selected = pivot;
            while (selected < n && a.zero(matrix[selected][col]))
                ++selected;
            if (selected == n)
                continue;
            if (selected != pivot) {
                State next = current();
                std::swap(next.branches[0].equations[pivot], next.branches[0].equations[selected]);
                Step s;
                s.rule = Rule::RowSwap;
                s.row = uint8_t(pivot);
                s.otherRow = uint8_t(selected);
                add(s, std::move(next));
                std::swap(matrix[pivot], matrix[selected]);
            }
            const gen divisor = matrix[pivot][col];
            if (!a.same(divisor, gen(1))) {
                for (unsigned j = 0; j <= n; ++j)
                    matrix[pivot][j] = a.normal(matrix[pivot][j] / divisor);
                equation(Rule::RowScale, rowLhs(pivot), matrix[pivot][n], divisor, pivot, 0, 0,
                         a.input.variables[col]);
            }
            for (unsigned i = 0; i < n; ++i)
                if (i != pivot && !a.zero(matrix[i][col])) {
                    const gen factor = -matrix[i][col];
                    for (unsigned j = 0; j <= n; ++j)
                        matrix[i][j] = a.normal(matrix[i][j] + factor * matrix[pivot][j]);
                    equation(Rule::RowAdd, rowLhs(i), matrix[i][n], factor, i, 0, pivot,
                             a.input.variables[col]);
                }
            ++pivot;
        }
        bool inconsistent = false;
        for (unsigned i = pivot; i < n; ++i)
            inconsistent = inconsistent || !a.zero(matrix[i][n]);
        terminal(Rule::SystemFinish, inconsistent ? Conclusion::Empty
                                     : pivot < n  ? Conclusion::Family
                                                  : Conclusion::Finite);
    }
    void run() {
        Vector<Condition> conditions;
        std::vector<gen> denominators;
        capture(a, conditions, denominators);
        if (a.input.variables.size() > 1 && !denominators.empty())
            for (const auto &v : denominators)
                if (!a.rational(a.normal(v)))
                    throw UnsupportedInput{__LINE__};
        for (const auto &c : conditions) {
            State next = current();
            next.conditions.push_back(c);
            Step s;
            s.rule = Rule::Domain;
            s.operand = c.nonzero;
            add(s, std::move(next));
        }
        if (!a.input.bindings.empty()) {
            State next = current();
            for (auto &e : next.branches[0].equations) {
                e.lhs = a.print(a.bound(a.parse(e.lhs)));
                e.rhs = a.print(a.bound(a.parse(e.rhs)));
            }
            Step s;
            s.rule = Rule::SubstituteValues;
            add(s, std::move(next));
        }
        if (a.input.variables.size() == 1) {
            gen multiplier(1);
            for (const auto &den : denominators) {
                a.tick();
                multiplier =
                    giac::_lcm(gen(giac::makevecteur(multiplier, den), giac::_SEQ__VECT), a.ctx);
            }
            multiplier = a.normal(multiplier);
            if (conditions.empty()) {
                const auto &e = current().branches[0].equations[0];
                const gen x = a.parse(a.input.variables[0]), l = a.parse(e.lhs), r = a.parse(e.rhs);
                // Already isolated rational values and pure squares need no denominator
                // clearing: doing so would immediately require the inverse operation.
                if ((a.same(l, x) && a.rational(a.normal(r))) ||
                    (a.same(r, x) && a.rational(a.normal(l))) ||
                    (a.same(l, x * x) && a.rational(a.normal(r))))
                    multiplier = gen(1);
            }
            if (!a.same(multiplier, gen(1))) {
                auto e = current().branches[0].equations[0];
                // Establish the supported resulting degree before publishing a
                // clearing step. Higher-degree rational problems are unsupported,
                // not failures of a mathematical rule.
                a.coefficients(a.normal((a.parse(e.lhs) - a.parse(e.rhs)) * multiplier),
                               a.parse(a.input.variables[0]));
                equation(Rule::ClearDenominators, a.normal(a.parse(e.lhs) * multiplier),
                         a.normal(a.parse(e.rhs) * multiplier), multiplier);
            }
            single();
        } else
            system();
    }
};
void groupPresentation(Derivation &d) {
    for (size_t i = 0; i < d.steps.size(); ++i) {
        auto &first = d.steps[i];
        if (first.relation != Relation::Equivalent)
            continue;
        size_t end = i + 1;
        while (end < d.steps.size() && end - i < 3 &&
               d.steps[end].relation == Relation::Equivalent && d.steps[end].group == first.group &&
               d.steps[end].branch == first.branch) {
            first.substeps.push_back(uint16_t(end));
            ++end;
        }
        i = end - 1;
    }
}
bool inputValid(const Snapshot &s) {
    if (s.authored.empty() || s.authored.size() > Limits::equations ||
        s.variables.size() != s.authored.size() || s.bindings.size() > 6 ||
        s.contextValues.size() > s.variables.size())
        return false;
    size_t bytes = 0;
    for (const auto &e : s.authored) {
        if (e.lhs.empty() || e.rhs.empty() || e.lhs.size() > Limits::sourceBytes ||
            e.rhs.size() > Limits::sourceBytes)
            return false;
        bytes += e.lhs.size() + e.rhs.size();
    }
    if (bytes > Limits::sourceBytes)
        return false;
    for (size_t i = 0; i < s.variables.size(); ++i) {
        if (!validIdentifier(s.variables[i]))
            return false;
        for (size_t j = 0; j < i; ++j)
            if (s.variables[i] == s.variables[j])
                return false;
    }
    // WHY: reject oversized or ambiguous snapshots before copying their owned
    // strings or parsing any identifier in the shared Giac context.
    for (size_t i = 0; i < s.bindings.size(); ++i) {
        const auto &b = s.bindings[i];
        if (b.variable.size() != 1 || b.variable[0] < 'A' || b.variable[0] > 'F' ||
            b.value.empty() || b.value.size() > Limits::expressionBytes)
            return false;
        for (size_t j = 0; j < i; ++j)
            if (s.bindings[j].variable == b.variable)
                return false;
    }
    for (size_t i = 0; i < s.contextValues.size(); ++i) {
        const auto &b = s.contextValues[i];
        if (std::find(s.variables.begin(), s.variables.end(), b.variable) == s.variables.end() ||
            b.value.empty() || b.value.size() > Limits::expressionBytes)
            return false;
        for (size_t j = 0; j < i; ++j)
            if (s.contextValues[j].variable == b.variable)
                return false;
    }
    return true;
}
void storedNames(const gen &g, giac::context *ctx, Vector<std::string> &names) {
    if (g.type == giac::_IDNT) {
        const auto name = g.print(ctx);
        if (name.size() == 1 && name[0] >= 'A' && name[0] <= 'F' &&
            std::find(names.begin(), names.end(), name) == names.end())
            names.push_back(name);
        return;
    }
    if (g.type == giac::_SYMB)
        storedNames(g._SYMBptr->feuille, ctx, names);
    else if (g.type == giac::_VECT)
        for (const auto &c : *g._VECTptr)
            storedNames(c, ctx, names);
    else if (g.type == giac::_FRAC) {
        storedNames(g._FRACptr->num, ctx, names);
        storedNames(g._FRACptr->den, ctx, names);
    }
}
bool rationalLiteral(const gen &g) {
    if (g.type == giac::_INT_ || g.type == giac::_ZINT)
        return true;
    if (g.type == giac::_FRAC)
        return rationalLiteral(g._FRACptr->num) && rationalLiteral(g._FRACptr->den);
    if (g.type != giac::_SYMB)
        return false;
    const auto op = g._SYMBptr->sommet;
    const auto &f = g._SYMBptr->feuille;
    if (op != giac::at_neg && op != giac::at_division && op != giac::at_inv)
        return false;
    if (f.type == giac::_VECT) {
        for (const auto &v : *f._VECTptr)
            if (!rationalLiteral(v))
                return false;
        return true;
    }
    return rationalLiteral(f);
}
Verdict replay(Algebra &a, const Derivation &d) {
    if (!inputValid(d.input) || d.states.empty() || d.states.size() != d.steps.size() + 1 ||
        d.steps.size() > Limits::steps || retainedBytes(d) > Limits::retainedBytes)
        return Verdict::Rejected;
    for (const auto &b : d.input.bindings) {
        if (b.variable.size() != 1 || b.variable[0] < 'A' || b.variable[0] > 'F')
            return Verdict::Rejected;
        const gen raw = a.parse(b.value);
        unsigned nodes = 0;
        sourceShape(raw, 0, nodes);
        if (!rationalLiteral(raw) || !a.rational(a.normal(raw)))
            return Verdict::Rejected;
    }
    const auto original = authoredState(d.input);
    if (!sameBranches(original.branches, d.states[0].branches) || !d.states[0].conditions.empty() ||
        d.states[0].conclusion != Conclusion::None)
        return Verdict::Rejected;
    Vector<Condition> conditions;
    std::vector<gen> den;
    capture(a, conditions, den);
    for (size_t i = 0; i < d.steps.size(); ++i) {
        if (d.steps[i].before != i || verifyStep(a, d, d.steps[i]) != Verdict::Verified)
            return Verdict::Rejected;
    }
    if (!sameConditions(conditions, d.states.back().conditions))
        return Verdict::Rejected;
    if (d.status == Status::Complete && d.states.back().conclusion == Conclusion::None)
        return Verdict::Rejected;
    return Verdict::Verified;
}
Verdict reconcile(Algebra &a, const Derivation &d, const StructuredSolveResult &answer) {
    if (!answer.ok())
        return Verdict::Unknown;
    const auto &s = d.states.back();
    if (s.conclusion == Conclusion::Empty)
        return answer.setKind == SolutionSetKind::NoSolution ? Verdict::Verified
                                                             : Verdict::Rejected;
    if (s.conclusion == Conclusion::Identity || s.conclusion == Conclusion::Family)
        return answer.setKind == SolutionSetKind::AllValues ? Verdict::Verified : Verdict::Rejected;
    std::vector<std::vector<gen>> values;
    for (const auto &b : s.branches) {
        if (b.status == BranchStatus::Rejected)
            continue;
        std::vector<gen> tuple;
        for (const auto &name : a.input.variables) {
            bool found = false;
            for (const auto &e : b.equations)
                if (a.same(a.parse(e.lhs), a.parse(name))) {
                    tuple.push_back(a.parse(e.rhs));
                    found = true;
                    break;
                }
            if (!found)
                return Verdict::Unknown;
        }
        values.push_back(std::move(tuple));
    }
    if (answer.setKind != SolutionSetKind::Solutions || answer.groups.size() != values.size())
        return Verdict::Rejected;
    for (const auto &tuple : values) {
        bool matched = false;
        for (const auto &group : answer.groups) {
            if (group.values.size() != tuple.size())
                continue;
            bool match = true;
            for (size_t i = 0; i < tuple.size(); ++i)
                match = match && group.values[i].variable == a.input.variables[i] &&
                        a.same(tuple[i], a.parse(group.values[i].exactText));
            matched = matched || match;
        }
        if (!matched)
            return Verdict::Rejected;
    }
    return Verdict::Verified;
}
} // namespace tutorkernel

tutor::Derivation GiacEngine::explainEquations(const tutor::Snapshot &request,
                                               const StructuredSolveResult &answer) {
    using namespace tutor;
    using namespace tutorkernel;
    Derivation d;
    if (!inputValid(request) || !begin())
        return d;
    CallGuard guard(_inCall);
    if (!guard.entered())
        return d;
    const auto start = std::chrono::steady_clock::now();
    const size_t priorLive = traceAllocations.live;
    traceAllocations.peak = priorLive;
    ConstructionBudget constructionBudget;
    try {
        d.input = request;
        d.input.engineGeneration = _generation;
        DiagnosticCapture captureLog(_state->ctx);
        // Capture only referenced stored values BEFORE quoting. No assignment,
        // purge, assumption change, locale lexer reset, or retained handle mutation.
        Vector<std::string> names;
        unsigned nodes = 0;
        for (const auto &e : d.input.authored)
            for (const auto &text : {e.lhs, e.rhs}) {
                gen g(text, _state->ctx);
                sourceShape(g, 0, nodes);
                storedNames(g, _state->ctx, names);
            }
        for (const auto &name : names) {
            auto it = std::find_if(d.input.bindings.begin(), d.input.bindings.end(),
                                   [&](const Binding &b) { return b.variable == name; });
            if (it == d.input.bindings.end()) {
                gen value = gen(name, _state->ctx).eval(1, _state->ctx);
                if (!rationalLiteral(value))
                    throw UnsupportedInput{__LINE__};
                d.input.bindings.push_back({name, value.print(_state->ctx)});
            }
        }
        d.input.contextValues.clear();
        for (const auto &name : d.input.variables) {
            gen value = gen(name, _state->ctx).eval(1, _state->ctx);
            d.input.contextValues.push_back({name, value.print(_state->ctx)});
            if (value.type == giac::_VECT && value.subtype == giac::_ASSUME__VECT)
                throw UnsupportedInput{__LINE__};
        }
        if (!inputValid(d.input))
            throw LimitReached{__LINE__};
        Scope scope(_state->ctx, d.input);
        // Only explicit rational stored-value snapshots are accepted; never assign
        // temporary values or modify the retained Grapher handles in the context.
        Algebra a{_state->ctx, d.input};
        a.measuredCalls = &d.metrics.symbolicCalls;
        for (auto &b : d.input.bindings) {
            const gen raw = a.parse(b.value);
            unsigned budget = 0;
            sourceShape(raw, 0, budget);
            if (b.variable.size() != 1 || b.variable[0] < 'A' || b.variable[0] > 'F' ||
                !rationalLiteral(raw) || !a.rational(a.normal(raw)))
                throw UnsupportedInput{__LINE__};
        }
        d.states.push_back(authoredState(d.input));
        Planner p{a, d};
        p.run();
        groupPresentation(d);
        d.status = Status::Complete;
        d.validity = replay(a, d);
        if (d.validity != Verdict::Verified)
            throw CheckFailure{};
        d.completeness = Verdict::Verified;
        d.candidates = Verdict::Verified;
        d.reconciliation = reconcile(a, d, answer);
        if (d.reconciliation == Verdict::Rejected) {
            d.status = Status::ReconciliationFailed;
            d.diagnostic = "reconciliation failure: replay corpus retains authored reproducer and "
                           "adapter answer";
        }
        d.metrics.symbolicCalls = a.calls;
    } catch (const LimitReached &e) {
        d.status = Status::Partial;
        d.diagnostic = "construction budget exhausted at " + std::to_string(e.line);
    } catch (const UnsupportedInput &e) {
        d.status = Status::Unsupported;
        d.diagnostic = "outside exact rational polynomial degree 2 / linear system contract at " +
                       std::to_string(e.line) + ": " + e.detail;
    } catch (const CheckFailure &) {
        d.status = Status::CheckFailed;
        d.validity = Verdict::Rejected;
    } catch (const std::bad_alloc &) {
        d.status = Status::Partial;
        // The allocation failure path must not allocate its own diagnostic.
        d.diagnostic.clear();
    } catch (...) {
        d.status = Status::CheckFailed;
        d.validity = Verdict::Unknown;
        d.diagnostic = "symbolic checker could not establish a proof";
    }
    if (retainedBytes(d) > Limits::retainedBytes) {
        d.states.clear();
        d.states.shrink_to_fit();
        d.steps.clear();
        d.steps.shrink_to_fit();
        d.status = Status::Partial;
        d.validity = d.completeness = d.candidates = Verdict::Unknown;
    }
    d.metrics.vectorHeapBytes = uint32_t(traceAllocations.live - priorLive);
    d.metrics.peakVectorHeapBytes = uint32_t(traceAllocations.peak - priorLive);
    d.metrics.retainedBytes = uint32_t(retainedBytes(d));
    d.metrics.elapsedMicros = uint32_t(std::chrono::duration_cast<std::chrono::microseconds>(
                                           std::chrono::steady_clock::now() - start)
                                           .count());
    return d;
}
tutor::Verdict GiacEngine::verifyDerivation(const tutor::Derivation &trace,
                                            const tutor::Snapshot &expected) {
    using namespace tutor;
    if (!tutorkernel::inputValid(trace.input) || !tutorkernel::inputValid(expected) ||
        fingerprint(trace.input) != fingerprint(expected) || !begin())
        return Verdict::Rejected;
    CallGuard guard(_inCall);
    if (!guard.entered())
        return Verdict::Unknown;
    try {
        DiagnosticCapture log(_state->ctx);
        tutorkernel::Scope scope(_state->ctx, trace.input);
        tutorkernel::Algebra a{_state->ctx, trace.input};
        return tutorkernel::replay(a, trace);
    } catch (...) {
        return Verdict::Unknown;
    }
}
bool GiacEngine::tutorSnapshotCurrent(const tutor::Snapshot &input, uint32_t epoch,
                                      bool complexDomain) {
    if (!tutorkernel::inputValid(input) || input.inputEpoch != epoch || input.engineGeneration != _generation ||
        input.complex != complexDomain || input.degrees != numos::angleModeIsDeg() || !begin())
        return false;
    CallGuard guard(_inCall);
    if (!guard.entered())
        return false;
    try {
        DiagnosticCapture log(_state->ctx);
        for (const auto &b : input.bindings)
            if (giac::gen(b.variable, _state->ctx).eval(1, _state->ctx).print(_state->ctx) !=
                b.value)
                return false;
        for (const auto &b : input.contextValues)
            if (giac::gen(b.variable, _state->ctx).eval(1, _state->ctx).print(_state->ctx) !=
                b.value)
                return false;
        return true;
    } catch (...) {
        return false;
    }
}
StructuredEngineResult GiacEngine::tutorFormula(const tutor::Equation &equation) {
    StructuredEngineResult out;
    if (!begin())
        return out;
    CallGuard guard(_inCall);
    if (!guard.entered())
        return out;
    try {
        DiagnosticCapture log(_state->ctx);
        giac::gen l(equation.lhs, _state->ctx), r(equation.rhs, _state->ctx);
        // Evaluate only pure typed mathematics to map parser subtraction/division
        // into the existing structured display boundary. No solve or simplification.
        tutor::Snapshot input;
        input.variables = {"x", "y", "z"};
        input.complex = true;
        tutorkernel::Scope scope(_state->ctx, input);
        tutorkernel::Algebra a{_state->ctx, input};
        tutor::State conditions;
        unsigned nodes = 0;
        // Original rational rows may still show a variable denominator: collect its
        // authored conditions for this presentation-only walk, without evaluating it.
        input.authored = {{equation.lhs, equation.rhs}};
        tutor::Vector<tutor::Condition> cs;
        std::vector<giac::gen> ds;
        try {
            tutorkernel::capture(a, cs, ds);
            conditions.conditions = cs;
        } catch (...) {
        }
        if (!tutorkernel::domainSafe(a, l, conditions, 0, nodes) ||
            !tutorkernel::domainSafe(a, r, conditions, 0, nodes))
            return out;
        int budget = kTreeNodeBudget;
        genToNode(giac::symb_equal(l.eval(1, _state->ctx), r.eval(1, _state->ctx)), _state->ctx,
                  out.tree, 0, budget);
        out.hasTree = out.tree.kind != EngineNodeKind::Unsupported;
        out.base.status = out.hasTree ? MathEngineStatus::Ok : MathEngineStatus::Unsupported;
    } catch (...) {
        out.base.status = MathEngineStatus::EvaluationError;
    }
    return out;
}
