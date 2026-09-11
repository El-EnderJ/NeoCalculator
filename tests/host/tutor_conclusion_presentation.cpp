// SPDX-License-Identifier: GPL-3.0-or-later
// Focused presentation checks using actual engine-checked derivations.
#include "math/giac/GiacEngine.h"
#include "math/tutor/TeachingPlan.h"
#include <cassert>
#include <iostream>
bool setting_complex_enabled = true;
using namespace numos;
using namespace numos::tutor;

int main() {
    auto& engine=GiacEngine::instance(); engine.begin();
    struct Fixture { const char* lhs; const char* rhs; bool complex, merge; };
    const Fixture cases[]={
        {"2*x^2+3*x-4","0",false,true},
        {"(x-1)^2","0",false,false},
        {"x^2+1","0",true,false},
        {"2*x^2+3*x+4","0",true,true},
        {"(x^2-1)/(x-1)","0",false,false},
        {"(2*x^2+3*x-4)/(x-10)","0",false,true}
    };
    unsigned transitions=0, guards=0;
    for(const auto& fixture:cases) {
        Snapshot input;input.variables={"x"};input.inputEpoch=1;
        input.complex=fixture.complex;input.authored={{fixture.lhs,fixture.rhs}};
        const auto answer=engine.solveStructured({fixture.lhs,fixture.rhs},"x",
            fixture.complex?SolveDomainPolicy::RealAndComplex:SolveDomainPolicy::RealOnly);
        const auto d=engine.explainEquations(input,answer);
        assert(d.status==Status::Complete && engine.verifyDerivation(d,d.input)==Verdict::Verified);
        const auto before=replayJson(d);
        unsigned merged=0;
        for(size_t i=0;i<d.steps.size();++i) if(quadraticHasUnchangedFinish(d,i)) {
            ++merged;
            auto reject=[&](const Derivation& changed) {assert(!quadraticHasUnchangedFinish(changed,i));++guards;};
            auto m=d;m.steps[i+1].verification=Verdict::Unknown;reject(m);
            m=d;m.completeness=Verdict::Unknown;reject(m);
            m=d;m.status=Status::Partial;reject(m);
            m=d;m.steps[i+1].rule=Rule::RejectCandidate;reject(m);
            m=d;m.steps[i+1].explanation=Message::EmptyAfterExclusions;reject(m);
            m=d;m.steps[i+1].introduced.push_back(0);reject(m);
            m=d;m.steps[i+1].discharged.push_back(0);reject(m);
            m=d;m.states[m.steps[i+1].after].branches.pop_back();reject(m);
            m=d;++m.states[m.steps[i+1].after].branches[0].id;reject(m);
            m=d;m.states[m.steps[i+1].after].branches[0].status=BranchStatus::Rejected;reject(m);
            m=d;m.states[m.steps[i+1].after].branches[0].equations[0].rhs="999";reject(m);
            if(!d.states[d.steps[i+1].after].conditions.empty()) {
                m=d;m.states[m.steps[i+1].after].conditions.clear();reject(m);
                m=d;m.states[m.steps[i+1].after].conditions[0].source.children.push_back(7);reject(m);
            }
        }
        assert(merged==unsigned(fixture.merge));
        for(bool guided:{true,false}) {
            Vector<unsigned> covered(d.steps.size(),0);
            visitTeachingPages(d,guided,[&](unsigned index,TeachingPage page) {
                for(unsigned i=page.first;i<=page.last;++i)++covered[i];
                if(page.kind==TeachingKind::Final && page.first!=page.last) {
                    assert(quadraticHasUnchangedFinish(d,page.first));
                    assert(d.steps[page.last].rule==Rule::Finish);
                    assert(d.states[d.steps[page.last].after].conclusion==Conclusion::Finite);
                    assert(teachingPageAt(d,guided,index).last==page.last);
                    const auto toggled=teachingPageAt(d,!guided,teachingPageFor(d,!guided,page));
                    assert(toggled.kind==TeachingKind::Final && toggled.first==page.first && toggled.last==page.last);
                }
                if(fixture.merge)assert(page.kind!=TeachingKind::Roots);
            });
            for(auto count:covered)assert(count>0);
        }
        assert(before==replayJson(d));
        transitions+=d.steps.size();
    }
    std::cout<<"{\"fixtures\":6,\"checked_steps\":"<<transitions
             <<",\"merge_guards_rejected\":"<<guards<<",\"trace_unchanged\":true}\n";
}
