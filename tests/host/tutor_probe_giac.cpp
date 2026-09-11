// TUTOR-ENGINE-01: isolated vendored-Giac step-facility evidence, never production.
// Build/run: powershell -File scripts/tutor-probe-giac.ps1
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include "gen.h"
#include "global.h"
#include "unary.h"
#include "symbolic.h"
#include "solve.h"
namespace giac { void check_browser_functions(); }

bool setting_complex_enabled = true;
namespace {
struct Event { unsigned special; std::string format; std::vector<std::string> operands; };
std::vector<Event>* sink = nullptr;
void hook(unsigned special, const std::string& format, const giac::vecteur& args,
          const giac::context* ctx) {
    if (!sink) return;
    Event e{special,format,{}};
    for (const auto& a:args) e.operands.push_back(a.print(ctx));
    sink->push_back(std::move(e));
}
std::string json(const std::string& s) {
    std::string r="\"";
    for (unsigned char c:s) {
        if (c=='"' || c=='\\') { r+='\\'; r+=c; }
        else if (c=='\n') r+="\\n";
        else if (c=='\r') r+="\\r";
        else if (c=='\t') r+="\\t";
        else if (c>=32) r+=c;
    }
    return r+'"';
}
// WHY: callback and cout/cerr destinations are process globals, unlike context flags.
// This probe owns its fresh context. No production/shared context is created or touched.
struct ScopedProbe {
    giac::context& c; std::ostringstream text;
    std::ostream* oldLog; std::streambuf *oldOut,*oldErr;
    decltype(giac::my_gprintf) oldHook;
    int oldStep,oldLanguage,oldEval,oldXcas;
    bool oldAngle,oldApprox,oldComplex,oldVariables;
    ScopedProbe(giac::context& ctx,int step,int lang,bool useHook,
                std::vector<Event>& events):c(ctx),oldLog(giac::logptr(&c)),
        oldOut(std::cout.rdbuf(text.rdbuf())),oldErr(std::cerr.rdbuf(text.rdbuf())),
        oldHook(giac::my_gprintf),oldStep(giac::step_infolevel(&c)),
        oldLanguage(giac::language(&c)),oldEval(giac::eval_level(&c)),
        oldXcas(giac::xcas_mode(&c)),oldAngle(giac::angle_radian(&c)),
        oldApprox(giac::approx_mode(&c)),oldComplex(giac::complex_mode(&c)),
        oldVariables(giac::complex_variables(&c)) {
        giac::logptr(&text,&c); giac::step_infolevel(step,&c);
        giac::language(lang,&c); giac::eval_level(&c)=1;
        giac::xcas_mode(0,&c); giac::angle_radian(true,&c);
        giac::approx_mode(false,&c); giac::complex_mode(false,&c);
        giac::complex_variables(false,&c);
        sink=&events; giac::my_gprintf=useHook?hook:nullptr;
    }
    ~ScopedProbe() {
        giac::logptr(oldLog,&c); std::cout.rdbuf(oldOut); std::cerr.rdbuf(oldErr);
        giac::my_gprintf=oldHook; sink=nullptr;
        giac::step_infolevel(oldStep,&c); giac::language(oldLanguage,&c);
        giac::eval_level(&c)=oldEval; giac::xcas_mode(oldXcas,&c);
        giac::angle_radian(oldAngle,&c); giac::approx_mode(oldApprox,&c);
        giac::complex_mode(oldComplex,&c); giac::complex_variables(oldVariables,&c);
    }
};
void run(const char* label,const char* authored,const char* command,int step,int lang,bool useHook) {
    giac::context ctx;
    auto* oldLog=giac::logptr(&ctx); auto* oldOut=std::cout.rdbuf();auto* oldErr=std::cerr.rdbuf();
    auto oldHook=giac::my_gprintf;
    const int oldStep=giac::step_infolevel(&ctx),oldLanguage=giac::language(&ctx);
    const int oldEval=giac::eval_level(&ctx),oldXcas=giac::xcas_mode(&ctx);
    const bool oldAngle=giac::angle_radian(&ctx),oldApprox=giac::approx_mode(&ctx);
    const bool oldComplex=giac::complex_mode(&ctx),oldVariables=giac::complex_variables(&ctx);
    const int globalStep=giac::step_infolevel(nullptr),globalLanguage=giac::language(nullptr);
    std::vector<Event> events;
    std::string parsed, evaluated, result, logs; int resultType=-1;
    const auto start=std::chrono::steady_clock::now();
    {
        ScopedProbe guard(ctx,step,lang,useHook,events);
        giac::gen input(authored,&ctx);
        parsed=input.print(&ctx); evaluated=input.eval(1,&ctx).print(&ctx);
        giac::gen answer;
        if(std::string(label)=="direct_raw_solve" || std::string(label)=="direct_evaluated_solve") {
            const auto equation=std::string(label)=="direct_raw_solve"?input:input.eval(1,&ctx);
            answer=giac::gen(giac::solve(equation,giac::gen("x",&ctx),0,&ctx),giac::_LIST__VECT);
        } else if(std::string(label)=="direct_hook_control") {
            giac::gprintf(0,"CONTROL %gen",giac::vecteur(1,input),&ctx);
            answer=1;
        } else answer=giac::gen(command,&ctx).eval(1,&ctx);
        result=answer.print(&ctx); resultType=answer.type;
        logs=guard.text.str();
    }
    bool restored=giac::logptr(&ctx)==oldLog && std::cout.rdbuf()==oldOut && std::cerr.rdbuf()==oldErr &&
        giac::my_gprintf==oldHook && giac::step_infolevel(&ctx)==oldStep &&
        giac::eval_level(&ctx)==oldEval && giac::xcas_mode(&ctx)==oldXcas &&
        giac::angle_radian(&ctx)==oldAngle && giac::approx_mode(&ctx)==oldApprox &&
        giac::complex_mode(&ctx)==oldComplex && giac::complex_variables(&ctx)==oldVariables &&
        giac::language(&ctx)==oldLanguage && giac::step_infolevel(nullptr)==globalStep &&
        giac::language(nullptr)==globalLanguage;
    const auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"{\"case\":"<<json(label)<<",\"input\":"<<json(authored)<<",\"command\":"<<json(command)
      <<",\"level\":"<<step<<",\"language\":"<<lang<<",\"hook\":"<<(useHook?"true":"false")
      <<",\"parsed\":"<<json(parsed)<<",\"evaluated\":"<<json(evaluated)
      <<",\"answer_type\":"<<resultType<<",\"answer\":"<<json(result)<<",\"text\":"<<json(logs)
      <<",\"restored\":"<<(restored?"true":"false")<<",\"us\":"<<us<<",\"events\":[";
    for (size_t i=0;i<events.size();++i) {
        if(i)std::cout<<',';
        std::cout<<"{\"special\":"<<events[i].special<<",\"format\":"<<json(events[i].format)<<",\"operands\":[";
        for(size_t j=0;j<events[i].operands.size();++j){if(j)std::cout<<',';std::cout<<json(events[i].operands[j]);}
        std::cout<<"]}";
    }
    std::cout<<"]}\n";
}
}
int main(int argc,char** argv) {
    giac::check_browser_functions();
    if(argc==5 && std::string(argv[1])=="--case") {
        run(argv[2],argv[3],argv[4],2,0,true);
        return 0;
    }
    struct Case {const char *label,*authored,*command;};
    const Case cases[]={
        {"already_solved","x=1","solve(x=1,x)"},
        {"linear","3*x+5=20","solve(3*x+5=20,x)"},
        {"quadratic","x^2-5*x+6=0","solve(x^2-5*x+6=0,x)"},
        {"rational","(x^2-1)/(x-1)=0","solve((x^2-1)/(x-1)=0,x)"},
        {"domain_identity","x/x=1","solve(x/x=1,x)"},
        {"system","[x+y=3,x-y=1]","linsolve([x+y=3,x-y=1],[x,y])"},
        {"radical","sqrt(x+1)=x-1","solve(sqrt(x+1)=x-1,x)"},
        {"logarithmic","ln(x-1)=0","solve(ln(x-1)=0,x)"},
        {"rref","[[1,1,3],[1,-1,1]]","rref([[1,1,3],[1,-1,1]])"},
        {"derivative_control","sin(x)*exp(x)","diff(sin(x)*exp(x),x)"},
        {"integral_control","x*exp(x)","integrate(x*exp(x),x)"},
        {"printf_control","x+1","printf(\"CONTROL %gen\",x+1)"},
        {"variation_control","x^2","tabvar(x^2,x)"}
        ,{"direct_hook_control","x+1","<direct gprintf API control>"}
    };
    for (const auto& c:cases) for(int level: {0,1,2}) for(int language:{0,1,3})
        for(bool useHook:{false,true}) run(c.label,c.authored,c.command,level,language,useHook);
}
