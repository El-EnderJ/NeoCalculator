/*
 * Modified for NumOS / NeoCalculator in 2026 for ESP32-S3 and host integration.
 * See ../NUMOS_CHANGES.md for provenance and the file-by-file history.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// -*- mode:C++ ; compile-command: "g++-3.4 -I.. -g -c rpn.cc  -DIN_GIAC -DHAVE_CONFIG_H" -*-
#include "giacPCH.h"
/*
 *  Copyright (C) 2001,14 B. Parisse, Institut Fourier, 38402 St Martin d'Heres
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

using namespace std;
#include "rpn.h"
#include "symbolic.h"
#include "unary.h"
#include <algorithm>
#include "prog.h"
#include "usual.h"
#include "identificateur.h"
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#if !defined(NSPIRE) && !defined FXCG && !defined(__VISUALC__) && !defined(NUMWORKS) && !defined TICE// #ifndef NSPIRE
#include <dirent.h>
#ifndef __MINGW_H
#include <pwd.h>
#endif // MINGW
#endif // NSPIRE
#endif // UNISTD
#include "input_lexer.h"
#include "plot.h"
#include "tex.h"
#include "ti89.h"
#include "maple.h"
#include "misc.h"
#include "permu.h"
#include "intg.h"
#include "derive.h"
#include "sym2poly.h"
#include "input_parser.h"
#include "solve.h"
#include "subst.h"
#include "csturm.h"
#include "giacintl.h"

#ifdef GIAC_HAS_STO_38
#include "aspen.h"
//THPObj *EditMat(int);
#endif


#ifndef NO_NAMESPACE_GIAC
namespace giac {
#endif // ndef NO_NAMESPACE_GIAC

#ifndef T_RPN_OP
#define T_RPN_OP 0
#endif

  string printasconstant(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    return sommetstr;
  }
  gen return_undef(const gen &,GIAC_CONTEXT){
    return undef;
  }
  static const char _rpn_s []="rpn";
  static define_unary_function_eval2 (__rpn,&return_undef,_rpn_s,&printasconstant);
  define_unary_function_ptr5( at_rpn ,alias_at_rpn ,(unary_function_eval*)&__rpn,0,true);

  static const char _alg_s []="alg";
  static define_unary_function_eval2 (__alg,&return_undef,_alg_s,&printasconstant);
  define_unary_function_ptr5( at_alg ,alias_at_alg ,(unary_function_eval *)&__alg,0,true);

  static const char _NOP_s []="NOP";
  static define_unary_function_eval2_index (136,__NOP,&return_undef,_NOP_s,&printasconstant);
  define_unary_function_ptr5( at_NOP ,alias_at_NOP ,&__NOP,0,T_RPN_OP);

  static const char _IFTE_s []="IFTE";
  static define_unary_function_eval2_index (126,__IFTE,&return_undef,_IFTE_s,&printasconstant);
  define_unary_function_ptr5( at_IFTE ,alias_at_IFTE ,&__IFTE,0,T_RPN_OP);
  static const char _RPN_LOCAL_s []="RPN_LOCAL";
  static define_unary_function_eval2_index (130,__RPN_LOCAL,&return_undef,_RPN_LOCAL_s,&printasconstant);
  define_unary_function_ptr5( at_RPN_LOCAL ,alias_at_RPN_LOCAL ,&__RPN_LOCAL,0,T_RPN_OP);

  static const char _RPN_FOR_s []="RPN_FOR";
  static define_unary_function_eval2_index (132,__RPN_FOR,&return_undef,_RPN_FOR_s,&printasconstant);
  define_unary_function_ptr5( at_RPN_FOR ,alias_at_RPN_FOR ,&__RPN_FOR,0,T_RPN_OP);

  static const char _RPN_WHILE_s []="RPN_WHILE";
  static define_unary_function_eval2_index (134,__RPN_WHILE,&return_undef,_RPN_WHILE_s,&printasconstant);
  define_unary_function_ptr5( at_RPN_WHILE ,alias_at_RPN_WHILE ,&__RPN_WHILE,0,T_RPN_OP);

  static const char _RPN_CASE_s []="RPN_CASE";
  static define_unary_function_eval2_index (128,__RPN_CASE,&return_undef,_RPN_CASE_s,&printasconstant);
  define_unary_function_ptr5( at_RPN_CASE ,alias_at_RPN_CASE ,&__RPN_CASE,0,T_RPN_OP);
  gen symb_rpn_prog(const gen & args){
    return symbolic(at_rpn_prog,args);
  }
  static const char _rpn_prog_s []="rpn_prog";
  static define_unary_function_eval2_index (83,__rpn_prog,&return_undef,_rpn_prog_s,&printasconstant);
  define_unary_function_ptr5( at_rpn_prog ,alias_at_rpn_prog,&__rpn_prog,_QUOTE_ARGUMENTS,0);

  gen _VARS(const gen & args,const context * contextptr) {
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    bool val=is_one(args);
    bool valsto=is_minus_one(args);
    bool strng=args==2;
    bool strngeq=args==3;
    bool valonly=args==-2;
    vecteur res;
    if (contextptr){
      if (contextptr->globalcontextptr && contextptr->globalcontextptr->tabptr){
	sym_tab::const_iterator it=contextptr->globalcontextptr->tabptr->begin(),itend=contextptr->globalcontextptr->tabptr->end();
#if defined FXCG || defined TICE || defined NSPIRE_NEWLIB
	vecteur * keywordsptr=0;
#else
	vecteur * keywordsptr=keywords_vecteur_ptr();
#endif
	for (;it!=itend;++it){
	  lastprog_name(it->first,contextptr);
	  gen g=identificateur(it->first);
	  if (keywordsptr==0 || !equalposcomp(*keywordsptr,g)){
	    if (strng)
	      g=string2gen(it->first,false);
	    if (strngeq){
	      res.push_back(string2gen(it->first,false));
	      int t=it->second.type;
#if 0 // !defined GIAC_HAS_STO_38 && !defined FXCG && !defined TICE
	      if ( (t==_SYMB && it->second._SYMBptr->sommet!=at_program) || t==_FRAC || t<=_REAL || t==_VECT)
		g=_mathml(makesequence(it->second,1),contextptr);
	      else
#endif
		g=string2gen(it->second.print(contextptr),false);
	    }
	    if (valonly)
	      res.push_back(it->second);
	    else {
	      if (val)
		g=symbolic(at_equal,makesequence(g,it->second));
	      if (valsto)
		g=symb_sto(it->second,g);
	      res.push_back(g);
	    }
	  }
	}
      }
      return res;
    }
    if (!variables_are_files(contextptr)){
      lock_syms_mutex();  
      sym_string_tab::const_iterator it=syms().begin(),itend=syms().end();
      for (;it!=itend;++it){
	gen id=it->second;
	if (id.type==_IDNT && id._IDNTptr->value){
	  res.push_back(id);
	}
      }
      unlock_syms_mutex();  
      return res;
    }
    return undef;
  }

  static const char _VARS_s []="VARS";
  static define_unary_function_eval (__VARS,&_VARS,_VARS_s);
  define_unary_function_ptr( at_VARS ,alias_at_VARS ,&__VARS);

  gen purgenoassume(const gen & args,const context * contextptr){
    if (args.type==_VECT){
      vecteur & v=*args._VECTptr;
      vecteur res;
      for (unsigned i=0;i<v.size();++i)
	res.push_back(purgenoassume(v[i],contextptr));
      return res;
    }
    if (args.type!=_IDNT)
      return gensizeerr("Invalid purgenoassume "+args.print(contextptr));
    if (!contextptr)
      return _purge(args,0);
    const char * ch=args._IDNTptr->id_name;
    if (strlen(ch)==1){
      if (ch[0]=='O' && (series_flags(contextptr) & (1<<6)) )
	series_flags(contextptr) ^= (1<<6);
      if (ch[0]==series_variable_name(contextptr)){
	if (series_flags(contextptr) & (1<<5))
	  series_flags(contextptr) ^= (1<<5);
	if (series_flags(contextptr) & (1<<6))
	  series_flags(contextptr) ^= (1<<6);
      }
    }
    // purge a global variable
    sym_tab::iterator it=contextptr->tabptr->find(ch),itend=contextptr->tabptr->end();
    if (it==itend)
      return string2gen("No such variable "+args.print(contextptr),false);
    gen res=it->second;
    if (contextptr->previous)
      it->second=identificateur(it->first);
    else
      contextptr->tabptr->erase(it);
    if (res.is_symb_of_sommet(at_rootof))
      _purge(res,contextptr);
    return res;
  }

  gen _purge(const gen & args,const context * contextptr) {
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (rpn_mode(contextptr) && (args.type==_VECT)){
      if (!args._VECTptr->size())
	return gentoofewargs("purge");
      gen apurger=args._VECTptr->back();
      _purge(apurger,contextptr);
      args._VECTptr->pop_back();
      return gen(*args._VECTptr,_RPN_STACK__VECT);
    }
    if (args.type==_VECT)
      return apply(args,contextptr,_purge);
    if (args.is_symb_of_sommet(at_at)){
      gen & f = args._SYMBptr->feuille;
      if (f.type==_VECT && f._VECTptr->size()==2){
	gen m = eval(f._VECTptr->front(),eval_level(contextptr),contextptr);
	gen indice=eval(f._VECTptr->back(),eval_level(contextptr),contextptr);
	if (m.type==_MAP){
	  gen_map::iterator it=m._MAPptr->find(indice),itend=m._MAPptr->end();
	  if (it==itend)
	    return gensizeerr(gettext("Bad index")+indice.print(contextptr));
	  m._MAPptr->erase(it);
	  return 1;
	}
      }
    }
    if (contextptr && args.is_symb_of_sommet(at_rootof)){
      gen a=eval(args,1,contextptr);
      if (!a.is_symb_of_sommet(at_rootof))
	return gensizeerr(gettext("Bad rootof"));
      if (!contextptr->globalcontextptr->rootofs)
	contextptr->globalcontextptr->rootofs=new vecteur;
      gen Pmin=a._SYMBptr->feuille;
      if (Pmin.type!=_VECT || Pmin._VECTptr->size()!=2 || Pmin._VECTptr->front()!=makevecteur(1,0))
	return gensizeerr(gettext("Bad rootof"));
      Pmin=Pmin._VECTptr->back();
      vecteur & r =*contextptr->globalcontextptr->rootofs;
      for (unsigned i=0;i<r.size();++i){
	gen ri=r[i];
	if (ri.type==_VECT && ri._VECTptr->size()==2 && Pmin==ri._VECTptr->front()){
	  gen a=ri._VECTptr->back();
	  r.erase(r.begin()+i);
	  return _purge(a,contextptr);
	}
      }
      return 0;
    }
    if (args.type!=_IDNT)
      return symbolic(at_purge,args);
    // REMOVED! args.eval(eval_level(contextptr),contextptr); 
    if (contextptr){
      if (contextptr->globalcontextptr!=contextptr){ 
	// purge a local variable = set it to assume(DOM_SYMBOLIC)
	gen a2(_SYMB);
	a2.subtype=1;
	return sto(gen(makevecteur(a2),_ASSUME__VECT),args,contextptr);
      }
      return purgenoassume(args,contextptr);
    }
    if (args._IDNTptr->value){
      gen res=*args._IDNTptr->value;
      if (res.type==_VECT && res.subtype==_FOLDER__VECT){
	if (res._VECTptr->size()!=1)
	  return gensizeerr(gettext("Non-empty folder"));
      }
      delete args._IDNTptr->value;
      args._IDNTptr->value=0;
      return res;
    }
    else
      return string2gen(args.print(contextptr)+" not assigned",false);
  }
  static const char _purge_s []="purge";
  static define_unary_function_eval_quoted (__purge,&_purge,_purge_s);
  define_unary_function_ptr5( at_purge ,alias_at_purge,&__purge,_QUOTE_ARGUMENTS,0);

  gen _REPLACE(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    vecteur v(gen2vecteur(args));
    if (v.size()==3 && v[0].type==_STRNG && v[1].type==_STRNG && v[2].type==_STRNG){
      string s(*v[0]._STRNGptr),f(*v[1]._STRNGptr),rep(*v[2]._STRNGptr),res;
      int fs=int(f.size());
      for (;;){
	int pos=s.find(f);
	if (pos<0 || pos>int(s.size())-fs)
	  break;
	res += s.substr(0,pos)+rep;
	s=s.substr(pos+fs,int(s.size())-pos-fs);
      }
      res += s;
      return string2gen(res,false);
    }
    return gensizeerr(contextptr);
  }

  static const char _replace_s[]="replace";
  static define_unary_function_eval (__replace,&_REPLACE,_replace_s);
  define_unary_function_ptr5( at_replace ,alias_at_replace,&__replace,0,T_UNARY_OP);

  static string printasdivision(const gen & feuille,const char * s,GIAC_CONTEXT){
    if (feuille.type!=_VECT || feuille._VECTptr->size()!=2)
      return printsommetasoperator(feuille,s,contextptr);
    gen n=feuille._VECTptr->front();
    bool need=need_parenthesis(n);
    string res;
    if (need) res+='(';
    res += n.print(contextptr);
    if (need) res += ')';
    res += '/';
    gen f=feuille._VECTptr->back();
    if ( (f.type==_SYMB && ( f._SYMBptr->sommet==at_plus || f._SYMBptr->sommet==at_prod || f._SYMBptr->sommet==at_division || f._SYMBptr->sommet==at_inv  || need_parenthesis(f._SYMBptr->sommet) )) || (f.type==_CPLX) || (f.type==_MOD)){
      res += '(';
      res += f.print(contextptr);
      res += ')';
    }
    else
      res += f.print(contextptr);
    return res;
  }

  static gen symb_division(const gen & args){
    return symbolic(at_division,args);
  }
  gen _division(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if ( (args.type!=_VECT) || (args._VECTptr->size()!=2) )
      return symb_division(args);
    gen a=args._VECTptr->front(),b=args._VECTptr->back();
    if (a.is_approx()){
      gen b1;
      if (has_evalf(b,b1,1,contextptr) && b.type!=b1.type){
#ifdef HAVE_LIBMPFR
	if (a.type==_REAL){
	  gen b2=accurate_evalf(b,mpfr_get_prec(a._REALptr->inf));
	  if (b2.is_approx())
	    return (*a._REALptr)/b2;
	}
#endif
	return rdiv(a,b1,contextptr);
      }
    }
    if (b.is_approx()){
      gen a1;
      if (has_evalf(a,a1,1,contextptr) && a.type!=a1.type){
#ifdef HAVE_LIBMPFR
	if (b.type==_REAL){
	  gen a2=accurate_evalf(a,mpfr_get_prec(b._REALptr->inf));
	  if (a2.is_approx())
	    return a2/b;
	}
#endif
	return rdiv(a1,b,contextptr);
      }
    }
    return rdiv(a,b,contextptr);
  }
  static const char _division_s []="/";
  static define_unary_function_eval4_index (10,__division,&_division,_division_s,&printasdivision,&texprintsommetasoperator);
  define_unary_function_ptr( at_division ,alias_at_division ,&__division);

  gen _EXPM1(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type==_DOUBLE_ && g._DOUBLE_val < 0.01 && g._DOUBLE_val > -0.01 ) {
      double x=g._DOUBLE_val;
      double r=(((((x/5040+1./720)*x+1./120)*x+1./24)*x+1./6)*x+.5)*x+1;
      return x*r;
    }
    return exp(g,contextptr)-1;
  }
  static const char _expm1_s[]="expm1";
  static define_unary_function_eval (__expm1,&_EXPM1,_expm1_s);
  define_unary_function_ptr5( at_expm1 ,alias_at_expm1,&__expm1,0,T_UNARY_OP);

  gen _copysign(const gen & g0,GIAC_CONTEXT){
    if (g0.type==_STRNG && g0.subtype==-1) return g0;
    if (g0.type!=_VECT || g0.subtype!=_SEQ__VECT || g0._VECTptr->size()!=2)
      return gensizeerr(contextptr);
    return abs(g0._VECTptr->front(),contextptr)*sign(g0._VECTptr->back(),contextptr);
  }
  static const char _copysign_s[]="copysign";
  static define_unary_function_eval (__copysign,&_copysign,_copysign_s); 
  define_unary_function_ptr5( at_copysign ,alias_at_copysign,&__copysign,0,T_UNARY_OP);

  char * hp38_display_in_maj(const char * s){
    return (char *)s;
  }
  static gen symb_binary_minus(const gen & args){
    return symbolic(at_binary_minus,args);
  }
  gen _binary_minus(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if ( (args.type!=_VECT) || (args._VECTptr->size()!=2) )
      return symb_binary_minus(args);
    return args._VECTptr->front()-args._VECTptr->back();
  }
  static const char _binary_minus_s[]="-";
  static define_unary_function_eval4_index (6,__binary_minus,&_binary_minus,_binary_minus_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_binary_minus ,alias_at_binary_minus ,&__binary_minus);
  
  static string printasNTHROOT(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if (feuille.type==_VECT && feuille._VECTptr->size()==2 && abs_calc_mode(contextptr)!=38)
      return "surd("+feuille[1].print(contextptr)+","+feuille[0].print(contextptr)+")";
    // return '('+(printsommetasoperator(feuille," NTHROOT ",contextptr))+')';
    return printsommetasoperator(feuille," NTHROOT ",contextptr);
  }
  gen _NTHROOT(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT || args._VECTptr->size()!=2)
      return gensizeerr(contextptr);
    return _surd(makesequence(args._VECTptr->back(),args._VECTptr->front()),contextptr);
  }
  static const char _NTHROOT_s[]="NTHROOT";
  static define_unary_function_eval4 (__NTHROOT,&_NTHROOT,_NTHROOT_s,&printasNTHROOT,&texprintsommetasoperator);
  define_unary_function_ptr5( at_NTHROOT ,alias_at_NTHROOT,&__NTHROOT,0,T_POW);
  
  gen _SVL(const gen & args0,GIAC_CONTEXT){
    if ( args0.type==_STRNG && args0.subtype==-1) return  args0;
    if (!ckmatrix(args0))
      return gentypeerr(contextptr);
    gen args=evalf(args0,1,contextptr);
    return _svd(makesequence(args,-2),contextptr);
  }
  static const char _SVL_s[]="SVL";
  static define_unary_function_eval (__SVL,&_SVL,_SVL_s); 
  define_unary_function_ptr5( at_SVL ,alias_at_SVL,&__SVL,0,T_UNARY_OP_38);

  gen _COND(const gen & args0,GIAC_CONTEXT){
    if ( args0.type==_STRNG && args0.subtype==-1) return  args0;
    // COND(matrix,2) L2norm condition number
    // otherwise COLNORM(args0)*COLNORM(inv(args0))
    if (args0.type==_VECT && args0._VECTptr->size()==2){ 
      if (args0._VECTptr->back()==1)
	return _COND(args0._VECTptr->front(),contextptr);
      if (args0._VECTptr->back()==2){
	gen args=args0._VECTptr->front();
	if (!ckmatrix(args))
	  return gentypeerr(contextptr);
	double save_eps=epsilon(contextptr);
	epsilon(0.0,contextptr); // otherwise small singular values are cancelled and condition is infinity
	gen g=_SVL(args,contextptr);
	epsilon(save_eps,contextptr);
	if (is_undef(g)) return g;
	if (g.type!=_VECT)
	  return undef;
	vecteur & v =*g._VECTptr;
	int s=int(v.size());
	gen mina(plus_inf),maxa(0);
	for (int i=0;i<s;++i){
	  gen tmp=abs(v[i],contextptr);
	  if (ck_is_strictly_greater(mina,tmp,contextptr))
	    mina=tmp;
	  if (ck_is_strictly_greater(tmp,maxa,contextptr))
	    maxa=tmp;
	}
	return maxa/mina;
      }
      if (is_inf(args0._VECTptr->back())){
	gen args=evalf(args0._VECTptr->front(),1,contextptr);
	if (!is_squarematrix(args))
	  return gensizeerr(contextptr);
	gen invargs=inv(args,contextptr);
	if (is_undef(invargs))
	  return undef;
	return _rowNorm(args,contextptr)*_rowNorm(invargs,contextptr);
      }
    }
    gen args=evalf(args0,1,contextptr);
    if (!is_squarematrix(args))
      return gensizeerr(contextptr);
    gen invargs=inv(args,contextptr);
    if (is_undef(invargs))
      return undef;
    return _colNorm(args,contextptr)*_colNorm(invargs,contextptr);
    // return _colNorm(args,contextptr)*_rowNorm(args,contextptr)/abs(_det(args,contextptr),contextptr);
  }
  static const char _cond_s[]="cond";
  static define_unary_function_eval (__cond,&_COND,_cond_s); // FIXME
  define_unary_function_ptr5( at_cond ,alias_at_cond,&__cond,0,T_UNARY_OP);

  gen _INT(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type==_VECT)
      return apply(g,_INT,contextptr);
    if (g.type==_CPLX)
      return gen(_INT(*g._CPLXptr,contextptr),_INT(*(g._CPLXptr+1),contextptr));
    if (is_positive(g,contextptr))
      return _floor(g,contextptr);
    else {
      if (is_positive(-g,contextptr))
	return _ceil(g,contextptr);
      gen sg=sign(g,contextptr);
      return sg*_floor(g*sg,contextptr);//symbolic(at_when,makesequence(symbolic(at_superieur_egal,makesequence(g,0)),symbolic(at_floor,g),symbolic(at_ceil,g)));
    }
  }
  static const char _INT_s[]="IP";
  static define_unary_function_eval (__INT,&_INT,_INT_s);
  define_unary_function_ptr5( at_INT ,alias_at_INT,&__INT,0,T_UNARY_OP_38);

  gen _polar_complex(const gen & g,GIAC_CONTEXT){
    if (g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type!=_VECT)
      return makevecteur(abs(g,contextptr),arg(g,contextptr));
    if (g._VECTptr->size()!=2)
      return gensizeerr(contextptr);
    gen res= g._VECTptr->front();
    gen angle=g._VECTptr->back();
    return res*gen(cos(angle,contextptr),sin(angle,contextptr));
  }
#ifdef BCD
  static const char _polar_complex_s[]="\xe2\x88\xa1";
#else
  static const char _polar_complex_s[]="polar_complex";//"\xE6\xBC";//"∡"; // " polar_complex ";
#endif
  static define_unary_function_eval (__polar_complex,&_polar_complex,_polar_complex_s);//,&printsommetasoperator,&texprintsommetasoperator); 
  define_unary_function_ptr5( at_polar_complex ,alias_at_polar_complex,&__polar_complex,0,T_UNARY_OP);   // T_MOD
  static const char _ggb_ang_s []="ggb_ang"; // prefixed version of polar complex
  static define_unary_function_eval (__ggb_ang,&_polar_complex,_ggb_ang_s);
  define_unary_function_ptr5( at_ggb_ang ,alias_at_ggb_ang,&__ggb_ang,0,true);
  

#ifndef NO_NAMESPACE_GIAC
} // namespace giac
#endif // ndef NO_NAMESPACE_GIAC
