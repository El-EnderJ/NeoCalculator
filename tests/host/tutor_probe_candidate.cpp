// Independent mathematical review runner. This is not a planner fixture.
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include "math/AngleModeRuntime.h"
#include "math/giac/GiacEngine.h"
bool setting_complex_enabled = true;
namespace {
std::vector<std::string> split(const std::string& text,char delim) {
    std::vector<std::string> out;std::istringstream in(text);std::string part;
    while(std::getline(in,part,delim))out.push_back(part);
    return out;
}
void refresh(numos::tutor::Derivation& d){for(auto& s:d.states)s.fingerprint=numos::tutor::fingerprint(s);}
std::string jsonString(const std::string& value) {
    std::string out="\"";
    for(unsigned char c:value){if(c=='"'||c=='\\'){out+='\\';out+=char(c);}else if(c=='\n')out+="\\n";else if(c=='\r')out+="\\r";else if(c=='\t')out+="\\t";else if(c>=32)out+=char(c);}
    return out+'"';
}
void mutation(numos::GiacEngine& engine,const char* name,numos::tutor::Derivation d,
              const numos::tutor::Snapshot& expected) {
    refresh(d);
    const auto verdict=engine.verifyDerivation(d,expected);
    std::cout<<"{\"mutation\":\""<<name<<"\",\"verdict\":"<<unsigned(verdict)<<"}\n";
}
int contextReview(){
    using namespace numos;using namespace numos::tutor;
    auto& engine=GiacEngine::instance();engine.begin();setAngleMode(vpam::AngleMode::DEG);
    engine.evaluate("A:=5");engine.evaluate("B:=17");engine.evaluate("x:=7");engine.evaluate("assume(z>0)");
    const auto generation=engine.generation();const auto assumption=engine.evaluate("abs(z)").exactText;
    auto graph=engine.compileNumeric("sin(x)","x",true);double before=0,after=0;
    bool ok=engine.evaluateNumeric(graph,30,before)&&std::abs(before-0.5)<1e-12;
    unsigned completed=0,unsupported=0;
    for(unsigned i=0;i<30;++i){
        Snapshot s;s.inputEpoch=i+1;s.degrees=true;s.variables={"x"};s.complex=i%3==2;
        s.authored=i%3==0?Vector<Equation>{{"A*x","10"}}:i%3==1?Vector<Equation>{{"sqrt(x)","2"}}:Vector<Equation>{{"x^2+1","0"}};
        auto answer=engine.solveStructured({s.authored[0].lhs,s.authored[0].rhs},"x",s.complex?SolveDomainPolicy::RealAndComplex:SolveDomainPolicy::RealOnly);
        auto d=engine.explainEquations(s,answer);
        if(d.status==Status::Complete){++completed;ok=ok&&engine.verifyDerivation(d,d.input)==Verdict::Verified;}else if(d.status==Status::Unsupported)++unsupported;else ok=false;
    }
    ok=ok&&completed==20&&unsupported==10&&engine.generation()==generation&&graph.valid()&&engine.evaluateNumeric(graph,30,after)&&before==after;
    ok=ok&&angleModeIsDeg()&&engine.evaluate("sin(30)").exactText=="1/2"&&engine.evaluate("A").exactText=="5"&&engine.evaluate("B").exactText=="17"&&engine.evaluate("x").exactText=="7"&&engine.evaluate("abs(z)").exactText==assumption;
    std::cout<<"{\"context_sequence\":30,\"complete\":"<<completed<<",\"unsupported\":"<<unsupported<<",\"retained_sample_before\":"<<before<<",\"retained_sample_after\":"<<after<<",\"pass\":"<<(ok?"true":"false")<<"}\n";
    engine.evaluate("purge(A)");engine.evaluate("purge(B)");engine.evaluate("purge(x)");engine.evaluate("purge(z)");setAngleMode(vpam::AngleMode::RAD);
    return ok?0:1;
}
int snapshotReview(){
    using namespace numos;using namespace numos::tutor;
    auto& engine=GiacEngine::instance();engine.begin();
    engine.evaluate("A:=17");engine.evaluate("B:=19");
    Snapshot input;input.inputEpoch=9;input.variables={"x"};input.authored={{"x","1"}};
    const auto answer=engine.solveStructured({"x","1"},"x",SolveDomainPolicy::RealOnly);
    const auto trace=engine.explainEquations(input,answer);const auto generation=engine.generation();
    bool pass=trace.status==Status::Complete;unsigned count=0;
    auto check=[&](const char* name,Snapshot invalid){
        auto forged=trace;forged.input=invalid;
        const auto verdict=engine.verifyDerivation(forged,invalid);
        const auto expectedVerdict=engine.verifyDerivation(trace,invalid);
        const auto current=engine.tutorSnapshotCurrent(invalid,invalid.inputEpoch,invalid.complex);
        const auto generated=engine.explainEquations(invalid,answer);
        const bool safe=engine.evaluate("A").exactText=="17"&&engine.evaluate("B").exactText=="19"&&engine.generation()==generation;
        const bool ok=verdict==Verdict::Rejected&&expectedVerdict==Verdict::Rejected&&!current&&
            generated.status==Status::Unsupported&&generated.input.authored.empty()&&safe;
        pass=pass&&ok;++count;
        std::cout<<"{\"snapshot_case\":\""<<name<<"\",\"verdict\":"<<unsigned(verdict)
                 <<",\"expected_verdict\":"<<unsigned(expectedVerdict)<<",\"side_effect_free\":"<<(safe?"true":"false")
                 <<",\"pass\":"<<(ok?"true":"false")<<"}\n";
    };
    auto bad=trace.input;bad.bindings={{"A",std::string(Limits::expressionBytes+1,'7')}};check("oversized_binding",bad);
    bad=trace.input;bad.contextValues={{"x",std::string(Limits::expressionBytes+1,'7')}};check("oversized_context_value",bad);
    bad=trace.input;bad.contextValues={{"x","x"},{"y","y"}};check("excessive_context_values",bad);
    bad=trace.input;bad.variables={"A:=999"};bad.contextValues.clear();check("effectful_solve_identifier",bad);
    bad=trace.input;bad.bindings={{"B:=999","1"}};check("effectful_binding_identifier",bad);
    bad=trace.input;bad.contextValues={{"A:=999","1"}};check("effectful_context_identifier",bad);
    bad=trace.input;bad.bindings={{"A","17"},{"A","18"}};check("duplicate_bindings",bad);
    bad=trace.input;for(unsigned i=0;i<7;++i)bad.bindings.push_back({"A","17"});check("excessive_bindings",bad);
    bad=trace.input;bad.authored.push_back({"x","2"});bad.variables.push_back("x");check("duplicate_variables",bad);
    bad=trace.input;bad.authored.push_back({"y","2"});bad.variables.push_back("y");bad.contextValues={{"x","x"},{"x","x"}};check("duplicate_context_values",bad);
    std::cout<<"{\"snapshot_cases\":"<<count<<",\"pass\":"<<(pass?"true":"false")<<"}\n";
    engine.evaluate("purge(A)");engine.evaluate("purge(B)");return pass?0:1;
}
}
int main(int argc,char** argv) {
    if(argc==2&&std::string(argv[1])=="--context-test")return contextReview();
    if(argc==2&&std::string(argv[1])=="--snapshot-test")return snapshotReview();
    if(argc<3){std::cerr<<"usage: tutor_probe_candidate 'lhs=rhs;lhs=rhs' 'x,y' [complex]\n";return 2;}
    using namespace numos;using namespace numos::tutor;
    auto& engine=GiacEngine::instance();engine.begin();
    Snapshot input;input.inputEpoch=1;input.complex=argc>3&&std::string(argv[3])=="complex";
    const bool bindingTest=argc>3&&std::string(argv[3])=="--binding-test";
    if(bindingTest){engine.evaluate("A:=5");input.bindings.push_back({"A","5"});}
    const auto solveVariables=split(argv[2],',');input.variables.assign(solveVariables.begin(),solveVariables.end());
    std::vector<SolveEquation> equations;
    for(const auto& text:split(argv[1],';')) {
        const auto eq=text.find('='); if(eq==std::string::npos)return 2;
        Equation e{text.substr(0,eq),text.substr(eq+1)};input.authored.push_back(e);
        equations.push_back({e.lhs,e.rhs});
    }
    const auto policy=input.complex?SolveDomainPolicy::RealAndComplex:SolveDomainPolicy::RealOnly;
    std::cerr<<"ANSWER_BEGIN\n"<<std::flush;
    auto answer=equations.size()==1?engine.solveStructured(equations[0],input.variables[0],policy)
        :engine.solveSystemStructured(equations,solveVariables,policy);
    std::cerr<<"TUTOR_BEGIN\n"<<std::flush;
    auto d=engine.explainEquations(input,answer);
    const auto expected=d.input;
    std::cout<<replayJson(d)<<'\n';
    std::cout<<std::flush;std::cerr<<"REPLAY_BEGIN\n"<<std::flush;
    std::cout<<"{\"replay_verdict\":"<<unsigned(engine.verifyDerivation(d,expected))<<"}\n";
    std::cout<<"{\"ordinary_answer\":true,\"status\":"<<unsigned(answer.status)<<",\"set_kind\":"<<unsigned(answer.setKind)<<",\"raw\":"<<jsonString(answer.rawExactText)<<",\"groups\":"<<answer.groups.size()<<",\"diagnostic\":"<<jsonString(answer.diagnostic)<<"}\n";
    if(d.status!=Status::Complete && d.status!=Status::ReconciliationFailed)return 0;
    if(bindingTest){
        auto m=d;m.input.bindings[0].value="B:=999";const auto forgedExpected=m.input;
        engine.evaluate("B:=17");mutation(engine,"assignment_binding_rejected",m,forgedExpected);
        const auto sentinel=engine.evaluate("B").exactText;
        std::cout<<"{\"context_check\":\"rejected_binding_preserves_B\",\"value\":\""<<sentinel<<"\",\"pass\":"<<(sentinel=="17"?"true":"false")<<"}\n";
        engine.evaluate("purge(B)");
        for(const auto& step:d.steps)if(step.rule==Rule::SubstituteValues){
            m=d;auto& lhs=m.states[step.after].branches[0].equations[0].lhs;lhs="("+lhs+")+0/(x-2)";
            mutation(engine,"substitution_introduces_pole",m,expected);
            m=d;m.states.resize(step.after+1);m.steps.resize(step.after);
            m.states.back().branches[0].status=BranchStatus::Rejected;
            State terminal=m.states.back();terminal.conclusion=Conclusion::Empty;m.states.push_back(terminal);
            Step finish;finish.rule=Rule::Finish;finish.relation=Relation::Terminal;finish.before=step.after;finish.after=step.after+1;
            finish.explanation=Message::EmptyAfterExclusions;finish.affected.push_back({0,2,{}});finish.verification=Verdict::Verified;m.steps.push_back(finish);
            mutation(engine,"substitution_rejects_valid_branch",m,expected);break;
        }
    }
    if(input.authored.size()==1 && input.authored[0].lhs=="1" && input.authored[0].rhs=="1") {
        auto m=d;m.states.resize(1);m.steps.clear();
        State changed=m.states[0];changed.branches[0].equations[0].lhs="x/x";
        State terminal=changed;terminal.conclusion=Conclusion::Identity;
        m.states.push_back(changed);m.states.push_back(terminal);
        Step collect;collect.rule=Rule::Collect;collect.relation=Relation::Equivalent;collect.before=0;collect.after=1;
        collect.explanation=Message::Collect;collect.auxiliaries={"1","1"};
        collect.parameters={{ParameterKind::Expression,"1"},{ParameterKind::Expression,"1"}};
        collect.affected.push_back({0,2,{}});
        collect.verification=Verdict::Verified;m.steps.push_back(collect);
        Step identity;identity.rule=Rule::Identity;identity.relation=Relation::Terminal;identity.before=1;identity.after=2;
        identity.explanation=Message::Identity;identity.verification=Verdict::Verified;m.steps.push_back(identity);
        m.steps.back().affected.push_back({0,2,{}});
        mutation(engine,"collect_introduces_undefined_zero",m,expected);
        m.states[1].branches[0].equations[0].lhs="1+0";
        m.states[2].branches[0].equations[0].lhs="1+0";
        mutation(engine,"collect_invents_adding_zero",m,expected);
    }
    if(input.authored.size()==1 && input.authored[0].lhs=="x^2" && input.authored[0].rhs=="x") {
        auto m=d;m.states.resize(1);m.steps.clear();
        State divided=m.states[0];divided.branches[0].equations[0]={"x","1"};
        State terminal=divided;terminal.conclusion=Conclusion::Finite;
        m.states.push_back(divided);m.states.push_back(terminal);
        Step division;division.rule=Rule::DivideBoth;division.relation=Relation::Equivalent;division.before=0;division.after=1;
        division.operand="x";division.explanation=Message::Divide;division.parameters={{ParameterKind::Expression,"x"}};division.verification=Verdict::Verified;m.steps.push_back(division);
        m.steps.back().affected.push_back({0,2,{}});
        Step finish;finish.rule=Rule::Finish;finish.relation=Relation::Terminal;finish.before=1;finish.after=2;
        finish.explanation=Message::Finish;finish.verification=Verdict::Verified;m.steps.push_back(finish);
        m.steps.back().affected.push_back({0,2,{}});
        mutation(engine,"division_by_x_loses_zero",m,expected);
    }
    Snapshot stale=expected;stale.inputEpoch++;
    mutation(engine,"stale_epoch",d,stale);
    bool balance=false,word=false,division=false,branch=false,row=false;
    for(size_t i=0;i<d.steps.size();++i){const auto& s=d.steps[i];
        if(!balance && s.rule==Rule::AddBoth){
            auto m=d;m.steps[i].operand="-("+s.operand+")";mutation(engine,"balance_wrong_sign",m,expected);
            m=d;
            m.states[s.after].branches[s.branch].equations[s.row].rhs="("+d.states[s.before].branches[s.branch].equations[s.row].rhs+")-("+s.operand+")";
            mutation(engine,"balance_wrong_sign_result",m,expected);
            engine.evaluate("A:=17");m=d;m.steps[i].operand="A:=999";
            mutation(engine,"assignment_operand_rejected",m,expected);
            const auto sentinel=engine.evaluate("A").exactText;
            std::cout<<"{\"context_check\":\"rejected_operand_preserves_A\",\"value\":\""<<sentinel<<"\",\"pass\":"<<(sentinel=="17"?"true":"false")<<"}\n";
            engine.evaluate("purge(A)");balance=true;
        }
        if(!word && !s.parameters.empty() && (s.rule==Rule::AddBoth||s.rule==Rule::DivideBoth||s.rule==Rule::QuadraticFormula)){
            auto m=d;m.steps[i].parameters[0].value="999";mutation(engine,"explanation_wrong_operand",m,expected);word=true;}
        if(!division && s.rule==Rule::DivideBoth){auto m=d;m.steps[i].operand=input.variables[0];if(!m.steps[i].parameters.empty())m.steps[i].parameters[0].value=input.variables[0];mutation(engine,"divide_by_variable",m,expected);division=true;}
        if(!branch && s.relation==Relation::ExhaustiveSplit && d.states[s.after].branches.size()>1){
            auto m=d;const auto lostId=m.states[s.after].branches.back().id;
            for(auto& state:m.states) for(size_t j=state.branches.size();j-->0;)if(state.branches[j].id==lostId)state.branches.erase(state.branches.begin()+j);
            mutation(engine,"missing_branch",m,expected);branch=true;}
        if(!row && s.rule==Rule::RowAdd){
            auto m=d;m.steps[i].operand="999";mutation(engine,"row_wrong_operand",m,expected);
            m=d;auto& changed=m.states[s.after].branches[s.branch].equations[s.row];changed.rhs="("+changed.rhs+")+1";
            mutation(engine,"row_wrong_result",m,expected);row=true;
        }
    }
    if(!d.states.empty()&&!d.states.back().conditions.empty()){
        auto m=d;for(auto& state:m.states)state.conditions.clear();
        for(auto& step:m.steps){step.prerequisites.clear();step.introduced.clear();step.discharged.clear();}
        mutation(engine,"removed_denominator_exclusions",m,expected);
        for(const auto& step:d.steps)if(step.rule==Rule::RejectCandidate){
            m=d;m.states.resize(step.before+1);m.steps.resize(step.before);
            State terminal=m.states.back();terminal.conclusion=Conclusion::Finite;m.states.push_back(terminal);
            Step forged;forged.rule=Rule::AlreadySolved;forged.relation=Relation::Terminal;forged.before=step.before;forged.after=step.before+1;
            forged.branch=step.branch;forged.row=step.row;forged.explanation=Message::AlreadySolved;
            forged.parameters={{ParameterKind::Variable,input.variables.front()}};
            forged.prerequisites=step.prerequisites;forged.affected=step.affected;forged.verification=Verdict::Verified;m.steps.push_back(forged);
            mutation(engine,"already_solved_keeps_excluded_root",m,expected);break;
        }
    }
    if(!d.steps.empty()){
        auto m=d;auto& s=m.steps[0];s.verification=Verdict::Verified;
        if(!m.states[s.after].branches.empty()&&!m.states[s.after].branches[0].equations.empty()){
            m.states[s.after].branches[0].equations[0].rhs="999";
            mutation(engine,"forged_verified_state",m,expected);
        }
    }
}
