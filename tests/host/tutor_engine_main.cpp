// SPDX-License-Identifier: GPL-3.0-or-later
#include "math/giac/GiacEngine.h"
#include <iostream>
#include <string>
bool setting_complex_enabled = false;
int main(int argc,char** argv){
 using namespace numos;using namespace numos::tutor;
 auto& engine=GiacEngine::instance();engine.begin();
 Snapshot s;s.inputEpoch=1;s.engineGeneration=engine.generation();std::vector<SolveEquation> equations;
 for(int i=1;i<argc;++i){std::string arg=argv[i];if(arg=="--complex"){s.complex=true;continue;}const auto pos=arg.find('=');if(pos==std::string::npos)return 2;s.authored.push_back({arg.substr(0,pos),arg.substr(pos+1)});equations.push_back({arg.substr(0,pos),arg.substr(pos+1)});}
 for(size_t i=0;i<equations.size();++i)s.variables.push_back(std::string(1,"xyz"[i]));
 if(equations.empty())return 2;
 const auto policy=s.complex?SolveDomainPolicy::RealAndComplex:SolveDomainPolicy::RealOnly;
 const auto answer=equations.size()==1?engine.solveStructured(equations[0],s.variables[0],policy):engine.solveSystemStructured(equations,std::vector<std::string>(s.variables.begin(),s.variables.end()),policy);
 auto d=engine.explainEquations(s,answer);
 std::cout<<replayJson(d)<<'\n';
 if(d.status==Status::Complete && engine.verifyDerivation(d,d.input)!=Verdict::Verified){std::cerr<<"independent replay failed\n";return 3;}
 return 0;
}
