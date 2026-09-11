// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Derivation.h"

namespace numos::tutor {
// A bounded projection of an immutable proof, not another derivation. No page
// vector is retained: resolving a page scans at most Limits::steps primitives.
enum class TeachingKind : uint8_t { Start, Transition, Chain, Coefficients,
    Discriminant, QuadraticFormula, Roots, Final };
struct TeachingPage {
    uint16_t first = 0, last = 0;
    TeachingKind kind = TeachingKind::Transition;
};
inline bool teachingNeedsOperand(const Step& s) {
    if(s.rule!=Rule::AddBoth && s.rule!=Rule::DivideBoth)return false;
    for(const auto& p:s.parameters)if(p.kind==ParameterKind::Expression)return false;
    return true;
}
inline bool terminalRule(Rule rule) {
    return rule == Rule::AlreadySolved || rule == Rule::Identity ||
           rule == Rule::Contradiction || rule == Rule::Finish || rule == Rule::SystemFinish;
}

inline bool quadraticHasUnchangedFinish(const Derivation& d, size_t index) {
    if (index + 1 >= d.steps.size() || d.status != Status::Complete ||
        d.validity != Verdict::Verified || d.completeness != Verdict::Verified ||
        d.candidates != Verdict::Verified || d.reconciliation != Verdict::Verified) return false;
    const auto& roots = d.steps[index];
    const auto& finish = d.steps[index + 1];
    if (roots.rule != Rule::QuadraticFormula || roots.verification != Verdict::Verified ||
        finish.rule != Rule::Finish || finish.verification != Verdict::Verified ||
        finish.relation != Relation::Terminal || finish.explanation != Message::Finish ||
        roots.after != finish.before || roots.after >= d.states.size() ||
        finish.after >= d.states.size() || !finish.introduced.empty() ||
        !finish.discharged.empty()) return false;
    const auto& before = d.states[roots.after];
    const auto& after = d.states[finish.after];
    if (before.conclusion != Conclusion::None || after.conclusion != Conclusion::Finite ||
        before.branches.empty() || before.branches.size() != after.branches.size() ||
        before.conditions.size() != after.conditions.size()) return false;
    // WHY: compare the checked model's exact mathematical payload and provenance,
    // never rendered formulas or prose. Only terminal classification may change.
    for (size_t b = 0; b < before.branches.size(); ++b) {
        const auto& a = before.branches[b]; const auto& z = after.branches[b];
        if (a.id != z.id || a.status != BranchStatus::Active || a.status != z.status ||
            a.equations.size() != z.equations.size()) return false;
        for (size_t e = 0; e < a.equations.size(); ++e)
            if (a.equations[e].lhs != z.equations[e].lhs ||
                a.equations[e].rhs != z.equations[e].rhs) return false;
    }
    for (size_t c = 0; c < before.conditions.size(); ++c) {
        const auto& a = before.conditions[c]; const auto& z = after.conditions[c];
        if (a.nonzero != z.nonzero || a.source.equation != z.source.equation ||
            a.source.side != z.source.side || a.source.children != z.source.children) return false;
    }
    return true;
}
template<class Visit> inline unsigned visitTeachingPages(const Derivation& d, bool guided, Visit visit) {
    unsigned count = 0;
    auto emit = [&](size_t first, size_t last, TeachingKind kind) {
        visit(count++, TeachingPage{uint16_t(first), uint16_t(last), kind});
    };
    if (!d.steps.empty() && d.input.authored.size() > 1) emit(0, 0, TeachingKind::Start);
    for (size_t i = 0; i < d.steps.size(); ++i) {
        const auto& step = d.steps[i];
        if (step.verification != Verdict::Verified || step.after >= d.states.size()) break;
        if (step.rule == Rule::QuadraticFormula) {
            emit(i, i, TeachingKind::Coefficients);
            emit(i, i, TeachingKind::Discriminant);
            if (d.states[step.after].conclusion != Conclusion::Empty) {
                emit(i, i, TeachingKind::QuadraticFormula);
                if (quadraticHasUnchangedFinish(d, i)) {
                    emit(i, i + 1, TeachingKind::Final);
                    ++i; // presentation spans both primitives; the proof is untouched
                } else if (guided) emit(i, i, TeachingKind::Roots);
            } else emit(i, i, TeachingKind::Final);
        } else if (terminalRule(step.rule)) emit(i, i, TeachingKind::Final);
        else {
            size_t last = i;
            const auto& before = d.states[step.before];
            // WHY: a summary chain needs one formula for its start and every
            // intermediate result. Cases, systems and domain changes keep their
            // own pages; none can disappear into a three-operation summary.
            if (!guided && !teachingNeedsOperand(step) && before.branches.size() == 1 &&
                before.branches[0].equations.size() == 1 && before.conditions.empty()) {
                for (auto child : step.substeps) {
                    if (child != last + 1 || child >= d.steps.size() || child - i >= 3) break;
                    const auto& candidate = d.steps[child];
                    if (teachingNeedsOperand(candidate) || candidate.verification != Verdict::Verified ||
                        candidate.relation != Relation::Equivalent ||
                        !d.states[candidate.after].conditions.empty()) break;
                    last = child;
                }
            }
            emit(i, last, last == i ? TeachingKind::Transition : TeachingKind::Chain);
            i = last;
        }
    }
    return count;
}
inline unsigned teachingPageCount(const Derivation& d, bool guided) {
    return visitTeachingPages(d, guided, [](unsigned, TeachingPage) {});
}
inline TeachingPage teachingPageAt(const Derivation& d, bool guided, unsigned index) {
    TeachingPage result;
    visitTeachingPages(d, guided, [&](unsigned n, TeachingPage page) { if (n == index) result = page; });
    return result;
}
inline unsigned teachingPageFor(const Derivation& d, bool guided, TeachingPage old) {
    unsigned result = 0; bool found = false;
    visitTeachingPages(d, guided, [&](unsigned n, TeachingPage page) {
        if (old.first >= page.first && old.first <= page.last) {
            if (!found || page.kind == old.kind) { result = n; found = true; }
        }
    });
    return result;
}
} // namespace numos::tutor
