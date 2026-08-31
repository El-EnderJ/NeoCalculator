/*
 * Modified for NumOS / NeoCalculator in 2026 for ESP32-S3 and host integration.
 * See ../NUMOS_CHANGES.md for provenance and the file-by-file history.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// -*- mode:C++ ; compile-command: "g++-3.4 -I.. -I../include -g -c usual.cc -Wall -D_I386_ -DHAVE_CONFIG_H -DIN_GIAC -msse" -*-
#include "giacPCH.h"
#include <errno.h>
#include <math.h>

/*
 *  Copyright (C) 2000,14 B. Parisse, Institut Fourier, 38402 St Martin d'Heres
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
#include <stdexcept>
#include <cmath>
#include <cstdlib>
#include "gen.h"
#include "identificateur.h"
#include "symbolic.h"
#include "poly.h"
#include "usual.h"
#include "series.h"
#include "modpoly.h"
#include "sym2poly.h"
#include "moyal.h"
#include "subst.h"
#include "gausspol.h"
#include "identificateur.h"
#include "ifactor.h"
#include "prog.h"
#include "rpn.h"
#include "plot.h"
#include "pari.h"
#include "tex.h"
#include "unary.h"
#include "intg.h"
#include "ti89.h"
#include "solve.h"
#include "alg_ext.h"
#include "lin.h"
#include "derive.h"
#include "series.h"
#include "misc.h"
#include "sparse.h"
#include "input_parser.h"
#include "giacintl.h"
#ifdef VISUALC
#include <float.h>
#endif
#ifdef HAVE_LIBGSL
#include <gsl/gsl_math.h>
#include <gsl/gsl_sf_gamma.h>
#include <gsl/gsl_sf_psi.h>
#include <gsl/gsl_sf_zeta.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_sf_erf.h>
#include <gsl/gsl_sf_expint.h>
#endif
//#ifdef TARGET_OS_IPHONE
//#include "psi.h"
//#endif
#ifdef USE_GMP_REPLACEMENTS
#undef HAVE_GMPXX_H
#undef HAVE_LIBMPFR
#undef HAVE_LIBPARI
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef NO_NAMESPACE_GIAC
namespace giac {
#endif // ndef NO_NAMESPACE_GIAC

  // must be declared before any function declaration with special handling
  vector<const unary_function_ptr *> & limit_tractable_functions(){
    static vector<const unary_function_ptr *> * ans = 0;
    if (!ans) ans=new vector<const unary_function_ptr *>;
    return * ans;
  }
  vector<gen_op_context> & limit_tractable_replace(){
    static vector<gen_op_context> * ans = 0;
    if (!ans) ans=new vector<gen_op_context>;
    return * ans;
  }
#ifdef HAVE_SIGNAL_H_OLD
  string messages_to_print ;
#endif

  gen frac_neg_out(const gen & g,GIAC_CONTEXT){
    if ( (is_integer(g) && is_strictly_positive(-g,contextptr)) || (g.type==_FRAC && (g._FRACptr->num.type<=_DOUBLE_ ) && is_strictly_positive(-g._FRACptr->num,contextptr)) )
      return symb_neg(-g);//symbolic(at_neg,-g);
    if (g.is_prod()){
      // count neg
      gen f=g._SYMBptr->feuille;
      vecteur fv(gen2vecteur(f));
      int count=0,fvs=int(fv.size());
      for (int i=0;i<fvs;++i){
	gen & fvi = fv[i];
	fvi=frac_neg_out(fvi,contextptr);
	if (fvi.is_neg()){
	  ++count;
	  fvi=fvi._SYMBptr->feuille;
	}
      }
      if (fvs==1)
	f=fv[0];
      else {
	if (f.type==_VECT && *f._VECTptr==fv) // nothing changed
	  f=g;
	else
	  f=symb_prod(fv);
      }
      if (count%2)
	return symb_neg(f); // symbolic(at_neg,f);
      else
	return f;
    }
    return g;
  }

  // utilities for trig functions
  enum { trig_deno=24 };

  static bool is_multiple_of_12(const gen & k0,int & l){
    if (!k0.is_integer())
      return false;
    gen k=smod(k0,trig_deno);
    if (k.type!=_INT_)
      return false;
    l=k.val+trig_deno/2;
    return true;
  }
  //grad
  static bool is_multiple_of_pi_over_12(const gen & a,int & l,GIAC_CONTEXT){
    if (is_zero(a,contextptr)){
      l=0;
      return true;
    }
    gen k;
    if (angle_radian(contextptr)){
      if (!contains(a,cst_pi))
	return false;
      k=derive(a,cst_pi,contextptr);
      if (is_undef(k) || !is_constant_wrt(k,cst_pi,contextptr) || !is_zero(ratnormal(a-k*cst_pi,contextptr)))
	return false;
      k=(trig_deno/2)*k;
      if (k.type==_SYMB)
	k=ratnormal(k,contextptr);
      /*
      gen k1=normal(rdiv(a*gen(trig_deno/2),cst_pi),contextptr);
      if (k!=k1)
	setsizeerr();
      */
    }
    else if(angle_degree(contextptr))
      k=rdiv(a,15,context0);
    //grad
    else 
      k=rdiv(a,rdiv(50,3),context0); //50/3 grads,  due to 200/12
    return is_multiple_of_12(k,l);
  }

  static bool is_rational(const gen & a,int &n,int &d){
    gen num,den;
    fxnd(a,num,den);
    if (num.type!=_INT_ || den.type!=_INT_)
      return false;
    n=num.val;
    d=den.val;
    return true;
  }
  // checking utility
  static bool check_2d_vecteur(const gen & args) {
    if (args.type!=_VECT)
      return false; // settypeerr(gettext("check_2d_vecteur"));
    if (args._VECTptr->size()!=2)
      return false; // setsizeerr(gettext("check_2d_vecteur"));
    return true;
  }

  // zero arg
  /*
  unary_function_constant __1(1);
  unary_function_ptr at_one (&__1);
  unary_function_constant __0(0);
  unary_function_ptr at_zero (&__0);
  */
  gen _constant_one(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    return 1;
  }
  static const char _constant_one_s []="1";
  static define_unary_function_eval (__constant_one,&_constant_one,_constant_one_s);
  define_unary_function_ptr( at_one ,alias_at_one ,&__constant_one);
  
  gen _constant_zero(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    return 0;
  }
  static const char _constant_zero_s []="0";
  static define_unary_function_eval (__constant_zero,&_constant_zero,_constant_zero_s);
  define_unary_function_ptr( at_zero ,alias_at_zero ,&__constant_zero);
  
  gen _rm_a_z(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
#if !defined RTOS_THREADX && !defined BESTA_OS && !defined FREERTOS && !defined FXCG && !defined TICE && !defined NSPIRE_NEWLIB
    if (variables_are_files(contextptr)){
      char a_effacer[]="a.cas";
      for (;a_effacer[0]<='z';++a_effacer[0]){
	unlink(a_effacer);
      }
    }
#endif
    for (char c='a';c<='z';c++){
      purgenoassume(gen(string(1,c),contextptr),contextptr);
    }
    return args;
  }
  static const char _rm_a_z_s []="rm_a_z";
  static define_unary_function_eval (__rm_a_z,&_rm_a_z,_rm_a_z_s);
  define_unary_function_ptr5( at_rm_a_z ,alias_at_rm_a_z,&__rm_a_z,0,true);

  gen _rm_all_vars(const gen & args,const context * contextptr){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    gen g=_VARS(args,contextptr);
    if (g.type!=_VECT)
      return g;
    vecteur & v=*g._VECTptr;
    const_iterateur it=v.begin(),itend=v.end();
    for (;it!=itend;++it){
      gen tmp=*it;
      if (tmp.is_symb_of_sommet(at_sto))
	tmp=tmp._SYMBptr->feuille[1];
      if (tmp.type==_IDNT && (tmp!=cst_pi) )
	purgenoassume(tmp,contextptr);
    }
    return g;
  }
  static const char _rm_all_vars_s []="rm_all_vars";
  static define_unary_function_eval (__rm_all_vars,&_rm_all_vars,_rm_all_vars_s);
  define_unary_function_ptr5( at_rm_all_vars ,alias_at_rm_all_vars,&__rm_all_vars,0,true);

  bool is_equal(const gen & g){
    return (g.type==_SYMB) && (g._SYMBptr->sommet==at_equal || g._SYMBptr->sommet==at_equal2);
  }

  gen apply_to_equal(const gen & g,const gen_op & f){
    if (g.type!=_SYMB || (g._SYMBptr->sommet!=at_equal && g._SYMBptr->sommet!=at_equal2) || g._SYMBptr->feuille.type!=_VECT)
      return f(g);
    vecteur & v=*g._SYMBptr->feuille._VECTptr;
    if (v.empty())
      return gensizeerr(gettext("apply_to_equal"));
    return symbolic(g._SYMBptr->sommet,makesequence(f(v.front()),f(v.back())));
  }

  gen apply_to_equal(const gen & g,gen (* f) (const gen &, GIAC_CONTEXT),GIAC_CONTEXT){
    if (g.type!=_SYMB || (g._SYMBptr->sommet!=at_equal && g._SYMBptr->sommet!=at_equal2) || g._SYMBptr->feuille.type!=_VECT)
      return f(g,contextptr);
    vecteur & v=*g._SYMBptr->feuille._VECTptr;
    if (v.empty())
      return gensizeerr(contextptr);
    return symbolic(g._SYMBptr->sommet,makesequence(f(v.front(),contextptr),f(v.back(),contextptr)));
  }

  // one arg
  gen _id(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    return args;
  }
  define_partial_derivative_onearg_genop(D_at_id,"D_at_id",_constant_one);
  static const char _id_s []="id";
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval3 (__id,&_id,(size_t)&D_at_idunary_function_ptr,_id_s);
#else
  static define_unary_function_eval3 (__id,&_id,D_at_id,_id_s);
#endif
  define_unary_function_ptr5( at_id ,alias_at_id,&__id,0,true);

  gen symb_not(const gen & args){
    return symbolic(at_not,args);
  }
  gen _not(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type==_VECT || args.type==_MAP){
      if (python_compat(contextptr)){
	if (args.type==_VECT && args._VECTptr->empty())
	  return 1;
	if (args.type==_MAP && args._MAPptr->empty())
	  return 1;
      }
      return apply(args,_not,contextptr);
    }
    return !equaltosame(args);
  }
  static const char _not_s []="not";
  static define_unary_function_eval_index (64,__not,&_not,_not_s);
  define_unary_function_ptr( at_not ,alias_at_not ,&__not);

  gen symb_neg(const gen & args){
    return symbolic(at_neg,args);
  }
  gen _neg(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    return -args;
  }
  define_partial_derivative_onearg_genop( D_at_neg,"D_at_neg",_neg);
  static const char _neg_s []="-";
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval3_index (4,__neg,&_neg,(size_t)&D_at_negunary_function_ptr,_neg_s);
#else
  static define_unary_function_eval3_index (4,__neg,&_neg,D_at_neg,_neg_s);
#endif
  define_unary_function_ptr( at_neg ,alias_at_neg ,&__neg);

  gen symb_inv(const gen & a){
    return symbolic(at_inv,a);
  }
  gen _inv(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if ((args.type!=_VECT) || ckmatrix(args))
      return inv(args,contextptr);
    if (args.subtype==_SEQ__VECT){
      iterateur it=args._VECTptr->begin(), itend=args._VECTptr->end();
      gen prod(1);
      for (;it!=itend;++it)
	prod = prod * (*it);
      return inv(prod,contextptr);
    }
    return apply(args,_inv,contextptr);
  }
  static const char _inv_s []="inv";
  static define_unary_function_eval_index (12,__inv,&_inv,_inv_s);
  define_unary_function_ptr5( at_inv ,alias_at_inv,&__inv,0,true);

  gen symb_ln(const gen & e){
    return symbolic(at_ln,e);
  }

  double my_ln(double arg){
#if 0 //def TICE
#define	sqrto2	0.707106781186548
    float x,z, z2, temp;
    int exp;
    
    if (arg<=0.0) {
      errno = EDOM;
      return -HUGE_VALF;
    }
    x = frexpf(arg, & exp);
    if (x<sqrto2){
      x *= 2;
      exp--;
    }
    // now x is in [sqrt(2)/2,sqrt(2)]
    // ln(x)=ln((1+z)/(1-z)) where z=(x-1)/(x+1) is such that |z|<=3-2*sqrt(2)
    // = z*sum( (z^2)^k*2/(2k+1),k=0..inf)
    // for float precision, k=4 is enough, relative precision 2e-9/(1-|z|^2)
    z = (x-1)/(x+1);
    z2 = z*z;
    // seq(2./(2k+1),k,0,5)=[2.0,0.666666666667,0.4,0.285714285714,0.222222222222,0.181818181818]
    temp = (((0.222222222222*z2+0.285714285714)*z2+0.4)*z2+0.666666666667)*z2+2.0;
    temp = temp*z + exp*M_LN2;
    return temp;
#else
#ifdef _SOFTMATH_H
    return std::giac_gnuwince_log(arg);
#else
    return std::log(arg);
#endif
#endif
  }

  gen ln(const gen & e,GIAC_CONTEXT){
    // if (abs_calc_mode(contextptr)==38 && do_lnabs(contextptr) && !complex_mode(contextptr) && (e.type<=_POLY || e.type==_FLOAT_) && !is_positive(e,contextptr)) return gensizeerr(contextptr);
    if (!escape_real(contextptr) && !complex_mode(contextptr) && (e.type<=_POLY ) && !is_positive(e,contextptr)) return gensizeerr(contextptr);
#if 0
    if (e.type==_FLOAT_){
#ifdef BCD
      if (!is_positive(e,contextptr))
	return fln(-e._FLOAT_val)+cst_ipi();
      return fln(e._FLOAT_val);
#else
      return ln(get_double(e._FLOAT_val),contextptr);
#endif
    }
#endif
    if (e.type==_DOUBLE_){
      if (e._DOUBLE_val==0)
	return minus_inf;
      if (e._DOUBLE_val>0){
        return my_ln(e._DOUBLE_val);
      }
      else {
	if (!escape_real(contextptr) && !complex_mode(contextptr))
	  *logptr(contextptr) << "Taking ln of negative real " << e << endl;
#ifdef _SOFTMATH_H
	return M_PI*cst_i+std::giac_gnuwince_log(-e._DOUBLE_val);
#else
	return M_PI*cst_i+std::log(-e._DOUBLE_val);
#endif
      }
    }
    if (e.type==_SPOL1){
      gen expo=e._SPOL1ptr->empty()?undef:e._SPOL1ptr->front().exponent;
      if (is_zero(expo))
	return series(*e._SPOL1ptr,*at_ln,0,contextptr);
    }
#if 0
    if (e.type==_REAL){
      if (is_positive(e,contextptr))
	return e._REALptr->log();
      else {
	if (!escape_real(contextptr) && !complex_mode(contextptr))
	  *logptr(contextptr) << "Taking ln of negative real " << e << endl;
	return gen((-e)._REALptr->log(),cst_pi);//(-e)._REALptr->log()+cst_pi*cst_i;
      }
    }
#endif
    if (e.type==_CPLX){ 
      if (e.subtype){
#ifdef _SOFTMATH_H
	return std::giac_gnuwince_log(gen2complex_d(e));
#else
	return std::log(gen2complex_d(e));
#endif
      }
#if 0
      if (e._CPLXptr->type==_REAL || e._CPLXptr->type==_FLOAT_){
	//grad
	int mode=get_mode_set_radian(contextptr);
	gen res(ln(abs(e,contextptr),contextptr),arg(e,contextptr));
	angle_mode(mode,contextptr);
	
	return res;
      }
#endif
      if (is_zero(*e._CPLXptr,contextptr)){
	if (is_one(*(e._CPLXptr+1)))
	  return cst_i*cst_pi_over_2;
	if (is_minus_one(*(e._CPLXptr+1)))
	  return -cst_i*cst_pi_over_2;
      }
    }
    if (is_squarematrix(e))
      return analytic_apply(at_ln,*e._VECTptr,contextptr);
    if (e.type==_VECT){
#ifdef NSPIRE
      if (e.subtype==_SEQ__VECT && e._VECTptr->size()==2)
	return _logb(e,contextptr);
#endif
      return apply(e,ln,contextptr);
    }
    if (is_zero(e,contextptr))
      return minus_inf; // calc_mode(contextptr)==1?unsigned_inf:minus_inf;
    if (is_one(e))
      return 0;
    if (is_minus_one(e))
      return cst_ipi();
    if (is_integer(e) && is_strictly_positive(-e,contextptr))
      return cst_ipi()+ln(-e,contextptr);
    if (is_undef(e))
      return e;
    if ( (e==unsigned_inf) || (e==plus_inf))
      return e;
    if (e==minus_inf)
      return unsigned_inf;
    if (is_equal(e))
      return apply_to_equal(e,ln,contextptr);
    if (e.type==_SYMB){
      if (e._SYMBptr->sommet==at_inv && e._SYMBptr->feuille.type!=_VECT)
	return -ln(e._SYMBptr->feuille,contextptr);
      if (e._SYMBptr->sommet==at_exp){ 
	if (is_real(e._SYMBptr->feuille,contextptr) ) 
	  return e._SYMBptr->feuille;
      }
    }
    if (e.type==_FRAC && e._FRACptr->num==1)
      return -ln(e._FRACptr->den,contextptr);
    gen a,b;
    if (is_algebraic_program(e,a,b))
      return symb_prog3(a,0,ln(b,contextptr));
    if (e.is_pow() && e._SYMBptr->feuille.type==_VECT && e._SYMBptr->feuille._VECTptr->size()==2){
      gen a=e._SYMBptr->feuille._VECTptr->front();
      gen b=e._SYMBptr->feuille._VECTptr->back();
      // ln(a^b)
      if (is_positive(a,contextptr))
	return b*ln(a,contextptr);
    }
    return symb_ln(e);
  }
  gen log(const gen & e,GIAC_CONTEXT){
    return ln(e,contextptr);
  }
  static const char _ln_s []="ln"; // Using C notation, log works also for natural
  static gen d_ln(const gen & args,GIAC_CONTEXT){
    return inv(args,contextptr);
  }
  define_partial_derivative_onearg_genop( D_at_ln,"D_at_ln",&d_ln);
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval3_index (18,__ln,&ln,(size_t)&D_at_lnunary_function_ptr,_ln_s);
#else
  static define_unary_function_eval3_index (18,__ln,&ln,D_at_ln,_ln_s);
#endif
  define_unary_function_ptr5( at_ln ,alias_at_ln,&__ln,0,true);

  gen log10(const gen & e,GIAC_CONTEXT){
#if 0
    if (e.type==_FLOAT_) {
      if (is_positive(e,contextptr)){
#ifdef BCD
	return flog10(e._FLOAT_val);
#else
	return log10(get_double(e._FLOAT_val),contextptr);
#endif
      }
      return ln(e,contextptr)/ln(10,contextptr);
    }
#endif
    if (e.type==_DOUBLE_ && e._DOUBLE_val>=0 ){
#ifdef _SOFTMATH_H
      return std::giac_gnuwince_log10(e._DOUBLE_val);
#else
      return std::log10(e._DOUBLE_val);
#endif
    }
    if ( e.type==_DOUBLE_ || (e.type==_CPLX && e.subtype)){
#ifdef _SOFTMATH_H
      return std::giac_gnuwince_log(gen2complex_d(e))/std::log(10.0);
#else
      return std::log(gen2complex_d(e))/std::log(10.0);
#endif
    }
#if 0
    if (e.type==_CPLX && (e._CPLXptr->type==_REAL || e._CPLXptr->type==_FLOAT_)){
      return gen(ln(abs(e,contextptr),contextptr),arg(e,contextptr))/ln(10,contextptr);
    }
#endif
    if (is_squarematrix(e))
      return analytic_apply(at_log10,*e._VECTptr,contextptr);
    if (e.type==_VECT){
#ifdef NSPIRE
      if (e.subtype==_SEQ__VECT && e._VECTptr->size()==2)
	return _logb(e,contextptr);
#endif
      return apply(e,log10,contextptr);
    }
    gen a,b;
    // if (abs_calc_mode(contextptr)==38 && has_evalf(e,a,1,contextptr)) return log10(a,contextptr);
    if (is_algebraic_program(e,a,b))
      return symb_prog3(a,0,log10(b,contextptr));
    int n=0; gen e1(e),q;
    if (is_integer(e1) && !is_zero(e1)){
      while (is_zero(irem(e1,10,q))){
	if (q.type==_ZINT)
	  e1=*q._ZINTptr;
	else
	  e1=q;
	++n;
      }
    }
    return rdiv(ln(e1,contextptr),ln(10,contextptr),contextptr)+n;
  }
  static const char _log10_s []="log10"; // Using C notation, log for natural
  static gen d_log10(const gen & args,GIAC_CONTEXT){
    return inv(args*ln(10,contextptr),contextptr);
  }
  define_partial_derivative_onearg_genop(D_at_log10,"D_at_log10",&d_log10);
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval3 (__log10,&log10,(size_t)&D_at_log10unary_function_ptr,_log10_s);
#else
  static define_unary_function_eval3 (__log10,&log10,D_at_log10,_log10_s);
#endif
  define_unary_function_ptr5( at_log10 ,alias_at_log10,&__log10,0,true);

  gen alog10(const gen & e,GIAC_CONTEXT){
#ifdef BCD
    if (e.type==_FLOAT_)
      return falog10(e._FLOAT_val);
#endif
    if (is_squarematrix(e))
      return analytic_apply(at_alog10,*e._VECTptr,0);
    if (e.type==_VECT)
      return apply(e,contextptr,alog10);
    if (is_equal(e))
      return apply_to_equal(e,alog10,contextptr);
    gen a,b;
    if (is_algebraic_program(e,a,b))
      return symb_prog3(a,0,alog10(b,contextptr));
    return pow(gen(10),e,contextptr);
  }
  static const char _alog10_s []="alog10"; 
  static define_unary_function_eval (__alog10,&alog10,_alog10_s);
  define_unary_function_ptr5( at_alog10 ,alias_at_alog10,&__alog10,0,true);

  gen symb_atan(const gen & e){
    return symbolic(at_atan,e);
  }
  static gen atanasln(const gen & e,GIAC_CONTEXT){
    return plus_one_half*cst_i*ln(rdiv(cst_i+e,cst_i-e,contextptr),contextptr);
  }

float pade_atan(float arg){
  // pade(atan(x),x,11,6)
  // (231*x^5+1190*x^3+1155*x)/(25*x^6+525*x^4+1575*x^2+1155)
  float argsq;
  float value;
  argsq = arg*arg;
  value = ((9.24*argsq+47.6)*argsq+46.2)/(((argsq+21)*argsq+63)*argsq+46.2);
  return(value*arg);
}

double my_atan(double arg) {
#if 0 // def TICE
#define sq2p1	2.41421356237309
#define sq2m1	0.414213562373095
#define pio2	1.57079632679489e0
#define pio4	0.785398163397448e0
  if (arg<0)
    return -my_atan(-arg);
  if (arg<sq2m1)
    return pade_atan(arg);
  if (arg>sq2p1)
    return pio2 - pade_atan(1.0/arg);
  return pio4+pade_atan((arg-1.0)/(arg+1.0));
#else
#ifdef _SOFTMATH_H
  return std::giac_gnuwince_atan(arg);
#else
  return std::atan(arg);
#endif
#endif
}

  
  gen atan(const gen & e0,GIAC_CONTEXT){
#if 0
    if (e0.type==_FLOAT_){
#ifdef BCD
      return fatan(e0._FLOAT_val,angle_mode(contextptr));
#else
      return atan(get_double(e0._FLOAT_val),contextptr);
#endif
    }
#endif
    //dbg_printf("atan %s %i\n",e0.print().c_str(),e0.type);
    if (e0.type==_DOUBLE_){
      double res=my_atan(e0._DOUBLE_val);
      //dbg_printf("atan %f\n",res);
      if (angle_radian(contextptr)) 
	return res;
      else if(angle_degree(contextptr))
        return res*rad2deg_d;
      else
        return res*rad2grad_d;
    }
    gen e=frac_neg_out(e0,contextptr);
    if (e.type==_SPOL1){
      gen expo=e._SPOL1ptr->empty()?undef:e._SPOL1ptr->front().exponent;
      if (is_positive(expo,contextptr))
	return series(*e._SPOL1ptr,*at_atan,0,contextptr);
    }
#if 0
    if (e.type==_REAL){
      if (angle_radian(contextptr)) 
	return e._REALptr->atan();
      else if(angle_degree(contextptr))
	return 180*e._REALptr->atan()/cst_pi;
      //grad
      else
        return 200*e._REALptr->atan()/cst_pi;
    }
#endif
    if ( (e.type==_CPLX) && (e.subtype )){
      if (angle_radian(contextptr)) 
	return no_context_evalf(atanasln(e,contextptr));
      else if(angle_degree(contextptr))
	      return no_context_evalf(atanasln(e,contextptr))*gen(rad2deg_d);
      //grad
      else
        return no_context_evalf(atanasln(e, contextptr))*gen(rad2grad_d);
    }
    if (is_squarematrix(e))
      return analytic_apply(at_atan,*e._VECTptr,contextptr);
    if (e.type==_VECT)
      return apply(e,atan,contextptr);
    if (is_zero(e,contextptr))
      return e;
    if (is_one(e)){
      if (angle_radian(contextptr)) 
	return rdiv(cst_pi,4,contextptr);
      else if(angle_degree(contextptr))
      return 45;
      //grad
      else
        return 50;
    }
    if (is_minus_one(e)){
      if (angle_radian(contextptr)) 
	return rdiv(-cst_pi,4,contextptr);
      else if(angle_degree(contextptr))
      return -45;
      //grad
      else
        return -50;
    }
    if (e==plus_sqrt3_3){
      if (angle_radian(contextptr)) 
	return rdiv(cst_pi,6,contextptr);
      else if(angle_degree(contextptr))
      return 30;
      //grad
      else
        return rdiv(100,3); //100/3 grads
    }
    if (e==plus_sqrt3){
      if (angle_radian(contextptr)) 
	return rdiv(cst_pi,3,contextptr);
      else if(angle_degree(contextptr))
      return 60;
      //grad
      else
        return rdiv(200,3); //200/3 grads
    }
    if (e==plus_inf){
      if (angle_radian(contextptr)) 
	return cst_pi_over_2;
      else if(angle_degree(contextptr))
      return 90;
      //grad
      else
        return 100;
    }
    if (e==minus_inf){
      if (angle_radian(contextptr)) 
	return -cst_pi_over_2;
      else if(angle_degree(contextptr))
      return -90;
      //grad
      else
        return -100;
    }
    if (is_undef(e))
      return e;
    if (e==unsigned_inf)
      return undef;
    gen a,b;
    if (is_algebraic_program(e,a,b))
      return symb_prog3(a,0,atan(b,contextptr));
    gen tmp=evalf_double(e,0,contextptr);
    if (tmp.type==_DOUBLE_){
      double ed=tmp._DOUBLE_val;
      // detect if atan is a multiples of pi/10
      gen edh=horner(makevecteur(-5,60,-126,60,-5),tmp*tmp);
      if (absdouble(edh._DOUBLE_val)<1e-7 &&
	  normal(horner(makevecteur(-5,60,-126,60,-5),e*e),contextptr)==0){
	int res=int(std::floor(std::atan(absdouble(ed))*10/M_PI+.5));
	if (res%2)
	  return (ed>0?res:-res)*(angle_radian(contextptr)?cst_pi/10:(angle_degree(contextptr)?gen(18):gen(20))); //grad
	else
	  return (ed>0?res/2:-res/2)*(angle_radian(contextptr)?cst_pi/5:(angle_degree(contextptr)?gen(36):gen(40))); //grad
      }
      edh=horner(makevecteur(-3,55,-198,198,-55,3),tmp*tmp);
      if (absdouble(edh._DOUBLE_val)<1e-7){      
	int res=int(std::floor(std::atan(absdouble(ed))*12/M_PI+.5));
	int den=12;
	int g=gcd(res,den);
	res /=g; den /=g;
	return (ed>0?res:-res)*(angle_radian(contextptr)?cst_pi/den:(angle_degree(contextptr)?gen(15*g):rdiv(50,3)*gen(g))); //grad   50/3*g grads
      }
      edh=horner(makevecteur(1,-6,1),ed*ed);
      if (absdouble(edh._DOUBLE_val)<1e-7 &&
	  normal(horner(makevecteur(1,-6,1),e*e),contextptr)==0){
	int res=int(std::floor(std::atan(absdouble(ed))*8/M_PI+.5));
	return (ed>0?res:-res)*(angle_radian(contextptr)?cst_pi/8:(angle_degree(contextptr)?gen(45)/2:gen(25))); //grad 
      }
    }
    if ((e.type==_SYMB) && (e._SYMBptr->sommet==at_neg))
      return -atan(e._SYMBptr->feuille,contextptr);
    if ( (e.type==_INT_) && (e.val<0) )
      return -atan(-e,contextptr);
    if (is_equal(e))
      return apply_to_equal(e,atan,contextptr);
    vecteur v1(loptab(e,sincostan_tab));
    if ((series_flags(contextptr)&8)==0 && v1.size()>1){
      gen e1=ratnormal(_trigtan(e,contextptr),contextptr);
      if (loptab(e1,sincostan_tab).size()<=1)
	return atan(e1,contextptr);
    }
    // if (e.is_inv()) return sign(e._SYMBptr->feuille,contextptr)*cst_pi/2-atan(e._SYMBptr->feuille,contextptr);
    if (e.is_tan()){
      if (atan_tan_no_floor(contextptr))
	return e._SYMBptr->feuille;
      gen tmp=cst_pi;
      if(!angle_radian(contextptr))
      {
        if(angle_degree(contextptr))
	  tmp=180;
        //grad
        else
          tmp = 200;
      }
      gen tmp2=evalf(e._SYMBptr->feuille,1,contextptr);
      if (tmp2.type<_IDNT)
	tmp2=_floor(tmp2/tmp+plus_one_half,contextptr);
      else
	tmp2=_floor(e._SYMBptr->feuille/tmp+plus_one_half,contextptr);
#if 0
      if (tmp2.type==_FLOAT_)
	tmp2=get_int(tmp2._FLOAT_val);
#endif
      return operator_minus(e._SYMBptr->feuille,tmp2*tmp,contextptr);
    }
    vecteur ve=lvar(e);
    if (ve.size()==1){
      // atan((1+t)/(1-t))=atan((tan(pi/4)+t)/(1-tan(pi/4+t)))=atan(tan(pi/4+atan(t)))
      gen t=ve.front();
      gen test=(1+t)/(1-t);
      test=ratnormal(e/test,contextptr);
      if (is_one(test))
	return atan(symb_tan(cst_pi/4+atan(t,contextptr)),contextptr);
      if (is_minus_one(test))
	return -atan(symb_tan(cst_pi/4+atan(t,contextptr)),contextptr);
      test=(-1+t)/(1+t);
      test=ratnormal(e/test,contextptr);
      if (is_one(test))
	return atan(symb_tan(-cst_pi/4+atan(t,contextptr)),contextptr);
      if (is_minus_one(test))
	return -atan(symb_tan(-cst_pi/4+atan(t,contextptr)),contextptr);
    }
    return symb_atan(e);
  }
  static gen d_atan(const gen & args,GIAC_CONTEXT){
    gen g=inv(1+pow(args,2),contextptr);
    if (angle_radian(contextptr))
      return g;
    else if(angle_degree(contextptr))
      return g*rad2deg_e;
    //grad
    else
      return g*rad2grad_e;
  }
  define_partial_derivative_onearg_genop( D_at_atan," D_at_atan",&d_atan);
  static gen taylor_atan (const gen & lim_point,const int ordre,const unary_function_ptr & f, int direction,gen & shift_coeff,GIAC_CONTEXT){
    if (ordre<0)
      return 0; // no symbolic preprocessing
    shift_coeff=0;
    if (!is_inf(lim_point))
      return taylor(lim_point,ordre,f,0,shift_coeff,contextptr);
    vecteur v;
    identificateur x(" ");
    taylor(atan(x,contextptr),x,0,ordre,v,contextptr);
    v=negvecteur(v);
    v.front()=atan(lim_point,contextptr);
    return v;
  }
  static const char _atan_s []="atan";
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval_taylor_index (42,__atan,&atan,(size_t)&D_at_atanunary_function_ptr,&taylor_atan,_atan_s);
#else
  static define_unary_function_eval_taylor_index (42,__atan,&atan,D_at_atan,&taylor_atan,_atan_s);
#endif
  define_unary_function_ptr5( at_atan ,alias_at_atan,&__atan,0,true);

  gen symb_exp(const gen & e){
    return symbolic(at_exp,e);
  }
  static gen numeric_matrix_exp(const gen & e,double eps,GIAC_CONTEXT){
    gen res=midn(int(e._VECTptr->size()));
    gen eee(e);
    for (double j=2;j<max_numexp && linfnorm(eee,contextptr)._DOUBLE_val>eps;++j){
      res = res + eee;
      eee = gen(1/j) * eee * e ; 
    }
    return res;
  }

  gen expi(const gen &g,GIAC_CONTEXT){
    return exp(cst_i*g,contextptr);
  }

  double my_exp(double arg){
#if 0 // def TICE
#define	log2e	1.44269504088896
#define	sqrt2	1.41421356237310
#define	maxf	100 // was 10000
      float fraction;
      float temp1, temp2, xsq;
      int ent;
      if (arg==0.0)
        return 1.0;
      if (arg<-maxf){
        return 0.0;
      }
      if (arg>maxf){
        errno = ERANGE;
        return HUGE_VALF;
      }
      // 1* 1 floor 2 -
      arg *= log2e;
      ent = floor( arg );
      fraction = arg - ent - 0.5;
      fraction *= M_LN2;
      xsq = fraction * fraction;
      // pade(exp(x),x,6,4) relative error<6e-9
      // (-x^3-12*x^2-60*x-120)/(x^3-12*x^2+60*x-120)
      temp1=(xsq+60.0)*fraction;
      temp2=12.0*(xsq+10.0);
      return ldexp( sqrt2 * (temp2+temp1) / (temp2-temp1), ent );
#else
#ifdef _SOFTMATH_H
    return std::giac_gnuwince_sin(arg);
#else
    return std::exp(arg);
#endif
#endif
  }    
  
  gen exp(const gen & e0,GIAC_CONTEXT){
#if 0
    if (e0.type==_FLOAT_){
#ifdef BCD
      return fexp(e0._FLOAT_val);
#else
      return exp(get_double(e0._FLOAT_val),contextptr);
#endif
    }
#endif
    if (e0.type==_DOUBLE_){
      return my_exp(e0._DOUBLE_val);
    }
    if (is_integer(e0) && is_strictly_greater(0,e0,contextptr))
      return symb_inv(symb_exp(-e0));
    gen e=frac_neg_out(e0,contextptr);
    if (e.type==_SPOL1){
      gen expo=e._SPOL1ptr->empty()?undef:e._SPOL1ptr->front().exponent;
      if (is_positive(expo,contextptr))
	return series(*e._SPOL1ptr,*at_exp,0,contextptr);
    }
    //if (e.type==_REAL) return e._REALptr->exp();
    if (e.type==_CPLX){ 
      if (e.subtype){
#ifdef _SOFTMATH_H
	return std::giac_gnuwince_exp(gen2complex_d(e));
#else
	return std::exp(gen2complex_d(e));
#endif
      }
#if 0
      if (e._CPLXptr->type==_REAL || e._CPLXptr->type==_FLOAT_){
	//grad
	int mode=get_mode_set_radian(contextptr);
	gen res=exp(*e._CPLXptr,contextptr)*gen(cos(*(e._CPLXptr+1),contextptr),sin(*(e._CPLXptr+1),contextptr));
	angle_mode(mode,contextptr);
	
	return res;
      }
#endif
    }
    if (e.type==_VECT){
      if (is_squarematrix(e)){ 
	// check for numeric entries -> numeric exp
	if (is_fully_numeric(e))
	  return numeric_matrix_exp(e,epsilon(contextptr),contextptr);
	return analytic_apply(at_exp,*e._VECTptr,contextptr);
      }
      return apply(e,contextptr,exp);
    }
    if (is_zero(e,contextptr))
      return 1;
    if (is_undef(e) || e==plus_inf)
      return e;
    if (e==unsigned_inf)
      return undef;
    if (e==minus_inf)
      return 0;
    if (e.type==_SYMB && e._SYMBptr->sommet==at_ln)
      return e._SYMBptr->feuille;
    if (e.type==_SYMB && e._SYMBptr->sommet==at_neg && e._SYMBptr->feuille.type==_SYMB && e._SYMBptr->feuille._SYMBptr->sommet==at_ln)
      return inv(e._SYMBptr->feuille._SYMBptr->feuille,contextptr);
    gen a,b;
    if (is_algebraic_program(e,a,b))
      return symb_prog3(a,0,exp(b,contextptr));
    int k;
    if (simplify_sincosexp_pi && contains(e,cst_pi)){ // if (!approx_mode(contextptr)) 
      gen a,b;
      if (is_linear_wrt(e,cst_pi,a,b,contextptr) && !is_zero(a)){ 
	if (is_multiple_of_12(a*cst_i*gen(trig_deno/2),k))
	  return gen(*table_cos[k],(*table_cos[(k+6)%24]))*exp(b,contextptr);
	else {
	  gen kk;
	  kk=normal(a*cst_i,contextptr);
	  if (is_assumed_integer(kk,contextptr)){ 
	    if (is_assumed_integer(normal(rdiv(kk,plus_two,contextptr),contextptr),contextptr))
	      return exp(b,contextptr);
	    else
	      return pow(minus_one,kk,contextptr)*exp(b,contextptr);
	  }
	  int n,d,q,r;
	  if (is_rational(kk,n,d)){
	    if (b==0 && (d==5 || d==10) && calc_mode(contextptr)!=1)
	      return gen(cos(kk*cst_pi,contextptr),-sin(kk*cst_pi,contextptr));
	    if (d<7){ 
	      q=-n/d;
	      r=-n%d;
	      if (q%2)
		q=-1;
	      else
		q=1;
	      if (d<0){ r=-r; d=-d; }
	      if (r<0) r += 2*d;
	      // exp(r*i*pi/d) -> use rootof([1,..,0],cyclotomic(2*d))
	      vecteur vr(r+1);
	      vr[0]=1;
	      vecteur vc(cyclotomic(2*d));
	      if (!is_undef(vc))
		return q*symb_rootof(vr,vc,contextptr)*exp(b,contextptr);
	      // initially it was return q*symb_exp(r*(cst_pi*cst_i/d));
	    }
	  }
	} // end else multiple of pi/12
      } // end is_linear_wrt
    } // end if contains(e,_IDNT_pi)
    if (is_equal(e))
      return apply_to_equal(e,exp,contextptr);
    return symb_exp(e);
  }
  define_partial_derivative_onearg_genop( D_at_exp,"D_at_exp",exp);
  static gen taylor_exp (const gen & lim_point,const int ordre,const unary_function_ptr & f,int direction,gen & shift_coeff,GIAC_CONTEXT){
    if (ordre<0)
      return 0; // no symbolic preprocessing
    shift_coeff=0;
    gen image=f(lim_point,contextptr); // should simplify if contains i*pi
    vecteur v(1,image);
    if (is_undef(image))
      return v;
    gen factorielle(1);
    for (int i=1;i<=ordre;++i,factorielle=factorielle*gen(i))
      v.push_back(rdiv(image,factorielle,contextptr));
    v.push_back(undef);
    return v;
  }
  static const char _exp_s []="exp";
  string printasexp(const gen & g,const char * s,GIAC_CONTEXT){
    return "exp("+g.print(contextptr)+")";
  }
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval_taylor2_index(16,__exp,&exp,(size_t)&D_at_expunary_function_ptr,&taylor_exp,_exp_s,0,&texprintsommetasoperator);
#else
  static define_unary_function_eval_taylor2_index(16,__exp,&exp,D_at_exp,&taylor_exp,_exp_s,0,&texprintsommetasoperator);
#endif
  define_unary_function_ptr5( at_exp ,alias_at_exp,&__exp,0,true);

  // static symbolic symb_sqrt(const gen & e){  return symbolic(at_sqrt,e);  }

  void zint2simpldoublpos(const gen & e,gen & simpl,gen & doubl,bool & pos,int d,GIAC_CONTEXT){
    simpl=1;
    doubl=1;
    if (!is_integer(e)){
      pos=true;
      simpl=e;
      return;
    }
    if (is_zero(e)){
      simpl=e;
      return;
    }
    gen e_copy;
    pos=ck_is_positive(e,context0); // ok
    if (!pos)
      e_copy=-e;
    else
      e_copy=e;
    vecteur u;
#ifdef USE_GMP_REPLACEMENTS
    bool trial=true;
#else
    bool trial=false;
    if (e_copy.type==_ZINT && mpz_sizeinbase(*e_copy._ZINTptr,2)>128){
      // detect perfect square
      if (mpz_perfect_power_p(*e_copy._ZINTptr)){
	int nbits=mpz_sizeinbase(*e_copy._ZINTptr,2);
	gen h=accurate_evalf(e_copy,nbits);
	h=pow(h,inv(d,contextptr),contextptr);
	h=_floor(h,contextptr);
	if (pow(h,d,contextptr)==e_copy){
	  simpl=1;
	  doubl=h;
	  return ;
	}
      }
      // trial division only
      trial=true;
    }
#endif
    if (trial)
      u=pfacprem(e_copy,true,contextptr);
    else {
#ifdef NO_STDEXCEPT
      u=ifactors(e_copy,contextptr);
      if (is_undef(u)){
	*logptr(contextptr) << gettext("Unable to factor ") << e << endl;
	simpl=e;
	pos=true;
	return;
      }
#else
      try {
	u=ifactors(e_copy,contextptr);
      } catch (std::runtime_error & err){
	last_evaled_argptr(contextptr)=NULL;
	*logptr(contextptr) << gettext("Unable to factor ") << e << endl;
	simpl=e;
	pos=true;
	return;      
      }
#endif // no_stdexcept
    }
    // *logptr(contextptr) << u.size() << endl;
    gen f;
    int m,k;
    const_iterateur it=u.begin(),itend=u.end();
    for (;it!=itend;++it){
      f=*it;
      ++it;
      m=it->val;
#ifndef USE_GMP_REPLACEMENTS
      if (f.type==_ZINT && mpz_perfect_power_p(*f._ZINTptr)){
	int nbits=mpz_sizeinbase(*f._ZINTptr,2);
	gen h=accurate_evalf(f,nbits);
	h=pow(h,inv(d,contextptr),contextptr);
	h=_floor(h,contextptr);
	if (pow(h,d,contextptr)==f){
	  f=h;
	  m=m*d;
	}
      }
#endif
      if (m%d)
	simpl = simpl*pow(f,m%d,contextptr);
      for (k=0;k<m/d;++k)
	doubl = doubl*f;
    }
  }

  // simplified sqrt without taking care of sign
  gen sqrt_noabs(const gen & e,GIAC_CONTEXT){
    identificateur tmpx(" x");
    vecteur w=solve(tmpx*tmpx-e,tmpx,1,contextptr); 
    if (lidnt(w).empty())
      w=protect_sort(w,contextptr);
    if (w.empty())
      return gensizeerr(gettext("sqrt_noabs of ")+e.print(contextptr));
    return w.back();
  }

  static float fsqrt(float f){
    return std::sqrt(f);
  }
  static gen sqrt_mod_pn(const gen & a0,const gen & p,const gen & n,gen & pn,GIAC_CONTEXT){
    pn=pow(p,n,context0);
    gen a(a0);
    int l=legendre(a,p);
    if (l==-1)
      return undef;
    gen res;
    if (n.type!=_INT_ || n.val<1)
      return undef;
    int N=n.val;
    gen pdiv2=1;
    if (p==2){
      for (;N>=2 && smod(a,2)==0;pdiv2=2*pdiv2){
	if (smod(a,4)!=0)
	  return undef;
	a=a/4;
	N-=2;
      }
      if (N==1)
	return smod(a,2)*pdiv2;
      // now a is odd
      if (N==2){
	if (is_one(smod(a,4)))
	  return pdiv2;
	return undef;
      }
      // find x square root of a modulo 8 then Hensel lift
      gen x=smod(a,8);
      if (x!=1)
	return undef;
      gen powk=8;
      for (int Nn=3;;Nn=2*Nn-1){
	// assume x^2=a mod 2^k, then find y / (x+2^k*y)^2=x^2+2^(k+1)*y*x=a mod 2^(2k)
	// => y=[(a-x^2)/2^(k+1)]/x mod 2^(k-1)
	gen y=(a-x*x)/powk;
	powk=powk/2;
	y=y*invmod(x,powk);
	x=x+powk*y;
	powk=powk*powk;
	x=smod(x,powk);
	if (Nn>N)
	  break;
      }
      return smod(x*pdiv2,pn);
    }
    if (is_zero(smod(a,p)))
      res=0;
    else {
      if (is_zero(smod(p+1,4)))
	res=powmod(smod(a,p),(p+1)/4,p);
      else {
	// could use Shank-Tonneli algorithm, here use gcd(x^2-a,powmod(x+rand,(p-1)/2,p,x^2-a)-1) to split x^2- in 2 parts with proba 1/2
	environment env;
	env.moduloon=true;
	env.modulo=p;
	modpoly A(3),B(2,1),C,D;
	A[0]=1; A[2]=-a; 
	while (true){
	  gen r=smod(gen(giac_rand(contextptr)),p);
	  B[1]=r;
	  D=powmod(B,(p-1)/2,A,&env);
	  D.back()=D.back()-1;
	  if (is_zero(D.front()))
	    continue;
	  gcdmodpoly(A,D,&env,C);
	  if (C.size()==2){
	    res=C[1];
	    break;
	  }
	}
      }
    }
    if (n.val>1){
      // Hensel lift res mod p^n
      pn=p;
      gen invmodu=invmod(2*res,p);
      for (int i=1;i<n.val;++i){
	res=res+pn*(smod((a-res*res)/pn*invmodu,p));
	pn=pn*p;
      }
    }
    return res;
  }
  gen sqrt_mod(const gen & a,const gen & b,bool isprime,GIAC_CONTEXT){
    if (!is_integer(b))
      return gensizeerr(contextptr);
    if (is_one(a) || is_zero(a))
      return a;
    if (b.type==_INT_){
      int A=smod(a,b).val,p=b.val;
      if (A<0) A+=p;
      if (A==0 || A==1) return A;
      if (isprime && p>1024 && (p+1)%4==0){
	A=powmod(A,(unsigned long)((p+1)/4),p);
	if (A>p-A)
	  A=p-A;
	return A;
      }
      if (p<65536){
	int sq=0,add=1;
	for (;add<=p;add+=2){
	  sq+=add;
	  if (sq>=p)
	    sq-=p;
	  if (sq==A)
	    return add/2+1;
	}
	return undef;
      }
    }
    int l=legendre(a,b);
    if (l==-1)
      return undef;
    vecteur v=ifactors(b,contextptr);
    gen oldres(0),pip(1);
    for (unsigned i=0;i<v.size()/2;++i){
      gen p=v[2*i],n=v[2*i+1],pn;
      gen res=sqrt_mod_pn(a,p,n,pn,contextptr);
      if (is_undef(res))
	return res;
      // ichinrem step
      if (i)
	oldres=ichinrem(oldres,res,pip,pn);
      else
	oldres=res;
      pip=pip*pn;
    }
    if (is_positive(-oldres,contextptr))
      oldres=-oldres;
    pip=b-oldres;
    if (is_greater(oldres,pip,contextptr))
      oldres=pip;
    return oldres;
  }

  gen sqrt(const gen & e,GIAC_CONTEXT){
    // if (abs_calc_mode(contextptr)==38 && do_lnabs(contextptr) &&!complex_mode(contextptr) && (e.type<=_POLY || e.type==_FLOAT_) && !is_positive(e,contextptr)) return gensizeerr(contextptr);
    if (!escape_real(contextptr) && !complex_mode(contextptr) && (e.type<=_POLY ) && !is_positive(e,contextptr)) return gensizeerr(contextptr);
    if (e.type==_DOUBLE_){
      if (e._DOUBLE_val>=0){
#ifdef _SOFTMATH_H
	return std::giac_gnuwince_sqrt(e._DOUBLE_val);
#else
	return std::sqrt(e._DOUBLE_val);
#endif
      }
      else
#ifdef _SOFTMATH_H
	return gen(0.0,std::giac_gnuwince_sqrt(-e._DOUBLE_val));
#else
	return gen(0.0,std::sqrt(-e._DOUBLE_val));
#endif
    }
#if 0
    if (e.type==_REAL){
      if (is_strictly_positive(-e,contextptr))
	return cst_i*sqrt(-e,contextptr);
      return e._REALptr->sqrt();
    }
#endif
    // if (e.type==_USER){ return e._USERptr->sqrt(contextptr); }
    gen a,b;
    if (e.type==_MOD){
      a=*e._MODptr;
      b=*(e._MODptr+1);
      a=sqrt_mod(a,b,false,contextptr);
      if (is_undef(a))
	return a;
      if (is_positive(-a,contextptr))
	a=-a;
      return makemod(a,b);
    }
    if (e.type==_CPLX || has_i(e)){
      if (e.type==_CPLX && e.subtype){
#ifdef _SOFTMATH_H
	return std::giac_gnuwince_sqrt(gen2complex_d(e));
#else
#ifdef EMCC
	return std::exp(std::log(gen2complex_d(e))/2.0);
#else
	return std::sqrt(gen2complex_d(e));
#endif
#endif
      }
      // sqrt of an exact complex number
      if (!lop(e,at_exp).empty())
	return pow(e,plus_one_half,contextptr);
      a=re(e,contextptr);b=ratnormal(im(e,contextptr),contextptr);
      if (a!=e && is_zero(b,contextptr))
	return sqrt(a,contextptr);
      if ( has_i(a) || has_i(b) )
	return pow(e,plus_one_half,contextptr);
      gen rho=pow(a,2,contextptr)+pow(b,2,contextptr);
      rho=ratnormal(rho,contextptr);
      if (abs_calc_mode(contextptr)==38 && !lvarfracpow(rho).empty())
	return pow(e,plus_one_half,contextptr);
      if (lvar(rho).empty()) rho=eval(rho,1,contextptr);
      rho=sqrt(rho,contextptr);
      if (rho.type!=_FRAC && rho.type>=_IDNT){ // was abs_calc_mode(contextptr)==38
	rho=evalf(rho,1,contextptr);
	if (rho.type>=_IDNT)
	  return pow(e,plus_one_half,contextptr);
	*logptr(contextptr) << "Warning converting to approx sqrt"<<endl;
      }
#ifdef EMCC
      if (rho.type>=_IDNT)
	rho=evalf(rho,1,contextptr);
#endif
      gen realpart=normalize_sqrt(sqrt(2*(a+rho),contextptr),contextptr);
      return ratnormal(realpart/2,contextptr)*gen(1,b/(a+rho)); // (1+cst_i*b/(a+rho));
    }
    if (e.type==_VECT){
      if (is_squarematrix(e))
	return analytic_apply(at_sqrt,*e._VECTptr,contextptr);
      return apply(e,sqrt,contextptr);
    }
    if ( (is_zero(e) && !e.is_symb_of_sommet(at_unit)) || is_undef(e) || (e==plus_inf) || (e==unsigned_inf))
      return e;
    if (is_perfect_square(e))
      return isqrt(e);
    if (e.type==_INT_ || e.type==_ZINT){ 
      // factorization 
      if (e.type==_INT_ && e.val>0){
	switch (e.val){
	case 2:
	  return plus_sqrt2;
	case 3:
	  return plus_sqrt3;
	case 6:
	  return plus_sqrt6;
	}
      }
      bool pos=true;
      zint2simpldoublpos(e,a,b,pos,2,contextptr);
      if (!pos)
	return (a==1)?cst_i*b:cst_i*b*symb_pow(a,plus_one_half);
      else
	return b*symb_pow(a,plus_one_half);
    }
    if (e.type==_FRAC)
      return sqrt(e._FRACptr->num*e._FRACptr->den,contextptr)/abs(e._FRACptr->den,contextptr);
    if (is_algebraic_program(e,a,b))
      return symb_prog3(a,0,sqrt(b,contextptr));
    if (e.is_inv())
      return inv(sqrt(e._SYMBptr->feuille,contextptr),contextptr);
    if (e.type==_SYMB){
      vecteur v=lvar(e);
      if (v.size()==1 && v.front().is_pow() && v.front()._SYMBptr->feuille[1]==plus_one_half && is_integer(v.front()._SYMBptr->feuille[0])){
	gen a,b,c=v.front()._SYMBptr->feuille[0];
	if (is_linear_wrt(e,v.front(),b,a,contextptr) && (is_integer(a) ||a.type==_FRAC) && (is_integer(b) || b.type==_FRAC)){
	  gen d=a*a-b*b*c;
	  if (is_positive(d,contextptr)){
	    d=sqrt(d,contextptr);
	    if (is_integer(d) || d.type==_FRAC){
	      return sqrt((a+d)/2,contextptr)+sign(b,contextptr)*sqrt((a-d)/2,contextptr);
	    }
	  }
	}
      }
      for (unsigned i=0;i<v.size();++i){
	gen vi=v[i];
	if (vi.is_symb_of_sommet(at_cos)){
	  gen a,b;
	  if (is_linear_wrt(e,vi,a,b,contextptr)){
	    if (a==b)
	      return sqrt(2*a,contextptr)*abs(cos(vi._SYMBptr->feuille/2,contextptr),contextptr);
	    if (a==-b)
	      return sqrt(-2*a,contextptr)*abs(sin(vi._SYMBptr->feuille/2,contextptr),contextptr);
	  }
	}
	if (vi.is_symb_of_sommet(at_sin)){
	  gen a,b;
	  if (is_linear_wrt(e,vi,a,b,contextptr)){
	    if (a==b)
	      return sqrt(2*a,contextptr)*abs(cos(vi._SYMBptr->feuille/2-cst_pi/4,contextptr),contextptr);
	    if (a==-b)
	      return sqrt(-2*a,contextptr)*abs(sin(vi._SYMBptr->feuille/2-cst_pi/4,contextptr),contextptr);
	  }
	}
      } // end loop on vars
    }
    return pow(e,plus_one_half,contextptr);
  }
  static gen d_sqrt(const gen & e,GIAC_CONTEXT){
    return inv(gen(2)*sqrt(e,contextptr),contextptr);
  }
  define_partial_derivative_onearg_genop( D_at_sqrt," D_at_sqrt",&d_sqrt);
  static const char _sqrt_s []="sqrt";
  static string printassqrt(const gen & g,const char * s,GIAC_CONTEXT){
    return "sqrt("+g.print(contextptr)+")";
  }
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval5 (__sqrt,&sqrt,(size_t)&D_at_sqrtunary_function_ptr,_sqrt_s,&printassqrt,&texprintsommetasoperator);
#else
  static define_unary_function_eval5 (__sqrt,&sqrt,D_at_sqrt,_sqrt_s,&printassqrt,&texprintsommetasoperator);
#endif
  define_unary_function_ptr5( at_sqrt ,alias_at_sqrt,&__sqrt,0,true);

  gen _sq(const gen & e,GIAC_CONTEXT){
    if ( e.type==_STRNG && e.subtype==-1) return  e;
    gen a,b;
    if (is_algebraic_program(e,a,b))
      return symb_prog3(a,0,_sq(b,contextptr));
    return pow(e,2,contextptr);
  }
  static gen d_sq(const gen & e,GIAC_CONTEXT){
    return gen(2)*e;
  }
  define_partial_derivative_onearg_genop( D_at_sq," D_at_sq",&d_sq);
  static const char _sq_s []="sq";
  // static string texprintassq(const gen & g,const char * s,GIAC_CONTEXT){  return gen2tex(g,contextptr)+"^2";}
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval3_index (158,__sq,(const gen_op_context)_sq,(size_t)&D_at_squnary_function_ptr,_sq_s);
#else
  static define_unary_function_eval3_index (158,__sq,(const gen_op_context)_sq,D_at_sq,_sq_s);
#endif
  define_unary_function_ptr5( at_sq ,alias_at_sq,&__sq,0,true);

  float taylor_sin(float d){
    // x*(1-x^2/3!+x^4/5!-x^6/7!+x^8/9!)
    // == x* (2.7557319224e-06*x^8-0.000198412698413*x^6+0.00833333333333*x^4-0.166666666667*x^2+1.0)
    float x2=d*d;
    return d*(1.0+x2*(-0.166666666667+x2*(0.00833333333333+x2*(-0.000198412698413+x2*2.7557319224e-06)))); // 6* 4+
  }

  float taylor_cos(float d){
    // 1-x^2/2+x^4/24-x^6/720+x^8/40320
    // =2.48015873016e-05*x^8-0.00138888888889*x^6+0.0416666666667*x^4-0.5*x^2+1.0
    float x2=d*d;
    return 1.0+x2*(-.5+x2*(0.0416666666667+x2*(-0.00138888888889+x2*2.48015873016e-05))); // 5* 4+
  }
  
  gen symb_cos(const gen & e){
    return symbolic(at_cos,e);
  }
  double my_cos(double d){
#if 0 // def TICE
#define twoopi 0.636619772367581 // 2/pi
#define piotwo 1.57079632679
    bool neg=false;
    if (d<0)
      d=-d;
    // cos(x)=+/-sin_or_cos(x-k*pi/2), k=round(x/(pi/2))
    int k=d*twoopi+.5;
    d-=k*piotwo; // d in [-pi/4,pi/4]
    if (k&2)
      neg=!neg;
    if (k&1)
      d=-taylor_sin(d);
    else
      d=taylor_cos(d);
    return neg?-d:d;
#else
#ifdef _SOFTMATH_H
    return std::giac_gnuwince_cos(d);
#else
    return std::cos(d);
#endif
#endif
  }
  gen cos(const gen & e0,GIAC_CONTEXT){
#if 0
    if (e0.type==_FLOAT_){
#ifdef BCD
      return fcos(e0._FLOAT_val,angle_mode(contextptr));
#else
      return cos(get_double(e0._FLOAT_val),contextptr);
#endif
    }
#endif
    if (e0.type==_DOUBLE_){
      double d;
      if (angle_radian(contextptr)) 
	d=e0._DOUBLE_val;
      else if(angle_degree(contextptr))
        d=e0._DOUBLE_val*deg2rad_d;
      //grad
      else
        d = e0._DOUBLE_val*grad2rad_d;
      return my_cos(d);
    }
    gen e=frac_neg_out(e0,contextptr);
    if (e.type==_SPOL1){
      gen expo=e._SPOL1ptr->empty()?undef:e._SPOL1ptr->front().exponent;
      if (is_positive(expo,contextptr))
	return series(*e._SPOL1ptr,*at_cos,0,contextptr);
    }
#if 0
    if (e.type==_REAL){
      if (angle_radian(contextptr)) 
	return e._REALptr->cos();
      else if(angle_degree(contextptr))
        return ((e*cst_pi)/180)._REALptr->cos();
      //grad
      else
	return ((e*cst_pi)/200)._REALptr->cos();
    }
#endif
    if (e.type==_CPLX){ 
      if (e.subtype){
	complex_double d;
	if (angle_radian(contextptr)) 
	  d=gen2complex_d(e);
  else if(angle_degree(contextptr))
	  d=gen2complex_d(e)*deg2rad_d;
  //grad
	else
    d=gen2complex_d(e)*grad2rad_d;
#ifdef _SOFTMATH_H
	return std::giac_gnuwince_cos(d);
#else
	return std::cos(d);
#endif
      }
#if 0
      if (e._CPLXptr->type==_REAL || e._CPLXptr->type==_FLOAT_){
	gen e1=e;
	if(!angle_radian(contextptr))
	  {
	    //grad
	    if(angle_degree(contextptr))
	      e1=e*deg2rad_g;
	    else
	      e1 = e*grad2rad_g;
	  }
	gen e2=im(e1,contextptr);
	e1=re(e1,contextptr);
	//grad
	int mode=get_mode_set_radian(contextptr);
	e1= gen(cos(e1,contextptr)*cosh(e2,contextptr),-sinh(e2,contextptr)*sin(e1,contextptr));//cos(e1,contextptr)*cosh(e2,contextptr)-cst_i*sinh(e2,contextptr)*sin(e1,contextptr);
	angle_mode(mode,contextptr);
	return e1;
      }
#endif
    }
    if (is_squarematrix(e))
      return analytic_apply(at_cos,*e._VECTptr,contextptr);
    if (e.type==_VECT)
      return apply(e,cos,contextptr);
    if (is_zero(e,contextptr))
      return 1;
    if ( (e.type==_INT_) && (e.val<0) )
      return cos(-e,contextptr);
    if (is_undef(e))
      return e;
    if (is_inf(e))
      return undef;
    int k;
    gen a,b;
    if (is_algebraic_program(e,a,b))
      return symb_prog3(a,0,cos(b,contextptr));
    bool doit=false,est_multiple;
    if (angle_radian(contextptr)){
      if (simplify_sincosexp_pi && contains(e,cst_pi) && is_linear_wrt(e,cst_pi,a,b,contextptr)){
	if (is_zero(a)){
	  if (is_zero(b)) 
	    return 1;
	} else {
	  if (b==0 && a.type==_FRAC && a._FRACptr->den==10 && a._FRACptr->num.type==_INT_)
	    return sin(cst_pi/2-e,contextptr);
	  if (b==0 && a.type==_FRAC && a._FRACptr->den==5 && a._FRACptr->num.type==_INT_){
	    int n=a._FRACptr->num.val % 10;
	    if (n<0)
	      n += 10;
	    if (n>=5)
	      n=10-n;
	    gen sqrt5=sqrt(5,contextptr);
	    gen cospi5=(sqrt5+1)/4;
	    gen cos2pi5=(sqrt5-1)/4;
	    if (n==1) return cospi5;
	    if (n==2) return cos2pi5;
	    if (n==3) return -cos2pi5;
	    if (n==4) return -cospi5;
	  }
	  if (b==0 && a.type==_FRAC && a._FRACptr->den==8 && a._FRACptr->num.type==_INT_){
	    int n=a._FRACptr->num.val % 16;
	    if (n<0)
	      n += 16;
	    if (n>=8)
	      n=16-n;
	    if (n==1 || n==7){
	      gen cospi8=sqrt(2+plus_sqrt2,contextptr)/2;
	      if (n==1) return cospi8;
	      if (n==7) return -cospi8;
	    }
	    gen cos3pi8=sqrt(2-plus_sqrt2,contextptr)/2;
	    if (n==3) return cos3pi8;
	    if (n==5) return -cos3pi8;
	  }
	  est_multiple=is_multiple_of_12(a*gen(trig_deno/2),k);
	  doit=true;
	}
      } // if (simplify_sincosexp...)
    }
    else {
      est_multiple=is_multiple_of_pi_over_12(e,k,contextptr);
      doit=est_multiple;
    }
    if (doit){ 
      if (est_multiple){
	if (is_zero(b))
	  return *table_cos[k];
	gen C=cos(b,contextptr),S=sin(b,contextptr);
	if (k%6==0 || C.type!=_SYMB || S.type!=_SYMB)
	  return (*table_cos[k])*C+(*table_cos[(k+6)%24])*S;
      }
      else {
	if (is_assumed_integer(a,contextptr)){
	  if (is_assumed_integer(normal(rdiv(a,plus_two,contextptr),contextptr),contextptr))
	    return cos(b,contextptr);
	  else
	    return pow(minus_one,a,contextptr)*cos(b,contextptr);
	}
	a=expand(a,contextptr);
	if (a.is_plus() && a._SYMBptr->feuille.type==_VECT){
	  vecteur av=*a._SYMBptr->feuille._VECTptr;
	  vecteur av1;
	  int neg=0;
	  for (unsigned i=0;i<av.size();++i){
	    if (is_integer(av[i]))
	      neg += smod(av[i],2).val;
	    else
	      av1.push_back(av[i]);
	  }
	  if (neg){
	    if (av1.empty())
	      return (neg%2?-1:1)*cos(b,contextptr);
	    if (av1.size()==1)
	      return (neg%2?-1:1)*cos(av1.front()*cst_pi+b,contextptr);
	    return (neg%2?-1:1)*cos(symb_plus(av1)*cst_pi+b,contextptr);
	  }
	}
	int n,d,q,r;
	if (is_zero(b,contextptr) && is_rational(a,n,d)){
	  q=n/d;
	  r=n%d;
	  if (r>d/2){
	    r -= d;
	    ++q;
	  }
	  if (q%2)
	    q=-1;
	  else
	    q=1;
	  if (r<0)
	    r=-r;
	  if (!(d%2) && d%4){ 
	    d=d/2; // cos(r/(2*d)*pi) = sin(pi/2(1-r/d))
	    if (angle_radian(contextptr)) 
	      return -q*sin((r-d)/2*cst_pi/d,contextptr);
      else if(angle_degree(contextptr))
	      return -q*sin(rdiv((r-d)*90,d,contextptr),contextptr);
      //grad
	    else 
        return -q*sin(rdiv((r - d) * 100, d, contextptr), contextptr);
	  }
	  if (angle_radian(contextptr)) 
	    return q*symb_cos(r*cst_pi/d);
    else if(angle_degree(contextptr))
	    return q*symb_cos(rdiv(r*180,d,contextptr));
    //grad
	  else
      return q*symb_cos(rdiv(r*200,d,contextptr));
	}
      }
    }
    if (e.type==_SYMB) {
      unary_function_ptr u=e._SYMBptr->sommet;
      gen f=e._SYMBptr->feuille;
      if (u==at_neg)
	return cos(f,contextptr);
      if (u==at_acos)
	return f;
      if (u==at_asin)
	return sqrt(1-pow(f,2),contextptr);
      if (u==at_atan)
	return sqrt(inv(pow(f,2)+1,contextptr),contextptr);
    }
    if (is_equal(e))
      return apply_to_equal(e,cos,contextptr);
    return symb_cos(e);
  }
  static gen d_cos(const gen & e ,GIAC_CONTEXT){
    if (angle_radian(contextptr)) 
      return -(sin(e,contextptr));
    else if(angle_degree(contextptr))
      return -deg2rad_e*sin(e,contextptr);
    //grad
    else
      return -grad2rad_e*sin(e,contextptr);
  }
  define_partial_derivative_onearg_genop( D_at_cos," D_at_cos",d_cos);
  static const char _cos_s []="cos";
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval3_index (34,__cos,&cos,(size_t)&D_at_cosunary_function_ptr,_cos_s);
#else
  static define_unary_function_eval3_index (34,__cos,&cos,D_at_cos,_cos_s);
#endif
  define_unary_function_ptr5( at_cos ,alias_at_cos,&__cos,0,true);

  double my_sin(double d){
#if 0 // def TICE
    bool neg=false;
    if (d<0){
      d=-d;
      neg=true;
    }
    // sin(x)=+/-sin_or_cos(x-k*pi/2), k=round(x/(pi/2))
    int k=d*twoopi+.5;
    d-=k*piotwo; // d in [-pi/4,pi/4]
    if (k&2)
      neg=!neg;
    if (k&1)
      d=taylor_cos(d);
    else
      d=taylor_sin(d);
    return neg?-d:d;
#else
#ifdef _SOFTMATH_H
    return std::giac_gnuwince_sin(d);
#else
    return std::sin(d);
#endif
#endif
  }
    
  gen symb_sin(const gen & e){
    return symbolic(at_sin,e);
  }
  gen sin(const gen & e0,GIAC_CONTEXT){
#if 0
    if (e0.type==_FLOAT_){
#ifdef BCD
      return fsin(e0._FLOAT_val,angle_mode(contextptr));
#else
      return sin(get_double(e0._FLOAT_val),contextptr);
#endif
    }
#endif
    if (e0.type==_DOUBLE_){
      double d;
      if (angle_radian(contextptr)) 
	d=e0._DOUBLE_val;
      else if(angle_degree(contextptr))
	d=e0._DOUBLE_val*deg2rad_d;
      //grad
      else
        d=e0._DOUBLE_val*grad2rad_d;
      return my_sin(d);
    }
    gen e=frac_neg_out(e0,contextptr);
    if (e.type==_SPOL1){
      gen expo=e._SPOL1ptr->empty()?undef:e._SPOL1ptr->front().exponent;
      if (is_positive(expo,contextptr))
	return series(*e._SPOL1ptr,*at_sin,0,contextptr);
    }
#if 0
    if (e.type==_REAL){
      if (angle_radian(contextptr)) 
	return e._REALptr->sin();
      else if(angle_degree(contextptr))
	return ((e*cst_pi)/180)._REALptr->sin();
      //grad
      else
        return ((e*cst_pi)/200)._REALptr->sin();
    }
#endif
    if (e.type==_CPLX){ 
      if (e.subtype){
	complex_double d;
	if (angle_radian(contextptr)) 
	  d=gen2complex_d(e);
	else if(angle_degree(contextptr))
	  d=gen2complex_d(e)*deg2rad_d;
	//grad
	else
	  d=gen2complex_d(e)*grad2rad_d;
#ifdef _SOFTMATH_H
	return std::giac_gnuwince_sin(d);
#else
	return std::sin(d);
#endif
      }
#if 0
      if (e._CPLXptr->type==_REAL || e._CPLXptr->type==_FLOAT_){
	gen e1=e;
	if(!angle_radian(contextptr)){
	  if(angle_degree(contextptr)) 
	    e1=e*deg2rad_g;
	  //grad
	  else
	    e1 = e*grad2rad_g;
	}
	gen e2=im(e1,contextptr);
	e1=re(e1,contextptr);
	//grad
	int mode=get_mode_set_radian(contextptr);
	gen res=gen(sin(e1,contextptr)*cosh(e2,contextptr),sinh(e2,contextptr)*cos(e1,contextptr));//sin(e1,contextptr)*cosh(e2,contextptr)+cst_i*sinh(e2,contextptr)*cos(e1,contextptr);
	angle_mode(mode,contextptr);
	return res;
      }
#endif
    }
    if (is_squarematrix(e))
      return analytic_apply(at_sin,*e._VECTptr,contextptr);
    if (e.type==_VECT)
      return apply(e,sin,contextptr);
    if (is_zero(e,contextptr))
      return e;
    if ( (e.type==_INT_) && (e.val<0) )
      return -sin(-e,contextptr);
    if (is_undef(e))
      return e;
    if (is_inf(e))
      return undef;
    int k;
    gen a,b;
    if (is_algebraic_program(e,a,b))
      return symb_prog3(a,0,sin(b,contextptr));
    bool doit=false,est_multiple;
    if (angle_radian(contextptr)){
      if (simplify_sincosexp_pi && contains(e,cst_pi) && is_linear_wrt(e,cst_pi,a,b,contextptr)){
	if (is_zero(a)){
	  if (is_zero(b)) 
	    return 0;
	} else {
	  if (b==0 && a.type==_FRAC && a._FRACptr->den==10 && a._FRACptr->num.type==_INT_)
	    return cos(cst_pi/2-e,contextptr);
	  if (b==0 && a.type==_FRAC && a._FRACptr->den==5 && a._FRACptr->num.type==_INT_){
	    int n=a._FRACptr->num.val % 10;
	    if (n<0)
	      n+=10;
	    gen sqrt5=sqrt(5,contextptr);
	    gen sinpi5=sqrt(-2*sqrt5+10,contextptr)/4;
	    gen sin2pi5=sqrt(2*sqrt5+10,contextptr)/4;
	    if (n==1 || n==4) return sinpi5;
	    if (n==2 || n==3) return sin2pi5;
	    if (n==6 || n==9) return -sinpi5;
	    if (n==7 || n==8) return -sin2pi5;
	  }
	  if (b==0 && a.type==_FRAC && a._FRACptr->den==8 && a._FRACptr->num.type==_INT_){
	    int n=a._FRACptr->num.val % 16;
	    if (n<0)
	    n+=16;
	    gen sinpi8=sqrt(2-plus_sqrt2,contextptr)/2;
	    gen sin3pi8=sqrt(2+plus_sqrt2,contextptr)/2;
	    if (n==1 || n==7) return sinpi8;
	    if (n==3 || n==5) return sin3pi8;
	    if (n==9 || n==15) return -sinpi8;
	    if (n==11 || n==13) return -sin3pi8;
	  }
	  est_multiple=is_multiple_of_12(a*gen(trig_deno/2),k);
	  doit=true;
	} 
      } // if (simplify_sincospexp...)
    }
    else {
      est_multiple=is_multiple_of_pi_over_12(e,k,contextptr);
      doit=est_multiple;
    }
    if (doit){ 
      if (est_multiple){
	if (is_zero(b))
	  return *table_cos[(k+18)%24];
	gen C=cos(b,contextptr),S=sin(b,contextptr);
	if (k%6==0 || C.type!=_SYMB || S.type!=_SYMB)
	  return *table_cos[(k+18)%24]*C+(*table_cos[k%24])*S;
      }
      else {
	if (is_assumed_integer(a,contextptr)){
	  if (is_assumed_integer(normal(a/2,contextptr),contextptr))
	    return sin(b,contextptr);
	  else
	    return pow(minus_one,a,contextptr)*sin(b,contextptr);
	}
	a=expand(a,contextptr);
	if (a.is_plus() && a._SYMBptr->feuille.type==_VECT){
	  vecteur av=*a._SYMBptr->feuille._VECTptr;
	  vecteur av1;
	  int neg=0;
	  for (unsigned i=0;i<av.size();++i){
	    if (is_integer(av[i]))
	      neg += smod(av[i],2).val;
	    else
	      av1.push_back(av[i]);
	  }
	  if (neg){
	    if (av1.empty())
	      return (neg%2?-1:1)*sin(b,contextptr);
	    if (av1.size()==1)
	      return (neg%2?-1:1)*sin(av1.front()*cst_pi+b,contextptr);
	    return (neg%2?-1:1)*sin(symb_plus(av1)*cst_pi+b,contextptr); 
	  }
	}
	int n,d,q,r;
	if (is_zero(b,contextptr) && is_rational(a,n,d)){
	  q=n/d;
	  r=n%d;
	  if (r>d/2){
	    r -= d;
	    ++q;
	  }
	  if (q%2)
	    q=-1;
	  else
	    q=1;
	  if (r<0){
	    r=-r;
	    q=-q;
	  }
	  if (!(d%2) && d%4){ 
	    d=d/2; // sin(r/(2*d)*pi) = cos(pi/2(1-r/d))
	    if (angle_radian(contextptr))
	      return q*cos((r-d)/2*cst_pi/d,contextptr);
      else if(angle_degree(contextptr))
	      return q*cos(rdiv((r-d)*90,d,contextptr),contextptr);
      //grad
	    else
        return q*cos(rdiv((r-d)*100,d,contextptr),contextptr);
	  }
	  if (angle_radian(contextptr)) 
	    return q*symb_sin(r*cst_pi/d);
    else if(angle_degree(contextptr))
	    return q*symb_sin(rdiv(r*180,d,contextptr));
    //grad
	  else
      return q*symb_sin(rdiv(r*200,d,contextptr));
	}
      }
    }
    if (e.type==_SYMB) {
      unary_function_ptr u=e._SYMBptr->sommet;
      gen f=e._SYMBptr->feuille;
      if (u==at_neg)
	return -sin(f,contextptr);
      if (u==at_asin)
	return f;
      if (u==at_acos)
	return sqrt(1-pow(f,2),contextptr);
      if (u==at_atan)
	return rdiv(f,sqrt(pow(f,2)+1,contextptr),contextptr);
    }
    if (is_equal(e))
      return apply_to_equal(e,sin,contextptr);
    return symb_sin(e);
  }
  static gen d_sin(const gen & g,GIAC_CONTEXT){
    if (angle_radian(contextptr)) 
      return cos(g,contextptr);
    else if(angle_degree(contextptr))
      return deg2rad_e*cos(g,contextptr);
    //grad
    else
      return grad2rad_e*cos(g,contextptr);
  }
  static const char _sin_s []="sin";
  define_partial_derivative_onearg_genop( D_at_sin," D_at_sin",&d_sin);
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval3_index (32,__sin,&sin,(size_t)&D_at_sinunary_function_ptr,_sin_s);
#else
  static define_unary_function_eval3_index (32,__sin,&sin,D_at_sin,_sin_s);
#endif
  define_unary_function_ptr5( at_sin ,alias_at_sin,&__sin,0,true);

double my_tan(double arg){
#if 0 // def TICE
#define invpi 	1.27323954473516   // 4/pi
  float temp, e, argsq;
  int i,sign;
  
  sign = 1;
  if (arg<0.){
    arg = -arg;
    sign = -1;
  }
  i = arg*invpi;
  arg -= i*pio4;
  i &= 3;
  if (i&1) // pi/4-arg=pi/2-(arg+pi/4) => tan(arg+pi/4)=1/tan(pi/4-arg)
    arg=pio4-arg;
  if (i/2)
    sign=-sign;
  // P:=pade(tan(x),x,10,6)
  // (x^5-105*x^3+945*x)/(15*x^4-420*x^2+945)
  argsq = arg*arg;
  temp = ((argsq-105)*argsq+945)*arg/((15*argsq-420)*argsq+945);
  if (i==1 || i==2) {
    if (temp==0.) {
      errno = ERANGE;
      if (sign>0)
        return HUGE_VALF;
      return -HUGE_VALF;
    }
    temp = 1./temp;
  }
  return sign==1?temp:-temp;
#else
#ifdef _SOFTMATH_H
  return std::giac_gnuwince_tan(arg);
#else
  return std::tan(arg);
#endif
#endif
}

  gen symb_tan(const gen & e){
    return symbolic(at_tan,e);
  }
  gen tan(const gen & e0,GIAC_CONTEXT){
#if 0
    if (e0.type==_FLOAT_){
#ifdef BCD
      return ftan(e0._FLOAT_val,angle_mode(contextptr));
#else
      return tan(get_double(e0._FLOAT_val),contextptr);
#endif
    }
#endif
    if (e0.type==_DOUBLE_){
      double d;
      if (angle_radian(contextptr)) 
	d=e0._DOUBLE_val;
      else if(angle_degree(contextptr))
        d=e0._DOUBLE_val*deg2rad_d;
      //grad
      else
        d=e0._DOUBLE_val*grad2rad_d;
      return my_tan(d);
    }
    gen e=frac_neg_out(e0,contextptr);
    if (e.type==_SPOL1){
      gen expo=e._SPOL1ptr->empty()?undef:e._SPOL1ptr->front().exponent;
      if (is_positive(expo,contextptr))
	return series(*e._SPOL1ptr,*at_tan,0,contextptr);
    }
#if 0
    if (e.type==_REAL){
      if (angle_radian(contextptr)) 
	return e._REALptr->tan();
      else if(angle_degree(contextptr))
	      return ((e*cst_pi)/180)._REALptr->tan();
      //grad
      else
        return ((e*cst_pi)/200)._REALptr->tan();
    }
#endif
    if (e.type==_CPLX){ 
      if (e.subtype){
	complex_double c(gen2complex_d(e));
  if(!angle_radian(contextptr))
  {
    //grad
    if(angle_degree(contextptr))
	  c *= deg2rad_d;
    else
      c *= grad2rad_d;
  }
#ifdef _SOFTMATH_H
	return std::giac_gnuwince_tan(c);
#else
	return std::sin(c)/std::cos(c);
#endif
      }
#if 0
      if (e._CPLXptr->type==_REAL || e._CPLXptr->type==_FLOAT_){
	gen e1=e;
	if (!angle_radian(contextptr)) 
	  {
	    //grad
	    if(angle_degree(contextptr))
	      e1=e*deg2rad_g;
	    else
	      e1 = e*grad2rad_g;
	  }
	
	gen e2=im(e1,contextptr);
	e1=re(e1,contextptr);
	//grad
	int mode=get_mode_set_radian(contextptr);
	e1=tan(e1,contextptr);
	angle_mode(mode,contextptr);
	
	e2=cst_i*tanh(e2,contextptr);
	return (e1+e2)/(1-e1*e2);
      }
#endif
    }
    if (is_squarematrix(e))
      return analytic_apply(at_tan,*e._VECTptr,contextptr);
    if (e.type==_VECT)
      return apply(e,contextptr,tan);
    if (is_zero(e,contextptr))
      return e;
    if (is_undef(e))
      return e;
    if (is_inf(e))
      return undef;
    if ( (e.type==_INT_) && (e.val<0) )
      return -tan(-e,contextptr);
    gen a,b;
    if (is_algebraic_program(e,a,b))
      return symb_prog3(a,0,tan(b,contextptr));
    int k;
    if (!approx_mode(contextptr)){ 
      if (is_multiple_of_pi_over_12(e,k,contextptr)) //grad
	return *table_tan[(k%12)];
      if (is_multiple_of_pi_over_12(2*e,k,contextptr)) //grad
	return normal(sin(2*e,contextptr)/(1+cos(2*e,contextptr)),contextptr); 
      else {
	gen kk;
	if (angle_radian(contextptr)) 
	  kk=normal(rdiv(e,cst_pi,contextptr),contextptr);
	else if(angle_degree(contextptr))
	  kk=normal(rdiv(e,180,contextptr),contextptr);
	//grad
	else
	  kk = normal(rdiv(e, 200, contextptr), contextptr);
	if (is_assumed_integer(kk,contextptr))
	  return zero;
	int n,d;
	if (is_rational(kk,n,d)){
	  if (d==10)
	    return inv(tan((angle_radian(contextptr)?cst_pi/2:(angle_degree(contextptr)?90:100))-e,contextptr),contextptr); //grad
	  if (d==5){
	    n %= 5;
	    if (n<0)
	      n+=5;
	    gen sqrt5=sqrt(5,contextptr);
	    if (n==1 || n==4)
	      sqrt5=5-2*sqrt5;
	    else
	      sqrt5=5+2*sqrt5;
	    sqrt5=sqrt(sqrt5,contextptr);
	    return n<=2?sqrt5:-sqrt5;
	  }
	  if (d%2==0 && n<d/2 && n>d/4){
	    n = d/2-n; gen res;
	    if (angle_radian(contextptr)) 
	      res=symb_tan((n%d)*inv(d,contextptr)*cst_pi);
	    else if(angle_degree(contextptr))
	      res= symb_tan(rdiv((n%d)*180,d,contextptr));
	    else // grad
	      res= symb_tan(rdiv((n%d)*200,d,contextptr));
	    return inv(res,contextptr);
	  }
	  if (angle_radian(contextptr)) 
	    return symb_tan((n%d)*inv(d,contextptr)*cst_pi);
	  else if(angle_degree(contextptr))
	    return symb_tan(rdiv((n%d)*180,d,contextptr));
	  else // grad
	    return symb_tan(rdiv((n%d)*200,d,contextptr));
	}
      }
    }
    if (e.type==_SYMB) {
      unary_function_ptr u=e._SYMBptr->sommet;
      gen f=e._SYMBptr->feuille;
      if (u==at_neg)
	return -tan(f,contextptr);
      if (u==at_atan)
	return f;
      if (u==at_acos)
	return rdiv(sqrt(1-pow(f,2),contextptr),f,contextptr);
      if (u==at_asin)
	return rdiv(f,sqrt(1-pow(f,2),contextptr),contextptr);
    }
    if (is_equal(e))
      return apply_to_equal(e,tan,contextptr);
    return symb_tan(e);
  }
  static gen d_tan(const gen & e,GIAC_CONTEXT){
    if (angle_radian(contextptr)) 
      return 1+pow(tan(e,contextptr),2);
    else if(angle_degree(contextptr))
      return deg2rad_e*(1+pow(tan(e,contextptr),2));
    //grad
    else
      return grad2rad_e*(1+pow(tan(e,contextptr),2));
  }
  define_partial_derivative_onearg_genop( D_at_tan," D_at_tan",&d_tan);
  static const char _tan_s []="tan";
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval3_index (36,__tan,&tan,(size_t)&D_at_tanunary_function_ptr,_tan_s);
#else
  static define_unary_function_eval3_index (36,__tan,&tan,D_at_tan,_tan_s);
#endif
  define_unary_function_ptr5( at_tan ,alias_at_tan,&__tan,0,true);

  gen symb_asin(const gen & e){
    return symbolic(at_asin,e);
  }
  static gen asinasln(const gen & x,GIAC_CONTEXT){
    return -cst_i*ln(cst_i*x+sqrt(1-x*x,contextptr),contextptr);
    // return cst_i*ln(sqrt(x*x-1,contextptr)+x,contextptr)+cst_pi_over_2;
  }
  gen * normal_sin_pi_12_ptr_(){
    static gen * ans=0;
    if (!ans) ans=new gen(ratnormal(sin_pi_12,context0));
    return ans;
  }
  gen * normal_cos_pi_12_ptr_(){
    static gen * ans=0;
    if (!ans) ans=new gen(ratnormal(cos_pi_12,context0));
    return ans;
  }
  gen asin(const gen & e0,GIAC_CONTEXT){
    if ( (calc_mode(contextptr)==38 || !escape_real(contextptr) ) && !complex_mode(contextptr) && (e0.type<=_POLY) && (!is_positive(e0+1,contextptr) || !is_positive(1-e0,contextptr)))
      return gensizeerr(contextptr);
    if (e0.type==_SPOL1){
      gen expo=e0._SPOL1ptr->empty()?undef:e0._SPOL1ptr->front().exponent;
      if (is_positive(expo,contextptr))
	return series(*e0._SPOL1ptr,*at_asin,0,contextptr);
    }
#if 0
    if (e0.type==_FLOAT_){
      if (!is_positive(e0+1,contextptr) || !is_positive(1-e0,contextptr))
	return asinasln(e0,contextptr)*gen(angle_radian(contextptr)?1.0:(angle_degree(contextptr)?rad2deg_d:rad2grad_d)); //grad // cst_i*ln(sqrt(e0*e0-1,contextptr)+e0,contextptr)+evalf(cst_pi_over_2,1,contextptr);
#ifdef BCD
      return fasin(e0._FLOAT_val,angle_mode(contextptr));
#else
      return asin(get_double(e0._FLOAT_val),contextptr);
#endif
    }
#endif
#ifndef VISUALC
    gen * normal_sin_pi_12_ptr=normal_sin_pi_12_ptr_();
    gen * normal_cos_pi_12_ptr=normal_cos_pi_12_ptr_();
#endif
    gen e=frac_neg_out(e0,contextptr);
    if (e.type==_DOUBLE_){
      if (e._DOUBLE_val>=-1 && e._DOUBLE_val<=1){
#ifdef _SOFTMATH_H
	double d= std::giac_gnuwince_asin(e._DOUBLE_val);
#else
	double d=std::asin(e._DOUBLE_val);
#endif
	if (angle_radian(contextptr)) 
	  return d;
	else if(angle_degree(contextptr))
	  return d*rad2deg_d;
	//grad
	else
	  return d*rad2grad_d;
      }
    }
#if 0
    if (e.type==_REAL){
      if (angle_radian(contextptr)) 
	return e._REALptr->asin();
      else if(angle_degree(contextptr))
	return 180*e._REALptr->asin()/cst_pi;
      //grad
      else
        return 200*e._REALptr->asin()/cst_pi;
    }
#endif
    if ( e.type==_DOUBLE_ || (e.type==_CPLX && (e.subtype) )){
      if (angle_radian(contextptr)) 
	return no_context_evalf(asinasln(e,contextptr));
      else if(angle_degree(contextptr))
	return no_context_evalf(asinasln(e,contextptr))*gen(rad2deg_d);
      //grad
      else
        return no_context_evalf(asinasln(e,contextptr))*gen(rad2grad_d);
    }
    if (is_squarematrix(e))
      return analytic_apply(at_asin,*e._VECTptr,contextptr);
    if (e.type==_VECT)
      return apply(e,asin,contextptr);
    if (is_zero(e,contextptr))
      return e;
    if (is_one(e)){
      if (is_zero(e)) fonction_bidon();
      if (angle_radian(contextptr))
	return cst_pi_over_2;
      else if(angle_degree(contextptr))
      return 90;
      //grad
      else
        return 100;
    }
    if (e==sin_pi_12 
#ifndef VISUALC
	|| e==*normal_sin_pi_12_ptr
#endif
	){
      if (angle_radian(contextptr))
	return rdiv(cst_pi,12,contextptr);
      else if(angle_degree(contextptr))
      return 15;
      //grad
      else
        return rdiv(50, 3); //50/3 grads
    }
    if (e==cos_pi_12 
#ifndef VISUALC
	|| e==*normal_cos_pi_12_ptr
#endif
	){
      if (angle_radian(contextptr))
	return 5*cst_pi/12;
      else if(angle_degree(contextptr))
      return 75;
      //grad
      else
        return rdiv(250,3); //250/3 grads
    }
    if (e==plus_sqrt3_2){
      if (angle_radian(contextptr))
	return rdiv(cst_pi,3,contextptr);
      else if(angle_degree(contextptr))
      return 60;
      //grad
      else
        return rdiv(200,3); //200/3 grads
    }
    if (e==plus_sqrt2_2){
      if (angle_radian(contextptr)) 
	return rdiv(cst_pi,4,contextptr);
      else if(angle_degree(contextptr))
      return 45;
      //grad
      else
        return 50;
    }
    if (e==plus_one_half){
      if (angle_radian(contextptr)) 
	return rdiv(cst_pi,6,contextptr);
      else if(angle_degree(contextptr))
      return 30;
      //grad
      else
        return rdiv(100,3); //100/3 grads
    }
    gen edg=evalf_double(e,1,contextptr);
    if (edg.type==_DOUBLE_){
      double ed=edg._DOUBLE_val;
      // detect if asin is a multiples of pi/10
      gen edh=horner(makevecteur(256,-512,336,-80,5),edg*edg);
      if (absdouble(edh._DOUBLE_val)<1e-9 &&
	  normal(horner(makevecteur(256,-512,336,-80,5),e*e),contextptr)==0){
	int res=int(std::floor(std::asin(absdouble(ed))*10/M_PI+.5));
	if (res%2)
	  return (ed>0?res:-res)*(angle_radian(contextptr)?cst_pi/10:(angle_degree(contextptr)?gen(18):gen(20))); //grad
	else
	  return (ed>0?res/2:-res/2)*(angle_radian(contextptr)?cst_pi/5:(angle_degree(contextptr)?gen(36):gen(40))); //grad
      }
      edh=horner(makevecteur(512,-1280,1152,-448,70,-3),edg*edg);
      if (absdouble(edh._DOUBLE_val)<1e-9 &&
	  normal(horner(makevecteur(512,-1280,1152,-448,70,-3),e*e),contextptr)==0){
	int res=int(std::floor(std::asin(absdouble(ed))*12/M_PI+.5));
	int den=12;
	int g=gcd(res,den);
	res /=g; den /=g;
	return (ed>0?res:-res)*(angle_radian(contextptr)?cst_pi/den:(angle_degree(contextptr)?gen(15*g):rdiv(50,3)*gen(g))); //grad   50/3*g grads
      }
      edh=horner(makevecteur(64,-128,80,-16,1),edg*edg);
      if (absdouble(edh._DOUBLE_val)<1e-9 &&
	  normal(horner(makevecteur(64,-128,80,-16,1),e*e),contextptr)==0){
	int res=int(std::floor(std::asin(absdouble(ed))*8/M_PI+.5));
	int den=8;
	int g=gcd(res,den);
	res /=g; den /=g;
	return (ed>0?res:-res)*(angle_radian(contextptr)?cst_pi/den:(angle_degree(contextptr)?gen(45*g)/2:gen(25))); //grad
      }
    }
    if (is_undef(e))
      return e;
    gen a,b;
    if (is_algebraic_program(e,a,b))
      return symb_prog3(a,0,asin(b,contextptr));
    if ((e.type==_SYMB) && (e._SYMBptr->sommet==at_neg))
      return -asin(e._SYMBptr->feuille,contextptr);
    gen cste=angle_radian(contextptr)?cst_pi:(angle_degree(contextptr)?180:200);
    if (e.is_symb_of_sommet(at_cos))
      e=symb_sin(cste/2-e._SYMBptr->feuille);
    if (e.is_symb_of_sommet(at_sin) && has_evalf(e._SYMBptr->feuille,a,1,contextptr)){
      // asin(sin(a))==a-2*k*pi or pi-a-2*k*pi
      gen n=_round(a/cste,contextptr);
      b=a-n*cste; // in [-pi/2,pi/2]
      if (n.type==_INT_ && n.val%2==0)
	return e._SYMBptr->feuille-n*cste;
      return n*cste-e._SYMBptr->feuille;
    }
    if (e.is_symb_of_sommet(at_sin)){
      a=e._SYMBptr->feuille;
      gen n=_round(a/cste,contextptr);
      return symb_pow(-1,n)*(a-n*cste);
    }
    if ( (e.type==_INT_) && (e.val<0) )
      return -asin(-e,contextptr);
    if (is_equal(e))
      return apply_to_equal(e,asin,contextptr);
    if (lidnt(e).empty() && is_positive(e*e-1,contextptr))
      return (angle_radian(contextptr)?1:(angle_degree(contextptr)?rad2deg_d:rad2grad_d))*asinasln(e,contextptr);
    return symb_asin(e);
  }
  static gen d_asin(const gen & args,GIAC_CONTEXT){
    gen g=inv(recursive_normal(sqrt(1-pow(args,2),contextptr),contextptr),contextptr);
    if (angle_radian(contextptr))
      return g;
    else if(angle_degree(contextptr))
      return g*rad2deg_e;
    //grad
    else
      return g*rad2grad_e;
  }
  static gen taylor_asin (const gen & lim_point,const int ordre,const unary_function_ptr & f, int direction,gen & shift_coeff,GIAC_CONTEXT){
    if (ordre<0)
      return 0; // no symbolic preprocessing
    if (is_one(lim_point)){
      shift_coeff=plus_one_half;
      identificateur x(" "); vecteur v;
      taylor(pow(2+x,minus_one_half,contextptr),x,0,ordre,v,contextptr);
      // integration with shift 
      v=integrate(v,shift_coeff);
      if (!direction)
	direction=1;
      return normal((gen(-direction)*cst_i)*gen(v),contextptr);
    }
    if (is_minus_one(lim_point)){
      shift_coeff=plus_one_half;
      identificateur x(" "); vecteur v;
      taylor(pow(2-x,minus_one_half,contextptr),x,0,ordre,v,contextptr);
      // integration with shift 
      v=integrate(v,shift_coeff);
      return v;
    }
    return taylor(lim_point,ordre,f,direction,shift_coeff,contextptr);
  }
  define_partial_derivative_onearg_genop( D_at_asin," D_at_asin",&d_asin);
  static const char _asin_s []="asin";
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval_taylor_index( 38,__asin,&asin,(size_t)&D_at_asinunary_function_ptr,&taylor_asin,_asin_s);
#else
  static define_unary_function_eval_taylor_index( 38,__asin,&asin,D_at_asin,&taylor_asin,_asin_s);
#endif
  define_unary_function_ptr5( at_asin ,alias_at_asin,&__asin,0,true);

  gen symb_acos(const gen & e){
    return symbolic(at_acos,e);
  }
  gen acos(const gen & e0,GIAC_CONTEXT){
    if ( (calc_mode(contextptr)==38 || !escape_real(contextptr) ) && !complex_mode(contextptr) && (e0.type<=_POLY) && (!is_positive(e0+1,contextptr) || !is_positive(1-e0,contextptr)))
      return gensizeerr(contextptr);
#if 0
    if (e0.type==_FLOAT_ && is_positive(e0+1,contextptr) && is_positive(1-e0,contextptr)){
#ifdef BCD
      return facos(e0._FLOAT_val,angle_mode(contextptr));
#else
      return acos(get_double(e0._FLOAT_val),contextptr);
#endif
    }
#endif
    gen e=frac_neg_out(e0,contextptr);
    if (e.type==_DOUBLE_){
      if (e._DOUBLE_val>=-1 && e._DOUBLE_val<=1){
#ifdef _SOFTMATH_H
	double d= std::giac_gnuwince_acos(e._DOUBLE_val);
#else
	double d=std::acos(e._DOUBLE_val);
#endif
	if (angle_radian(contextptr)) 
	  return d;
	else if(angle_degree(contextptr))
	  return d*rad2deg_d;
	//grad
	else
	  return d*rad2grad_d;
      }
    }
    if (e.type==_SPOL1){
      gen expo=e._SPOL1ptr->empty()?undef:e._SPOL1ptr->front().exponent;
      if (is_positive(expo,contextptr))
	return series(*e._SPOL1ptr,*at_exp,0,contextptr);
    }
#if 0
    if (e.type==_REAL){
      if (angle_radian(contextptr)) 
	return e._REALptr->acos();
      else if(angle_degree(contextptr))
	return 180*e._REALptr->acos()/cst_pi;
      //grad
      else
        return 200*e._REALptr->acos()/cst_pi;
    }
#endif
    if ( e.type==_DOUBLE_ || (e.type==_CPLX && (e.subtype)) ){
      gen res=cst_pi/2-asinasln(e,contextptr); // -cst_i*no_context_evalf(ln(sqrt(e*e-1,contextptr)+e,contextptr));
      if (angle_radian(contextptr)) 
	return res;
      else if(angle_degree(contextptr))
	return res*gen(rad2deg_d);
      //grad
      else
        return res*gen(rad2grad_d);
    }
    if (is_squarematrix(e))
      return analytic_apply(at_acos,*e._VECTptr,contextptr);
    if (e.type==_VECT)
      return apply(e,acos,contextptr);
    if (is_equal(e))
      return apply_to_equal(e,acos,contextptr);
    gen a,b;
    if (is_algebraic_program(e,a,b))
      return symb_prog3(a,0,acos(b,contextptr));
    gen g=asin(e,contextptr);
    if ( (g.type==_SYMB) && (g._SYMBptr->sommet==at_asin) )
      return symb_acos(e);
    if (angle_radian(contextptr)) 
      return normal(cst_pi_over_2-asin(e,contextptr),contextptr);
    else if(angle_degree(contextptr))
      return 90-asin(e,contextptr);
    //grad
    else
      return 100-asin(e,contextptr);
  }
  static gen d_acos(const gen & args,GIAC_CONTEXT){
    gen g= -inv(recursive_normal(sqrt(1-pow(args,2),contextptr),contextptr),contextptr);
    if (angle_radian(contextptr))
      return g;
    else if(angle_degree(contextptr))
      return g*rad2deg_e;
    //grad
    else
      return g*rad2grad_e;
  }
  define_partial_derivative_onearg_genop( D_at_acos," D_at_acos",&d_acos);
  static gen taylor_acos (const gen & lim_point,const int ordre,const unary_function_ptr & f, int direction,gen & shift_coeff,GIAC_CONTEXT){
    if (ordre<0)
      return 0; // no symbolic preprocessing
    if (is_one(lim_point)){
      shift_coeff=plus_one_half;
      identificateur x(" "); vecteur v;
      taylor(pow(2+x,minus_one_half,contextptr),x,0,ordre,v,contextptr);
      // integration with shift 
      v=integrate(v,shift_coeff);
      if (!direction)
	direction=1;
      return -normal((gen(-direction)*cst_i)*gen(v),contextptr);
    }
    if (is_minus_one(lim_point)){
      shift_coeff=plus_one_half;
      identificateur x(" "); vecteur v;
      taylor(pow(2-x,minus_one_half,contextptr),x,0,ordre,v,contextptr);
      // integration with shift 
      v=integrate(v,shift_coeff);
      return -v;
    }
    return taylor(lim_point,ordre,f,direction,shift_coeff,contextptr);
  }
  static const char _acos_s []="acos";
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval_taylor_index( 40,__acos,&acos,(size_t)&D_at_acosunary_function_ptr,&taylor_acos,_acos_s);
#else
  static define_unary_function_eval_taylor_index( 40,__acos,&acos,D_at_acos,&taylor_acos,_acos_s);
#endif
  define_unary_function_ptr5( at_acos ,alias_at_acos,&__acos,0,true);

  gen symb_sinh(const gen & e){
    return symbolic(at_sinh,e);
  }
  gen sinh(const gen & e0,GIAC_CONTEXT){
#if 0
    if (e0.type==_FLOAT_){
#ifdef BCD
      return fsinh(e0._FLOAT_val);
#else
      return sinh(get_double(e0._FLOAT_val),contextptr);
#endif
    }
#endif
    gen e=frac_neg_out(e0,contextptr);
    if (e.type==_DOUBLE_){
#ifdef _SOFTMATH_H
      return std::giac_gnuwince_sinh(e._DOUBLE_val);
#else
      return std::sinh(e._DOUBLE_val);
#endif
    }
    if (e.type==_SPOL1){
      gen expo=e._SPOL1ptr->empty()?undef:e._SPOL1ptr->front().exponent;
      if (is_positive(expo,contextptr))
	return series(*e._SPOL1ptr,*at_sinh,0,contextptr);
    }
    // if (e.type==_REAL) return e._REALptr->sinh();
    if (e.type==_CPLX){
      if (e.subtype){
#ifdef _SOFTMATH_H
      return std::giac_gnuwince_sinh(gen2complex_d(e));
#else
      return std::sinh(gen2complex_d(e));
#endif
      }
#if 0
      if (e._CPLXptr->type==_REAL || e._CPLXptr->type==_FLOAT_){
	gen g=exp(e,contextptr);
	return (g-inv(g,contextptr))/2;
      }
#endif
    }
    if (is_squarematrix(e))
      return analytic_apply(at_sinh,*e._VECTptr,contextptr);
    if (e.type==_VECT)
      return apply(e,sinh,contextptr);
    if ( is_zero(e,contextptr) || (is_undef(e)) || (is_inf(e)))
      return e;
    if (is_equal(e))
      return apply_to_equal(e,sinh,contextptr);
    gen a,b;
    if (is_algebraic_program(e,a,b))
      return symb_prog3(a,0,sinh(b,contextptr));
    if (e.is_neg())
      return -sinh(e._SYMBptr->feuille,contextptr);
    if (e.type==_SYMB && has_i(e)){
      gen ee=simplifier(-cst_i*e,contextptr);
      return cst_i*sin(ee,contextptr);
    }
    return symb_sinh(e);
  }
  static gen d_at_sinh(const gen & e,GIAC_CONTEXT){
    return cosh(e,contextptr);
  }
  define_partial_derivative_onearg_genop( D_at_sinh," D_at_sinh",&d_at_sinh);
  static const char _sinh_s []="sinh";
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval3_index (44,__sinh,&sinh,(size_t)&D_at_sinhunary_function_ptr,_sinh_s);
#else
  static define_unary_function_eval3_index (44,__sinh,&sinh,D_at_sinh,_sinh_s);
#endif
  define_unary_function_ptr5( at_sinh ,alias_at_sinh,&__sinh,0,true);

  gen symb_cosh(const gen & e){
    return symbolic(at_cosh,e);
  }
  gen cosh(const gen & e0,GIAC_CONTEXT){
#if 0
    if (e0.type==_FLOAT_){
#ifdef BCD
      return fcosh(e0._FLOAT_val);
#else
      return cosh(get_double(e0._FLOAT_val),contextptr);
#endif
    }
#endif
    gen e=frac_neg_out(e0,contextptr);
    if (e.type==_DOUBLE_){
#ifdef _SOFTMATH_H
      return std::giac_gnuwince_cosh(e._DOUBLE_val);
#else
      return std::cosh(e._DOUBLE_val);
#endif
    }
    if (e.type==_SPOL1){
      gen expo=e._SPOL1ptr->empty()?undef:e._SPOL1ptr->front().exponent;
      if (is_positive(expo,contextptr))
	return series(*e._SPOL1ptr,*at_cosh,0,contextptr);
    }
    // if (e.type==_REAL) return e._REALptr->cosh();
    if (e.type==_CPLX){
      if (e.subtype){
#ifdef _SOFTMATH_H
      return std::giac_gnuwince_cosh(gen2complex_d(e));
#else
      return std::cosh(gen2complex_d(e));
#endif
      }
#if 0
      if (e._CPLXptr->type==_REAL || e._CPLXptr->type==_FLOAT_){
	gen g=exp(e,contextptr);
	return (g+inv(g,contextptr))/2;
      }
#endif
    }
    if (is_squarematrix(e))
      return analytic_apply(at_cosh,*e._VECTptr,contextptr);
    if (e.type==_VECT)
      return apply(e,cosh,contextptr);
    if (is_zero(e,contextptr))
      return 1;
    if (is_undef(e))
      return e;
    if (is_inf(e))
      return plus_inf;
    if (is_equal(e))
      return apply_to_equal(e,cosh,contextptr);
    gen a,b;
    if (is_algebraic_program(e,a,b))
      return symb_prog3(a,0,cosh(b,contextptr));
    if (e.is_neg())
      return cosh(e._SYMBptr->feuille,contextptr);
    if (e.type==_SYMB && has_i(e)){
      gen ee=simplifier(-cst_i*e,contextptr);
      return cos(ee,contextptr);
    }
    return symb_cosh(e);
  }
  define_partial_derivative_onearg_genop( D_at_cosh,"D_at_cosh",sinh);
  static const char _cosh_s []="cosh";
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval3_index (46,__cosh,&cosh,(size_t)&D_at_coshunary_function_ptr,_cosh_s);
#else
  static define_unary_function_eval3_index (46,__cosh,&cosh,D_at_cosh,_cosh_s);
#endif
  define_unary_function_ptr5( at_cosh ,alias_at_cosh,&__cosh,0,true);

  // static symbolic symb_tanh(const gen & e){ return symbolic(at_tanh,e);  }
  gen tanh(const gen & e0,GIAC_CONTEXT){
#if 0
    if (e0.type==_FLOAT_){
#ifdef BCD
      return ftanh(e0._FLOAT_val);
#else
      return tanh(get_double(e0._FLOAT_val),contextptr);
#endif
    }
#endif
    gen e=frac_neg_out(e0,contextptr);
    if (e.type==_DOUBLE_){
#ifdef _SOFTMATH_H
      return std::giac_gnuwince_tanh(e._DOUBLE_val);
#else
      return std::tanh(e._DOUBLE_val);
#endif
    }
    if (e.type==_SPOL1){
      gen expo=e._SPOL1ptr->empty()?undef:e._SPOL1ptr->front().exponent;
      if (is_positive(expo,contextptr))
	return series(*e._SPOL1ptr,*at_tanh,0,contextptr);
    }
    // if (e.type==_REAL) return e._REALptr->tanh();
    if (e.type==_CPLX){
      if (e.subtype){
	complex_double c(gen2complex_d(e));
#ifdef _SOFTMATH_H
	return std::giac_gnuwince_tanh(c);
#else
	return std::sinh(c)/std::cosh(c);
#endif
      }
#if 0
      if (e._CPLXptr->type==_REAL || e._CPLXptr->type==_FLOAT_){
	gen g=exp(2*e,contextptr);
	return (g+1)/(g-1);
      }
#endif
    }
    if (is_squarematrix(e))
      return analytic_apply(at_tanh,*e._VECTptr,contextptr);
    if (e.type==_VECT)
      return apply(e,tanh,contextptr);
    if (is_zero(e,contextptr)) 
      return e;
    if (is_undef(e) || (e==unsigned_inf))
      return undef;
    if (e==plus_inf)
      return 1;
    if (e==minus_inf)
      return -1;
    if (is_equal(e))
      return apply_to_equal(e,tanh,contextptr);
    gen a,b;
    if (is_algebraic_program(e,a,b))
      return symb_prog3(a,0,tanh(b,contextptr));
    if (e.is_neg())
      return -tanh(e._SYMBptr->feuille,contextptr);
    if (e.type==_SYMB && has_i(e)){
      gen ee=simplifier(-cst_i*e,contextptr);
      return cst_i*tan(ee,contextptr);
    }
    return symbolic(at_tanh,e);
  }
  static gen d_tanh(const gen & e,GIAC_CONTEXT){
    return 1-pow(tanh(e,contextptr),2);
  }
  define_partial_derivative_onearg_genop( D_at_tanh," D_at_tanh",&d_tanh);
  static const char _tanh_s []="tanh";
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval3_index (48,__tanh,&tanh,(size_t)&D_at_tanhunary_function_ptr,_tanh_s);
#else
  static define_unary_function_eval3_index (48,__tanh,&tanh,D_at_tanh,_tanh_s);
#endif
  define_unary_function_ptr5( at_tanh ,alias_at_tanh,&__tanh,0,true);

  // static symbolic symb_asinh(const gen & e){  return symbolic(at_asinh,e);  }
  static gen asinhasln(const gen & x,GIAC_CONTEXT){
    return ln(x+sqrt(x*x+1,contextptr),contextptr);
  }
  gen asinh(const gen & e0,GIAC_CONTEXT){
#if 0
    if (e0.type==_FLOAT_){
#ifdef BCD
      return fasinh(e0._FLOAT_val);
#else
      return asinh(get_double(e0._FLOAT_val),contextptr);
#endif
    }
#endif
    gen e=frac_neg_out(e0,contextptr);
    if (e.type==_DOUBLE_)
      return asinhasln(e,contextptr);
    if (e.type==_SPOL1){
      gen expo=e._SPOL1ptr->empty()?undef:e._SPOL1ptr->front().exponent;
      if (is_positive(expo,contextptr))
	return series(*e._SPOL1ptr,*at_asinh,0,contextptr);
    }
    // if (e.type==_REAL) return e._REALptr->asinh();
    if ( (e.type==_CPLX) && (e.subtype))
      return no_context_evalf(asinhasln(e,contextptr));
    if (is_squarematrix(e)){
      context tmp;
      return analytic_apply(at_asinh,*e._VECTptr,&tmp); 
    }
    if (e.type==_VECT)
      return apply(e,asinh,contextptr);
    if (is_zero(e,contextptr) || is_inf(e))
      return e;
    if (is_undef(e))
      return e;
    if (is_equal(e))
      return apply_to_equal(e,asinh,contextptr);
    gen a,b;
    if (is_algebraic_program(e,a,b))
      return symb_prog3(a,0,asinh(b,contextptr));
    if (keep_acosh_asinh(contextptr))
      return symbolic(at_asinh,e);
    return ln(e+sqrt(pow(e,2)+1,contextptr),contextptr);
  }
  static gen d_asinh(const gen & args,GIAC_CONTEXT){
    return inv(recursive_normal(sqrt(pow(args,2)+1,contextptr),contextptr),contextptr);
  }
  define_partial_derivative_onearg_genop( D_at_asinh," D_at_asinh",&d_asinh);
  static const char _asinh_s []="asinh";
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval3_index (50,__asinh,&asinh,(size_t)&D_at_asinhunary_function_ptr,_asinh_s);
#else
  static define_unary_function_eval3_index (50,__asinh,&asinh,D_at_asinh,_asinh_s);
#endif
  define_unary_function_ptr5( at_asinh ,alias_at_asinh,&__asinh,0,true);

  // static symbolic symb_acosh(const gen & e){  return symbolic(at_cosh,e);  }
  static gen acoshasln(const gen & x,GIAC_CONTEXT){
    if (re(x,contextptr)==x)
      return ln(x+sqrt(x*x-1,contextptr),contextptr); // avoid multiple sqrt but it's the opposite for example for x non real
    return ln(x+sqrt(x+1,contextptr)*sqrt(x-1,contextptr),contextptr);
  }
  gen acosh(const gen & e0,GIAC_CONTEXT){
#if 0
    if (e0.type==_FLOAT_){
      if (is_strictly_greater(1,e0,contextptr))
	return ln(e0+sqrt(pow(e0,2)-1,contextptr),contextptr);
#ifdef BCD
      return facosh(e0._FLOAT_val);
#else
      return acosh(get_double(e0._FLOAT_val),contextptr);
#endif
    }
#endif
    gen e=frac_neg_out(e0,contextptr);
    if (e.type==_DOUBLE_)
      return acoshasln(e,contextptr);
    if (e.type==_SPOL1){
      gen expo=e._SPOL1ptr->empty()?undef:e._SPOL1ptr->front().exponent;
      if (is_positive(expo,contextptr))
	return series(*e._SPOL1ptr,*at_acosh,0,contextptr);
    }
    // if (e.type==_REAL) return e._REALptr->acosh();
    if ( (e.type==_CPLX) && (e.subtype))
      return no_context_evalf(acoshasln(e,contextptr));
    if (is_squarematrix(e))
      return analytic_apply(at_acosh,*e._VECTptr,0);
    if (e.type==_VECT)
      return apply(e,acosh,contextptr);
    if (is_one(e))
      return 0;
    if (e==plus_inf)
      return plus_inf;
    if (is_undef(e))
      return e;
    if (is_equal(e))
      return apply_to_equal(e,acosh,contextptr);
    gen a,b;
    if (is_algebraic_program(e,a,b))
      return symb_prog3(a,0,acosh(b,contextptr));
    if (keep_acosh_asinh(contextptr))
      return symbolic(at_acosh,e);
    return acoshasln(e,contextptr);
    // return ln(e+sqrt(pow(e,2)-1,contextptr),contextptr);
  }
  static gen d_acosh(const gen & args,GIAC_CONTEXT){
    return inv(recursive_normal(sqrt(pow(args,2)-1,contextptr),contextptr),contextptr);
  }
  define_partial_derivative_onearg_genop( D_at_acosh," D_at_acosh",&d_acosh);
  static const char _acosh_s []="acosh";
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval3_index (52,__acosh,&acosh,(size_t)&D_at_acoshunary_function_ptr,_acosh_s);
#else
  static define_unary_function_eval3_index (52,__acosh,&acosh,D_at_acosh,_acosh_s);
#endif
  define_unary_function_ptr5( at_acosh ,alias_at_acosh,&__acosh,0,true);

  // static symbolic symb_atanh(const gen & e){  return symbolic(at_atanh,e);}
  gen atanh(const gen & e0,GIAC_CONTEXT){
#if 0
    if (e0.type==_FLOAT_){
      if (is_strictly_greater(e0,1,contextptr) || is_strictly_greater(-1,e0,contextptr))
	return rdiv(ln(rdiv(1+e0,1-e0),contextptr),plus_two,contextptr);
#ifdef BCD
      return fatanh(e0._FLOAT_val);
#else
      return atanh(get_double(e0._FLOAT_val),contextptr);
#endif
    }
#endif
    gen e=frac_neg_out(e0,contextptr);
    if (e.type==_DOUBLE_ && fabs(e._DOUBLE_val)<1){
#ifdef _SOFTMATH_H
      return std::giac_gnuwince_log((1+e._DOUBLE_val)/(1-e._DOUBLE_val))/2;
#else
      return std::log((1+e._DOUBLE_val)/(1-e._DOUBLE_val))/2;
#endif
    }
    if (e.type==_SPOL1){
      gen expo=e._SPOL1ptr->empty()?undef:e._SPOL1ptr->front().exponent;
      if (is_positive(expo,contextptr))
	return series(*e._SPOL1ptr,*at_atanh,0,contextptr);
    }
    // if (e.type==_REAL) return e._REALptr->atanh();
    if ( (e.type==_CPLX) && (e.subtype ))
      return no_context_evalf(rdiv(ln(rdiv(1+e,1-e,contextptr),contextptr),plus_two));
    if (is_squarematrix(e))
      return analytic_apply(at_atanh,*e._VECTptr,0);
    if (e.type==_VECT)
      return apply(e,atanh,contextptr);
    if (is_zero(e,contextptr))
      return e;
    if (is_one(e))
      return plus_inf;
    if (is_minus_one(e))
      return minus_inf;
    if (is_undef(e))
      return e;
    if (is_equal(e))
      return apply_to_equal(e,atanh,contextptr);
    gen a,b;
    if (is_algebraic_program(e,a,b))
      return symb_prog3(a,0,atanh(b,contextptr));
    return rdiv(ln(rdiv(1+e,1-e,contextptr),contextptr),plus_two);
    // return symbolic(at_atanh,e);
  }
  static gen d_atanh(const gen & args,GIAC_CONTEXT){
    return inv(1-pow(args,2),contextptr);
  }
  define_partial_derivative_onearg_genop( D_at_atanh," D_at_atanh",&d_atanh);
  static const char _atanh_s []="atanh";
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval3_index (54,__atanh,&atanh,(size_t)&D_at_atanhunary_function_ptr,_atanh_s);
#else
  static define_unary_function_eval3_index (54,__atanh,&atanh,D_at_atanh,_atanh_s);
#endif
  define_unary_function_ptr5( at_atanh ,alias_at_atanh,&__atanh,0,true);

  static string printasquote(const gen & g,const char * s,GIAC_CONTEXT){
#if 0
    if (calc_mode(contextptr)==38)
      return "QUOTE("+g.print(contextptr)+")";
    else
#endif
      return "'"+g.print(contextptr)+"'"; 
  }
  gen symb_quote(const gen & arg){
    return symbolic(at_quote,arg);
  }
  gen quote(const gen & args,GIAC_CONTEXT){
    if (args.type==_VECT && args.subtype==_SEQ__VECT && !args._VECTptr->empty() && args._VECTptr->front().type==_FUNC){
      const unary_function_ptr & u =*args._VECTptr->front()._FUNCptr;
      vecteur v=vecteur(args._VECTptr->begin()+1,args._VECTptr->end());
      gen arg=eval(gen(v,_SEQ__VECT),eval_level(contextptr),contextptr);
      return symbolic(u,arg);
    }
    return args;
  }
  define_partial_derivative_onearg_genop( D_at_quote," D_at_quote",&quote);
  static const char _quote_s []="quote";
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval5_quoted (__quote,&quote,(size_t)&D_at_quoteunary_function_ptr,_quote_s,&printasquote,0);
#else
  static define_unary_function_eval5_quoted (__quote,&quote,D_at_quote,_quote_s,&printasquote,0);
#endif
  define_unary_function_ptr5( at_quote ,alias_at_quote,&__quote,0,true);

  // symbolic symb_unquote(const gen & arg){    return symbolic(at_unquote,arg);  }
  gen unquote(const gen & arg,GIAC_CONTEXT){
    return eval(arg,1,contextptr);
  }
  define_partial_derivative_onearg_genop( D_at_unquote," D_at_unquote",(const gen_op_context)unquote);
  static const char _unquote_s []="unquote";
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval3 (__unquote,(const gen_op_context)unquote,(size_t)&D_at_unquoteunary_function_ptr,_unquote_s);
#else
  static define_unary_function_eval3 (__unquote,(const gen_op_context)unquote,D_at_unquote,_unquote_s);
#endif
  define_unary_function_ptr5( at_unquote ,alias_at_unquote,&__unquote,0,true);

  static gen symb_order_size(const gen & e){
    return symbolic(at_order_size,e);
  }
  gen order_size(const gen & arg,GIAC_CONTEXT){
    if (arg.type==_SPOL1 && arg._SPOL1ptr->size()==1){
      gen expo=arg._SPOL1ptr->front().exponent;
      char sv=series_variable_name(contextptr);
      if (expo!=1)
	*logptr(contextptr) << "order_size argument should always be the series variable name. This means that O("<<sv<<"^"<<expo << ") should be written "<< sv << "^" << expo <<"*order_size("<< sv << ")" << endl;
      return sparse_poly1(1,monome(undef,0));
    }
    return symb_order_size(arg);
  }
  define_partial_derivative_onearg_genop( D_at_order_size," D_at_order_size",order_size);
  static const char _order_size_s []="order_size";
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval3 (__order_size,&order_size,(size_t)&D_at_order_sizeunary_function_ptr,_order_size_s);
#else
  static define_unary_function_eval3 (__order_size,&order_size,D_at_order_size,_order_size_s);
#endif
  define_unary_function_ptr5( at_order_size ,alias_at_order_size,&__order_size,0,true);

  gen re(const gen & a,GIAC_CONTEXT){
    if (is_equal(a))
      return apply_to_equal(a,re,contextptr);
    gen a1,b;
    if (is_algebraic_program(a,a1,b))
      return symb_prog3(a1,0,symbolic_re(b));
    return a.re(contextptr);
  }
  static const char _re_s []="re";
  static define_unary_function_eval4 (__re,(const gen_op_context)re,_re_s,0,&texprintsommetasoperator);
  define_unary_function_ptr5( at_re ,alias_at_re,&__re,0,true);

  gen im(const gen & a,GIAC_CONTEXT){
    if (is_equal(a))
      return apply_to_equal(a,im,contextptr);
    gen a1,b;
    if (is_algebraic_program(a,a1,b))
      return symb_prog3(a1,0,symbolic_im(b));
    return a.im(contextptr);
  }
  static const char _im_s []="im";
  static define_unary_function_eval4 (__im,(const gen_op_context)im,_im_s,0,&texprintsommetasoperator);
  define_unary_function_ptr5( at_im ,alias_at_im,&__im,0,true);

  gen symb_conj(const gen & e){
    return symbolic(at_conj,e);
  }
  gen conj(const gen & a,GIAC_CONTEXT){
    if (is_equal(a))
      return apply_to_equal(a,conj,contextptr);
    gen a1,b;
    if (is_algebraic_program(a,a1,b))
      return symb_prog3(a1,0,symb_conj(b));
    return a.conj(contextptr);
  }
  static const char _conj_s []="conj";
  static define_unary_function_eval4 (__conj,(const gen_op_context)conj,_conj_s,0,&texprintsommetasoperator);
  define_unary_function_ptr5( at_conj ,alias_at_conj,&__conj,0,true);

  static gen taylor_sign (const gen & lim_point,const int ordre,const unary_function_ptr & f, int direction,gen & shift_coeff,GIAC_CONTEXT){
    if (ordre<0)
      return 0; // no symbolic preprocessing
    shift_coeff=0;
    if (is_strictly_positive(lim_point,contextptr) || (is_zero(lim_point,contextptr) && direction==1))
      return makevecteur(1);
    if (is_strictly_positive(-lim_point,contextptr) || (is_zero(lim_point,contextptr) && direction==-1))
      return makevecteur(-1);
    // FIXME? maybe add 
    if (!is_zero(lim_point)) return makevecteur(symbolic(at_sign,lim_point));
    return gensizeerr(gettext("Taylor sign with unsigned limit"));
  }

  gen _sign(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    return apply(g,contextptr,sign);
  }
  // static gen symb_sign(const gen & e){    return symbolic(at_sign,e);  }
  static const char _sign_s []="sign";
  define_partial_derivative_onearg_genop( D_at_sign,"D_at_sign",_constant_zero);
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval_taylor( __sign,_sign,(size_t)&D_at_signunary_function_ptr,&taylor_sign,_sign_s);
#else
  static define_unary_function_eval_taylor( __sign,_sign,D_at_sign,&taylor_sign,_sign_s);
#endif
  define_unary_function_ptr5( at_sign ,alias_at_sign,&__sign,0,true);

  gen _abs(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return abs(args,contextptr);
    if (ckmatrix(args))
      return _l2norm(args,contextptr);
    if (args.subtype==_POINT__VECT || args.subtype==_GGBVECT)
      return _l2norm(args,contextptr);
    return apply(args,contextptr,abs);
  }
  gen symb_abs(const gen & e){
    return symbolic(at_abs,e);
  }
  static gen taylor_abs (const gen & lim_point,const int ordre,const unary_function_ptr & f, int direction,gen & shift_coeff,GIAC_CONTEXT){
    if (ordre<0)
      return 0; // no symbolic preprocessing
    shift_coeff=0;
    if (is_strictly_positive(lim_point,contextptr) || (is_zero(lim_point,contextptr) && direction==1))
      return makevecteur(lim_point,1);
    if (is_strictly_positive(-lim_point,contextptr) || (is_zero(lim_point,contextptr) && direction==-1))
      return makevecteur(-lim_point,-1);
    return gensizeerr(gettext("Taylor abs with unsigned limit"));
  }
  static const char _abs_s []="abs";
  static gen d_abs(const gen & g,GIAC_CONTEXT){
    return symbolic(at_abs,g)/g;
  }
  define_partial_derivative_onearg_genop( D_at_abs,"D_at_abs",d_abs);
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval_taylor_index(20, __abs,&_abs,(size_t)&D_at_absunary_function_ptr,&taylor_abs,_abs_s);
#else
  static define_unary_function_eval_taylor_index(20, __abs,&_abs,D_at_abs,&taylor_abs,_abs_s);
#endif
  define_unary_function_ptr5( at_abs ,alias_at_abs,&__abs,0,true);

  // gen symb_arg(const gen & e){ return symbolic(at_arg,e);  }
  static const char _arg_s []="arg";
  define_unary_function_eval_index (22,__arg,&arg,_arg_s);
  define_unary_function_ptr5( at_arg ,alias_at_arg,&__arg,0,true);

  static gen symb_cyclotomic(const gen & e){
    return symbolic(at_cyclotomic,e);
  }
  gen _cyclotomic(const gen & a,GIAC_CONTEXT){
    if ( a.type==_STRNG && a.subtype==-1) return  a;
    if (a.type==_VECT && a._VECTptr->size()==2 && a._VECTptr->front().type==_INT_)
      return symb_horner(cyclotomic(a._VECTptr->front().val),a._VECTptr->back());
    if (a.type!=_INT_)
      return gentypeerr(contextptr); // symb_cyclotomic(a);
    return cyclotomic(a.val);
  }
  static const char _cyclotomic_s []="cyclotomic";
  static define_unary_function_eval (__cyclotomic,&_cyclotomic,_cyclotomic_s);
  define_unary_function_ptr5( at_cyclotomic ,alias_at_cyclotomic,&__cyclotomic,0,true);

  string printassto(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if ( (feuille.type!=_VECT) || (feuille._VECTptr->size()!=2) )
      return string(sommetstr)+('('+feuille.print(contextptr)+')');
    vecteur & v=*feuille._VECTptr;
    if (feuille.subtype==_SORTED__VECT && feuille._VECTptr->front().is_program()){
      gen prog=feuille._VECTptr->front()._SYMBptr->feuille;
      if (prog.type==_VECT && prog._VECTptr->size()==3 && (prog._VECTptr->front()!=_VECT || prog._VECTptr->front()._VECTptr->size()==2)){
	const gen & val=prog._VECTptr->back();
	gen arg=prog._VECTptr->front();
	if (arg.type==_VECT && arg.subtype==_SEQ__VECT && arg._VECTptr->size()==1)
	  arg=arg._VECTptr->front();
	prog=symb_of(feuille._VECTptr->back(),arg); //symbolic(at_of,makesequence(feuille._VECTptr->back(),arg));
	return printassto(gen(makevecteur(val,prog),_SORTED__VECT),sommetstr,contextptr);
      }
    }
#if 0
    if (abs_calc_mode(contextptr)==38 && v.back().type!=_VECT){
      string s=v.back().print(contextptr);
      if (s.size()>2 && s[0]=='1' && s[1]=='_')
	s=s.substr(1,s.size()-1);
      return v.front().print(contextptr)+(calc_mode(contextptr)==38?"\xe2\x96\xba":"=>")+s;
    }
    if (xcas_mode(contextptr)==3){
      if ( (v.front().type==_SYMB) && (v.front()._SYMBptr->sommet==at_program)){
	gen & b=v.front()._SYMBptr->feuille._VECTptr->back();
	if  (b.type==_VECT || (b.type==_SYMB && (b._SYMBptr->sommet==at_local || b._SYMBptr->sommet==at_bloc))){
	  string s(v.front().print(contextptr));
	  s=s.substr(10,s.size()-10);
	  return ":"+v.back().print(contextptr)+s;
	}
	else {
	  vecteur & tmpv = *v.front()._SYMBptr->feuille._VECTptr;
	  if (tmpv[0].type==_VECT && tmpv[0].subtype==_SEQ__VECT && tmpv[0]._VECTptr->size()==1)
	    return tmpv[2].print(contextptr)+" => "+v.back().print(contextptr)+"("+tmpv[0]._VECTptr->front().print(contextptr)+")";
	  else
	    return tmpv[2].print(contextptr)+" => "+v.back().print(contextptr)+"("+tmpv[0].print(contextptr)+")";
	}
      }
      else 
	return v.front().print(contextptr)+" => "+v.back().print(contextptr);
    }
#endif
    string stos=python_compat(contextptr)?"=":":=";
#ifndef GIAC_HAS_STO_38
    if (v.back().is_symb_of_sommet(at_of) && feuille.subtype!=_SORTED__VECT){
      gen f=v.back()._SYMBptr->feuille;
      if (f.type==_VECT && f._VECTptr->size()==2){
	return f._VECTptr->front().print(contextptr)+"[["+f._VECTptr->back().print(contextptr)+"]] "+stos+" "+ v.front().print(contextptr);
      }
    }
#endif
    string s(v.back().print(contextptr)+stos);
    if (v.front().type==_SEQ__VECT)
      return s+"("+v.front().print(contextptr)+")";
    else
      return s+v.front().print(contextptr);
  }

  gen _calc_mode(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    int & mode=calc_mode(contextptr);
    if (args.type==_INT_)
      mode=args.val;
    if (args.type==_DOUBLE_)
      mode=int(args._DOUBLE_val);
#if 0
    if (args.type==_FLOAT_)
      mode=get_int(args._FLOAT_val);
#endif
    return mode;
  }
  static const char _calc_mode_s []="calc_mode";
  static define_unary_function_eval (__calc_mode,&_calc_mode,_calc_mode_s);
  define_unary_function_ptr5( at_calc_mode ,alias_at_calc_mode,&__calc_mode,0,true); 
  
  bool is_numericv(const vecteur & v, int withfracint){
    const_iterateur it=v.begin(),itend=v.end();
    for (;it!=itend;++it){
      if (it->type==_VECT || !is_fully_numeric(*it, withfracint))
	return false;
    }
    return true;
  }
  bool is_numericm(const vecteur & v, int withfracint){
    const_iterateur it=v.begin(),itend=v.end();
    for (;it!=itend;++it){
      if (it->type!=_VECT || !is_numericv(*it->_VECTptr, withfracint))
	return false;
    }
    return true;
  }

  bool check_vect_38(const string & s){
    return false;
  }
  // check value type for storing value in s using 38 compatibility mode
  bool check_sto_38(gen & value,const char * s){
    return false;
  }

#ifdef GIAC_HAS_STO_38
  bool do_storcl_38(gen & value,const char * name_space,const char * idname,gen indice,bool at_of,GIAC_CONTEXT, gen const *sto,bool OnlyLocal);
  bool (*storcl_38)(gen & value,const char * name_space,const char * idname,gen indice,bool at_of,GIAC_CONTEXT, gen const *sto,bool OnlyLocal)=do_storcl_38;
#else
  bool (*storcl_38)(gen & value,const char * name_space,const char * idname,gen indice,bool at_of,GIAC_CONTEXT, gen const *sto,bool OnlyLocal)=0;
#endif
  gen_op_context * interactive_op_tab = 0;
  int (*is_known_name_38)(const char * name_space,const char * idname)=0; // Not used anymore!
  gen (*of_pointer_38)(const void * appptr,const void * varptr,const gen & args)=0;

  // store a in b
#ifdef HAVE_SIGNAL_H_OLD
  bool signal_store=true;
#endif

  bool is_local(const gen & b,GIAC_CONTEXT){
    if (b.type!=_IDNT)
      return false;
    if (contextptr){
      const context * ptr=contextptr;
      for (;ptr->previous && ptr->tabptr;ptr=ptr->previous){
	sym_tab::iterator it=ptr->tabptr->find(b._IDNTptr->id_name),itend=ptr->tabptr->end();
	if (it!=itend)
	  return true;
      }
    }
    return false;
  }

  static bool in_stomap(gen_map & m,const gen & indice,const gen & a){
    if (indice.is_interval() && indice._SYMBptr->feuille.type==_VECT && indice._SYMBptr->feuille._VECTptr->size()==2){
      gen deb=indice._SYMBptr->feuille._VECTptr->front();
      gen fin=indice._SYMBptr->feuille._VECTptr->back();
      if (!is_integral(deb) || !is_integral(fin) || deb.type!=_INT_ || fin.type!=_INT_)
	return false;
      if (a.type==_VECT){
	if (a._VECTptr->size()!=fin.val-deb.val+1)
	  return false;
	for (int i=deb.val;i<=fin.val;++i)
	  m[i]=(*a._VECTptr)[i-deb.val];	  
	return true;
      }
      for (int i=deb.val;i<=fin.val;++i)
	m[i]=a;
      return true;
    }
    if (indice.type!=_VECT || indice._VECTptr->size()!=2){
      m[indice]=a;
      return true;
    }
    gen ligne=indice._VECTptr->front(),col=indice._VECTptr->back();
    if (ligne.is_interval() && ligne._SYMBptr->feuille.type==_VECT && ligne._SYMBptr->feuille._VECTptr->size()==2){
      gen deb=ligne._SYMBptr->feuille._VECTptr->front();
      gen fin=ligne._SYMBptr->feuille._VECTptr->back();
      if (!is_integral(deb) || !is_integral(fin) || deb.type!=_INT_ || fin.type!=_INT_)
	return false;
      bool both=col.is_interval()&& col._SYMBptr->feuille.type==_VECT && col._SYMBptr->feuille._VECTptr->size()==2;
      int shift=0;
      if (both){
	gen coldeb=col._SYMBptr->feuille._VECTptr->front();
	gen colend=col._SYMBptr->feuille._VECTptr->back();
	if (!is_integral(coldeb) || !is_integral(colend) || coldeb.type!=_INT_ || colend.type!=_INT_ || colend.val-coldeb.val!=fin.val-deb.val)
	  return false;
	shift=coldeb.val-deb.val;
      }
      if (a.type==_VECT){
	if (a._VECTptr->size()!=fin.val-deb.val+1)
	  return false;
	if (both){
	  for (int i=deb.val;i<=fin.val;++i)
	    m[makesequence(i,i+shift)]=(*a._VECTptr)[i-deb.val];
	}
	else {
	  for (int i=deb.val;i<=fin.val;++i)
	    m[makesequence(i,col)]=(*a._VECTptr)[i-deb.val];
	}
	return true;
      }
      if (both){
	for (int i=deb.val;i<=fin.val;++i)
	  m[makesequence(i,i+shift)]=a;
      }
      else {
	for (int i=deb.val;i<=fin.val;++i)
	  m[makesequence(i,col)]=a;
      }
      return true;
    }
    if (col.is_interval()&& col._SYMBptr->feuille.type==_VECT && col._SYMBptr->feuille._VECTptr->size()==2){
      gen deb=col._SYMBptr->feuille._VECTptr->front();
      gen fin=col._SYMBptr->feuille._VECTptr->back();
      if (!is_integral(deb) || !is_integral(fin) || deb.type!=_INT_ || fin.type!=_INT_)
	return false;
      if (a.type==_VECT){
	if (a._VECTptr->size()!=fin.val-deb.val+1)
	  return false;
	for (int i=deb.val;i<=fin.val;++i)
	  m[makesequence(ligne,i)]=(*a._VECTptr)[i-deb.val];	  
	return true;
      }
      for (int i=deb.val;i<=fin.val;++i)
	m[makesequence(ligne,i)]=a;
      return true;
    }
    m[indice]=a;
    return true;
  }

  bool stomap(gen_map & m,const gen & indice,const gen & a){
    if (!in_stomap(m,indice,a))
      return false;
    return true;
  }

  /* TI variable OS support in ti/vars.h
     accurate real number support ti/real.h, type real_t
     https://ce-programming.github.io/toolchain/headers/ti/vars.html
     int os_CreateEquation(const char *name, const equ_t *data);
     equ_t *os_GetEquationData(const char *name, int *archived);

     int os_GetMatrixDims(const char *name, int *rows, int *cols);
     int os_SetMatrixDims(const char *name, int rows, int col;
     int os_GetMatrixElement(const char *name, int row, int col, real_t *value);
     int os_SetMatrixElement(const char *name, int row, int col, const real_t *value);

     int os_RunPrgm(const char *prgm, void *data, size_t size, os_runprgm_callback_t callback);

     struct matrix_t in vars.h uint8_t cols,rows; real_t items[1];
     struct equ_t (#include <vars.h>)    uint16_t len; char data[1];
     ti/tokens.h
     tokens are 1 or 2 bytes, if two 1st token is a marker like OS_TOK_EQU
   */
  
  gen sto(const gen & a,const gen & b,const context * contextptr){
    return sto(a,b,false,contextptr);
  }
  // in_place==true to store in vector/matrices without making a new copy
  gen sto(const gen & a,const gen & b,bool in_place,const context * contextptr_){
    if (a.type==_STRNG && is_undef(a))
      return a;
    if ( (a.type==_IDNT || a.is_symb_of_sommet(at_at)) && b.is_symb_of_sommet(at_rootof) && contextptr_){
      if (!contextptr_->globalcontextptr->rootofs)
	contextptr_->globalcontextptr->rootofs=new vecteur;
      gen b_=eval(b,1,contextptr_);
      gen Pmin=b_._SYMBptr->feuille;
      if (Pmin.type!=_VECT || Pmin._VECTptr->size()!=2 || Pmin._VECTptr->front()!=makevecteur(1,0))
	return gensizeerr(gettext("Bad rootof (in sto)"));
      Pmin=Pmin._VECTptr->back();
      vecteur & r =*contextptr_->globalcontextptr->rootofs;
      for (unsigned i=0;i<r.size();++i){
	gen ri=r[i];
	if (ri.type==_VECT && ri._VECTptr->size()==2 && Pmin==ri._VECTptr->front()){
	  ri._VECTptr->back()=a;
	  return sto(b,a,in_place,contextptr_);
	}
      }
      r.push_back(makevecteur(Pmin,a));
      return sto(b_,a,in_place,contextptr_);
    }
    // *logptr(contextptr) << "Sto " << "->" << b << endl;
    const context * contextptr=contextptr_;
    if (contextptr && contextptr->parent)
      contextptr=contextptr->parent;
    if (b.type==_SYMB){
#ifdef WITH_UNITS
      if (b.is_symb_of_sommet(at_unit))
	return _convert(gen(makevecteur(a,b),_SEQ__VECT),contextptr);
#endif
      if (b._SYMBptr->sommet==at_hash && b._SYMBptr->feuille.type==_STRNG)
	return sto(a,gen(*b._SYMBptr->feuille._STRNGptr,contextptr),in_place,contextptr);
      if (b._SYMBptr->sommet==at_double_deux_points){ 
	// variable of another named context?
	gen a1,bb,error;
	if (!check_binary(b._SYMBptr->feuille,a1,bb))
	  return a1;
        gen ret;
#ifndef RTOS_THREADX
#if !defined BESTA_OS && !defined NSPIRE_NEWLIB && !defined FXCG && !defined TICE
#ifdef HAVE_LIBPTHREAD
	pthread_mutex_lock(&context_list_mutex);
#endif
	if (a1.type==_INT_ && a1.subtype==0 && a1.val>=0 && a1.val<(signed)context_list().size()){
	  context * ptr =context_list()[a1.val];
#ifdef HAVE_LIBPTHREAD
	  pthread_mutex_unlock(&context_list_mutex);
#endif
	  return sto(a,bb,in_place,ptr);
	}
	if (context_names){
	  map<string,context *>::iterator it=context_names->find(a1.print()),itend=context_names->end();
	  if (it!=itend){
	    context * ptr = it->second;
#ifdef HAVE_LIBPTHREAD
	    pthread_mutex_unlock(&context_list_mutex);
#endif
	    return sto(a,bb,in_place,ptr);
	  }
	}
#ifdef HAVE_LIBPTHREAD
	pthread_mutex_unlock(&context_list_mutex);
#endif
#endif
#endif
	// TI path
	gen ab=a1.eval(eval_level(contextptr),contextptr);
	if (ab.type==_VECT){
	  vecteur v=*ab._VECTptr;
	  iterateur it=v.begin(),itend=v.end();
	  for (;it!=itend;++it){
	    if (it->type!=_VECT || it->_VECTptr->size()!=2)
	      continue;
	    vecteur & w=*it->_VECTptr;
	    if (w[0]==bb)
	      w[1]=a;
	  }
	  if (it==itend)
	    v.push_back(makevecteur(bb,a));
	  return sto(gen(v,_FOLDER__VECT),a1,in_place,contextptr);
	}
	if (a1.type==_IDNT)
	  return sto(gen(vecteur(1,makevecteur(bb,a)),_FOLDER__VECT),a1,in_place,contextptr);
      } // end TI path
    }
    if (b.type==_IDNT){
      // typed variable name must end with _d (double) or _i (int)
      const char * name=b._IDNTptr->id_name;
      int bl=int(strlen(name));
      if (bl==1){
	if (name[0]=='O' && (series_flags(contextptr) & (1<<6)) )
	  series_flags(contextptr) ^= (1<<6);
	if (name[0]==series_variable_name(contextptr)){
	  if (series_flags(contextptr) & (1<<5))
	    series_flags(contextptr) ^= (1<<5);
	  if (series_flags(contextptr) & (1<<6))
	    series_flags(contextptr) ^= (1<<6);
	}
      }
      if (bl>=3){
	if (name[bl-2]=='_'){
	  switch (name[bl-1]){
	  case 'd':
	    if (a.type!=_INT_ && a.type!=_DOUBLE_ && a.type!=_FRAC)
	      return gensizeerr(gettext("Unable to convert to float (in sto) ")+a.print(contextptr));
	    break;
	  case 'f':
	    if (a.type==_FRAC)
	      break;
	  case 'i': case 'l':
	    if (a.type==_DOUBLE_ && a._DOUBLE_val<=RAND_MAX && a._DOUBLE_val>=-RAND_MAX){
	      int i=int(a._DOUBLE_val);
	      if (i!=a._DOUBLE_val)
		*logptr(contextptr) << gettext("Converting ") << a._DOUBLE_val << gettext(" to integer ") << i << endl;
	      return sto(i,b,in_place,contextptr);
	    }
	    if (a.type!=_INT_){
	      if (a.type!=_ZINT || mpz_sizeinbase(*a._ZINTptr,2)>62)
		return gensizeerr(gettext("Unable to convert to integer (in sto) ")+a.print(contextptr));
	    }
	    break;
	  case 'v':
	    if (a.type!=_VECT)
	      return gensizeerr(gettext("Unable to convert to vector (in sto) ")+a.print(contextptr));
	    break;
	  case 's':
	    if (a.type!=_STRNG)
	      return sto(string2gen(a.print(contextptr),false),b,in_place,contextptr);
	    break;
	  }
	}
      }
      if (!contextptr){
	// Remove stale local assignements
#ifdef NO_STDEXCEPT
	b._IDNTptr->eval(1,b,contextptr); 
#else
	try {
	  b._IDNTptr->eval(1,b,contextptr); 
	} catch (std::runtime_error & ) { 
	  last_evaled_argptr(contextptr)=NULL;
	}
#endif
      }
      gen aa(a);
      if (strcmp(name,string_pi)==0 || strcmp(name,string_infinity)==0 || strcmp(name,string_undef)==0 
#ifdef GIAC_HAS_STO_38
	  || name[0]=='_' 
#endif
	  )
	return gensizeerr(b.print(contextptr)+": reserved word (in sto)");
      if (a.type==_IDNT && a==b)
	return purgenoassume(b,contextptr);
      gen ans(aa);
      if ( (a.type==_SYMB) && (a._SYMBptr->sommet==at_parameter)){
	gen inter=a._SYMBptr->feuille,debut,fin,saut;
	bool calc_aa=false;
	if (inter.type==_VECT){
	  vecteur & interv=*inter._VECTptr;
	  int inters=int(interv.size());
	  if (inters>=3){
	    debut=interv[0];
	    fin=interv[1];
	    if (is_strictly_greater(debut,fin,contextptr))
	      swapgen(debut,fin);
	    aa=interv[2];
	    if (is_strictly_greater(aa,fin,contextptr))
	      aa=fin;
	    if (is_strictly_greater(debut,aa,contextptr))
	      aa=debut;
	    if (inters>=4)
	      saut=interv[3];
	  }
	  if (inters==2){
	    aa=interv.back();
	    inter=interv.front();
	  }
	}
	else
	  calc_aa=true;
	if ( (inter.type==_SYMB) && (inter._SYMBptr->sommet==at_interval) ){
	  debut=inter._SYMBptr->feuille._VECTptr->front();
	  fin=inter._SYMBptr->feuille._VECTptr->back();
	}
	if (calc_aa)
	  aa=rdiv(debut+fin,plus_two,contextptr);
	if (is_zero(saut,contextptr))
	  saut=(fin-debut)/100.;
	ans=symbolic(at_parameter,makesequence(b,debut,fin,aa,saut));
      } // end parameter
      if (b._IDNTptr->quoted)
	*b._IDNTptr->quoted |= 2; // set dirty bit
      if (contextptr){
	const context * ptr=contextptr;
	bool done=false;
	for (;ptr->previous && ptr->tabptr;ptr=ptr->previous){
	  sym_tab::iterator it=ptr->tabptr->find(name),itend=ptr->tabptr->end();
	  if (it!=itend){ // found in current local context
	    // check that the current value is a thread pointer
	    it->second=aa;
	    done=true;
	    break;
	  }
	}
	if (!done) {// store b globally
	  if (contains(lidnt(a),b)){
	    if (a.is_symb_of_sommet(at_when) || a.is_symb_of_sommet(at_ifte) || a.is_program())
	      *logptr(contextptr) << b.print(contextptr)+gettext(": recursive definition") << endl;
	    else
	      return gensizeerr(b.print(contextptr)+gettext(": recursive definition (in sto) "));
	  }
	  sym_tab * symtabptr=contextptr->globalcontextptr?contextptr->globalcontextptr->tabptr:contextptr->tabptr;
	  sym_tab::iterator it=symtabptr->find(name),itend=symtabptr->end();
	  if (it!=itend){ 
	    // check that the current value is a thread pointer
	    it->second=aa;
	  }
	  else
	    (*symtabptr)[name]=aa;
	}
#ifdef HAVE_SIGNAL_H_OLD
	if (!child_id && signal_store)
	  _signal(symb_quote(symbolic(at_sto,makesequence(aa,b))),contextptr);
#endif
	return ans;
      } // end if (contextptr)
      if (contains(lidnt(a),b)){
	if (a.is_symb_of_sommet(at_when) || a.is_symb_of_sommet(at_ifte) || a.is_program())
	  *logptr(contextptr) << b.print(contextptr)+gettext(": recursive definition") << endl;
	else
	  return gensizeerr(b.print(contextptr)+gettext(": recursive definition (in sto) "));
      }
      if (b._IDNTptr->localvalue && !b._IDNTptr->localvalue->empty() && (b.subtype!=_GLOBAL__EVAL))
	b._IDNTptr->localvalue->back()=aa;
      else {
	if (0){
	}
	else {
	  if (b._IDNTptr->value)
	    delete b._IDNTptr->value;
	  if (b._IDNTptr->ref_count) 
	    b._IDNTptr->value = new gen(aa);
#ifdef HAVE_SIGNAL_H_OLD
	  if (!child_id && signal_store)
	    _signal(symb_quote(symbolic(at_sto,makesequence(aa,b))),contextptr);
#endif
#if !defined NSPIRE_NEWLIB && !defined FXCG && !defined TICE
	  if (!secure_run && variables_are_files(contextptr)){
	    ofstream a_out((b._IDNTptr->name()+string(cas_suffixe)).c_str());
	    a_out << aa << endl;
	  }
#endif
	}
      }
      return ans;
    } // end b.type==_IDNT
    if (b.type==_VECT){
      if (a.type!=_VECT)
	return gentypeerr(contextptr);
      return apply(a,b,contextptr,sto);
    }
    if ( (b.type==_SYMB) && (b._SYMBptr->sommet==at_at || b._SYMBptr->sommet==at_of) ){
      // Store a in a vector or array or map
      gen destination=b._SYMBptr->feuille._VECTptr->front(),error; // variable name
      if (destination.is_symb_of_sommet(at_at) && b._SYMBptr->sommet==at_at){
	destination=symbolic(at_at,makesequence(destination._SYMBptr->feuille[0],makesequence(destination._SYMBptr->feuille[1],b._SYMBptr->feuille._VECTptr->back())));
	return sto(a,destination,in_place,contextptr);
      }
      // if (sto_38 && destination.is_symb_of_sommet(at_double_deux_points) && destination._SYMBptr->feuille.type==_VECT && destination._SYMBptr->feuille._VECTptr->size()==2 &&destination._SYMBptr->feuille._VECTptr->front().type==_IDNT && destination._SYMBptr->feuille._VECTptr->back().type==_IDNT && sto_38(a,destination._SYMBptr->feuille._VECTptr->front()._IDNTptr->id_name,destination._SYMBptr->feuille._VECTptr->back()._IDNTptr->id_name,b,error,contextptr))	
      // return is_undef(error)?error:a;
      gen ret;
      if (storcl_38 && destination.type==_IDNT && storcl_38(ret,0,destination._IDNTptr->id_name,b,true,contextptr,&a,false))
        return ret;
      if (destination.type==_IDNT && destination._IDNTptr->quoted)
	*destination._IDNTptr->quoted |= 2; // set dirty bit
      gen valeur;
      if (!contextptr && in_place && destination.type==_IDNT && destination._IDNTptr->localvalue && !destination._IDNTptr->localvalue->empty() && local_eval(contextptr) )
	valeur=do_local_eval(*destination._IDNTptr,eval_level(contextptr),false);
      else
	valeur=destination.eval(in_place?1:eval_level(contextptr),contextptr);
      if ( valeur.type==_INT_ && valeur.val==0 && destination.type==_IDNT && destination._IDNTptr->localvalue && !destination._IDNTptr->localvalue->empty() )
	valeur=destination; // non (0) initialized local var
      gen indice=b._SYMBptr->feuille._VECTptr->back().eval(eval_level(contextptr),contextptr);
      if (indice.type==_VECT && indice.subtype==_SEQ__VECT && indice._VECTptr->size()==1)
	indice=indice._VECTptr->front();
      is_integral(indice);
      if (b._SYMBptr->sommet==at_of && valeur.type==_VECT && (1 || abs_calc_mode(contextptr)==38)){ // matrices and vector indices in HP38 compatibility mode
	if (indice.type==_INT_)
	  indice -= 1;
	if (indice.type==_VECT)
	  indice = indice - vecteur(indice._VECTptr->size(),1);
      }
      if ( (destination.type!=_IDNT && !destination.is_symb_of_sommet(at_double_deux_points)) || (valeur.type!=_VECT && valeur.type!=_MAP && valeur.type!=_IDNT && valeur.type!=_STRNG && valeur.type!=_SYMB) ){
	string endstring=" not allowed.";
	if (b.is_symb_of_sommet(at_at))
	  endstring += " Run purge if you want to create a sparse matrix in "+b[1].print(contextptr)+".";
	return gentypeerr(gettext("sto ")+b.print(contextptr)+ ":="+valeur.print(contextptr)+endstring);
      }
      if (valeur.type==_IDNT){ 
	// no previous vector at destination, 
	// create one in TI mode or create a map
	gen g;
	if (xcas_mode(contextptr)==3 && indice.type==_INT_ && indice.val>=0 ){
	  vecteur v(indice.val+1,zero);
	  v[indice.val]=a;
	  g=gen(v,destination.subtype);
	}
	else {
	  g=makemap();
	  if (!stomap(*g._MAPptr,indice,a))
	    return gendimerr(contextptr); // (*g._MAPptr)[indice]=a;
	}
	return sto(g,destination,in_place,contextptr);
      }
      if (valeur.type==_STRNG){
	bool indicedeuxpoints=indice.is_symb_of_sommet(*at_deuxpoints);
	if ( (indice.is_interval() || indicedeuxpoints)&& indice._SYMBptr->feuille.type==_VECT && indice._SYMBptr->feuille._VECTptr->size()==2){
	  gen deb=indice._SYMBptr->feuille._VECTptr->front();
	  gen fin=indice._SYMBptr->feuille._VECTptr->back()+(indicedeuxpoints?minus_one:zero);
	  if (!is_integral(deb) || !is_integral(fin) || deb.type!=_INT_ || fin.type!=_INT_ || a.type!=_STRNG)
	    return gendimerr();
	  int d=deb.val,f=fin.val;
	  if (!in_place)
	    valeur=string2gen(*valeur._STRNGptr,false);
	  string & vs=*valeur._STRNGptr;
	  string *as=a._STRNGptr;
	  if (d<0) d+=vs.size();
	  if (f<0) f+=vs.size();
	  if (d<0 || d>f || f>=vs.size() || f<0 || f-d>=as->size())
	    return gendimerr(contextptr);
	  for (int i=d;i<=f;++i){
	    vs[i]=(*as)[i-d];
	  }
	  if (in_place)
	    return string2gen("Done",false);
	  return sto(valeur,destination,in_place,contextptr);
	}
	if (indice.type!=_INT_ || a.type!=_STRNG || a._STRNGptr->empty())
	  return gensizeerr(contextptr);
	if (indice.val<0) indice+=(int) valeur._STRNGptr->size();
	if (indice.val<0 || indice.val>=(int) valeur._STRNGptr->size())
	  return gendimerr(contextptr);
	if (in_place){
	  (*valeur._STRNGptr)[indice.val]=(*a._STRNGptr)[0];
	  return string2gen("Done",false);
	}
	else {
	  string m(*valeur._STRNGptr);
	  m[indice.val]=(*a._STRNGptr)[0];
	  return sto(string2gen(m,false),destination,in_place,contextptr);
	}
      }
      if (valeur.type==_SYMB){
	if (indice.type==_VECT){
	  gen v(valeur);
	  vecteur empile;
	  iterateur it=indice._VECTptr->begin(),itend=indice._VECTptr->end();
	  for (;;){
	    if (it->type!=_INT_)
	      return gentypeerr(gettext("Bad index (in sto) ")+indice.print(contextptr));
	    empile.push_back(v);
	    v=v[*it];
	    ++it;
	    if (it==itend)
	      break;
	  }
	  --itend;
	  v=empile.back();
	  if (v.type==_VECT){
	    vecteur vv=*v._VECTptr;
	    if (itend->val>=0&&itend->val<int(vv.size())) // additional check
	      vv[itend->val]=a;
	    v=gen(vv,v.subtype);
	  }
	  else {
	    if (v.type==_SYMB){
	      if (itend->val==0){
		if (a.type==_FUNC)
		  v=symbolic(*a._FUNCptr,v._SYMBptr->feuille);
		else
		  v=symb_of(a,v._SYMBptr->feuille);//symbolic(at_of,makesequence(a,v._SYMBptr->feuille));
	      }
	      else {
		if (v._SYMBptr->feuille.type!=_VECT)
		  v=symbolic(v._SYMBptr->sommet,a);
		else {
		  vecteur vv=*v._SYMBptr->feuille._VECTptr;
		  if (itend->val>0&&itend->val<=int(vv.size())) // additional check
		    vv[itend->val-1]=a;
		  v=symbolic(v._SYMBptr->sommet,gen(vv,v._SYMBptr->feuille.subtype));
		}
	      }
	    }
	    else
	      v=a;
	  }
	  gen oldv;
	  it=indice._VECTptr->begin();
	  for (;;){
	    if (itend==it)
	      break;
	    --itend;
	    empile.pop_back();
	    oldv=empile.back();
	    if (oldv.type==_VECT){
	      vecteur vv=*oldv._VECTptr;
	      if (itend->val>=0&&itend->val<int(vv.size())) // additional check
		vv[itend->val]=v;
	      v=gen(vv,oldv.subtype);
	    }
	    else {
	      if (oldv.type==_SYMB){
		if (oldv._SYMBptr->feuille.type!=_VECT) // index should be 1
		  v=symbolic(oldv._SYMBptr->sommet,v);
		else {
		  vecteur vv=*oldv._SYMBptr->feuille._VECTptr;
		  if (itend->val>0 && itend->val<=int(vv.size())) // additional check
		    vv[itend->val-1]=v;
		  v=symbolic(oldv._SYMBptr->sommet,gen(vv,oldv._SYMBptr->feuille.subtype));
		}
	      }
	    } // end else oldv.type==_VECT
	  } // end for loop
	  return sto(v,destination,in_place,contextptr);
	}
	if (indice.type!=_INT_)
	  return gensizeerr(contextptr);
	if (indice.val<0 || indice.val>(int) gen2vecteur(valeur._SYMBptr->feuille).size())
	  return gendimerr(contextptr);
	gen nvaleur;
	if (indice.val==0){
	  if (a.type==_FUNC)
	    nvaleur=symbolic(*a._FUNCptr,valeur._SYMBptr->feuille);
	  else
	    nvaleur=symb_of(a,valeur._SYMBptr->feuille); // symbolic(at_of,makesequence(a,valeur._SYMBptr->feuille));
	}
	else {
	  nvaleur=valeur._SYMBptr->feuille;
	  if (indice==1 && nvaleur.type!=_VECT)
	    nvaleur=a;
	  else {
	    nvaleur=gen(*nvaleur._VECTptr,nvaleur.subtype);
	    (*nvaleur._VECTptr)[indice.val-1]=a;
	  }
	  nvaleur=symbolic(valeur._SYMBptr->sommet,nvaleur);
	}
	return sto(nvaleur,destination,in_place,contextptr);
      } // end valeur.type==_SYMB
      if (valeur.type==_MAP){
	if (valeur.subtype==1){ // array
	  gen_map::iterator it=valeur._MAPptr->find(indice),itend=valeur._MAPptr->end();
	  if (it==itend)
	    return gendimerr(gettext("Index outside of range (in sto) "));
	  if (xcas_mode(contextptr)==1)
	    in_place=true;
	}
	if (in_place){
	  if (!stomap(*valeur._MAPptr,indice,a))
	    return gendimerr(contextptr);// (*valeur._MAPptr)[indice]=a;
	  return string2gen("Done",false);
	}
	else {
	  gen_map m(*valeur._MAPptr);
	  if (!stomap(m,indice,a))
	    return gendimerr(contextptr);// m[indice]=a;
	  return sto(m,destination,in_place,contextptr);
	}
      } // valeur.type==_MAP
      vecteur * vptr=0;
      vecteur v;
      if (in_place)
	vptr=valeur._VECTptr;
      else {
	v=*valeur._VECTptr;
	vptr=&v;
      }
      bool indicedeuxpoints=indice.is_symb_of_sommet(*at_deuxpoints);
      if ( (indice.is_interval() || indicedeuxpoints)&& indice._SYMBptr->feuille.type==_VECT && indice._SYMBptr->feuille._VECTptr->size()==2){
	gen deb=indice._SYMBptr->feuille._VECTptr->front();
	gen fin=indice._SYMBptr->feuille._VECTptr->back()+(indicedeuxpoints?minus_one:zero);
	if (!is_integral(deb) || !is_integral(fin) || deb.type!=_INT_ || fin.type!=_INT_ || fin.val>=LIST_SIZE_LIMIT)
	  return gendimerr();
	if (deb.val<0) deb.val+=int(vptr->size());
	if (fin.val<0) fin.val+=int(vptr->size());
	if (deb.val<0 || fin.val<0 || deb.val>fin.val)
	  return gendimerr();
	if (a.type==_VECT && a._VECTptr->size()!=fin.val-deb.val+1)
	  return gendimerr(contextptr);
	int is=int(in_place?vptr->size():v.size());
	for (;is<=fin.val;++is){
	  vptr->push_back(zero);
	}
	if (a.type==_VECT){
	  for (int i=deb.val;i<=fin.val;++i)
	    (*vptr)[i]=(*a._VECTptr)[i-deb.val];	    
	}
	else {
	  for (int i=deb.val;i<=fin.val;++i)
	    (*vptr)[i]=a;
	}
	if (in_place)
	  return valeur; // string2gen("Done",false);
	return sto(gen(v,valeur.subtype),destination,in_place,contextptr);
      }
      if (indice.type!=_VECT){
	if (indice.type==_INT_ && indice.val<0)
	  indice += int(vptr->size());
	if (indice.type!=_INT_ || indice.val<0 || indice.val>=LIST_SIZE_LIMIT)
	  return gentypeerr(gettext("Bad index (in sto) ")+indice.print(contextptr));
	// check size
	int is=int(vptr->size());
	for (;is<=indice.val;++is){
	  vptr->push_back(zero);
	}
	// change indice's value
	(*vptr)[indice.val]=a;
	if (in_place)
	  return valeur; // string2gen("Done",false);
	return sto(gen(v,valeur.subtype),destination,in_place,contextptr);
      }
      // here indice is of type _VECT, we store inside a matrix
      vecteur empile;
      iterateur it=indice._VECTptr->begin(),itend=indice._VECTptr->end();
      if (itend-it==2){
	gen i2=*(it+1);
	bool itdeuxpoints=it->is_symb_of_sommet(*at_deuxpoints);
	if ( (it->is_interval() || itdeuxpoints ) && it->_SYMBptr->feuille.type==_VECT && it->_SYMBptr->feuille._VECTptr->size()==2){
	  gen deb=it->_SYMBptr->feuille._VECTptr->front();
	  gen fin=it->_SYMBptr->feuille._VECTptr->back()+(itdeuxpoints?minus_one:zero);
	  if (!is_integral(deb) || !is_integral(fin) || deb.type!=_INT_ || fin.type!=_INT_ )
	    return gendimerr(contextptr);
	  if (deb.val<0) deb.val+=int(vptr->size());
	  if (fin.val<0) fin.val+=int(vptr->size());
	  if (deb.val<0 || fin.val<0 || deb.val>fin.val || fin.val>LIST_SIZE_LIMIT)
	    return gendimerr(contextptr);
	  if (a.type==_VECT && a._VECTptr->size()!=fin.val-deb.val+1)
	    return gendimerr(contextptr);
	  if (!ckmatrix(*vptr))
	    return gendimerr(contextptr);
	  gen add=zero*vptr->front();
	  int is=int(vptr->size());
	  int cols=int(vptr->front()._VECTptr->size());
	  for (;is<=fin.val;++is){
	    vptr->push_back(add);
	  }
	  if (!in_place){
	    for (int i=deb.val;i<=fin.val;++i)
	      (*vptr)[i]=*(*vptr)[i]._VECTptr;
	  }
	  bool i2deuxpoints=i2.is_symb_of_sommet(*at_deuxpoints);
	  if ( (i2.is_interval() || i2deuxpoints) && i2._SYMBptr->feuille.type==_VECT && i2._SYMBptr->feuille._VECTptr->size()==2){
	    gen deb2=i2._SYMBptr->feuille._VECTptr->front();
	    gen fin2=i2._SYMBptr->feuille._VECTptr->back()+(i2deuxpoints?minus_one:zero);
	    if (!is_integral(deb2) || !is_integral(fin2) || deb2.type!=_INT_ || fin2.type!=_INT_) 
	      return gendimerr(contextptr);
	    if (deb2.val<0) deb2.val+=cols;
	    if (fin2.val<0) fin2.val+=cols;
	    if (deb2.val<0 || fin2.val<0 || fin2.val>=cols )
	      return gendimerr(contextptr);
	    if (ckmatrix(a)){
	      if (fin2.val-deb2.val+1!=a._VECTptr->front()._VECTptr->size())
		return gendimerr(contextptr);	      
	      for (int i=deb.val;i<=fin.val;++i){
		vecteur & target=*(*vptr)[i]._VECTptr;
		const vecteur & source=*(*a._VECTptr)[i-deb.val]._VECTptr;
		if (target.size()<=fin2.val)
		  target.resize(fin2.val+1);
		for (int j=deb2.val;j<=fin2.val;++j){
		  target[j]=source[j-deb2.val];
		}
	      }
	      if (in_place)
		return valeur; // string2gen("Done",false);
	      return sto(gen(v,valeur.subtype),destination,in_place,contextptr);
	    }
	    if (fin2.val-deb2.val!=fin.val-deb.val)
	      return gendimerr(contextptr);	      
	    int shift=deb2.val-deb.val;
	    if (a.type==_VECT){
	      for (int i=deb.val;i<=fin.val;++i)
		(*(*vptr)[i]._VECTptr)[i+shift]=(*a._VECTptr)[i-deb.val];	     
	    }
	    else {
	      for (int i=deb.val;i<=fin.val;++i)
		(*(*vptr)[i]._VECTptr)[i+shift]=a;
	    }
	  }
	  else {
	    if (i2.type==_INT_ && i2.val<0) i2.val += cols;
	    if (i2.type!=_INT_ || i2.val<0 || i2.val>=cols)
	      return gendimerr(contextptr);
	    if (a.type==_VECT){
	      for (int i=deb.val;i<=fin.val;++i)
		(*(*vptr)[i]._VECTptr)[i2.val]=(*a._VECTptr)[i-deb.val];
	    }
	    else {
	      for (int i=deb.val;i<=fin.val;++i)
		(*(*vptr)[i]._VECTptr)[i2.val]=a;
	    }
	  }
	  if (in_place)
	    return valeur; // string2gen("Done",false);
	  return sto(gen(v,valeur.subtype),destination,in_place,contextptr);
	} // end first value interval
	if (it->type==_INT_ && it->val<0) it->val += vptr->size();
	if (it->type!=_INT_ || it->val<0)
	  return gentypeerr(gettext("Bad index (in sto) ")+indice.print(contextptr));
	int i1=it->val;
	bool i2deuxpoints=i2.is_symb_of_sommet(*at_deuxpoints);
	if ( (i2.is_interval() || i2deuxpoints) && i2._SYMBptr->feuille.type==_VECT && i2._SYMBptr->feuille._VECTptr->size()==2){
	  if (!ckmatrix(*vptr))
	    return gendimerr(contextptr);
	  if (!in_place)
	    (*vptr)[i1]=*(*vptr)[i1]._VECTptr;
	  gen deb2=i2._SYMBptr->feuille._VECTptr->front();
	  gen fin2=i2._SYMBptr->feuille._VECTptr->back()+(i2deuxpoints?minus_one:zero);
	  if (!is_integral(deb2) || !is_integral(fin2) || deb2.type!=_INT_ || fin2.type!=_INT_ )
	    return gendimerr(contextptr);
	  if (deb2.val<0) deb2.val += int(vptr->front()._VECTptr->size());
	  if (fin2.val<0) fin2.val += int(vptr->front()._VECTptr->size());
	  if (deb2.val<0 || fin2.val <deb2.val || fin2.val>=int(vptr->front()._VECTptr->size()))
	    return gendimerr(contextptr);
	  if (a.type==_VECT){
	    for (int i=deb2.val;i<=fin2.val;++i)
	      (*(*vptr)[i1]._VECTptr)[i]=(*a._VECTptr)[i-deb2.val];	     
	  }
	  else {
	    for (int i=deb2.val;i<=fin2.val;++i)
	      (*(*vptr)[i1]._VECTptr)[i]=a;
	  }
	  if (in_place)
	    return valeur; // string2gen("Done",false);
	  return sto(gen(v,valeur.subtype),destination,in_place,contextptr);	  
	}
      } // end itend-it==2
      for (;;){
	if (it->type!=_INT_)
	  return gentypeerr(gettext("Bad index (in sto) ")+indice.print(contextptr));
	if (!in_place)
	  empile.push_back(v);
	gen tmp;
	if (in_place){
	  if (it->val<0) it->val += (int)(vptr->size());
	  if (it->val<0 || it->val>= (int)(vptr->size()) )
	    return gendimerr(contextptr);
	  tmp=(*vptr)[it->val];
	}
	else {
	  if (it->val<0) it->val += (int)(v.size());
	  if ( it->val<0 || it->val>= (int)(v.size()) )
	    return gendimerr(contextptr);
	  tmp=v[it->val];	  
	}
	++it;
	if (it==itend)
	  break;
	if (tmp.type!=_VECT)
	  return gentypeerr(gettext("Bad index (in sto) ")+indice.print(contextptr));
	if (in_place)
	  vptr= tmp._VECTptr;
	else
	  v=*tmp._VECTptr;
      }
      --itend;
      if (in_place){
	(*vptr)[itend->val]=a;
	return valeur; // string2gen("Done",false);
      }
      v[itend->val]=a;
      vecteur oldv;
      it=indice._VECTptr->begin();
      for (;;){
	if (itend==it)
	  break;
	--itend;
	empile.pop_back();
	oldv=*(empile.back()._VECTptr);
	oldv[itend->val]=v;
	v=oldv;
      }
      return sto(v,destination,in_place,contextptr);
    }
    if (b.type==_FUNC){
      if (b==at_of || b==at_index){ // shortcut for python_compat(0 or 1): of:=1 or 0
	if (a==0) {// index start 0 -> enable python compat
	  python_compat(1,contextptr);
	  return string2gen("[] index start 0",false);
	}
	if (a==1) { // index start 1 -> disable python compat
	  python_compat(0,contextptr);
	  return string2gen("[] index start 1",false);
	}
      }
      string errmsg=b.print(contextptr)+ gettext(" is a reserved word, sto not allowed:");
      //if (abs_calc_mode(contextptr)!=38)
	*logptr(contextptr) << errmsg << endl;
      return makevecteur(string2gen(errmsg,false),a);
    }
    if (a==b) return a;
    return gentypeerr(gettext("sto ")+b.print(contextptr)+ gettext(" not allowed!"));
  }
  gen symb_sto(const gen & a,gen & b,bool in_place){
    if (in_place)
      return symbolic(at_array_sto,makesequence(a,b));
    return symbolic(at_sto,makesequence(a,b));
  }
  gen symb_sto(const gen & e){
    return symbolic(at_sto,e);
  }
  gen _sto(const gen & a,const context * contextptr){
    if ( a.type==_STRNG && a.subtype==-1) return  a;
    if (a.type!=_VECT)
      return symb_sto(a);
    if (rpn_mode(contextptr)){
      if (a._VECTptr->size()<2)
	return gentoofewargs("STO");
      gen c=a._VECTptr->back();
      a._VECTptr->pop_back();
      gen b=a._VECTptr->back();
      a._VECTptr->pop_back();
      gen tmpsto=sto(b,c,contextptr);
      if (is_undef(tmpsto)) return tmpsto;
      return gen(*a._VECTptr,_RPN_STACK__VECT);
    }
    if (a._VECTptr->size()!=2)
      return gentypeerr(contextptr);
    return sto(a._VECTptr->front(),a._VECTptr->back(),contextptr);
  }
  static const char _sto_s []="sto";
  define_unary_function_eval4_index (30,__sto,&_sto,_sto_s,&printassto,&texprintsommetasoperator);
  define_unary_function_ptr5( at_sto ,alias_at_sto,&__sto,0,true); 
  // NB argument quoting for sto is done in eval in symbolic.cc

  gen _array_sto(const gen & a,const context * contextptr){
    if ( a.type==_STRNG && a.subtype==-1) return  a;
    if (a.type!=_VECT ||a._VECTptr->size()!=2)
      return gentypeerr(contextptr);
    gen value=a._VECTptr->front().eval(eval_level(contextptr),contextptr);
    return sto(value,a._VECTptr->back(),true,contextptr);
  }
  static const char _array_sto_s []="array_sto";
  static define_unary_function_eval_quoted (__array_sto,&_array_sto,_array_sto_s);
  define_unary_function_ptr5( at_array_sto ,alias_at_array_sto,&__array_sto,_QUOTE_ARGUMENTS,true);

  static string printasincdec(const gen & feuille,char ch,bool tex,GIAC_CONTEXT){
    if (feuille.type!=_VECT){
      string s(tex?gen2tex(feuille,contextptr):feuille.print(contextptr));
      return xcas_mode(contextptr)?((s+string(":=")+s+ch)+'1'):((s+ch)+ch);
    }
    vecteur & v = *feuille._VECTptr;
    if (v.size()!=2)
      return "printasincdec: bad dimension";
    gen & a=v.front();
    gen & b=v.back();
    string sa((tex?gen2tex(a,contextptr):a.print(contextptr)));
    string sb((tex?gen2tex(b,contextptr):b.print(contextptr)));
    return xcas_mode(contextptr)?sa+":="+sa+ch+sb:(sa+ch+'='+sb);
  }

  static string printasincrement(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    return printasincdec(feuille,'+',false,contextptr);
  }

  static string printasdecrement(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    return printasincdec(feuille,'-',false,contextptr);
  }

  static gen in_increment3(const gen & prev,const gen & val,const gen & var,int mult,GIAC_CONTEXT){
    if (mult==0)
      return sto(prev+val,var,true,contextptr);
    if (mult==1)
      return sto(prev*val,var,true,contextptr);
    if (mult==2)
      return sto(_iquo(makesequence(prev,val),contextptr),var,true,contextptr);
    if (mult==3)
      return sto(_irem(makesequence(prev,val),contextptr),var,true,contextptr);
    if (mult==4)
      return sto(_bitand(makesequence(prev,val),contextptr),var,true,contextptr);
    if (mult==5)
      return sto(_bitor(makesequence(prev,val),contextptr),var,true,contextptr);
    if (mult==6){
      if (python_compat(contextptr))
	return sto(_bitxor(makesequence(prev,val),contextptr),var,true,contextptr);
      return sto(_pow(makesequence(prev,val),contextptr),var,true,contextptr);
    }
#ifndef XLIGHT
    if (mult==7)
      return sto(_shift(makesequence(prev,val),contextptr),var,true,contextptr);
    if (mult==8)
      return sto(_rotate(makesequence(prev,val),contextptr),var,true,contextptr);
#endif
    if (mult==9)
      return sto(_pow(makesequence(prev,val),contextptr),var,true,contextptr);
    return gensizeerr(gettext("Increment"));
  }

  static gen in_increment2(gen & prev,const gen & val,int mult,GIAC_CONTEXT){
    if (mult==0)
      return prev=prev+val;
    if (mult==1)
      return prev=prev*val;
    if (mult==2)
      return prev=_iquo(makesequence(prev,val),contextptr);
    if (mult==3)
      return prev=_irem(makesequence(prev,val),contextptr);
    if (mult==4)
      return prev=_bitand(makesequence(prev,val),contextptr);
    if (mult==5)
      return prev=_bitor(makesequence(prev,val),contextptr);
    if (mult==6){
      if (python_compat(contextptr))
	return prev=_bitxor(makesequence(prev,val),contextptr);
      return prev=_pow(makesequence(prev,val),contextptr);
    }
#ifndef XLIGHT
    if (mult==7)
      return prev=_shift(makesequence(prev,val),contextptr);
    if (mult==8)
      return prev=_rotate(makesequence(prev,val),contextptr);
#endif
    if (mult==9)
      return prev=_pow(makesequence(prev,val),contextptr);
    return gensizeerr(gettext("Increment"));
  }

  // mult==0 for +/-, mult=1 for * and /, mult==2 for iquo (negatif=false), mult==3 for irem (negatif=false), mult==4 for &=, 5 for |=, 6 for ^=, 7 for !=
  static gen increment(const gen & var,const gen & val_orig,bool negatif,int mult,GIAC_CONTEXT){
    gen val=val_orig.eval(1,contextptr);
    if (negatif)
      val=mult==1?inv(val,contextptr):-val;
    if (var.type!=_IDNT){
      gen prev=eval(var,1,contextptr);
      return in_increment3(prev,val,var,mult,contextptr);
    }
    if (contextptr){
      sym_tab::iterator it,itend;
      const context * cptr=contextptr;
      for(;cptr;) {
	it=cptr->tabptr->find(var._IDNTptr->id_name);
	itend=cptr->tabptr->end();
	if (it!=itend)
	  break;
	if (!cptr->previous){
	  it=cptr->globalcontextptr->tabptr->find(var._IDNTptr->id_name);
	  if (it!=itend)
	    break;
	}
	cptr=cptr->previous;
      }
      if (!cptr){
	gen prev=eval(var,1,contextptr);
	return in_increment3(prev,val,var,mult,contextptr);
      }
      return in_increment2(it->second,val,mult,contextptr);
    }
    if (!var._IDNTptr->localvalue)
      var._IDNTptr->localvalue = new vecteur;
    vecteur * w=var._IDNTptr->localvalue;
    if (!w->empty() && var.subtype!=_GLOBAL__EVAL)
      return w->back()=w->back()+val;
    if (!var._IDNTptr->value)
      return gensizeerr(gettext("Non assigned variable"));
    return in_increment2(*var._IDNTptr->value,val,mult,contextptr);
  }
  gen _increment(const gen & a,const context * contextptr){
    if ( a.type==_STRNG && a.subtype==-1) return  a;
    if (a.type!=_VECT)
      return increment(a,1,false,false,contextptr);
    if (a._VECTptr->size()!=2)
      return gentypeerr(contextptr);
    return increment(a._VECTptr->front(),a._VECTptr->back(),false,0,contextptr);
  }
  static const char _increment_s []="increment";
  static define_unary_function_eval4_index (151,__increment,&_increment,_increment_s,&printasincrement,&texprintsommetasoperator);
  define_unary_function_ptr5( at_increment ,alias_at_increment,&__increment,_QUOTE_ARGUMENTS,true); 

  gen _decrement(const gen & a,const context * contextptr){
    if ( a.type==_STRNG && a.subtype==-1) return  a;
    if (a.type!=_VECT)
      return increment(a,1,true,false,contextptr);
    if (a._VECTptr->size()!=2)
      return gentypeerr(contextptr);
    return increment(a._VECTptr->front(),a._VECTptr->back(),true,0,contextptr);
  }
  static const char _decrement_s []="decrement";
  static define_unary_function_eval4_index (153,__decrement,&_decrement,_decrement_s,&printasdecrement,&texprintsommetasoperator);
  define_unary_function_ptr5( at_decrement ,alias_at_decrement,&__decrement,_QUOTE_ARGUMENTS,true); 

  static string printasmultcrement(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    return printasincdec(feuille,'*',false,contextptr);
  }

  static string printasdivcrement(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    return printasincdec(feuille,'/',false,contextptr);
  }
  gen _multcrement(const gen & a,const context * contextptr){
    if ( a.type==_STRNG && a.subtype==-1) return  a;
    if (a.type!=_VECT)
      return increment(a,1,false,true,contextptr);
    if (a.type!=_VECT || a._VECTptr->size()!=2)
      return gentypeerr(contextptr);
    return increment(a._VECTptr->front(),a._VECTptr->back(),false,1,contextptr);
  }
  static const char _multcrement_s []="multcrement";
  static define_unary_function_eval4_index (155,__multcrement,&_multcrement,_multcrement_s,&printasmultcrement,&texprintsommetasoperator);
  define_unary_function_ptr5( at_multcrement ,alias_at_multcrement,&__multcrement,_QUOTE_ARGUMENTS,true); 

  gen _divcrement(const gen & a,const context * contextptr){
    if ( a.type==_STRNG && a.subtype==-1) return  a;
    if (a.type!=_VECT)
      return increment(a,1,true,true,contextptr);
    if (a._VECTptr->size()!=2)
      return gentypeerr(contextptr);
    return increment(a._VECTptr->front(),a._VECTptr->back(),true,1,contextptr);
  }
  static const char _divcrement_s []="divcrement";
  static define_unary_function_eval4_index (157,__divcrement,&_divcrement,_divcrement_s,&printasdivcrement,&texprintsommetasoperator);
  define_unary_function_ptr5( at_divcrement ,alias_at_divcrement,&__divcrement,_QUOTE_ARGUMENTS,true); 

  gen _iquosto(const gen & a,const context * contextptr){
    if ( a.type==_STRNG && a.subtype==-1) return  a;
    if (a.type!=_VECT)
      return increment(a,1,true,true,contextptr);
    if (a._VECTptr->size()!=2)
      return gentypeerr(contextptr);
    return increment(a._VECTptr->front(),a._VECTptr->back(),false,2,contextptr);
  }
  static const char _iquosto_s []="iquosto";
  static define_unary_function_eval (__iquosto,&_iquosto,_iquosto_s);
  define_unary_function_ptr5( at_iquosto ,alias_at_iquosto,&__iquosto,_QUOTE_ARGUMENTS,true);

  gen _iremsto(const gen & a,const context * contextptr){
    if ( a.type==_STRNG && a.subtype==-1) return  a;
    if (a.type!=_VECT)
      return increment(a,1,true,true,contextptr);
    if (a._VECTptr->size()!=2)
      return gentypeerr(contextptr);
    return increment(a._VECTptr->front(),a._VECTptr->back(),false,3,contextptr);
  }
  static const char _iremsto_s []="iremsto";
  static define_unary_function_eval (__iremsto,&_iremsto,_iremsto_s);
  define_unary_function_ptr5( at_iremsto ,alias_at_iremsto,&__iremsto,_QUOTE_ARGUMENTS,true);

  gen _andsto(const gen & a,const context * contextptr){
    if ( a.type==_STRNG && a.subtype==-1) return  a;
    if (a.type!=_VECT)
      return increment(a,1,true,true,contextptr);
    if (a._VECTptr->size()!=2)
      return gentypeerr(contextptr);
    return increment(a._VECTptr->front(),a._VECTptr->back(),false,4,contextptr);
  }
  static const char _andsto_s []="andsto";
  static define_unary_function_eval (__andsto,&_andsto,_andsto_s);
  define_unary_function_ptr5( at_andsto ,alias_at_andsto,&__andsto,_QUOTE_ARGUMENTS,true);

  gen _orsto(const gen & a,const context * contextptr){
    if ( a.type==_STRNG && a.subtype==-1) return  a;
    if (a.type!=_VECT)
      return increment(a,1,true,true,contextptr);
    if (a._VECTptr->size()!=2)
      return gentypeerr(contextptr);
    return increment(a._VECTptr->front(),a._VECTptr->back(),false,5,contextptr);
  }
  static const char _orsto_s []="orsto";
  static define_unary_function_eval (__orsto,&_orsto,_orsto_s);
  define_unary_function_ptr5( at_orsto ,alias_at_orsto,&__orsto,_QUOTE_ARGUMENTS,true);

  gen _xorsto(const gen & a,const context * contextptr){
    if ( a.type==_STRNG && a.subtype==-1) return  a;
    if (a.type!=_VECT)
      return increment(a,1,true,true,contextptr);
    if (a._VECTptr->size()!=2)
      return gentypeerr(contextptr);
    return increment(a._VECTptr->front(),a._VECTptr->back(),false,6,contextptr);
  }
  static const char _xorsto_s []="xorsto";
  static define_unary_function_eval (__xorsto,&_xorsto,_xorsto_s);
  define_unary_function_ptr5( at_xorsto ,alias_at_xorsto,&__xorsto,_QUOTE_ARGUMENTS,true);

  gen _shiftsto(const gen & a,const context * contextptr){
    if ( a.type==_STRNG && a.subtype==-1) return  a;
    if (a.type!=_VECT)
      return increment(a,1,true,true,contextptr);
    if (a._VECTptr->size()!=2)
      return gentypeerr(contextptr);
    return increment(a._VECTptr->front(),a._VECTptr->back(),false,7,contextptr);
  }
  static const char _shiftsto_s []="shiftsto";
  static define_unary_function_eval (__shiftsto,&_shiftsto,_shiftsto_s);
  define_unary_function_ptr5( at_shiftsto ,alias_at_shiftsto,&__shiftsto,_QUOTE_ARGUMENTS,true);

  gen _rotatesto(const gen & a,const context * contextptr){
    if ( a.type==_STRNG && a.subtype==-1) return  a;
    if (a.type!=_VECT)
      return increment(a,1,true,true,contextptr);
    if (a._VECTptr->size()!=2)
      return gentypeerr(contextptr);
    return increment(a._VECTptr->front(),a._VECTptr->back(),false,8,contextptr);
  }
  static const char _rotatesto_s []="rotatesto";
  static define_unary_function_eval (__rotatesto,&_rotatesto,_rotatesto_s);
  define_unary_function_ptr5( at_rotatesto ,alias_at_rotatesto,&__rotatesto,_QUOTE_ARGUMENTS,true);

  gen _powsto(const gen & a,const context * contextptr){
    if ( a.type==_STRNG && a.subtype==-1) return  a;
    if (a.type!=_VECT)
      return increment(a,1,true,true,contextptr);
    if (a._VECTptr->size()!=2)
      return gentypeerr(contextptr);
    return increment(a._VECTptr->front(),a._VECTptr->back(),false,9,contextptr);
  }
  static const char _powsto_s []="powsto";
  static define_unary_function_eval (__powsto,&_powsto,_powsto_s);
  define_unary_function_ptr5( at_powsto ,alias_at_powsto,&__powsto,_QUOTE_ARGUMENTS,true);

  bool is_assumed_real(const gen & g,GIAC_CONTEXT){
    if (g.type!=_IDNT)
      return false;
    if (g==cst_euler_gamma || g==cst_pi)
      return true;
    gen tmp=g._IDNTptr->eval(1,g,contextptr);
    if (g.subtype==_GLOBAL__EVAL && contextptr){
      sym_tab::const_iterator it=contextptr->globalcontextptr->tabptr->find(g._IDNTptr->id_name),itend=contextptr->globalcontextptr->tabptr->end();
      if (it!=itend)
	tmp=it->second;
    }
    if (tmp.type==_VECT && tmp.subtype==_ASSUME__VECT){
      vecteur & v = *tmp._VECTptr;
      if (!v.empty()){
	if ((v.front()==_INT_ || v.front()==_ZINT || v.front()==_DOUBLE_) )
	  return true;
	if (v.front()==_CPLX)
	  return false;
      }
    }
    return !complex_variables(contextptr);
  }

  bool is_assumed_integer(const gen & g,GIAC_CONTEXT){
    if (is_integer(g))
      return true;
    if (g.type==_IDNT) {// FIXME GIAC_CONTEXT
      gen tmp=g._IDNTptr->eval(1,g,contextptr);
      if (tmp.type==_VECT && tmp.subtype==_ASSUME__VECT){
	vecteur & v = *tmp._VECTptr;
	if (!v.empty() && (v.front()==_INT_ || v.front()==_ZINT) )
	  return true;
      }
      return is_integer(tmp);
    }
    if (g.type!=_SYMB)
      return false;
    unary_function_ptr & u=g._SYMBptr->sommet;
    gen & f=g._SYMBptr->feuille;
    if ( (u==at_neg) || (u==at_abs) )
      return is_assumed_integer(f,contextptr);
    if ( (u==at_plus) || (u==at_prod) ){
      if (f.type!=_VECT)
	return is_assumed_integer(f,contextptr);
      const_iterateur it=f._VECTptr->begin(),itend=f._VECTptr->end();
      for (;it!=itend;++it)
	if (!is_assumed_integer(*it,contextptr))
	  return false;
      return true;
    }
    return false;
  }
  // v = previous assumptions, a=the real value, direction
  // is positive for [a,+inf[, negative for ]-inf,a]
  // |direction| = 1 (large) or 2 (strict) 
  gen doubleassume_and(const vecteur & v,const gen & a,int direction,bool or_assumption,GIAC_CONTEXT){
    vecteur v_intervalle,v_excluded;
    if ( (v.size()>=3) && (v[1].type==_VECT) && (v[2].type==_VECT) ){
      v_intervalle=*v[1]._VECTptr;
      v_excluded=*v.back()._VECTptr;
    }
    gen v0=_DOUBLE_;
    v0.subtype=1;
    if (!v.empty())
      v0=v.front();
    if (!(direction %2) && !equalposcomp(v_excluded,a))
      v_excluded.push_back(a);
    if (or_assumption){ 
      // remove excluded values if they are in the interval we add
      vecteur old_v(v_excluded);
      v_excluded.clear();
      const_iterateur it=old_v.begin(),itend=old_v.end();
      for (;it!=itend;++it){
	if (direction%2==0 && a==*it){
	  v_excluded.push_back(*it);
	  continue;
	}
	bool a_greater_sup=ck_is_greater(a,*it,contextptr);
	if (a_greater_sup && (direction<0) )
	  continue;
	if (!a_greater_sup && (direction>0) )
	  continue;
	v_excluded.push_back(*it);
      }
    }
    if (v_intervalle.empty() || or_assumption){
      if (direction>0)
	v_intervalle.push_back(makeline(a,plus_inf)); //gen(makevecteur(a,plus_inf),_LINE__VECT));
      else
	v_intervalle.push_back(makeline(minus_inf,a)); // gen(makevecteur(minus_inf,a),_LINE__VECT));
      if (or_assumption)
	return gen(makevecteur(v0,v_intervalle,v_excluded),_ASSUME__VECT);
    }
    else { // intersection of [a.+inf[ with every interval from v_intervalle
      vecteur old_v(v_intervalle);
      v_intervalle.clear();
      const_iterateur it=old_v.begin(),itend=old_v.end();
      for (;it!=itend;++it){
	if ( (it->type!=_VECT) || (it->subtype!=_LINE__VECT) || (it->_VECTptr->size()!= 2) )
	  return gensizeerr(contextptr);
	gen i_inf(it->_VECTptr->front()),i_sup(it->_VECTptr->back());
	bool a_greater_sup=ck_is_greater(a,i_sup,contextptr);
	if (a_greater_sup){
	  if (direction<0)
	    v_intervalle.push_back(*it);
	  continue;
	}
	bool a_greater_inf=ck_is_greater(a,i_inf,contextptr);
	if (!a_greater_inf){
	  if (direction>0)
	    v_intervalle.push_back(*it);
	  continue;
	}
	if (direction>0)
	  v_intervalle.push_back(makeline(a,i_sup));//gen(makevecteur(a,i_sup),_LINE__VECT));
	else
	  v_intervalle.push_back(makeline(i_inf,a)); //gen(makevecteur(i_inf,a),_LINE__VECT));
      }
    }
    return gen(makevecteur(v0,v_intervalle,v_excluded),_ASSUME__VECT);
  }
  // returns the assumed idnt name
  // used if assumptions are in OR conjonction
  gen assumesymbolic(const gen & a,gen idnt_must_be,GIAC_CONTEXT){
    if (a.type==_IDNT)
      return a._IDNTptr->eval(eval_level(contextptr),a,contextptr);
    if ( (a.type!=_SYMB) || (a._SYMBptr->feuille.type!=_VECT) )
      return gensizeerr(contextptr);
    while (idnt_must_be.type==_SYMB){
      idnt_must_be=idnt_must_be._SYMBptr->feuille;
      if ( (idnt_must_be.type==_VECT) && !(idnt_must_be._VECTptr->empty()) )
	idnt_must_be=idnt_must_be._VECTptr->front();
    }
    unary_function_ptr s(a._SYMBptr->sommet);
    vecteur v(*a._SYMBptr->feuille._VECTptr);
    int l=int(v.size());
    if (!l)
      return gensizeerr(contextptr);
    gen arg0(v.front()),arg1(v.back()),hyp(undef);
    if (s==at_sto){
      gen tmp(arg0);
      arg0=arg1;
      arg1=tmp;
    }
    if (s==at_and ){
      gen tmpg=assumesymbolic(arg0,0,contextptr);
      if (is_undef(tmpg)) return tmpg;
      return assumesymbolic(arg1,0,contextptr);
    }
    if (s==at_ou ){
      gen a0(assumesymbolic(arg0,0,contextptr));
      if (is_undef(a0)) return a0;
      return assumesymbolic(arg1,a0,contextptr);
    }
    if (arg0.type!=_IDNT)
      arg0=arg0.eval(eval_level(contextptr),contextptr);
    if ((arg0.type!=_IDNT || arg0==cst_pi) && arg1.type==_IDNT){
      gen swapped=makesequence(arg1,arg0);
      if (s==at_superieur_strict || s==at_superieur_egal || s==at_inferieur_strict || s==at_inferieur_egal ) return assumesymbolic(symbolic(s,swapped),idnt_must_be,contextptr);
    }
    if ( (arg0.type!=_IDNT) || (!is_zero(idnt_must_be,contextptr) && (arg0!=idnt_must_be) ) )
      return gensizeerr(contextptr);
    bool or_assumption= !is_zero(idnt_must_be,contextptr) && (arg0==idnt_must_be);
    vecteur last_hyp;
    arg1=arg0._IDNTptr->eval(eval_level(contextptr),arg0,contextptr);
    if ( (arg1.type!=_VECT) || (arg1.subtype!=_ASSUME__VECT) )
      last_hyp=makevecteur(vecteur(0),vecteur(0));
    else
      last_hyp=*arg1._VECTptr;
    if (l==2){
      if (s==at_sto)
	arg1=v[0].eval(eval_level(contextptr),contextptr);
      else
	arg1=v[1].eval(eval_level(contextptr),contextptr);
      gen borne_inf(gnuplot_xmin),borne_sup(gnuplot_xmax),pas;
      if ( s==at_equal || s== at_equal2 || s==at_same || s==at_sto ){     
	// ex: assume(a=[1.7,1.1,2.3])
	if (arg1.type==_VECT && arg1._VECTptr->size()>=3){
	  vecteur vtmp=*arg1._VECTptr;
	  borne_inf=evalf_double(vtmp[1],eval_level(contextptr),contextptr);
	  borne_sup=evalf_double(vtmp[2],eval_level(contextptr),contextptr);
	  pas=(borne_sup-borne_inf)/100;
	  if (vtmp.size()>3)
	    pas=evalf_double(vtmp[3],eval_level(contextptr),contextptr);
	  arg1=evalf_double(vtmp[0],eval_level(contextptr),contextptr);
	}
	gen tmp=arg1.type;
	tmp.subtype=1;
	hyp=gen(makevecteur(tmp,arg1),_ASSUME__VECT);
      }
      if (s==at_inferieur_strict) // ex: assume(a<1.7)
	hyp=doubleassume_and(last_hyp,arg1,-2,or_assumption,contextptr);
      if (s==at_inferieur_egal) 
	hyp=doubleassume_and(last_hyp,arg1,-1,or_assumption,contextptr);
      if (s==at_superieur_strict)
	hyp=doubleassume_and(last_hyp,arg1,2,or_assumption,contextptr);
      if (s==at_superieur_egal) 
	hyp=doubleassume_and(last_hyp,arg1,1,or_assumption,contextptr);
      if (!is_undef(hyp)){
	gen tmpsto=sto(hyp,arg0,contextptr); 
	if (is_undef(tmpsto)) return tmpsto;
	if ( s==at_equal || s==at_equal2 || s==at_same || s==at_sto )
	  return _parameter(makevecteur(arg0,borne_inf,borne_sup,arg1,pas),contextptr);
	return arg0;
      }
    }
    return gensizeerr(contextptr);
  }
  static void purge_assume(const gen & a,GIAC_CONTEXT){
    if (a.type==_SYMB && (a._SYMBptr->sommet==at_and || a._SYMBptr->sommet==at_ou  || a._SYMBptr->sommet==at_inferieur_strict || a._SYMBptr->sommet==at_inferieur_egal || a._SYMBptr->sommet==at_superieur_egal || a._SYMBptr->sommet==at_superieur_strict || a._SYMBptr->sommet==at_equal) ){
      purge_assume(a._SYMBptr->feuille,contextptr);
      return;
    }
    if (a.type==_VECT && !a._VECTptr->empty()){
      if (a._VECTptr->back().type==_IDNT && a._VECTptr->front().type!=_IDNT)
	purge_assume(a._VECTptr->back(),contextptr);
      else
	purge_assume(a._VECTptr->front(),contextptr);
    }
    else
      purgenoassume(a,contextptr);
  }
  gen giac_assume(const gen & a,GIAC_CONTEXT){
    if ( (a.type==_VECT) && (a._VECTptr->size()==2) ){
      gen a1(a._VECTptr->front()),a2(a._VECTptr->back());
      if (a2.type==_INT_){
	// assume(a,real) for example
	a2.subtype=1;
	gen tmpsto=sto(gen(makevecteur(a2),_ASSUME__VECT),a1,contextptr);
	if (is_undef(tmpsto)) return tmpsto;
	return a2;
      }
      if (a2==at_real || a2==at_float){
	a2=_DOUBLE_;
	a2.subtype=1;
	gen tmpsto=sto(gen(makevecteur(a2),_ASSUME__VECT),a1,contextptr);
	if (is_undef(tmpsto)) return tmpsto;
	return a2;
      }
      if (a2==at_complex){
	a2=_CPLX;
	a2.subtype=1;
	gen tmpsto=sto(gen(makevecteur(a2),_ASSUME__VECT),a1,contextptr);
	if (is_undef(tmpsto)) return tmpsto;
	return a2;
      }
      if ( (a2.type==_FUNC) && (*a2._FUNCptr==at_ou) ){
	purge_assume(a1,contextptr);
	return assumesymbolic(a1,a1,contextptr);
      }
      if (a2==at_additionally)
	return giac_additionally(a1,contextptr);
    }
    gen a_;
    if (a.type==_SYMB){
      if (a._SYMBptr->sommet==at_and || a._SYMBptr->sommet==at_ou || a._SYMBptr->sommet==at_inferieur_strict || a._SYMBptr->sommet==at_inferieur_egal || a._SYMBptr->sommet==at_superieur_egal || a._SYMBptr->sommet==at_superieur_strict || a._SYMBptr->sommet==at_equal) 
	a_=a;
      else
	a_=eval(a,1,contextptr);
    }
    purge_assume(a_,contextptr);
    return assumesymbolic(a_,0,contextptr);
  }
  static const char giac_assume_s []="assume";
  static define_unary_function_eval_quoted (giac__assume,&giac_assume,giac_assume_s);
  define_unary_function_ptr5( at_assume ,alias_at_assume,&giac__assume,_QUOTE_ARGUMENTS,true);

  gen giac_additionally(const gen & a,GIAC_CONTEXT){
    if ( (a.type==_VECT) && (a._VECTptr->size()==2) ){
      gen a1(a._VECTptr->front()),a2(a._VECTptr->back());
      if (a1.type!=_IDNT)
	return gensizeerr(contextptr);
      gen a1val=a1._IDNTptr->eval(1,a1,contextptr);
      if (a1val.type==_VECT && a1val.subtype==_ASSUME__VECT && !a1val._VECTptr->empty()){
	if (a2.type==_INT_){
	  // assume(a,real) for example
	  a2.subtype=1;
	  a1val._VECTptr->front()=a2;
	  return a2;
	}
	if (a2==at_real){
	  a2=_DOUBLE_;
	  a2.subtype=1;
	  a1val._VECTptr->front()=a2;
	  return a2;
	}
      }
      else {
	gen tmp=giac_assume(a,contextptr);
	if (is_undef(tmp)) return tmp;
      }
    }    
    return assumesymbolic(a,0,contextptr);
  }
  static const char giac_additionally_s []="additionally";
  static define_unary_function_eval_quoted (giac__additionally,&giac_additionally,giac_additionally_s);
  define_unary_function_ptr5( at_additionally ,alias_at_additionally,&giac__additionally,_QUOTE_ARGUMENTS,true);

  symbolic do_symb_plus(const gen & a,const gen &b){
    return symbolic(at_plus,makesequence(a,b));
  }
  // multiargs
  gen symb_plus(const gen & a){
    return symbolic(at_plus,a);
  }
  gen symb_plus(const vecteur & a){
    return symbolic(at_plus,gen(a,_SEQ__VECT));
  }
  gen symb_plus(const gen & a,const gen & b){
    if (a.is_plus() && !is_inf(a._SYMBptr->feuille)){
      if (b.is_plus() && !is_inf(b._SYMBptr->feuille))
	return symb_plus(mergevecteur(*(a._SYMBptr->feuille._VECTptr),*(b._SYMBptr->feuille._VECTptr)));
	else
	  return symbolic(*a._SYMBptr,b);
    }
    return do_symb_plus(a,b);
  }

  inline bool plus_idnt_symb(const gen & a){
    return (a.type==_IDNT && strcmp(a._IDNTptr->id_name,"undef") && strcmp(a._IDNTptr->id_name,"infinity")) || (a.type==_SYMB && !is_inf(a) && (a._SYMBptr->sommet==at_prod || a._SYMBptr->sommet==at_pow || a._SYMBptr->sommet==at_neg));
  }
  
  inline bool idnt_symb_int(const gen & b){
    return (b.type==_INT_ && b.val!=0) || b.type==_ZINT || (b.type==_SYMB && !is_inf(b) && b._SYMBptr->sommet!=at_unit && b._SYMBptr->sommet!=at_equal && b._SYMBptr->sommet!=at_equal2 && !equalposcomp(plot_sommets,b._SYMBptr->sommet) && !equalposcomp(inequality_tab,b._SYMBptr->sommet) ) || (b.type==_IDNT && strcmp(b._IDNTptr->id_name,"undef") && strcmp(b._IDNTptr->id_name,"infinity"));
  }

  gen _plus(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT){
      if ((args.type==_IDNT) && !strcmp(args._IDNTptr->id_name,string_infinity))
	return plus_inf;
      return args;
    }
    iterateur it=args._VECTptr->begin(), itend=args._VECTptr->end();
    if (itend-it==2){
      int t1=it->type,t2=(it+1)->type;
      if (t1<_IDNT && t2<_IDNT){
	unsigned t=(t1<< _DECALAGE) | t2;
	if (!t)
	  return((longlong) it->val+(it+1)->val);
	return operator_plus(*it,*(it+1),t,contextptr);
      }
    }
    if (it==itend)
      return zero;
    const gen & a=*it;
    ++it;
    if (itend==it)
      return a;
    const gen & b=*it;
    ++it;
    if (it==itend){
      // improve: if a is an idnt/symb and b also do not rebuild the vector
      if (idnt_symb_int(b) && plus_idnt_symb(a)){
	if (b.is_neg() && a==b._SYMBptr->feuille)
	  return chkmod(zero,a);
	if (a.is_neg() && b==a._SYMBptr->feuille)
	  return chkmod(zero,b);
	if (!b.is_program() 
	    && !b.is_plus()
	    )
	  return (symb_plus(args));
      }
      if (idnt_symb_int(a) && plus_idnt_symb(b)){
	if (b.is_neg() && a==b._SYMBptr->feuille)
	  return chkmod(zero,a);
	if (a.is_neg() && b==a._SYMBptr->feuille)
	  return chkmod(zero,b);
	if (!a.is_program() 
	    && !a.is_plus()
	    )
	  return (symb_plus(args));
      }
      return operator_plus(a,b,contextptr);
    }
    gen sum(operator_plus(a,b,contextptr));
    for (;it!=itend;++it){
      if (sum.type==_SYMB && sum._SYMBptr->sommet==at_plus && sum._SYMBptr->feuille.type==_VECT && sum._SYMBptr->feuille._VECTptr->size()>1 ){
	// Add remaining elements to the symbolic sum, check float/inf/undef
	// FIXME should crunch if it->type is _DOUBLE_/_FLOAT_/_REAL e.g. for 1+sqrt(2)+sqrt(3.0)
	ref_vecteur * vptr=new ref_vecteur(*sum._SYMBptr->feuille._VECTptr);
	vptr->v.reserve(vptr->v.size()+(itend-it));
	for (;it!=itend;++it){
	  if (it->type==_SYMB && it->_SYMBptr->sommet==at_plus && it->_SYMBptr->feuille.type==_VECT){
	    iterateur jt=it->_SYMBptr->feuille._VECTptr->begin(),jtend=it->_SYMBptr->feuille._VECTptr->end();
	    for (;jt!=jtend;++jt){
	      vptr->v.push_back(*jt);
	    }
	    continue;
	  }
	  // if (it->type==_USER && vptr->v.front().type==_USER){ vptr->v.front()=operator_plus(vptr->v.front(),*it,contextptr); continue;}
	  if ( it->type==_DOUBLE_ || (it->type<=_POLY && vptr->v.back().type<=_POLY) ) // N.B. _DOUBLE_ special case bad for f(x):= 6. + 3.*x + 2.*x^2;g(x) := 12. ; expand(f(x)+g(x));
	    vptr->v.back()=operator_plus(vptr->v.back(),*it,contextptr);
	  else {
	    if (is_inf(*it) || is_undef(*it) || (it->type==_SYMB && it->_SYMBptr->sommet==at_plus))
	      break;
	    if (!is_zero(*it,contextptr))
	      vptr->v.push_back(*it);
	  }
	}
	if (is_zero(vptr->v.back(),contextptr))
	  vptr->v.pop_back();
	if (vptr->v.size()==1){
	  sum=vptr->v.front();
	  delete vptr;
	}
	else
	  sum=symb_plus(vptr);
	if (it==itend)
	  break;
      }
      operator_plus_eq(sum ,*it,contextptr);
    }
    return sum;
  }

  /* derivative of + is handled in derive.cc
  static unary_function_ptr _D_at_plus (int i) {
    return at_one;
  }
  const partial_derivative_multiargs D_at_plus(&_D_at_plus);
  */
  static const char _plus_s []="+";
  static define_unary_function_eval2_index (2,__plus,&_plus,_plus_s,&printsommetasoperator);
  define_unary_function_ptr( at_plus ,alias_at_plus ,&__plus);

  gen pointplus(const gen &a,const gen &b,GIAC_CONTEXT){
    if (a.type==_VECT && b.type!=_VECT)
      return apply1st(a,b,contextptr,pointplus);
    if (a.type!=_VECT && b.type==_VECT)
      return apply2nd(a,b,contextptr,pointplus);
    return operator_plus(a,b,contextptr);
  }
  gen _pointplus(const gen & args,GIAC_CONTEXT){
    if (args.type!=_VECT && args._VECTptr->size()!=2)
      return gensizeerr();
    gen a=args._VECTptr->front(),b=args._VECTptr->back();
    return pointplus(a,b,contextptr);
  }
  static const char _pointplus_s []=".+";
  static define_unary_function_eval2_index (170,__pointplus,&_pointplus,_pointplus_s,&printsommetasoperator);
  define_unary_function_ptr( at_pointplus ,alias_at_pointplus ,&__pointplus);

  gen pointminus(const gen &a,const gen &b,GIAC_CONTEXT){
    if (a.type==_VECT && b.type!=_VECT)
      return apply1st(a,b,contextptr,pointminus);
    if (a.type!=_VECT && b.type==_VECT)
      return apply2nd(a,b,contextptr,pointminus);
    return operator_minus(a,b,contextptr);
  }
  gen _pointminus(const gen & args,GIAC_CONTEXT){
    if (args.type!=_VECT && args._VECTptr->size()!=2)
      return gensizeerr();
    gen a=args._VECTptr->front(),b=args._VECTptr->back();
    return pointminus(a,b,contextptr);
  }
  static const char _pointminus_s []=".-";
  static define_unary_function_eval2_index (172,__pointminus,&_pointminus,_pointminus_s,&printsommetasoperator);
  define_unary_function_ptr( at_pointminus ,alias_at_pointminus ,&__pointminus);

  inline bool prod_idnt_symb(const gen & a){
    return (a.type==_IDNT && strcmp(a._IDNTptr->id_name,"undef") && strcmp(a._IDNTptr->id_name,"infinity")) || (a.type==_SYMB && !is_inf(a) && (a._SYMBptr->sommet==at_plus || a._SYMBptr->sommet==at_pow || a._SYMBptr->sommet==at_neg));
  }

  gen symb_prod(const gen & g){
    return symbolic(at_prod,g);
  }
  gen symb_prod(const vecteur &v){
    return symbolic(at_prod,gen(v,_SEQ__VECT));
  }
  gen symb_prod2(const gen & a,const gen &b){
    return symbolic(at_prod,makesequence(a,b));
  }
  
  gen symb_prod(const gen & a,const gen & b){
    if (a.is_neg()){
      if (b.is_neg())
	return symb_prod(a._SYMBptr->feuille,b._SYMBptr->feuille);
      return -symb_prod(a._SYMBptr->feuille,b);
    }
    if (b.is_neg())
      return -symb_prod(a,b._SYMBptr->feuille);
    if ((a.type<=_REAL ) && is_strictly_positive(-a,context0))
      return -symb_prod(-a,b);
    if ((b.type<=_REAL) && is_strictly_positive(-b,context0))
      return -symb_prod(a,-b);
    return symb_prod2(a,b);
  }
  gen _prod(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return args;
    iterateur it=args._VECTptr->begin(), itend=args._VECTptr->end();
    if (itend-it==2 && it->type<_IDNT && (it+1)->type<_IDNT)
      return operator_times(*it,*(it+1),contextptr);
    gen prod(1);
    /*
    if (it==itend)
      return 1;
    const gen & a=*it;
    ++it;
    if (itend==it)
      return a;
    const gen & b=*it;
    ++it;
    if (it==itend){
      // improve: if a is an idnt/symb and b also do not rebuild the vector
      if (idnt_symb_int(b) && prod_idnt_symb(a))
	return new symbolic(at_prod,args);
      if (idnt_symb_int(a) && prod_idnt_symb(b))
	return new symbolic(at_prod,args);
      return operator_plus(a,b,contextptr);
    }
    gen prod(operator_times(a,b,contextptr));
    */
    if (debug_infolevel>3)
      CERR << CLOCK() << " begin _prod" << endl;
    for (;it!=itend;++it){
      if ( (it->type==_SYMB) && (it->_SYMBptr->sommet==at_inv) && (it->_SYMBptr->feuille.type!=_VECT) )
	prod = rdiv(prod,it->_SYMBptr->feuille,contextptr);
      else {
	if (prod.type==_INT_ && prod.val==1)
	  prod=*it;
	else
	  prod = operator_times(prod,*it,contextptr);
      }
      if (debug_infolevel>3)
	CERR << CLOCK() << " in _prod" << endl;
    }
    return prod;
  }
  /*
  unary_function_ptr _D_at_prod (int i) {
    vector<int> v;
    v.push_back(i);
    vector<unary_function_ptr> w;
    w.push_back(at_prod);
    w.push_back(new unary_function_innerprod(v));
    return new unary_function_compose(w);
  }
  const partial_derivative_multiargs D_at_prod(&_D_at_prod);
  static const char _prod_s []="*";
  unary_function_eval __prod(&_prod,D_at_prod,_prod_s,&printsommetasoperator);
  unary_function_ptr at_prod (&__prod);
  */
  static const char _prod_s []="*";
  static define_unary_function_eval2_index (8,__prod,&_prod,_prod_s,&printsommetasoperator);
  define_unary_function_ptr( at_prod ,alias_at_prod ,&__prod);

  std::string cprintaspow(const gen & feuille,const char * sommetstr_orig,GIAC_CONTEXT){
    gen f(feuille);
    if (f.type==_VECT)
      f.subtype=_SEQ__VECT;
    return "pow("+f.print(contextptr)+")";
  }
  gen symb_pow(const gen & a){
    return symbolic(at_pow,a);
  }
  gen symb_pow(const gen & a,const gen & b){
    return symbolic(at_pow,makesequence(a,b));
  }
  gen _pow(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT || is_undef(args))
      return args;
    vecteur & v = *args._VECTptr;
    if (v.size()==3)
      return _powmod(args,contextptr); // Python 3 compat
    if (v.size()!=2)
      return gensizeerr(gettext("bad pow ")+args.print(contextptr));
    const gen & a =v.front();
    const gen & b =v.back();
    // fast check for monomials, do not recreate the vector
    if (b.type==_INT_){
#ifdef COMPILE_FOR_STABILITY
      if (b.val > FACTORIAL_SIZE_LIMIT)
	return genstabilityerr(contextptr);
#endif
      if (b.val==1)
	return a;
      if (a.type==_IDNT){
	if (a==undef)
	  return a;
	if (a!=unsigned_inf)
	  return b.val?symb_pow(args):gen(1);
      }
      if (a.type==_SYMB && !is_inf(a) && (a._SYMBptr->sommet==at_plus || a._SYMBptr->sommet==at_prod)){
	return b.val?symb_pow(args):gen(1);
      }
    }
    return pow(a,b,contextptr);
  }
  /* derivative of ^ is handled in derive.cc
  static gen d1_pow(const gen & args,GIAC_CONTEXT){
    if (args.type!=_VECT)
      return gensizeerr(contextptr);
    vecteur & v=*args._VECTptr;
    if (v.size()!=2)
      return gensizeerr(gettext("bad pow ")+args.print(contextptr));
    if (v[1].type<=_REAL)
      return v[1]*pow(v[0],v[1]-1,contextptr);
    else
      return v[1]/v[0]*symb_pow(gen(v,_SEQ__VECT));
  }
  static gen d2_pow(const gen & args,GIAC_CONTEXT){
    if (args.type!=_VECT)
      return gensizeerr(contextptr);
    vecteur & v=*args._VECTptr;
    if (v.size()!=2)
      return gensizeerr(gettext("bad pow ")+args.print(contextptr));
    return ln(v[0],contextptr)*pow(v[0],v[1],contextptr);
  }
  static define_unary_function_eval(d1_pow_eval,&d1_pow,"d1_pow");
  define_unary_function_ptr( D1_pow,alias_D1_pow,&d1_pow_eval);
  static define_unary_function_eval(d2_pow_eval,&d2_pow,"d2_pow");
  define_unary_function_ptr( D2_pow,alias_D2_pow,&d2_pow_eval);
  static unary_function_ptr d_pow(int i){
    if (i==1)
      return D1_pow;
    if (i==2)
      return D2_pow;
    return gensizeerr(contextptr);
    return 0;
  }
  static const partial_derivative_multiargs D_pow(&d_pow);
  */
  const char _pow_s []="^";
#ifndef GIAC_HAS_STO_38
#if defined NSPIRE_NEWLIB || defined FXCG || defined TICE
  define_unary_function_eval2_index (14,__pow,&_pow,_pow_s,&printsommetasoperator);
#else
  unary_function_eval __pow(14,&_pow,0,_pow_s,&printsommetasoperator,0);
#endif
#else
  Defineunary_function_eval(__pow, 14, &_pow, 0, _pow_s, &printsommetasoperator, 14);
  #define __pow (*((unary_function_eval*)&unary__pow))
#endif
  define_unary_function_ptr( at_pow ,alias_at_pow ,&__pow);

  // print power like a^b (args==1), pow(a,b) (args==0) or a**b (args==-1)
  static gen _printpow(const gen & args,GIAC_CONTEXT){
#if defined NSPIRE_NEWLIB || defined FXCG || defined TICE
    return undef;
#else
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (is_zero(args,contextptr)){
      __pow.printsommet=&cprintaspow;
      return string2gen("pow",false);
    }
    else {
      __pow.printsommet=&printsommetasoperator;
      if (is_minus_one(args))
	__pow.s="**";
      else
	__pow.s="^";
      return string2gen(__pow.s,false);
    }
#endif
  }
  static const char _printpow_s []="printpow";
  static define_unary_function_eval (__printpow,&_printpow,_printpow_s);
  define_unary_function_ptr5( at_printpow ,alias_at_printpow,&__printpow,0,true);

  // static gen symb_powmod(const gen & a,const gen & b,const gen & n){     return symbolic(at_powmod,makevecteur(a,b,n));  }
  static gen symb_powmod(const gen & a){
    return symbolic(at_powmod,a);
  }
  static gen findmod(const gen & g){
    if (g.type==_MOD)
      return *(g._MODptr+1);
    if (g.type==_VECT){
      gen res;
      const_iterateur it=g._VECTptr->begin(),itend=g._VECTptr->end();
      for (;it!=itend;++it){
	res=findmod(*it);
	if (!is_exactly_zero(res))
	  return res;
      }
    }
    if (g.type==_SYMB)
      return findmod(g._SYMBptr->feuille);
    return 0;
  }
  gen _powmod(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    int s;
    if ( args.type!=_VECT || (s=int(args._VECTptr->size()))<3 )
      return symb_powmod(args);
    vecteur v = *args._VECTptr;
    gen a=v.front();
    is_integral(a);
    gen n=v[1];
    if (n.type==_VECT){
      vecteur w =*n._VECTptr;
      iterateur it=w.begin(),itend=w.end();
      for (;it!=itend;++it){
	v[1]=*it;
	*it=_powmod(gen(v,_SEQ__VECT),contextptr);
      }
      return gen(w,n.subtype);
    }
    if (!is_integral(n) || !is_integer(n))
      return symb_powmod(args);
    gen m=v[2];
    is_integral(m);
    if (s==3 && m.type!=_SYMB) // a^n mod m
      return powmod(v.front(),v[1],m);
    // powmod(a_x%m,n,p_x) or powmod(a_x,n,m,p_x,x)
    // a^n mod p,m or m,p or a^n mod p,m,x or m,p,x wrt var x
    gen var(vx_var()),p;
    bool modafter=false;
    p=unmod(m);
    m=findmod(m);
    if (is_zero(m)){
      // find m inside a
      m=findmod(a);
    }
    modafter=!is_zero(m);
    a=unmod(a);
    if (modafter && s>3)
      var=v[3];
    if (!modafter && s>3){
      m=v[2];
      p=v[3];
      if (is_integer(p)){
	p=v[2]; m=v[3]; 
      }
    }
    gen m1=findmod(p);
    if (!is_zero(m1)){
      if (is_zero(m))
	m=m1;
      else if (m1!=m)
	return gensizeerr(contextptr);
    }
    p=unmod(p);
    if (s>=5)
      var=v[4];
    vecteur lv(1,var);
    lvar(v,lv);
    if (lv.size()!=1)
      *logptr(contextptr) << gettext("Too many variables ")+gen(lv).print(contextptr) << endl;
    gen aa=e2r(a,lv,contextptr),aan,aad,bb=e2r(p,lv,contextptr),bbn,bbd;
    fxnd(aa,aan,aad);
    if ( (aad.type==_POLY) && (aad._POLYptr->lexsorted_degree() ) )
      return gensizeerr(contextptr);
    fxnd(bb,bbn,bbd);
    if ( (bbd.type==_POLY) && (bbd._POLYptr->lexsorted_degree() ) )
      return gensizeerr(contextptr); 
    if (bbn.type!=_POLY)
      return gensizeerr(contextptr);
    modpoly A;
    if (aan.type==_POLY)
      A=polynome2poly1(*aan._POLYptr);
    else
      A.push_back(aan);
    modpoly B=polynome2poly1(*bbn._POLYptr);
    environment env;
    if (!is_zero(m)){
      env.moduloon=true;
      env.modulo=m;
    }
    // if (!B.empty() && !is_zero(m)) mulmodpoly(B,invmod(B.front(),m),&env,B);
    modpoly res=powmod(A,n,B,&env);
    polynome R;
    if (lv.size()==1)
      R=poly12polynome(res);
    else
      R=poly12polynome(res,1,int(lv.size()));
    if (modafter)
      modularize(R,m);
    gen Res=r2e(R,lv,contextptr)/pow(r2e(aad,lv,contextptr),n,contextptr);
    return Res;
  }
  static const char _powmod_s []="powmod";
  static define_unary_function_eval (__powmod,&_powmod,_powmod_s);
  define_unary_function_ptr5( at_powmod ,alias_at_powmod,&__powmod,0,true);

  gen symb_inferieur_strict(const gen & a,const gen & b){
    return symbolic(at_inferieur_strict,makesequence(a,b));
  }
  gen symb_inferieur_strict(const gen & a){
    return symbolic(at_inferieur_strict,a);
  }
  gen _inferieur_strict(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return symb_inferieur_strict(args);
    gen res=inferieur_strict(args._VECTptr->front(),args._VECTptr->back(),contextptr);
    if (res.type==_INT_ 
#ifdef GIAC_HAS_STO_38
	&& abs_calc_mode(contextptr)!=38
#endif
	)
      res.subtype=_INT_BOOLEAN;
    return res;
  }
  static const char _inferieur_strict_s []="<";
  static define_unary_function_eval4_index (70,__inferieur_strict,&_inferieur_strict,_inferieur_strict_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_inferieur_strict ,alias_at_inferieur_strict ,&__inferieur_strict);

  gen symb_inferieur_egal(const gen & a,const gen & b){
    return symbolic(at_inferieur_egal,makesequence(a,b));
  }
  gen symb_inferieur_egal(const gen & a){
    return symbolic(at_inferieur_egal,a);
  }
  gen _inferieur_egal(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return symb_inferieur_egal(args);
    gen res=inferieur_egal(args._VECTptr->front(), args._VECTptr->back(),contextptr);
    if (res.type==_INT_
	//&& abs_calc_mode(contextptr)!=38
	)
      res.subtype=_INT_BOOLEAN;
    return res;
  }
  static const char _inferieur_egal_s []="<=";//"≤";
  static define_unary_function_eval4_index (72,__inferieur_egal,&_inferieur_egal,_inferieur_egal_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_inferieur_egal ,alias_at_inferieur_egal ,&__inferieur_egal);

  gen symb_superieur_strict(const gen & a,const gen & b){
    return symbolic(at_superieur_strict,makesequence(a,b));
  }
  gen symb_superieur_strict(const gen & a){
    return symbolic(at_superieur_strict,a);
  }
  gen _superieur_strict(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return symb_superieur_strict(args);
    gen res(superieur_strict(args._VECTptr->front(),args._VECTptr->back(),contextptr));
    if (res.type==_INT_
	//&& abs_calc_mode(contextptr)!=38
	)
      res.subtype=_INT_BOOLEAN;
    return res;
  }
  static const char _superieur_strict_s []=">";
  static define_unary_function_eval4_index (74,__superieur_strict,&_superieur_strict,_superieur_strict_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_superieur_strict ,alias_at_superieur_strict ,&__superieur_strict);

  gen symb_superieur_egal(const gen & a,const gen & b){
    return symbolic(at_superieur_egal,makesequence(a,b));
  }
  gen symb_superieur_egal(const gen & a){
    return symbolic(at_superieur_egal,a);
  }
  gen _superieur_egal(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return symb_superieur_egal(args);
    gen res=superieur_egal(args._VECTptr->front(), args._VECTptr->back(),contextptr);
    if (res.type==_INT_
	//&& abs_calc_mode(contextptr)!=38
	)
      res.subtype=_INT_BOOLEAN;
    return res;
  }
  static const char _superieur_egal_s []=">="; // "≥";
  static define_unary_function_eval4_index (76,__superieur_egal,&_superieur_egal,_superieur_egal_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_superieur_egal ,alias_at_superieur_egal ,&__superieur_egal);

  // static gen symb_different(const gen & a,const gen & b){    return symbolic(at_different,makevecteur(a,b));  }
  static gen symb_different(const gen & a){
    return symbolic(at_different,a);
  }
  gen _different(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return symb_different(args);
    gen res;
#if 1
    res=_same(args,contextptr);
    if (res.type==_INT_)
      return !res;
#endif
    res=args._VECTptr->front() != args._VECTptr->back();
    if (res.type==_INT_
	// && abs_calc_mode(contextptr)!=38
	)
      res.subtype=_INT_BOOLEAN;
    return res;
  }
  static const char _different_s []="!=";
  static define_unary_function_eval2_index (78,__different,&_different,_different_s,&printsommetasoperator);
  define_unary_function_ptr( at_different ,alias_at_different ,&__different);

  static string printasof_(const gen & feuille,const char * sommetstr,int format,GIAC_CONTEXT){
    if ( (feuille.type!=_VECT) || (feuille._VECTptr->size()!=2) )
      return string(sommetstr)+('('+gen2string(feuille,format,contextptr)+')');
    string s=print_with_parenthesis_if_required(feuille._VECTptr->front(),format,contextptr)+'(';
    gen & g=feuille._VECTptr->back();
    if (format==0 && g.type==_VECT && g.subtype==_SEQ__VECT)
      return s+printinner_VECT(*g._VECTptr,_SEQ__VECT,contextptr)+')';
    else
      return s+gen2string(g,format,contextptr)+')';
  }
  static string printasof(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    return printasof_(feuille,sommetstr,0,contextptr);
  }
  // Find the best interpretation of a(b) either as a function of or as a product (implicit *)

  static bool warn_implicit(const gen & a,const gen &b,GIAC_CONTEXT){
    if (contains(lidnt(b),i__IDNT_e))
      *logptr(contextptr) << gettext("Implicit multiplication does not work with complex numbers.")<<endl;
    else
      *logptr(contextptr) << gettext("Warning : using implicit multiplication for (") << a.print(contextptr) << ")(" << b.print(contextptr) << ')' << endl;
    return true;
  }
  gen check_symb_of(const gen& a,const gen & b0,GIAC_CONTEXT){
    if ( (a.type<_IDNT) && b0.type==_VECT && b0._VECTptr->empty())
      return a;
    gen b(b0);
    if (b0.type==_VECT && b0.subtype==_SEQ__VECT && b0._VECTptr->size()==1)
      b=b0._VECTptr->front();
    if (a.type<_IDNT){
      if (!warn_implicit(a,b,contextptr))
	return gensizeerr("Invalid implicit multiplication for ("+ a.print(contextptr)+")(" + b.print(contextptr)+')');
      return a*b;
    }
    vecteur va(lvar(a));
    if (va.empty()){
      //if (abs_calc_mode(contextptr)==38)
	return gensizeerr("Invalid implicit multiplication for ("+ a.print(contextptr)+")(" + b.print(contextptr)+')');
      //*logptr(contextptr) << "Warning, input parsed as a constant function " << a << " applied to " << b << endl;
    }
    if (!va.empty() && calc_mode(contextptr)==38){
      // check names in va
      bool implicit=false;
      const_iterateur it=va.begin(),itend=va.end();
      for (;it!=itend;++it){
	if (it->type!=_IDNT){
#ifdef CAS38_DISABLED
	  implicit=true;
#else
	  implicit=it->type!=_SYMB;
#endif
	  continue;
	}
	const char * ch = it->_IDNTptr->id_name;
	if (strlen(ch)==2 && (ch[0]=='F' || ch[0]=='R' || ch[0]=='X' || ch[0]=='Y') )
	  return symb_of(a,b);
	if (strlen(ch)==1 && ch[0]<='a')
	  implicit=true;
      }
      if (implicit){
	if (!warn_implicit(a,b,contextptr))
	  return gensizeerr("Invalid implicit multiplication for ("+ a.print(contextptr)+")(" + b.print(contextptr)+')');
	return a*b;
      }
    }
    vecteur vb(lvar(b));
    vecteur vab(lvar(makevecteur(a,b)));
    if (vab.size()==va.size()+vb.size()){
      vecteur lvarxb;
      if (va.size()!=1 || va.front().type!=_IDNT || (lvarxb=lvarx(b,va.front())).empty() || !lop(lvarxb,at_of).empty())
	return symb_of(a,b);
    }
    if (!warn_implicit(a,b,contextptr))
      return gensizeerr("Invalid implicit multiplication for ("+ a.print(contextptr)+")(" + b.print(contextptr)+')');
    return a*b;
  }
  gen symb_of(const gen & a,const gen & b){
    if (b.type==_VECT && b.subtype==_SEQ__VECT && b._VECTptr->size()==1)
      return symbolic(at_of,makesequence(a,b._VECTptr->front()));//symbolic(at_of,gen(makevecteur(a,b._VECTptr->front()),_SEQ__VECT));
    return symbolic(at_of,makesequence(a,b));
  }
  gen symb_of(const gen & a){
    gen aa(a);
    if (aa.type==_VECT)
      aa.subtype=_SEQ__VECT;
    return symbolic(at_of,aa);
  }
  static bool tri2_(const char * a,const char * b){
    return strcmp(a,b)<0;
  }

  gen _of(const gen & args,const context * contextptr){
    gen qf,b,f,value;
    // *logptr(contextptr) << &qf << endl;
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return symb_of(args);
    qf=args._VECTptr->front();
    b=args._VECTptr->back();
    bool quoteb=false;
    if (!quoteb){
      if (approx_mode(contextptr))
	b=b.evalf(eval_level(contextptr),contextptr);
      else
	b=b.eval(eval_level(contextptr),contextptr);
    }
    /*
    if (qf.type!=_IDNT || !(strcmp(qf._IDNTptr->id_name,"RECURSE")==0 ||
                            strcmp(qf._IDNTptr->id_name,"SOLVE")==0 ||
                            strcmp(qf._IDNTptr->id_name,"Do2VStats")==0 ||
                            strcmp(qf._IDNTptr->id_name,"Do1VStats")==0
                           )
      )
      b=b.eval(eval_level(contextptr),contextptr);
    */
    if (storcl_38){
      if (qf.type==_IDNT){
	if (storcl_38(value,0,qf._IDNTptr->id_name,b,true,contextptr,NULL,false)){
	  return value;
	}
      }
      if (qf.is_symb_of_sommet(at_double_deux_points)){
	f=qf._SYMBptr->feuille;
	if (f.type==_VECT && (*f._VECTptr)[0].type==_IDNT && (*f._VECTptr)[1].type==_IDNT){
	  if (storcl_38(value,(*f._VECTptr)[0]._IDNTptr->id_name,(*f._VECTptr)[1]._IDNTptr->id_name,b,true,contextptr,NULL,false)){
	    return value;
	  }
	}
      }
    }
    f=qf.eval(eval_level(contextptr),contextptr);
    if (f.is_symb_of_sommet(at_struct_dot) && f._SYMBptr->feuille.type==_VECT && f._SYMBptr->feuille._VECTptr->size()==2){
      gen v=f._SYMBptr->feuille._VECTptr->front(),op=f._SYMBptr->feuille._VECTptr->back();
      gen ve=eval(v,eval_level(contextptr),contextptr);
      if (b.type==_VECT && b.subtype==_SEQ__VECT && b._VECTptr->empty())
	;
      else
	ve=makesuite(ve,b._SYMBptr->feuille);
      ve=op(ve,contextptr);
      return sto(ve,v,contextptr);
    }
    if (f.type<=_POLY || f.type==_FRAC )
      *logptr(contextptr) << "Warning, constant function " << f << " applied to " << b << endl;
    if ( f.is_program() && qf.type==_IDNT ){
      value=f._SYMBptr->feuille;
      if (value.type!=_VECT)
	return gensizeerr(contextptr);
      value=gen(*value._VECTptr,value.subtype); // clone
#ifdef GIAC_DEFAULT_ARGS
      gen v1=(*value._VECTptr)[1];
      vecteur v1v(1,v1);
      if (v1.type==_VECT && v1.subtype==_SEQ__VECT)
	v1v=*v1._VECTptr;
      vecteur bv(1,b);
      if (b.type==_VECT && b.subtype==_SEQ__VECT)
	bv=*b._VECTptr;
      if (bv.size()<v1v.size())
	bv=mergevecteur(bv,vecteur(v1v.begin()+bv.size(),v1v.end()));
      if (v1.type!=_VECT && bv.size()==1)
	b=bv.front();
      else
	b=gen(v1v,v1.subtype);
#endif
      (*value._VECTptr)[1]=b;
      // vecteur v=(*value._VECTptr);
      // v[1]=b;
      // value=gen(v,value.subtype);
      return _program(value,qf,contextptr);
    }
    return f(b,contextptr);
  }
  static const char _of_s []="of";
  static define_unary_function_eval4_index (163,__of,&_of,_of_s,&printasof,&texprintsommetasoperator);
  define_unary_function_ptr5( at_of ,alias_at_of,&__of,_QUOTE_ARGUMENTS,0);

  string gen2string(const gen & g,int format,GIAC_CONTEXT){
    if (format==1) 
      return gen2tex(g,contextptr); 
    else 
      return g.print(contextptr);
  }

  string print_with_parenthesis_if_required(const gen & g,int format,GIAC_CONTEXT){
    if (g.type==_SYMB || g.type==_FRAC || g.type==_CPLX || (g.type==_VECT && g.subtype==_SEQ__VECT) )
      return '('+gen2string(g,format,contextptr)+')';
    else
      return gen2string(g,format,contextptr);
  }
  
  static string printasat_(const gen & feuille,const char * sommetstr,int format,GIAC_CONTEXT){
    if ( (feuille.type!=_VECT) || (feuille._VECTptr->size()!=2) )
      return string(sommetstr)+('('+gen2string(feuille,format,contextptr)+')');
    vecteur & v=*feuille._VECTptr;
    if (v.back().type!=_STRNG && array_start(contextptr)){ //(xcas_mode(contextptr) > 0 || abs_calc_mode(contextptr)==38)){
      gen indice;
      if (v.back().type==_VECT)
	indice=v.back()+vecteur(v.size(),plus_one);
      else
	indice=v.back()+plus_one;
      string s;
      return print_with_parenthesis_if_required(v.front(),format,contextptr)+'['+gen2string(indice,format,contextptr)+']';
    }
    else
      return print_with_parenthesis_if_required(feuille._VECTptr->front(),format,contextptr)+'['+gen2string(feuille._VECTptr->back(),format,contextptr)+']';
  }

  static string printasat(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    return printasat_(feuille,sommetstr,0,contextptr);
  }
  gen symb_at(const gen & a,const gen & b,GIAC_CONTEXT){
    if (array_start(contextptr)){ //xcas_mode(contextptr) || abs_calc_mode(contextptr)==38){
      gen bb;
      if (b.type==_VECT)
	bb=b-vecteur(b._VECTptr->size(),plus_one);
      else
	bb=b-plus_one;
      return symbolic(at_at,makesequence(a,bb));
    }
    else
      return symbolic(at_at,makesequence(a,b));
  }
  gen symb_at(const gen & a){
    gen aa(a);
    if (aa.type==_VECT)
      aa.subtype=_SEQ__VECT;
    return symbolic(at_at,aa);
  }
  gen _at(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return symb_at(args);
    vecteur & v=*args._VECTptr;
    if (v.size()!=2)
      return gensizeerr(contextptr);
    static bool alert_array_start=true;
    if (alert_array_start && contextptr->globalptr->_python_compat_){
      alert_array_start=false;
#ifdef GIAC_HAS_STO_38
      alert(gettext("Python compatibility enabled. List index will start at 0, run index:=1 or of:=1 to disable Python compatibility."),contextptr);
#else
      *logptr(contextptr) << gettext("// Python compatibility enabled. List index will start at 0, run index:=1 or python_compat(0) to disable Python compatibility.") << endl;
#endif
    }
    if (storcl_38){
      if (v.front().type==_IDNT){
	gen value;
	if (storcl_38(value,0,v.front()._IDNTptr->id_name,v.back(),false,contextptr,NULL,false)){ //CdB v.back() is actually never used because the at_of paramter is false. Is that intended?
	  return value;
	}
      }
      if (v.front().is_symb_of_sommet(at_double_deux_points)){
	gen & f=v.front()._SYMBptr->feuille;
	if (f[0].type==_IDNT && f[1].type==_IDNT){
	  gen value;
	  if (storcl_38(value,f[0]._IDNTptr->id_name,f[1]._IDNTptr->id_name,v.back(),false,contextptr,NULL,false)){ //CdB v.back() is actually never used because the at_of paramter is false. Is that intended?
	    return value;
	  }
	}
      }
    }
    gen a=v.front().eval(eval_level(contextptr),contextptr);
    gen b=v.back().eval(eval_level(contextptr),contextptr);
    if (a.type==_MAP){
      gen_map::const_iterator it=a._MAPptr->find(b),itend=a._MAPptr->end();
      if (it!=itend)
	return it->second;
      // if (a.subtype==_SPARSE_MATRIX)
	return 0;
	//return symb_at(makevecteur(v.front(),b));
    }
    return a.operator_at(b,contextptr);
  }
  static const char _at_s []="at";
  static define_unary_function_eval4_index (165,__at,&_at,_at_s,&printasat,&texprintsommetasoperator);
  define_unary_function_ptr5( at_at ,alias_at_at,&__at,_QUOTE_ARGUMENTS,0);

  gen _table(const gen & arg,GIAC_CONTEXT){
    if ( arg.type==_STRNG && arg.subtype==-1) return  arg;
    vecteur v(gen2vecteur(arg));
    const_iterateur it=v.begin(),itend=v.end();
#if 1 // def NSPIRE
    gen_map m;
#else
    gen_map m(ptr_fun(islesscomplexthanf));
#endif
    for (;it!=itend;++it){
      if (is_equal(*it) || it->is_symb_of_sommet(at_deuxpoints)){
	gen & f =it->_SYMBptr->feuille;
	if (f.type==_VECT && f._VECTptr->size()==2){
	  vecteur & w=*f._VECTptr;
	  gen bb=w.front();
	  if (array_start(contextptr)){ //(xcas_mode(contextptr) || abs_calc_mode(contextptr)==38)){
	    if (bb.type==_VECT)
	      bb=bb-vecteur(bb._VECTptr->size(),plus_one);
	    else
	      bb=bb-plus_one;
	  }
	  m[bb]=w.back();
	}
      }
    }
    return m;
  }
  static const char _table_s []="table";
  static define_unary_function_eval (__table,&_table,_table_s);
  define_unary_function_ptr5( at_table ,alias_at_table,&__table,0,true);

  string printasand(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
#if 0
    if (calc_mode(contextptr)==1)
      return printsommetasoperator(feuille," && ",contextptr);
#endif
    if (xcas_mode(contextptr) > 0 || python_compat(contextptr))
      return printsommetasoperator(feuille," and ",contextptr);
    else
      return "("+printsommetasoperator(feuille,sommetstr,contextptr)+")";
  }
  gen symb_and(const vecteur &v){
    return symbolic(at_and,gen(v,_SEQ__VECT));
  }
  gen symb_and(const gen & a,const gen & b){
    return symbolic(at_and,makesequence(a,b));
  }
  gen and2(const gen & a,const gen & b){
    return a && b;
  }
  gen _and(const gen & arg,GIAC_CONTEXT){
    if ( arg.type==_STRNG && arg.subtype==-1) return  arg;
    if (arg.type==_VECT && arg.subtype==_SEQ__VECT && arg._VECTptr->size()==2 && arg._VECTptr->front().type==_VECT)
      return apply(equaltosame(arg._VECTptr->front()).eval(eval_level(contextptr),contextptr),equaltosame(arg._VECTptr->back()).eval(eval_level(contextptr),contextptr),and2);
    gen args=apply(arg,equaltosame);
    if (arg.type!=_VECT || arg._VECTptr->empty())
      return equaltosame(arg).eval(eval_level(contextptr),contextptr);
    vecteur::const_iterator it=arg._VECTptr->begin(),itend=arg._VECTptr->end();
    gen res(eval(equaltosame(*it),eval_level(contextptr),contextptr));
    ++it;
    for (;it!=itend;++it){
      if (res.type==_INT_ && res.val==0)
	return res;
      res = res && eval(equaltosame(*it),eval_level(contextptr),contextptr);
    }
    return res;
  }
  static const char _and_s []="and";
  static define_unary_function_eval4_index (67,__and,&_and,_and_s,&printasand,&texprintsommetasoperator);
  define_unary_function_ptr5( at_and ,alias_at_and,&__and,_QUOTE_ARGUMENTS,T_AND_OP);

  string printasor(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
#if 0
    if (calc_mode(contextptr)==1)
      return printsommetasoperator(feuille," || ",contextptr);
#endif
    if (xcas_mode(contextptr) > 0 || python_compat(contextptr))
      return printsommetasoperator(feuille," or ",contextptr);
    else
      return "("+printsommetasoperator(feuille,sommetstr,contextptr)+")";
  }
  gen symb_ou(const vecteur &v){
    return symbolic(at_ou,gen(v,_SEQ__VECT));
  }
  gen symb_ou(const gen & a,const gen & b){
    return symbolic(at_ou,makesequence(a,b));
  }
  gen ou2(const gen & a,const gen & b){
    return a || b;
  }
  gen _ou(const gen & arg,GIAC_CONTEXT){
    if ( arg.type==_STRNG && arg.subtype==-1) return  arg;
    int el=eval_level(contextptr);
    if (arg.type==_VECT && arg.subtype==_SEQ__VECT && arg._VECTptr->size()==2 && arg._VECTptr->front().type==_VECT)
      return apply(equaltosame(arg._VECTptr->front()).eval(el,contextptr),equaltosame(arg._VECTptr->back()).eval(el,contextptr),ou2);
    if (arg.type!=_VECT || arg._VECTptr->empty())
      return eval(equaltosame(arg),el,contextptr);
    vecteur::const_iterator it=arg._VECTptr->begin(),itend=arg._VECTptr->end(); 
    gen res(eval(equaltosame(*it),el,contextptr));
    ++it;
    for (;it!=itend;++it){
      if (res.type==_INT_ && res.val)
	return res;
      res = res || eval(equaltosame(*it),el,contextptr);
    }
    return res;
  }
  static const char _ou_s []="or";
  static define_unary_function_eval4_index (69,__ou,&_ou,_ou_s,&printasor,&texprintsommetasoperator);
  define_unary_function_ptr5( at_ou ,alias_at_ou,&__ou,_QUOTE_ARGUMENTS,T_AND_OP);

  gen xor2(const gen & a,const gen & b,GIAC_CONTEXT){
    return is_zero(a,contextptr) ^ is_zero(b,contextptr);
  }
  gen _xor(const gen & arg,GIAC_CONTEXT){
    if ( arg.type==_STRNG && arg.subtype==-1) return  arg;
    if (arg.type==_VECT && arg.subtype==_SEQ__VECT && arg._VECTptr->size()==2)
      return apply(
		   equaltosame(arg._VECTptr->front()).eval(eval_level(contextptr),contextptr),
		   equaltosame(arg._VECTptr->back()).eval(eval_level(contextptr),contextptr),
		   contextptr,xor2);
    gen args=eval(apply(arg,equaltosame),eval_level(contextptr),contextptr);
    if (args.type!=_VECT)
      return args;
    vecteur::const_iterator it=args._VECTptr->begin(),itend=args._VECTptr->end();
    gen res=*it;
    ++it;
    for (;it!=itend;++it){
      if (is_zero(res,contextptr))
	res=*it;
      else
	res = !(*it);
    }
    return res;
  }
#ifdef GIAC_HAS_STO_38
  static const char _xor_s []="XOR";
#else
  static const char _xor_s []=" xor ";
#endif
  static define_unary_function_eval4_index (117,__xor,&_xor,_xor_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr5( at_xor ,alias_at_xor,&__xor,_QUOTE_ARGUMENTS,0);

  gen symb_min(const gen & a,const gen & b){
    return symbolic(at_min,makesequence(a,b));
  }
  gen _min(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return args;
    if (args.type==_POLY){
      vector< monomial<gen> >::const_iterator it=args._POLYptr->coord.begin(),itend=args._POLYptr->coord.end();
      if (it==itend)
	return undef;
      gen m(it->value);
      for (++it;it!=itend;++it){
	if (is_strictly_greater(m,it->value,contextptr))
	  m=it->value;
      }
      return m;
    }
    vecteur::const_iterator it=args._VECTptr->begin(),itend=args._VECTptr->end();
    if (it==itend)
      return gendimerr(contextptr);
    if (ckmatrix(args)){
      gen res=*it;
      for (++it;it!=itend;++it){
	res=apply(res,*it,contextptr,min);
      }
      return res;
    }
    if (itend-it==2 && it->type==_VECT && (it+1)->type==_VECT )
      return matrix_apply(*it,*(it+1),contextptr,min);
    gen res=*it;
    ++it;
    for (;it!=itend;++it)
      res = min(res,*it,contextptr);
    return res;
  }
  static const char _min_s []="min";
  static define_unary_function_eval (giac__min,&_min,_min_s);
  define_unary_function_ptr5( at_min ,alias_at_min,&giac__min,0,true);

  gen symb_max(const gen & a,const gen & b){
    return symbolic(at_max,makesequence(a,b));
  }
  gen _max(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type==_POLY){
      vector< monomial<gen> >::const_iterator it=args._POLYptr->coord.begin(),itend=args._POLYptr->coord.end();
      if (it==itend)
	return undef;
      gen m(it->value);
      for (++it;it!=itend;++it){
	if (is_strictly_greater(it->value,m,contextptr))
	  m=it->value;
      }
      return m;
    }
    if (args.type!=_VECT)
      return args;
    vecteur::const_iterator it=args._VECTptr->begin(),itend=args._VECTptr->end();
    if (itend==it)
      return undef; // gendimerr(contextptr);
    if (itend-it==1)
      return _max(*it,contextptr);
    if (ckmatrix(args)){
      gen res=*it;
      for (++it;it!=itend;++it){
	res=apply(res,*it,contextptr,max);
      }
      return res;
    }
    if (itend-it==2 && it->type==_VECT && (it+1)->type==_VECT )
      return matrix_apply(*it,*(it+1),contextptr,max);
    gen res=*it;
    ++it;
    for (;it!=itend;++it)
      res = max(res,*it,contextptr);
    return res;
  }
  static const char _max_s []="max";
  static define_unary_function_eval (giac__max,&_max,_max_s);
  define_unary_function_ptr5( at_max ,alias_at_max,&giac__max,0,true);

  // static gen symb_gcd(const gen & a,const gen & b){    return symbolic(at_gcd,makevecteur(a,b));  }
  gen _gcd(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (is_integer(args))
      return abs(args,contextptr);
    if (args.type!=_VECT)
      return args;
    if (debug_infolevel>2)
      CERR << "gcd begin " << CLOCK() << endl;
    vecteur::const_iterator it=args._VECTptr->begin(),itend=args._VECTptr->end();
    if (ckmatrix(args) && itend-it==2 && it->subtype!=_POLY1__VECT && (it+1)->subtype!=_POLY1__VECT)
      return apply(*it,*(it+1),contextptr,gcd);
    gen res(0);
    for (;it!=itend;++it)
      res=gcd(res,*it,contextptr);
    return res;
  }
  static const char _gcd_s []="gcd";
  static define_unary_function_eval (__gcd,&_gcd,_gcd_s);
  define_unary_function_ptr5( at_gcd ,alias_at_gcd,&__gcd,0,true);

  // static gen symb_lcm(const gen & a,const gen & b){    return symbolic(at_lcm,makevecteur(a,b));  }
  gen _lcm(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return args;
    vecteur::const_iterator it=args._VECTptr->begin(),itend=args._VECTptr->end();
    if (itend==it)
      return 1;
    if (ckmatrix(args) && itend-it==2 && it->subtype!=_POLY1__VECT && (it+1)->subtype!=_POLY1__VECT)
      return apply(*it,*(it+1),lcm);
    gen res(*it);
    for (++it;it!=itend;++it)
      res=lcm(res,*it);
    return res;
  }
  static const char _lcm_s []="lcm";
  static define_unary_function_eval (__lcm,&_lcm,_lcm_s);
  define_unary_function_ptr5( at_lcm ,alias_at_lcm,&__lcm,0,true);

  // static gen symb_egcd(const gen & a,const gen & b){    return symbolic(at_egcd,makevecteur(a,b));  }
  gen _egcd(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if ( (args.type!=_VECT) || args._VECTptr->empty() )
      return gensizeerr(contextptr);
    vecteur & a = *args._VECTptr;
    if ( (a.front().type==_VECT) && (a.back().type==_VECT) ){
      vecteur u,v,d;
      egcd(*a.front()._VECTptr,*a.back()._VECTptr,0,u,v,d);
      return gen(makevecteur(u,v,d),_POLY1__VECT);
    }
    vecteur lv;
    if (a.size()==3)
      lv=vecteur(1,vecteur(1,a[2]));
    else
      lv=vecteur(1,vecteur(1,vx_var()));
    alg_lvar(args,lv);
    gen aa=e2r(a[0],lv,contextptr),aan,aad,bb=e2r(a[1],lv,contextptr),bbn,bbd;
    fxnd(aa,aan,aad);
    if ( (aad.type==_POLY) && (aad._POLYptr->lexsorted_degree() ) )
      return gensizeerr(contextptr);
    fxnd(bb,bbn,bbd);
    if ( (bbd.type==_POLY) && (bbd._POLYptr->lexsorted_degree() ) )
      return gensizeerr(contextptr); 
    gen u,v,d;
    if ( (aan.type==_POLY) && (bbn.type==_POLY) ){
      polynome un(aan._POLYptr->dim),vn(aan._POLYptr->dim),dn(aan._POLYptr->dim);
      egcd(*aan._POLYptr,*bbn._POLYptr,un,vn,dn);
      u=un;
      v=vn;
      d=dn;
    }
    else {
      if (aan.type==_POLY){
	u=zero;
	v=plus_one;
	d=bbn;
      }
      else {
	u=plus_one;
	v=zero;
	d=aan;
      }
    }
    u=r2e(u*aad,lv,contextptr);
    v=r2e(v*bbd,lv,contextptr);
    d=r2e(d,lv,contextptr);
    return makevecteur(u,v,d);
  }
  static const char _egcd_s []="egcd";
  static define_unary_function_eval (__egcd,&_egcd,_egcd_s);
  define_unary_function_ptr5( at_egcd ,alias_at_egcd,&__egcd,0,true);

  // static gen symb_iegcd(const gen & a,const gen & b){    return symbolic(at_iegcd,makevecteur(a,b));  }
  gen _iegcd(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (!check_2d_vecteur(args)) return gensizeerr(contextptr);
    gen a(args._VECTptr->front()),b(args._VECTptr->back()),u,v,d;
    if (!is_integral(a) || !is_integral(b))
      return gentypeerr(contextptr);
    egcd(a,b,u,v,d);
    return makevecteur(u,v,d);
  }
  static const char _iegcd_s []="iegcd";
  static define_unary_function_eval (__iegcd,&_iegcd,_iegcd_s);
  define_unary_function_ptr5( at_iegcd ,alias_at_iegcd,&__iegcd,0,true);


  gen symb_equal(const gen & a,const gen & b){
    return symbolic(at_equal,makesequence(a,b));
  }
  static string printasequal(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if (python_compat(contextptr))
      return "equal("+feuille.print(contextptr)+")";
#ifdef GIAC_HAS_STO_38
    return printsommetasoperator(feuille," = ",contextptr);
#else
    return printsommetasoperator(feuille,"=",contextptr);
#endif
  }
  gen _equal(const gen & a,GIAC_CONTEXT){
    if ( a.type==_STRNG && a.subtype==-1) return  a;
    if (a.type!=_VECT || a._VECTptr->size()<2)
      return equal(a,gen(vecteur(0),_SEQ__VECT),contextptr);
    if (a._VECTptr->size()==2)
      return equal( (*(a._VECTptr))[0],(*(a._VECTptr))[1],contextptr );
    if (a.subtype==_SEQ__VECT && calc_mode(contextptr)==1)
      return symb_equal(a._VECTptr->front(),gen(vecteur(a._VECTptr->begin()+1,a._VECTptr->end()),a.subtype));
    return equal(gen(vecteur(a._VECTptr->begin(),a._VECTptr->end()-1),a.subtype),a._VECTptr->back(),contextptr);
  }
  static const char _equal_s []="=";
  static define_unary_function_eval4_index (80,__equal,&_equal,_equal_s,&printasequal,&texprintsommetasoperator);
  define_unary_function_ptr( at_equal ,alias_at_equal ,&__equal);

  gen _equal2(const gen & a,GIAC_CONTEXT){
    if ( a.type==_STRNG && a.subtype==-1) return  a;
    if ((a.type!=_VECT) || (a._VECTptr->size()!=2))
      return equal2(a,gen(vecteur(0),_SEQ__VECT),contextptr);
    return equal2( (*(a._VECTptr))[0],(*(a._VECTptr))[1],contextptr);
  }
  static const char _equal2_s []="%=";
  static define_unary_function_eval4_index (168,__equal2,&_equal2,_equal2_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_equal2 ,alias_at_equal2 ,&__equal2);

  static string printassame(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
#if 0
    if (xcas_mode(contextptr) > 0)
      return printsommetasoperator(feuille," = ",contextptr);
    else
#endif
      return "("+printsommetasoperator(feuille,sommetstr,contextptr)+")";
  }
  gen symb_same(const gen & a,const gen & b){
    return symbolic(at_same,makesequence(a,b));
  }
  gen symb_same(const gen & a){
    return symbolic(at_same,a);
  }
  bool same_warning=true;
  gen _same(const gen & a,GIAC_CONTEXT){
    if ( a.type==_STRNG && a.subtype==-1) return  a;
    if ((a.type!=_VECT) || (a._VECTptr->size()!=2))
      return symb_same(a);
    gen res=undef;
    if (a._VECTptr->front().type==_SYMB || a._VECTptr->back().type==_SYMB){
      if (!is_inf(a._VECTptr->front()) && !is_undef(a._VECTptr->front()) && !is_inf(a._VECTptr->back()) && !is_undef(a._VECTptr->back()) && a._VECTptr->front().type!=_VECT &&a._VECTptr->back().type!=_VECT ){
	if (same_warning){
	  string s=autosimplify(contextptr);
	  if (unlocalize(s)!="'simplify'"){
	    *logptr(contextptr) << gettext("Warning, the test a==b is performed by checking\nthat the internal representation of ") << s << gettext("(a-b) is not 0.\nTherefore a==b may return false even if a and b are mathematically equal,\nif they have different internal representations.\nYou can explicitly call a simplification function like simplify(a-b)==0 to avoid this.") << endl;
	    same_warning=false;
	  }
	}
	res=add_autosimplify(a._VECTptr->front()-a._VECTptr->back(),contextptr);
	if (res.type==_SYMB)
	  res=res._SYMBptr->sommet(res._SYMBptr->feuille,contextptr);
	res=is_zero(res,contextptr);
      }
    }
    if (is_undef(res))
      res=operator_equal(a._VECTptr->front(),a._VECTptr->back(),contextptr);
    if (res.type==_INT_
	// && abs_calc_mode(contextptr)!=38
	)
      res.subtype=_INT_BOOLEAN;
    return res;
  }
  static const char _same_s []="==";
  static define_unary_function_eval4_index (148,__same,&_same,_same_s,&printassame,&texprintsommetasoperator);
  define_unary_function_ptr( at_same ,alias_at_same ,&__same);

  // ******************
  // Arithmetic functions
  // *****************

  // gen symb_smod(const gen & a,const gen & b){ return symbolic(at_smod,makevecteur(a,b));  }
  gen _smod(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (!check_2d_vecteur(args)) return gensizeerr(contextptr);
    vecteur & v=*args._VECTptr;
    if (ckmatrix(v))
      return apply(v[0],v[1],smod);
    if (!is_cinteger(v.back()) )
      return v.front()-v.back()*_round(v.front()/v.back(),contextptr);
    return smod(args._VECTptr->front(),args._VECTptr->back());
  }
  static const char _smod_s []="smod";
  static define_unary_function_eval (__smod,&_smod,_smod_s);
  define_unary_function_ptr5( at_smod ,alias_at_smod,&__smod,0,true);

  gen unmod(const gen & g){
    if (g.type==_MOD)
      return *g._MODptr;
    if (g.type==_VECT)
      return apply(g,unmod);
    if (g.type==_SYMB){
      if (g._SYMBptr->sommet==at_normalmod)
	return g._SYMBptr->feuille[0];
      return symbolic(g._SYMBptr->sommet,unmod(g._SYMBptr->feuille));
    }
    return g;
  }
  gen unmodunprod(const gen & g){
    gen h=unmod(g);
    if (h.is_prod())
      h=_prod(h._SYMBptr->feuille,context0); // ok
    return h;
  }

  gen irem(const gen & a,const gen & b){
    gen q;
    return irem(a,b,q);
  }
  // gen symb_irem(const gen & a,const gen & b){    return symbolic(at_irem,makevecteur(a,b));  }
  gen _normalmod(const gen & g,GIAC_CONTEXT);
  gen _irem(const gen & args,GIAC_CONTEXT){
    if (args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type==_VECT && args._VECTptr->size()==2 && args._VECTptr->front().type==_INT_ && args._VECTptr->back().type==_INT_){
      int a=args._VECTptr->front().val,b=args._VECTptr->back().val;
      a %= b ;
      a -= (a>>23)*b;
      return a;
    }
    if (args.type==_VECT && args._VECTptr->size()>1 && args._VECTptr->front().type==_STRNG){
      vecteur v=*args._VECTptr;
      const char * fmt=v.front()._STRNGptr->c_str();
      char buf[256];
      size_t s=v.size();
      if (s==2){
	switch (v[1].type){
	case _INT_:
	  sprintf(buf,fmt,v[1].val);
	  break;
	case _DOUBLE_:
	  sprintf(buf,fmt,v[1]._DOUBLE_val);
	  break;
	case _STRNG:
	  sprintf(buf,fmt,v[1]._STRNGptr->c_str());
	  break;
	default:
	  return gentypeerr(contextptr);
	}
	return string2gen(buf,false);
      }
      if (s==3){
	unsigned t=(v[1].type<< _DECALAGE) | v[2].type;
	switch (t){
	case _INT___INT_:
	  sprintf(buf,fmt,v[1].val,v[2].val);
	  break;
	case _INT___DOUBLE_:
	  sprintf(buf,fmt,v[1].val,v[2]._DOUBLE_val);
	  break;
	case _INT___STRNG:
	  sprintf(buf,fmt,v[1].val,v[2]._STRNGptr->c_str());
	  break;
	case _DOUBLE___INT_:
	  sprintf(buf,fmt,v[1]._DOUBLE_val,v[2].val);
	  break;
	case _DOUBLE___DOUBLE_:
	  sprintf(buf,fmt,v[1]._DOUBLE_val,v[2]._DOUBLE_val);
	  break;
	case _DOUBLE___STRNG:
	  sprintf(buf,fmt,v[1]._DOUBLE_val,v[2]._STRNGptr->c_str());
	  break;
	case _STRNG__INT_:
	  sprintf(buf,fmt,v[1]._STRNGptr->c_str(),v[2].val);
	  break;
	case _STRNG__DOUBLE_:
	  sprintf(buf,fmt,v[1]._STRNGptr->c_str(),v[2]._DOUBLE_val);
	  break;
	case _STRNG__STRNG:
	  sprintf(buf,fmt,v[1]._STRNGptr->c_str(),v[2]._STRNGptr->c_str());
	  break;
	default:
	  return gentypeerr(contextptr);
	}
	return string2gen(buf,false);
      }
      if (s==4){
	gen v1=evalf_double(v[1],1,contextptr);
	if (v1.type!=_DOUBLE_ && v1.type!=_STRNG) return gentypeerr(contextptr);
	gen v2=evalf_double(v[2],1,contextptr);
	if (v2.type!=_DOUBLE_ && v2.type!=_STRNG) return gentypeerr(contextptr);
	gen v3=evalf_double(v[3],1,contextptr);
	if (v3.type!=_DOUBLE_ && v3.type!=_STRNG) return gentypeerr(contextptr);
	if (v1.type==_DOUBLE_){
	  if (v2.type==_DOUBLE_){
	    if (v3.type==_DOUBLE_)
	      sprintf(buf,fmt,v1._DOUBLE_val,v2._DOUBLE_val,v3._DOUBLE_val);
	    else
	      sprintf(buf,fmt,v1._DOUBLE_val,v2._DOUBLE_val,v3._STRNGptr->c_str());
	  }
	  else {
	    if (v3.type==_DOUBLE_)
	      sprintf(buf,fmt,v1._DOUBLE_val,v2._STRNGptr->c_str(),v3._DOUBLE_val);
	    else
	      sprintf(buf,fmt,v1._DOUBLE_val,v2._STRNGptr->c_str(),v3._STRNGptr->c_str());	    
	  }
	} else {
	  if (v2.type==_DOUBLE_){
	    if (v3.type==_DOUBLE_)
	      sprintf(buf,fmt,v1._STRNGptr->c_str(),v2._DOUBLE_val,v3._DOUBLE_val);
	    else
	      sprintf(buf,fmt,v1._STRNGptr->c_str(),v2._DOUBLE_val,v3._STRNGptr->c_str());
	  }
	  else {
	    if (v3.type==_DOUBLE_)
	      sprintf(buf,fmt,v1._STRNGptr->c_str(),v2._STRNGptr->c_str(),v3._DOUBLE_val);
	    else
	      sprintf(buf,fmt,v1._STRNGptr->c_str(),v2._STRNGptr->c_str(),v3._STRNGptr->c_str());	    
	  }
	}
	return string2gen(buf,false);	
      }
      return gendimerr(contextptr);
    }
    if (!check_2d_vecteur(args)) return gensizeerr(contextptr);
    if (ckmatrix(args))
      return apply(args._VECTptr->front(),args._VECTptr->back(),irem);
    gen q;
    vecteur & v=*args._VECTptr;
    if (v.front().type==_SYMB){
      gen arg=v.front()._SYMBptr->feuille;
      if (v.front()._SYMBptr->sommet==at_pow && arg.type==_VECT && arg._VECTptr->size()==2 ){
	if (is_integer(arg._VECTptr->front()) && is_integer(arg._VECTptr->back()) )
	  return powmod(_irem(makesequence(arg._VECTptr->front(),v.back()),contextptr),arg._VECTptr->back(),v.back());
	return pow(_irem(makesequence(arg._VECTptr->front(),v.back()),contextptr),arg._VECTptr->back(),contextptr);
      }
      if (v.front()._SYMBptr->sommet==at_neg)
	return _irem(makesequence(simplifier((v.back()-1)*arg,contextptr),v.back()),contextptr);
      if (v.front()._SYMBptr->sommet==at_prod || v.front()._SYMBptr->sommet==at_plus){
	return v.front()._SYMBptr->sommet(_irem(makesequence(arg,v.back()),contextptr),contextptr);
      }
      if (v.front()._SYMBptr->sommet==at_inv){
	gen g=invmod(arg,v.back());
	if (is_positive(g,contextptr))
	  return g;
	else
	  return g+v.back();
      }
      arg=_normalmod(makevecteur(arg,v.back()),contextptr);
      return unmod(v.front()._SYMBptr->sommet(arg,contextptr));
    }
    if (v.front().type==_FRAC){
      gen g=invmod(v.front()._FRACptr->den,v.back());
      if (!is_positive(g,contextptr))
	g= g+v.back();
      return _irem(makesequence(v.front()._FRACptr->num*g,v.back()),contextptr);
    }
    if (v.front().type==_VECT){
      const_iterateur it=v.front()._VECTptr->begin(),itend=v.front()._VECTptr->end();
      vecteur res;
      for (;it!=itend;++it)
	res.push_back(_irem(makesequence(*it,v.back()),contextptr));
      return gen(res,v.front().subtype);
    }
    if (v.front().type==_IDNT)
      return v.front();
    gen vf(v.front()),vb(v.back());
    if (!is_integral(vf) || !is_integral(vb) ){
      return vf-_floor(vf/vb,contextptr)*vb;
    }
    gen r=irem(vf,vb,q);
    if (is_integer(vb) && is_strictly_positive(-r,contextptr)){
      if (is_strictly_positive(vb,contextptr)){
	r = r + vb;
	q=q-1;
      }
      else {
	r = r - vb;
	q=q+1;
      }
    }
    return r;
  }
  static const char _irem_s []="irem";
  static string printasirem(const gen & g,const char * s,GIAC_CONTEXT){
    if (python_compat(contextptr) && g.type==_VECT && g._VECTptr->size()==2)
      return g._VECTptr->front().print(contextptr)+" % "+g._VECTptr->back().print(contextptr);
    return s+("("+g.print(contextptr)+")");
  }  
  static define_unary_function_eval2 (__irem,&_irem,_irem_s,printasirem);
  define_unary_function_ptr5( at_irem ,alias_at_irem,&__irem,0,true);

  static const char _mods_s []="mods";
  static define_unary_function_eval (__mods,&_smod,_mods_s);
  define_unary_function_ptr5( at_mods ,alias_at_mods,&__mods,0,true);

  gen _quote_pow(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT || args._VECTptr->size()!=2)
      return gentypeerr(contextptr);
    vecteur & v = *args._VECTptr;
    if (ckmatrix(v.front()))
      return pow(v.front(),v.back(),contextptr);
    return symb_pow(args);
  }
  static const char _quote_pow_s []="&^";
  static define_unary_function_eval4_index (120,__quote_pow,&_quote_pow,_quote_pow_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_quote_pow ,alias_at_quote_pow ,&__quote_pow);

  // gen symb_iquo(const gen & a,const gen & b){ return symbolic(at_iquo,makevecteur(a,b));  }
  bool is_integral(gen & indice){
    if (is_cinteger(indice))
      return true;
#if 0
    if (indice.type==_FLOAT_){
      gen tmp=get_int(indice._FLOAT_val);
      if (is_zero(tmp-indice)){
	indice=tmp;
	return true;
      }
    }
#endif
    if (indice.type==_DOUBLE_){
      gen tmp=int(indice._DOUBLE_val);
      if (is_zero(tmp-indice)){
	indice=tmp;
	return true;
      }
    }
    return false;
  }
  gen Iquo(const gen & f0,const gen & b0){
    if (f0.type==_VECT)
      return apply1st(f0,b0,Iquo);
    gen f(f0),b(b0);
    // if (!is_integral(f) || !is_integral(b) ) return gensizeerr(gettext("Iquo")); // return symbolic(at_iquo,args);
    if (is_exactly_zero(b))
      return 0;
    return (f-_irem(makesequence(f,b),context0))/b; // ok
  }
  gen _iquo(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (!check_2d_vecteur(args)) return gensizeerr(contextptr);
    gen & f=args._VECTptr->front();
    gen & b=args._VECTptr->back();
    if (ckmatrix(args))
      return apply(f,b,iquo);
    return Iquo(f,b);
  }
  static const char _iquo_s []="iquo";
  static string printasiquo(const gen & g,const char * s,GIAC_CONTEXT){
    if (python_compat(contextptr) && g.type==_VECT && g._VECTptr->size()==2)
      return g._VECTptr->front().print(contextptr)+" // "+g._VECTptr->back().print(contextptr);
    return s+("("+g.print(contextptr)+")");
  }  
  static define_unary_function_eval2 (__iquo,&_iquo,_iquo_s,printasiquo);
  define_unary_function_ptr5( at_iquo ,alias_at_iquo,&__iquo,0,true);

  static vecteur iquorem(const gen & a,const gen & b){
    gen q,r;
    //r=irem(a,b,q);
    r=_irem(makesequence(a,b),context0);
    q=(a-r)/b;
    return makevecteur(q,r);
  }
  // gen symb_iquorem(const gen & a,const gen & b){    return symbolic(at_iquorem,makevecteur(a,b));  }
  gen _iquorem(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (!check_2d_vecteur(args)) return gensizeerr(contextptr);
    vecteur v=*args._VECTptr;
    if (!is_integral(v.front()) || !is_integral(v.back()) )
      return gensizeerr(contextptr); // symbolic(at_iquorem,args);
    return iquorem(args._VECTptr->front(),args._VECTptr->back());
  }
  static const char _iquorem_s []="iquorem";
  static define_unary_function_eval (__iquorem,&_iquorem,_iquorem_s);
  define_unary_function_ptr5( at_iquorem ,alias_at_iquorem,&__iquorem,0,true);

  gen _divmod(const gen & args,GIAC_CONTEXT){
    gen res=_iquorem(args,contextptr);
    if (res.type==_VECT) res.subtype=_SEQ__VECT;
    return res;
  }
  static const char _divmod_s []="divmod";
  static define_unary_function_eval (__divmod,&_divmod,_divmod_s);
  define_unary_function_ptr5( at_divmod ,alias_at_divmod,&__divmod,0,true);

  static gen symb_quorem(const gen & a,const gen & b){    return symbolic(at_quorem,makevecteur(a,b));  }
  gen quorem(const gen & a,const gen & b){
    if ((a.type!=_VECT) || (b.type!=_VECT))
      return symb_quorem(a,b);
    if (b._VECTptr->empty())
      return gensizeerr(gettext("Division by 0"));
    vecteur q,r;
    environment * env=new environment;
    DivRem(*a._VECTptr,*b._VECTptr,env,q,r,true);
    delete env;
    return makevecteur(gen(q,_POLY1__VECT),gen(r,_POLY1__VECT));
  }
  gen _quorem(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if ((args.type!=_VECT) || (args._VECTptr->size()<2) )
      return gensizeerr(contextptr);
#ifndef XLIGHT
    if (args.type==_VECT && args._VECTptr->size()>=3 && args[2].type==_VECT){
      vecteur v = *args._VECTptr;
      v.push_back(at_quo);
      return _revlist(_greduce(gen(v,_SEQ__VECT),contextptr),contextptr);
    }
#endif
    vecteur & a =*args._VECTptr;
    if ( (a.front().type==_VECT) && (a[1].type==_VECT))
      return quorem(a.front(),a[1]);
    if ( (a.front().type==_POLY) && (a[1].type==_POLY)){
      int dim=a.front()._POLYptr->dim;
      if (a[1]._POLYptr->dim!=dim)
	return gendimerr(contextptr);
      // Possible improvement? compute quotient of a and b using heap division
      // then a-b*q with array multiplication instead of univariate conversion
      if (a.size()==3 && a.back().type==_INT_){
	polynome rem,quo;
	if ( !divrem1(*a.front()._POLYptr,*a[1]._POLYptr,quo,rem,args._VECTptr->back().val) )
	  return gensizeerr(gettext("Unable to divide, perhaps due to rounding error")+a.front().print(contextptr)+" / "+a.back().print(contextptr));
	return makevecteur(quo,rem);
      }
      vecteur aa(polynome2poly1(*a.front()._POLYptr,1));
      vecteur bb(polynome2poly1(*a.back()._POLYptr,1));
      vecteur q,r;
      DivRem(aa,bb,0,q,r);
      return makevecteur(poly12polynome(q,1,dim),poly12polynome(r,1,dim));
    }
    vecteur lv;
    if (a.size()>=3 && a[2].type!=_INT_)
      lv=vecteur(1,unmodunprod(a[2]));
    else
      lv=vecteur(1,vx_var());
    lvar(args,lv);
    gen aa=e2r(a[0],lv,contextptr),aan,aad,bb=e2r(a[1],lv,contextptr),bbn,bbd;
    fxnd(aa,aan,aad);
    if ( (aad.type==_POLY) && (aad._POLYptr->lexsorted_degree() ) )
      return gensizeerr(contextptr);
    fxnd(bb,bbn,bbd);
    if ( (bbd.type==_POLY) && (bbd._POLYptr->lexsorted_degree() ) )
      return gensizeerr(contextptr);
    gen u,v;
    gen ad(r2e(aad,lv,contextptr));
    if ( (aan.type==_POLY) && (bbn.type==_POLY) ){
      if (a.size()>=3 && a.back().type==_INT_){
	polynome rem,quo;
	if ( !divrem1(*aan._POLYptr,*bbn._POLYptr,quo,rem,args._VECTptr->back().val) )
	  return gensizeerr(gettext("Unable to divide, perhaps due to rounding error")+aan.print(contextptr)+" / "+bbn.print(contextptr));
	u=rdiv(r2e(bbd,lv,contextptr),ad,contextptr)*r2e(quo,lv,contextptr);
	v=inv(ad,contextptr)*r2e(rem,lv,contextptr);
	return makevecteur(u,v);
      }
      vecteur aav(polynome2poly1(*aan._POLYptr,1)),bbv(polynome2poly1(*bbn._POLYptr,1)),un,vn;
      environment env;
      DivRem(aav,bbv,&env,un,vn);
      vecteur lvprime(lv.begin()+1,lv.end());
      u=rdiv(r2e(bbd,lv,contextptr),ad,contextptr)*symb_horner(*r2e(un,lvprime,contextptr)._VECTptr,lv.front());
      v=inv(ad,contextptr)*symb_horner(*r2e(vn,lvprime,contextptr)._VECTptr,lv.front());
      return makevecteur(u,v);
    }
    else {
      if ( (bbn.type!=_POLY) || !bbn._POLYptr->lexsorted_degree() ){
	u=rdiv(aan,bbn,contextptr);
	v=zero;
      }
      else {
	u=zero;
	v=aan;
      }
    }
    // aan=u*bbn+v -> aan/aad=u*bbd/aad * bbn/bbd +v/aad
    u=r2e(u*bbd,lv,contextptr);
    v=r2e(v,lv,contextptr);
    return makevecteur(rdiv(u,ad,contextptr),rdiv(v,ad,contextptr));
  }
  static const char _quorem_s []="quorem";
  static define_unary_function_eval (__quorem,&_quorem,_quorem_s);
  define_unary_function_ptr5( at_quorem ,alias_at_quorem,&__quorem,0,true);

  // gen symb_quo(const gen & a,const gen & b){    return symbolic(at_quo,makevecteur(a,b));  }
  gen _quo(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
#ifndef XLIGHT
    if (args.type==_VECT && args._VECTptr->size()>=3 && args[2].type==_VECT){
      vecteur v = *args._VECTptr;
      v.push_back(at_quo);
      return _greduce(gen(v,_SEQ__VECT),contextptr)[1];
    }
#endif
    return _quorem(args,contextptr)[0];
  }
  static const char _quo_s []="quo";
  static define_unary_function_eval (__quo,&_quo,_quo_s);
  define_unary_function_ptr5( at_quo ,alias_at_quo,&__quo,0,true);

  // gen symb_rem(const gen & a,const gen & b){    return symbolic(at_rem,makevecteur(a,b));  }
  gen _rem(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
#ifndef XLIGHT
    if (args.type==_VECT && args._VECTptr->size()>=3 && args[2].type==_VECT){
      vecteur v = *args._VECTptr;
#if 0 
	 gen g(_WITH_COCOA);
	 g.subtype=_INT_GROEBNER;
	 v.push_back(symb_equal(g,0));
#endif
      return _greduce(gen(v,_SEQ__VECT),contextptr);
    }
#endif
    return _quorem(args,contextptr)[1];
  }
  static const char _rem_s []="rem";
  static define_unary_function_eval (__rem,&_rem,_rem_s);
  define_unary_function_ptr5( at_rem ,alias_at_rem,&__rem,0,true);

  gen double2gen(double d){
    if (my_isinf(d))
      return d;
    ref_mpz_t * m= new ref_mpz_t;
    mpz_set_d(m->z,d);
    return m;
  }
  static gen symb_floor(const gen & a){
    return symbolic(at_floor,a);
  }
  gen apply_unit(const gen & args,const gen_op_context & f,GIAC_CONTEXT){
    return symbolic(at_unit,gen(makevecteur(f(args._SYMBptr->feuille[0],contextptr),args._SYMBptr->feuille[1]),_SEQ__VECT));  
  }
  gen _floor(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (is_equal(args))
      return apply_to_equal(args,_floor,contextptr);
    if (is_inf(args)||is_undef(args))
      return args;
#ifdef WITH_UNITS
    if (args.is_symb_of_sommet(at_unit))
      return apply_unit(args,_floor,contextptr);
#endif
    if (args.is_symb_of_sommet(at_floor) || args.is_symb_of_sommet(at_ceil))
      return args;
    if (args.type==_VECT || args.type==_MAP)
      return apply(args,contextptr,_floor);
    if (args.type==_CPLX)
      return gen(_floor(*args._CPLXptr,contextptr),_floor(*(args._CPLXptr+1),contextptr));
    if ( (args.type==_INT_) || (args.type==_ZINT))
      return args;
    if (args.type==_FRAC){
      gen n=args._FRACptr->num,d=args._FRACptr->den;
      if (is_cinteger(d) && !is_integer(d)){
	n=n*conj(d,contextptr);
	d=d*conj(d,contextptr);
      }
      if (is_cinteger(n) && is_integer(d)){
	if (is_positive(args,contextptr))
	  return iquo(n,d);
	if (n.type!=_CPLX)
	  return iquo(n,d)-1;
	gen nr,ni;
	reim(n,nr,ni,contextptr);
	if (is_positive(nr,contextptr))
	  nr=iquo(nr,d);
	else
	  nr=iquo(nr,d)-1;
	if (is_positive(ni,contextptr))
	  ni=iquo(ni,d);
	else
	  ni=iquo(ni,d)-1;
	return gen(nr,ni);
      }
    }
    /* old code, changed for floor(sqrt(2))
    vecteur l(lidnt(args));
    vecteur lnew=*evalf(l,1,contextptr)._VECTptr;
    gen tmp=subst(args,l,lnew,false,contextptr);
    */
    vecteur l(lvar(args));
    gen chk;
    if (l.size()==2){
      if (l[0]==cst_pi)
	chk=l[1];
      if (l[1]==cst_pi)
	chk=l[0];
    }
    else {
      if (l.size()==1)
	chk=l[0];
    }
    gen a,b;
    if (chk.type==_IDNT && is_linear_wrt(args,chk,a,b,contextptr)){
      gen g2=chk._IDNTptr->eval(1,chk,contextptr);
      if ((g2.type==_VECT) && (g2.subtype==_ASSUME__VECT)){
	vecteur v=*g2._VECTptr;
	if ( (v.size()==3) && (v.front()==vecteur(0) || v.front()==_DOUBLE_ || v.front()==_ZINT || v.front()==_SYMB || v.front()==0) && (v[1].type==_VECT && v[1]._VECTptr->size()==1 && v[1]._VECTptr->front().type==_VECT) ){
	  vecteur v1=*v[1]._VECTptr->front()._VECTptr;
	  if (v1.size()==2){
	    gen A=a*v1[0]+b,B=a*v1[1]+b,Af,Bf;
	    Af=_floor(A,contextptr);
	    Bf=_floor(B,contextptr);
	    if (Af==Bf)
	      return Af;
	    if (Af==Bf+1 && is_zero(ratnormal(A-Af,contextptr)) && v[2].type==_VECT && equalposcomp(*v[2]._VECTptr,v1[0]))
	      return Bf;
	    if (Bf==Af+1 && is_zero(ratnormal(B-Bf,contextptr)) && v[2].type==_VECT){
	      if (equalposcomp(*v[2]._VECTptr,v1[1]))
		return Af;
	    }
	  }
	}
      }
    }
    vecteur lnew(l);
    int ls=int(l.size());
    for (int i=0;i<ls;i++){
      if (l[i].type==_IDNT || lidnt(l[i]).empty()){
	lnew[i]=evalf(l[i],1,contextptr);
#ifdef HAVE_LIBMPFR
	if (lnew[i].type==_DOUBLE_)
	  lnew[i]=accurate_evalf(lnew[i],100);
#endif
      }
    }
    gen tmp=subst(args,l,lnew,false,contextptr);
    if (tmp.type!=_DOUBLE_)
      return symb_floor(args);
    double dres=std::floor(tmp._DOUBLE_val);
    int ires=dres;
    if (ires==dres)
      return ires;
    else
      return dres;
  }
  static gen taylor_floor (const gen & lim_point,const int ordre,const unary_function_ptr & f, int direction,gen & shift_coeff,GIAC_CONTEXT){
    if (ordre<0)
      return 0; // no symbolic preprocessing
    shift_coeff=0;
    gen l=_floor(lim_point,contextptr);
    if (l==lim_point){
      if (direction==0)
	return gensizeerr(gettext("Taylor of floor with unsigned limit"));
      if (direction==-1)
	l=l-1;
    }
    return is_zero(l,contextptr)?vecteur(0):makevecteur(l);
  }
  static const char _floor_s []="floor";
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval_taylor( __floor,&_floor,(size_t)&D_at_signunary_function_ptr,&taylor_floor,_floor_s);
#else
  static define_unary_function_eval_taylor( __floor,&_floor,D_at_sign,&taylor_floor,_floor_s);
#endif
  define_unary_function_ptr5( at_floor ,alias_at_floor,&__floor,0,true);

  gen _ceil(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (is_inf(args)||is_undef(args))
      return args;
    if (args.type==_VECT || args.type==_MAP)
      return apply(args,contextptr,_ceil);
    if (args.is_symb_of_sommet(at_floor) || args.is_symb_of_sommet(at_ceil))
      return args;
    if (args.type==_CPLX)
      return gen(_ceil(*args._CPLXptr,contextptr),_ceil(*(args._CPLXptr+1),contextptr));
    if ( (args.type==_INT_) || (args.type==_ZINT))
      return args;
#ifdef BCD
    if (args.type==_FLOAT_)
      return fceil(args._FLOAT_val);
#endif
    return -_floor(-args,contextptr);
  }
  static gen taylor_ceil (const gen & lim_point,const int ordre,const unary_function_ptr & f, int direction,gen & shift_coeff,GIAC_CONTEXT){
    if (ordre<0)
      return 0; // no symbolic preprocessing
    shift_coeff=0;
    gen l=_ceil(lim_point,contextptr);
    if (l==lim_point){
      if (direction==0)
	return gensizeerr(gettext("Taylor of ceil with unsigned limit"));
      if (direction==1)
	l=l-1;
    }
    return is_zero(l,contextptr)?vecteur(0):makevecteur(l);
  }
  static const char _ceil_s []="ceil";
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval_taylor( __ceil,&_ceil,(size_t)&D_at_signunary_function_ptr,&taylor_ceil,_ceil_s);
#else
  static define_unary_function_eval_taylor( __ceil,&_ceil,D_at_sign,&taylor_ceil,_ceil_s);
#endif
  define_unary_function_ptr5( at_ceil ,alias_at_ceil,&__ceil,0,true);

  static gen ceiltofloor(const gen & g,GIAC_CONTEXT){
    return -symbolic(at_floor,-g);
  }
  gen ceil2floor(const gen & g,GIAC_CONTEXT,bool quotesubst){
    const vector< const unary_function_ptr *> ceil_v(1,at_ceil);
    const vector< gen_op_context > ceil2floor_v(1,ceiltofloor);
    return subst(g,ceil_v,ceil2floor_v,quotesubst,contextptr);
  }

  // static gen symb_round(const gen & a){    return symbolic(at_round,a);  }
  gen _round(const gen & args,GIAC_CONTEXT){
    if ( is_undef(args))
      return args;
    if (args.type==_STRNG && args.subtype==-1) return  args;
    if (is_equal(args))
      return apply_to_equal(args,_round,contextptr);
#ifdef WITH_UNITS
    if (args.is_symb_of_sommet(at_unit))
      return apply_unit(args,_round,contextptr);
#endif
    if (is_inf(args)||is_undef(args))
      return args;
    if (args.type==_VECT && (args.subtype!=_SEQ__VECT || args._VECTptr->size()!=2))
      return apply(args,contextptr,_round);
    if (args.type==_VECT && args.subtype==_SEQ__VECT){
      gen b=args._VECTptr->back();
      if (is_integral(b)){
#ifdef BCD
	if (args._VECTptr->front().type==_FLOAT_)
	  return fround(args._VECTptr->front()._FLOAT_val,b.val); 
#endif
	/*
#ifdef _SOFTMATH_H
	double d=std::giac_gnuwince_pow(10.0,double(b.val));
#else
	double d=std::pow(10.0,double(b.val));
#endif
	*/
	gen d=10.0;
	if (b.val<0){
	  gen gf=_floor(log10(abs(args._VECTptr->front(),contextptr),contextptr),contextptr); 
	  if (gf.type!=_INT_)
	    return gensizeerr(contextptr);
	  b=-1-b-gf;
	}
	if (b.val>14)
	  d=accurate_evalf(gen(10),int(b.val*3.32192809489+.5));
	d=pow(d,b.val,contextptr);
	gen e=_round(d*args._VECTptr->front(),contextptr);
	if (b.val>14)
	  e=accurate_evalf(e,int(b.val*3.32192809489+.5));
	e=rdiv(e,d,contextptr);
	return e;
      }
    }
    if (args.type==_CPLX)
      return gen(_round(*args._CPLXptr,contextptr),_round(*(args._CPLXptr+1),contextptr));
    gen r,i,tmp; 
    reim(args,r,i,contextptr);
    tmp=args+plus_one_half; // *(r.type<_POLY?sign(r,contextptr):1); 
    if (!is_zero(i))
      tmp=tmp+plus_one_half*cst_i; //  *(i.type<_POLY?sign(i,contextptr):plus_one);
    if (tmp.type==_VECT)
      tmp.subtype=args.subtype;
    return _floor(tmp,contextptr);
  }
  static gen taylor_round (const gen & lim_point,const int ordre,const unary_function_ptr & f, int direction,gen & shift_coeff,GIAC_CONTEXT){
    if (ordre<0)
      return 0; // no symbolic preprocessing
    shift_coeff=0;
    gen l=_round(lim_point,contextptr);
    if (is_zero(ratnormal(l-lim_point-plus_one_half,contextptr),contextptr)){
      if (direction==0)
	return gensizeerr(gettext("Taylor of round with unsigned limit"));
      if (direction==-1)
	l=l-1;
    }
    return is_zero(l,contextptr)?vecteur(0):makevecteur(l);
  }
  static const char _round_s []="round";
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval_taylor( __round,&_round,(size_t)&D_at_signunary_function_ptr,&taylor_round,_round_s);
#else
  static define_unary_function_eval_taylor( __round,&_round,D_at_sign,&taylor_round,_round_s);
#endif
  define_unary_function_ptr5( at_round ,alias_at_round,&__round,0,true);

  // static gen symb_print(const gen & a){    return symbolic(at_print,a);  }
  bool nl_sep(gen & tmp,string & nl,string & sep){
    if (tmp.type!=_VECT || tmp.subtype!=_SEQ__VECT)
      return false;
    vecteur v=*tmp._VECTptr;
    bool hasnl=false;
    for (size_t i=0;i<v.size();++i){
      if (v[i].is_equal()){
	gen f=v[i]._SYMBptr->feuille;
	if (f.type==_VECT && f._VECTptr->size()==2){
	  gen a=f._VECTptr->front();
	  gen b=f._VECTptr->back();
	  if (b.type==_STRNG && a.type==_IDNT){
	    if (strcmp("sep",a._IDNTptr->id_name)==0){
	      sep=*b._STRNGptr;
	      hasnl=true;
	      v.erase(v.begin()+i);
	      --i;
	    }
	    if (strcmp("endl",a._IDNTptr->id_name)==0){
	      nl=*b._STRNGptr;
	      hasnl=true;
	      v.erase(v.begin()+i);
	      --i;
	    }
	  }
	}
      }
    }
    if (hasnl){
      if (v.size()==1)
	tmp=v.front();
      else
	tmp=gen(v,tmp.subtype);
    }
    return hasnl;
  }
  gen _print(const gen & args,GIAC_CONTEXT){
#if 0
    gen tmp=args.eval(eval_level(contextptr),contextptr);
    *logptr(contextptr) << tmp << endl;
    return 1;
#else
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if ( debug_infolevel && (args.type==_IDNT) && args._IDNTptr->localvalue && (!args._IDNTptr->localvalue->empty()))
      *logptr(contextptr) << gettext("Local var protected ") << (*args._IDNTptr->localvalue)[args._IDNTptr->localvalue->size()-2].val << endl;
    gen tmp=args.eval(eval_level(contextptr),contextptr);
    string nl("\n"),sep(",");
    bool nlsep=nl_sep(tmp,nl,sep);
    // If giac used inside a console don't add to messages, since we print
    if (tmp.type==_VECT && !tmp._VECTptr->empty() && tmp._VECTptr->front()==gen("Unquoted",contextptr)){
      vecteur & v=*tmp._VECTptr;
      int s=int(v.size());
      for (int i=1;i<s;++i)
	*logptr(contextptr) << (v[i].type==_STRNG?(*v[i]._STRNGptr):unquote(v[i].print(contextptr)));
    }
    else {
      if (!nlsep && !python_compat(contextptr) && args.type==_IDNT)
	*logptr(contextptr) << args << ":";
      if (tmp.type==_STRNG)
	*logptr(contextptr) << tmp._STRNGptr->c_str() << nl;
      else {
	if (tmp.type==_VECT && tmp.subtype==_SEQ__VECT){
	  const vecteur & v=*tmp._VECTptr;
	  size_t s=v.size();
	  for (size_t i=0;i<s;){
	    *logptr(contextptr) << (v[i].type==_STRNG?(*v[i]._STRNGptr):unquote(v[i].print(contextptr)));
	    ++i;
	    if (i==s) break;
	    *logptr(contextptr) << sep;
	  }
	}
	else
	  *logptr(contextptr) << tmp;
	*logptr(contextptr) << nl;
      }
    }
    return 1; //__interactive.op(symbolic(at_print,tmp),contextptr);
#endif
  }
  static const char _print_s []="print";
#if 1 // def RTOS_THREADX
#ifdef NO_UNARY_FUNCTION_COMPOSE
  define_unary_function_eval(__print,&_print,_print_s);
#else
  static define_unary_function_eval(__print,&_print,_print_s);
#endif
#else
  const unary_function_eval __print(1,&_print,_print_s,&printasprint);
#endif
  define_unary_function_ptr5( at_print ,alias_at_print,&__print,_QUOTE_ARGUMENTS,true);

  // static gen symb_is_prime(const gen & a){    return symbolic(at_is_prime,a);  }
  gen _is_prime(const gen & args0,GIAC_CONTEXT){
    gen args(args0);
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    int certif=0;
#ifdef HAVE_LIBPARI
    if (args0.type==_VECT && args0.subtype==_SEQ__VECT && args0._VECTptr->size()==2 && args0._VECTptr->back().type==_INT_){
      args=args0._VECTptr->front();
      certif=args0._VECTptr->back().val;
    }
#endif
    if (args.type==_VECT)
      return apply(args,_is_prime,contextptr);
    if (!is_integral(args))
      return gentypeerr(contextptr);
#ifdef HAVE_LIBPARI
    return pari_isprime(args,certif);
#else
    return is_probab_prime_p(args);
#endif
  }
  static const char _is_prime_s []="is_prime";
  static define_unary_function_eval (__is_prime,&_is_prime,_is_prime_s);
  define_unary_function_ptr5( at_is_prime ,alias_at_is_prime,&__is_prime,0,true);

  gen _is_pseudoprime(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    return is_probab_prime_p(args);
  }
  static const char _is_pseudoprime_s []="is_pseudoprime";
  static define_unary_function_eval (__is_pseudoprime,&_is_pseudoprime,_is_pseudoprime_s);
  define_unary_function_ptr5( at_is_pseudoprime ,alias_at_is_pseudoprime,&__is_pseudoprime,0,true);

  gen nextprime1(const gen & a,GIAC_CONTEXT){
    if (is_strictly_greater(2,a,contextptr))
      return 2;
    return nextprime(a+1);
  }
  static const char _nextprime_s []="nextprime";
  static define_unary_function_eval (__nextprime,&nextprime1,_nextprime_s);
  define_unary_function_ptr5( at_nextprime ,alias_at_nextprime,&__nextprime,0,true);

  gen prevprime1(const gen & a,GIAC_CONTEXT){
    if (is_greater(2,a,contextptr))
      return gensizeerr(contextptr);
    return prevprime(a-1);
  }
  static const char _prevprime_s []="prevprime";
  static define_unary_function_eval (__prevprime,&prevprime1,_prevprime_s);
  define_unary_function_ptr5( at_prevprime ,alias_at_prevprime,&__prevprime,0,true);

  // static gen symb_jacobi_symbol(const gen & a,const gen & b){    return symbolic(at_jacobi_symbol,makevecteur(a,b));  }
  gen _jacobi_symbol(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (!check_2d_vecteur(args)) return gensizeerr(contextptr);
    gen a=args._VECTptr->front(),b=args._VECTptr->back();
    a=_irem(args,contextptr);
    int res=jacobi(a,b);
    if (res==-RAND_MAX)
      return gensizeerr(contextptr);
    return res;
  }
  static const char _jacobi_symbol_s []="jacobi_symbol";
  static define_unary_function_eval (__jacobi_symbol,&_jacobi_symbol,_jacobi_symbol_s);
  define_unary_function_ptr5( at_jacobi_symbol ,alias_at_jacobi_symbol,&__jacobi_symbol,0,true);

  // static gen symb_legendre_symbol(const gen & a,const gen & b){    return symbolic(at_legendre_symbol,makevecteur(a,b));  }
  gen _legendre_symbol(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (!check_2d_vecteur(args)) return gensizeerr(contextptr);
    gen a=args._VECTptr->front(),b=args._VECTptr->back();
    a=_irem(args,contextptr);
    return legendre(a,b);
  }
  static const char _legendre_symbol_s []="legendre_symbol";
  static define_unary_function_eval (__legendre_symbol,&_legendre_symbol,_legendre_symbol_s);
  define_unary_function_ptr5( at_legendre_symbol ,alias_at_legendre_symbol,&__legendre_symbol,0,true);

  // static gen symb_ichinrem(const gen & a,const gen & b){     return symbolic(at_ichinrem,makevecteur(a,b));  }

  gen ichinrem2(const gen  & a_orig,const gen & b_orig){
    gen a=a_orig;
    gen b=b_orig;
    if (a.type==_MOD)
      a=makevecteur(*a._MODptr,*(a._MODptr+1));
    if (b.type==_MOD)
      b=makevecteur(*b._MODptr,*(b._MODptr+1));
    vecteur l(lidnt(makevecteur(a,b)));
    if (l.empty()){
      if (!check_2d_vecteur(a)
	  || !check_2d_vecteur(b)) 
	return gensizeerr(gettext("Vector of 2 integer arguments expected"));
      vecteur & av=*a._VECTptr;
      vecteur & bv=*b._VECTptr;
      gen ab=av.back();
      gen bb=bv.back();
      gen aa=av.front();
      gen ba=bv.front();
      if (!is_integral(ab) || !is_integral(bb) || !is_integral(aa) || !is_integral(ba))
	return gentypeerr(gettext("Non integer argument"));
      if (is_greater(1,bb,context0) || is_greater(1,ab,context0))
	return gentypeerr(gettext("Bad mod value"));
      gen res=ichinrem(aa,ba,ab,bb);
      if (is_undef(res))
	return res;
      if (a_orig.type==_MOD)
	return makemod(res,lcm(ab,bb));
      return makevecteur(res,lcm(ab,bb));
    }
    l=lvar(a); lvar(b,l);    
    gen x=l.front();
    if (a.type!=_VECT || b.type!=_VECT ){
      // a and b are polynomial, must have the same degrees
      // build a new polynomial calling ichinrem2 on each element
      gen ax=_e2r(makevecteur(a_orig,x),context0),bx=_e2r(makevecteur(b_orig,x),context0); // ok
      if (ax.type!=_VECT || bx.type!=_VECT )
	return gensizeerr(gettext("ichinrem2 1"));
      int as=int(ax._VECTptr->size()),bs=int(bx._VECTptr->size());
      if (!as || !bs)
	return gensizeerr(gettext("Null polynomial"));
      while (as<bs){
	ax._VECTptr->insert(ax._VECTptr->begin(),0);
	++as;
      }
      while (bs<as){
	bx._VECTptr->insert(bx._VECTptr->begin(),0);
	++bs;
      }
      gen a0=ax._VECTptr->front(),b0=bx._VECTptr->front(),m,n;
      if (a0.type==_MOD)
	m=*(a0._MODptr+1);
      else
	return gensizeerr(gettext("Expecting modular coeff"));
      if (b0.type==_MOD)
	n=*(b0._MODptr+1);
      else
	return gensizeerr(gettext("Expecting modular coeff"));
      gen mn=lcm(m,n);
      const_iterateur it=ax._VECTptr->begin(),itend=ax._VECTptr->end(),jt=bx._VECTptr->begin();
      vecteur res;
      for (;as>bs;--as,++it){
	res.push_back(makemod(unmod(*it),mn));
      }
      for (;bs>as;--bs,++jt){
	res.push_back(makemod(unmod(*jt),mn));
      }
      for (;it!=itend;++it,++jt)
	res.push_back(ichinrem2(makemod(unmod(*it),m),makemod(unmod(*jt),n)));
      return _r2e(makesequence(res,x),context0); // ok
    }
    if (a.type==_VECT && a._VECTptr->size()==2 && b.type==_VECT && b._VECTptr->size()==2 ){
      // ax and bx are the polynomials, 
      gen ax=_e2r(makevecteur(a._VECTptr->front(),x),context0),bx=_e2r(makevecteur(b._VECTptr->front(),x),context0); // ok
      if (ax.type!=_VECT || bx.type!=_VECT )
	return gensizeerr(gettext("ichinrem2 2"));
      gen m=a._VECTptr->back(),n=b._VECTptr->back(),mn=lcm(m,n);
      int as=int(ax._VECTptr->size()),bs=int(bx._VECTptr->size());
      const_iterateur it=ax._VECTptr->begin(),itend=ax._VECTptr->end(),jt=bx._VECTptr->begin();
      vecteur res;
      for (;as>bs;--as,++it){
	gen tmp=ichinrem2(makevecteur(*it,m),makevecteur(0,n));
	if (tmp.type!=_VECT)
	  return gensizeerr(gettext("ichinrem2 3"));
	res.push_back(tmp._VECTptr->front());
      }
      for (;bs>as;--bs,++jt){
	gen tmp=ichinrem2(makevecteur(0,m),makevecteur(*jt,n));
	if (tmp.type!=_VECT)
	  return gensizeerr(gettext("ichinrem2 3"));
	res.push_back(tmp._VECTptr->front());
      }
      for (;it!=itend;++it,++jt){
	gen tmp=ichinrem2(makevecteur(*it,m),makevecteur(*jt,n));
	if (tmp.type!=_VECT)
	  return gensizeerr(gettext("ichinrem2 3"));
	res.push_back(tmp._VECTptr->front());
      }
      if (a_orig.type==_MOD)
	return makemod(_r2e(makesequence(res,x),context0),mn); // ok
      return makevecteur(_r2e(makesequence(res,x),context0),m*n); // ok
    }
    return gensizeerr(gettext("ichinrem2 4"));
  }
  gen _ichinrem(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return gentypeerr(gettext("[a % p, b % q,...]"));
    vecteur & v = *args._VECTptr;
    int s=int(v.size());
    if (s<2)
      return gendimerr(contextptr);
    if (is_integer(v[0]) && is_integer(v[1]))
      return v;
    gen res=ichinrem2(v[0],v[1]);
    for (int i=2;i<s;++i)
      res=ichinrem2(res,v[i]);
    if (res.type==_VECT && res._VECTptr->size()==2 && is_integer(res._VECTptr->front()) && is_integer(res._VECTptr->back()))
      res._VECTptr->front()=_irem(makesequence(res._VECTptr->front()+res._VECTptr->back(),res._VECTptr->back()),contextptr);
    return res;
  }
  static const char _ichinrem_s []="ichinrem";
  static define_unary_function_eval (__ichinrem,&_ichinrem,_ichinrem_s);
  define_unary_function_ptr5( at_ichinrem ,alias_at_ichinrem,&__ichinrem,0,true);
  
  gen _fracmod(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if ( (args.type!=_VECT) || (args._VECTptr->size()!=2))
      return symbolic(at_fracmod,args);
    vecteur & v=*args._VECTptr;
    return fracmod(v[0],v[1]);
  }
  static const char _fracmod_s []="fracmod";
  static define_unary_function_eval (__fracmod,&_fracmod,_fracmod_s);
  define_unary_function_ptr5( at_fracmod ,alias_at_fracmod,&__fracmod,0,true);
  
  static const char _iratrecon_s []="iratrecon"; // maple name, fracmod takes only 2 arg
  static define_unary_function_eval (__iratrecon,&_fracmod,_iratrecon_s);
  define_unary_function_ptr5( at_iratrecon ,alias_at_iratrecon,&__iratrecon,0,true);
  
  // static gen symb_chinrem(const gen & a,const gen & b){    return symbolic(at_chinrem,makevecteur(a,b));  }
  static vecteur polyvect(const gen & a,const vecteur & v){
    if (a.type==_POLY)
      return polynome2poly1(*a._POLYptr,1);
    return vecteur(1,a);
  }
  gen _chinrem(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if ( (args.type!=_VECT) || (args._VECTptr->size()<2) )
      return gensizeerr(contextptr);
    gen a=args._VECTptr->front();
    gen b=(*args._VECTptr)[1];
    if (!check_2d_vecteur(a) ||
	!check_2d_vecteur(b) )
      return gensizeerr(contextptr);
    if ((a._VECTptr->front().type!=_VECT) || (a._VECTptr->back().type!=_VECT) || (b._VECTptr->front().type!=_VECT) || (b._VECTptr->back().type!=_VECT) ){
      vecteur lv;
      if (args._VECTptr->size()==3)
	lv=vecteur(1,(*args._VECTptr)[2]);
      else
	lv=vecteur(1,vx_var());
      lvar(args,lv);
      vecteur lvprime(lv.begin()+1,lv.end());
      gen aa=e2r(a,lv,contextptr),bb=e2r(b,lv,contextptr),aan,aad,bbn,bbd;
      fxnd(aa,aan,aad);
      if (aad.type==_POLY){
	if (aad._POLYptr->lexsorted_degree() )
	  return gensizeerr(contextptr);
	else
	  aad=aad._POLYptr->trunc1();
      }
      fxnd(bb,bbn,bbd);
      if (bbd.type==_POLY){
	if (bbd._POLYptr->lexsorted_degree() )
	  return gensizeerr(contextptr);
	else
	  bbd=bbd._POLYptr->trunc1();
      }
      vecteur & aanv=*aan._VECTptr;
      vecteur & bbnv=*bbn._VECTptr;
      aanv[0]=polyvect(aanv[0],lv)/aad;
      aanv[1]=polyvect(aanv[1],lv);
      bbnv[0]=polyvect(bbnv[0],lv)/bbd;
      bbnv[1]=polyvect(bbnv[1],lv);
      gen tmpg=_chinrem(makevecteur(aanv,bbnv),contextptr);
      if (is_undef(tmpg)) return tmpg;
      vecteur res=*tmpg._VECTptr;
      // convert back
      res[0]=symb_horner(*r2e(res[0],lvprime,contextptr)._VECTptr,lv.front());
      res[1]=symb_horner(*r2e(res[1],lvprime,contextptr)._VECTptr,lv.front());
      return res;
    }
    modpoly produit=(*a._VECTptr->back()._VECTptr)**b._VECTptr->back()._VECTptr;
    return makevecteur(gen(chinrem(*a._VECTptr->front()._VECTptr,*b._VECTptr->front()._VECTptr,*a._VECTptr->back()._VECTptr,*b._VECTptr->back()._VECTptr,0),_POLY1__VECT),gen(produit,_POLY1__VECT));    
  }
  static const char _chinrem_s []="chinrem";
  static define_unary_function_eval (__chinrem,&_chinrem,_chinrem_s);
  define_unary_function_ptr5( at_chinrem ,alias_at_chinrem,&__chinrem,0,true);

  static string printasfactorial(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if (feuille.type==_IDNT || ((feuille.type<=_DOUBLE_ ) && is_positive(feuille,contextptr)))
      return feuille.print(contextptr)+"!";
    return "("+feuille.print(contextptr)+")!";
  }
  static gen d_factorial(const gen & args,GIAC_CONTEXT){
    return Psi(args+1,0)*_factorial(args,contextptr);
  }
  define_partial_derivative_onearg_genop( D_at_factorial," D_at_factorial",&d_factorial);
  gen _factorial(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type==_VECT)
      return apply(args,_factorial,contextptr);
    gen tmp=evalf_double(args,1,contextptr);
    if (tmp.type>=_IDNT)
      return symbolic(at_factorial,args);
    if (args.type!=_INT_)
      return Gamma(args+1,contextptr);
    if (args.val<0)
      return unsigned_inf;
    return factorial((unsigned long int) args.val);
  }
  static const char _factorial_s []="factorial";
  static define_unary_function_eval5 (__factorial,&_factorial,D_at_factorial,_factorial_s,&printasfactorial,0);
  define_unary_function_ptr5( at_factorial ,alias_at_factorial,&__factorial,0,true);

  gen double_is_int(const gen & g,GIAC_CONTEXT){
    gen f=_floor(g,contextptr);
#if 0
    if (f.type==_FLOAT_)
      f=get_int(f._FLOAT_val);
#endif
    gen f1=evalf(g-f,1,contextptr);
    if ( (f1.type==_DOUBLE_ && fabs(f1._DOUBLE_val)<epsilon(contextptr))
	 //|| (f1.type==_FLOAT_ && fabs(f1._FLOAT_val)<epsilon(contextptr))
	 )
      return f;
    else
      return g;
  }
  gen comb(const gen & a_orig,const gen &b_orig,GIAC_CONTEXT){
    gen a=double_is_int(a_orig,contextptr);
    gen b=double_is_int(b_orig,contextptr);
    if (a.type!=_INT_ || b.type!=_INT_)
      return Gamma(a+1,contextptr)/Gamma(b+1,contextptr)/Gamma(a-b+1,contextptr);
    if (a.val<0 || b.val<0){
      *logptr(contextptr) << "comb with negative argument " << a << "," << b <<endl;
      //return gensizeerr(contextptr);
    }
    return comb((unsigned long int) a.val,(unsigned long int) b.val);
  }
  gen _comb(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (ckmatrix(args))
      return apply(args._VECTptr->front(),args._VECTptr->back(),contextptr,comb);
    if ( (args.type!=_VECT) || (args._VECTptr->size()!=2))
      return gentypeerr(contextptr);
    vecteur & v=*args._VECTptr;
    if (v.front().type!=_INT_ || v.back().type!=_INT_)
      return comb(v.front(),v.back(),contextptr); 
    if (v.front().val<0){
      int n=v.front().val;
      int k=v.back().val;
      if (k<0)
	return gensizeerr(contextptr);
      gen res=1;
      for (int i=0;i<k;++i){
	res=(n-i)*res;
      }
      return res/factorial(k);
    }
    if (v.front().val<v.back().val)
      return zero;
    return comb((unsigned long int) v.front().val,(unsigned long int) v.back().val);
  }
  static const char _comb_s []="comb";
  static define_unary_function_eval (__comb,&_comb,_comb_s);
  define_unary_function_ptr5( at_comb ,alias_at_comb,&__comb,0,true);

  gen perm(const gen & a,const gen &b){
    if (a.type!=_INT_ || b.type!=_INT_)
      return symbolic(at_perm,makesequence(a,b));
    return perm((unsigned long int) a.val,(unsigned long int) b.val);
  }
  gen _perm(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (ckmatrix(args))
      return apply(args._VECTptr->front(),args._VECTptr->back(),perm);
    if ( (args.type!=_VECT) || (args._VECTptr->size()!=2))
      return gentypeerr(contextptr);
    if ( (args._VECTptr->front().type!=_INT_) || (args._VECTptr->back().type!=_INT_) )
      return _factorial(args._VECTptr->front(),contextptr)/_factorial(args._VECTptr->front()-args._VECTptr->back(),contextptr);
    if (args._VECTptr->front().val<args._VECTptr->back().val)
      return zero;
    if (args._VECTptr->front().val<0)
      return undef;
    return perm((unsigned long int) args._VECTptr->front().val,(unsigned long int) args._VECTptr->back().val);
  }
  static const char _perm_s []="perm";
  static define_unary_function_eval (__perm,&_perm,_perm_s);
  define_unary_function_ptr5( at_perm ,alias_at_perm,&__perm,0,true);

  // ******************
  // Matrix functions
  // *****************

  gen symb_tran(const gen & a){
    return symbolic(at_tran,a);
  }
  gen symb_trace(const gen & a){
    return symbolic(at_trace,a);
  }
  gen symb_rref(const gen & a){
    return symbolic(at_rref,a);
  }
  gen symb_idn(const gen & e){
    return symbolic(at_idn,e);
  }
  gen symb_ranm(const gen & e){
    return symbolic(at_ranm,e);
  }
  gen symb_det(const gen & a){
    return symbolic(at_det,a);
  }
  gen symb_pcar(const gen & a){
    return symbolic(at_pcar,a);
  }
  gen symb_ker(const gen & a){
    return symbolic(at_ker,a);
  }  
  gen symb_image(const gen & a){
    return symbolic(at_image,a);
  }
  gen _evalf(const gen & a,int ndigits,GIAC_CONTEXT){
    int save_decimal_digits=decimal_digits(contextptr);
#ifndef HAVE_LIBMPFR
    if (ndigits>14)
      return gensizeerr(gettext("Longfloat library not available"));
#endif
    set_decimal_digits(ndigits,contextptr);
    gen res=a.evalf(1,contextptr); 
    if (res.type==_CPLX)
      res=accurate_evalf(res,digits2bits(ndigits));
#if 0
    if (ndigits<=14 && calc_mode(contextptr)==1 && (res.type==_DOUBLE_ || res.type==_CPLX)){
      int decal=0;
      decal=int(std::floor(std::log10(abs(res,contextptr)._DOUBLE_val)));
      res=res*pow(10,ndigits-decal-1,contextptr);
      res=_floor(re(res,contextptr)+.5,contextptr)+cst_i*_floor(im(res,contextptr)+.5,contextptr);
      res=evalf(res,1,contextptr)*pow(10,decal+1-ndigits,contextptr);
    }
    else {
      if (ndigits<=14 && !is_undef(res)){
	res=_round(makesequence(res,ndigits),contextptr);
      }
    }
#else
    if (ndigits<=14 && !is_undef(res))
      res=gen(res.print(contextptr),contextptr);
#endif
    set_decimal_digits(save_decimal_digits,contextptr);
    return res;
  }

  gen evalf_nbits(const gen & g,int nbits){
    if (g.type==_CPLX)
      return evalf_double(g,1,context0);
    if (g.type==_VECT){
      vecteur v=*g._VECTptr;
      for (unsigned i=0;i<v.size();++i)
	v[i]=evalf_nbits(v[i],nbits);
      return gen(v,g.subtype);
    }
    if (g.type==_SYMB)
      return symbolic(g._SYMBptr->sommet,evalf_nbits(g._SYMBptr->feuille,nbits));
    return g;
  }

  bool need_workaround(const gen & g){
    // if (g.type==_REAL || (g.type==_CPLX && g._CPLXptr->type==_REAL && (g._CPLXptr+1)->type==_REAL)) return false;
    if (g.type<=_CPLX)
      return g!=0 && g/g!=1;
    if (is_inf(g) || is_undef(g))
      return true;
    if (g.type!=_VECT)
      return false;
    for (unsigned i=0;i<g._VECTptr->size();++i){
      if (need_workaround((*g._VECTptr)[i]))
	return true;
    }
    return false;
  }

  gen _evalf(const gen & a_orig,GIAC_CONTEXT){
    gen a(a_orig);
    if (a.type==_STRNG && a.subtype==-1) return  a;
    if (is_equal(a) &&a._SYMBptr->feuille.type==_VECT && a._SYMBptr->feuille._VECTptr->size()==2){
      vecteur & v(*a._SYMBptr->feuille._VECTptr);
      return symb_equal(evalf(v.front(),1,contextptr),evalf(v.back(),1,contextptr));//return symbolic(at_equal,gen(makevecteur(evalf(v.front(),1,contextptr),evalf(v.back(),1,contextptr)),_SEQ__VECT));
    }
    gen res;
    int ndigits=decimal_digits(contextptr);
    if (a.type==_VECT && a.subtype==_SEQ__VECT && a._VECTptr->size()==2 && a._VECTptr->back().type==_INT_){
      ndigits=a._VECTptr->back().val;
      a=a._VECTptr->front();
      res=_evalf(a,ndigits,contextptr);
    }
    else
      res=a.evalf(1,contextptr);
#ifdef HAVE_LIBMPFR
    if ( ndigits<=14 && need_workaround(res)){
      // evalf again with 30 digits (overflow workaround)
      res=_evalf(a,30,contextptr);
      // and round to ndigits
      int nbits=digits2bits(ndigits);
      res=evalf_nbits(res,nbits);
      return res;
    }
#endif
    return res;
  }
  static const char _evalf_s []="evalf";
  static define_unary_function_eval (__evalf,&_evalf,_evalf_s);
  define_unary_function_ptr5( at_evalf ,alias_at_evalf,&__evalf,0,true);
  gen symb_evalf(const gen & a){  
    return symbolic(at_evalf,a);  
  }

  gen _eval(const gen & a,GIAC_CONTEXT){
    if ( a.type==_STRNG && a.subtype==-1) return  a;
    if (python_compat(contextptr)){
      gen b=eval(a,1,contextptr);
      if (b.type==_STRNG)
	return _expr(b,contextptr);
    }
    if (is_equal(a) &&a._SYMBptr->feuille.type==_VECT && a._SYMBptr->feuille._VECTptr->size()==2){
      vecteur & v(*a._SYMBptr->feuille._VECTptr);
      return symb_equal(eval(v.front(),eval_level(contextptr),contextptr),eval(v.back(),eval_level(contextptr),contextptr)); // symbolic(at_equal,gen(makevecteur(eval(v.front(),eval_level(contextptr),contextptr),eval(v.back(),eval_level(contextptr),contextptr)),_SEQ__VECT));
    }
    if (a.type==_VECT && a.subtype==_SEQ__VECT && a._VECTptr->size()==2){
      gen a1=a._VECTptr->front(),a2=a._VECTptr->back();
      if (a2.type==_INT_)
	return a1.eval(a2.val,contextptr);
      return _subst(makesequence(eval(a1,eval_level(contextptr),contextptr),a2),contextptr);
    }
    return a.eval(1,contextptr).eval(eval_level(contextptr),contextptr);
  }
  static const char _eval_s []="eval";
  static define_unary_function_eval_quoted (__eval,&_eval,_eval_s);
  define_unary_function_ptr5( at_eval ,alias_at_eval,&__eval,_QUOTE_ARGUMENTS,true);
  gen symb_eval(const gen & a){    
    return symbolic(at_eval,a);  
  }
  
  static const char _evalm_s []="evalm";
  static define_unary_function_eval (__evalm,&_eval,_evalm_s);
  define_unary_function_ptr5( at_evalm ,alias_at_evalm,&__evalm,0,true);
  
  gen _ampersand_times(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type!=_VECT || g._VECTptr->size()!=2)
      return gensizeerr(contextptr);
    return g._VECTptr->front()*g._VECTptr->back();
  }
  static const char _ampersand_times_s []="&*";
  static define_unary_function_eval4_index (108,__ampersand_times,&_ampersand_times,_ampersand_times_s,&printsommetasoperator,&texprintsommetasoperator);
  define_unary_function_ptr( at_ampersand_times ,alias_at_ampersand_times ,&__ampersand_times);
  
  static const char _subst_s []="subst";
  gen _subst(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return gentypeerr(contextptr);
    vecteur & v = *args._VECTptr;
    int s=int(v.size());
    if (s==2){
      gen e=v.back();
      if (e.type==_VECT){
	vecteur w;
	if (ckmatrix(e))
	  aplatir(*e._VECTptr,w);
	else
	  w = *e._VECTptr;
	vecteur vin,vout;
	const_iterateur it=w.begin(),itend=w.end();
	for (;it!=itend;++it){
	  if (it->type!=_SYMB)
	    continue;
	  if (it->_SYMBptr->sommet!=at_equal && it->_SYMBptr->sommet!=at_equal2 && it->_SYMBptr->sommet!=at_same)
	    continue;
	  vin.push_back(it->_SYMBptr->feuille._VECTptr->front());
	  vout.push_back(it->_SYMBptr->feuille._VECTptr->back());
	}
	gen res=subst(v.front(),vin,vout,false,contextptr);
	return res;
      }
      if (e.type!=_SYMB)
	return gentypeerr(contextptr);
      if (e._SYMBptr->sommet!=at_equal && e._SYMBptr->sommet!=at_equal2 && e._SYMBptr->sommet!=at_same)
	return gensizeerr(contextptr);
      return subst(v.front(),e._SYMBptr->feuille._VECTptr->front(),e._SYMBptr->feuille._VECTptr->back(),false,contextptr);
    }
    if (s<3)
      return gentoofewargs(_subst_s);
    if (s>3)
      return gentoomanyargs(_subst_s);
    if (is_equal(v[1]))
      return _subst(makevecteur(v.front(),vecteur(v.begin()+1,v.end())),contextptr);
    return subst(v.front(),v[1],v.back(),false,contextptr);
  }
  static define_unary_function_eval (__subst,&_subst,_subst_s);
  define_unary_function_ptr5( at_subst ,alias_at_subst,&__subst,0,true);
  // static gen symb_subst(const gen & a){    return symbolic(at_subst,a);  }

  string printassubs(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    return sommetstr+("("+feuille.print(contextptr)+")");
    if (xcas_mode(contextptr)!=1 || feuille.type!=_VECT || feuille._VECTptr->size()!=2)
      return sommetstr+("("+feuille.print(contextptr)+")");
    vecteur & v=*feuille._VECTptr;
    vecteur w=mergevecteur(vecteur(1,v.back()),vecteur(v.begin(),v.end()-1));
    return sommetstr+("("+gen(w,_SEQ__VECT).print(contextptr)+")");
  }  
  gen _subs(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    return _subst(g,contextptr);
  }
  static const char _subs_s []="subs";
  static define_unary_function_eval2 (__subs,&_subs,_subs_s,&printassubs);
  define_unary_function_ptr( at_subs ,alias_at_subs ,&__subs);

  string printasmaple_subs(const gen & feuille,const char * sommetstr,GIAC_CONTEXT){
    if (xcas_mode(contextptr)==1 || feuille.type!=_VECT || feuille._VECTptr->size()<2)
      return sommetstr+("("+feuille.print(contextptr)+")");
    vecteur & v=*feuille._VECTptr;
    vecteur w=mergevecteur(vecteur(1,v.back()),vecteur(v.begin(),v.end()-1));
    return sommetstr+("("+gen(w,_SEQ__VECT).print(contextptr)+")");
  }  
  gen _maple_subs(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type!=_VECT || g._VECTptr->size()<2)
      return _subst(g,contextptr);
    vecteur &v=*g._VECTptr;
    if (v.size()==2)
      return _subst(makevecteur(v.back(),v.front()),contextptr);
    else
      return _subst(makevecteur(v.back(),vecteur(v.begin(),v.end()-1)),contextptr);
  }
  static const char _maple_subs_s []="subs";
  static define_unary_function_eval2 (__maple_subs,&_maple_subs,_maple_subs_s,&printasmaple_subs);
  define_unary_function_ptr( at_maple_subs ,alias_at_maple_subs ,&__maple_subs);


  string version(){
    return string("giac ")+GIAC_VERSION+string(", (c) B. Parisse and R. De Graeve, Institut Fourier, Universite de Grenoble I");
  }
  gen _version(const gen & a,GIAC_CONTEXT){
    if ( a.type==_STRNG && a.subtype==-1) return  a;
    return string2gen(gettext("Powered by Giac 1.8.x, B. Parisse and R. De Graeve, Institut Fourier, Universite Grenoble I, France"),false);
  }
  static const char _version_s []="version";
  static define_unary_function_eval (__version,&_version,_version_s);
  define_unary_function_ptr5( at_version ,alias_at_version,&__version,0,true);

  void prod2frac(const gen & g,vecteur & num,vecteur & den){
    num.clear();
    den.clear();
    if (g.type==_FRAC){
      vecteur num2,den2;
      prod2frac(g._FRACptr->num,num,den);
      prod2frac(g._FRACptr->den,den2,num2);
      num=mergevecteur(num,num2);
      den=mergevecteur(den,den2);
      return;      
    }
    if (g.is_neg()){
      prod2frac(g._SYMBptr->feuille,num,den);
      if (!num.empty()){
	num.front()=-num.front();
	return;
      }
      if (!den.empty()){
	den.front()=-den.front();
	return;
      }
    }
    if ( (g.type!=_SYMB) || (g._SYMBptr->sommet!=at_prod) || (g._SYMBptr->feuille.type!=_VECT)){
      if (g.is_symb_of_sommet(at_division)){
	vecteur num2,den2;
	prod2frac(g._SYMBptr->feuille._VECTptr->front(),num,den);
	prod2frac(g._SYMBptr->feuille._VECTptr->back(),den2,num2);
	num=mergevecteur(num,num2);
	den=mergevecteur(den,den2);
	return;
      }
      if (g.is_inv())
	prod2frac(g._SYMBptr->feuille,den,num);
      else
	num=vecteur(1,g);
      return;
    }
    vecteur & v=*g._SYMBptr->feuille._VECTptr;
    const_iterateur it=v.begin(),itend=v.end();
    for (;it!=itend;++it){
      if ( (it->type==_SYMB) && (it->_SYMBptr->sommet==at_inv) )
	den.push_back(it->_SYMBptr->feuille);
      else
	num.push_back(*it);
    }
  }

  gen vecteur2prod(const vecteur & num){
    if (num.empty())
      return plus_one;
    if (num.size()==1)
      return num.front();
    return symb_prod(num);
  }

  bool need_parenthesis(const gen & g){
    if (g.type==_INT_ || g.type==_ZINT)
      return is_strictly_positive(-g,context0);  // ok
    if (g.type==_CPLX){
      gen rg=re(-g,context0),ig(im(-g,context0)); // ok
      if ( is_exactly_zero(rg))
	return is_strictly_positive(ig,context0); // ok
      if (is_exactly_zero(ig) )
	return is_strictly_positive(rg,context0); // ok
      return true;
    }
    if (g.type==_FRAC)
      return true;
    if (g.type==_SYMB)
      return need_parenthesis(g._SYMBptr->sommet);
    if (g.type!=_FUNC)
      return false;
    unary_function_ptr & u=*g._FUNCptr;
    if (u==at_pow || u==at_division || u==at_prod)
      return false;
    if (u==at_neg || u==at_inv || u==at_minus || u==at_and || u==at_ou  || u==at_xor || u==at_same || u==at_equal || u==at_equal2 || u==at_superieur_egal || u==at_superieur_strict || u==at_inferieur_egal || u==at_inferieur_strict)
      return true;
    if (!u.ptr()->printsommet)
      return false;
    return true;
  }

  gen _multistring(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    string res;
    if (args.type==_VECT){
      const_iterateur it=args._VECTptr->begin(),itend=args._VECTptr->end();
      for (;it!=itend;){
	if (it->type!=_STRNG)
	  break;
	res += *it->_STRNGptr;
	++it;
	if (it==itend)
	  return string2gen(res,false);
	res += '\n';
      }
    }
    else {// newline added, otherwise Eqw_compute_size would fail
      if (args.type==_STRNG)
	res=*args._STRNGptr;
      else
	res=args.print(contextptr);
      res += '\n'; 
    }
    return string2gen(res,false);
  }
  static const char _multistring_s []="multistring";
  static define_unary_function_eval (__multistring,&_multistring,_multistring_s);
  define_unary_function_ptr( at_multistring ,alias_at_multistring ,&__multistring);

#ifndef HAVE_LONG_DOUBLE
  static const double LN_SQRT2PI = 0.9189385332046727418; //log(2*PI)/2
  static const double M_PIL=3.141592653589793238462643383279;
  static const double LC1 = 0.08333333333333333,
    LC2 = -0.002777777777777778,
    LC3 = 7.936507936507937E-4,
    LC4 = -5.952380952380953E-4;
  static const double L9[] = {
    0.99999999999980993227684700473478,
    676.520368121885098567009190444019,
    -1259.13921672240287047156078755283,
    771.3234287776530788486528258894,
    -176.61502916214059906584551354,
    12.507343278686904814458936853,
    -0.13857109526572011689554707,
    9.984369578019570859563e-6,
    1.50563273514931155834e-7
  };
#else
  static const long_double LN_SQRT2PI = 0.9189385332046727418L; //log(2*PI)/2
  static const long_double M_PIL=3.141592653589793238462643383279L;
  static const long_double LC1 = 0.08333333333333333L,
    LC2 = -0.002777777777777778L,
    LC3 = 7.936507936507937E-4L,
    LC4 = -5.952380952380953E-4L;
  static const long_double L9[] = {
    0.99999999999980993227684700473478L,
    676.520368121885098567009190444019L,
    -1259.13921672240287047156078755283L,
    771.3234287776530788486528258894L,
    -176.61502916214059906584551354L,
    12.507343278686904814458936853L,
    -0.13857109526572011689554707L,
    9.984369578019570859563e-6L,
    1.50563273514931155834e-7L
  };
#endif

  // Stirling/Lanczos approximation for ln(Gamma())
  double lngamma(double X){
    long_double res,x(X);
    if (x<0.5)
#ifndef HAVE_LONG_DOUBLE
      res=std::log(M_PIL) -std::log(std::sin(M_PIL*x)) - lngamma(1.-x);
#else
    // std::logl?
      res=std::log(M_PIL) -std::log(std::sin(M_PIL*x)) - lngamma(1.L-x);
#endif
    else {
      --x;
      if (x<20){
        // CdB Loop manually unrolled due to a IAR compiler bug!
        long_double a = L9[0] + L9[1]/(x+1.0) + L9[2]/(x+2.0) + L9[3]/(x+3.0) + L9[4]/(x+4.0) + L9[5]/(x+5.0) + L9[6]/(x+6.0) + L9[7]/(x+7.0) + L9[8]/(x+8.0);
//	long_double a = L9[0];
//	for (int i = 1; i < 9; ++i) {
//	  a+= L9[i]/(x+(long_double)(i));
//	}
#ifndef HAVE_LONG_DOUBLE
	res= (LN_SQRT2PI + std::log(a) - 7.) + (x+.5)*(std::log(x+7.5)-1.);
#else
	res= (LN_SQRT2PI + std::log(a) - 7.L) + (x+.5L)*(std::log(x+7.5L)-1.L);
#endif
      }
      else {
	long_double
#ifndef HAVE_LONG_DOUBLE
	  r1 = 1./x,
#else
	  r1 = 1.L/x,
#endif
	  r2 = r1*r1,
	  r3 = r1*r2,
	  r5 = r2*r3,
	  r7 = r3*r3*r1;
#ifndef HAVE_LONG_DOUBLE
	res=(x+.5)*std::log(x) - x + LN_SQRT2PI + LC1*r1 + LC2*r3 + LC3*r5 + LC4*r7;
#else
	res=(x+.5L)*std::log(x) - x + LN_SQRT2PI + LC1*r1 + LC2*r3 + LC3*r5 + LC4*r7;
#endif
      }
    }
    return res;
  }
  
  static complex_long_double lngamma(complex_long_double x){
    complex_long_double res;
    if (x.real()<0.5){
#if !defined(HAVE_LONG_DOUBLE) || defined(PNACL)
#ifdef FREERTOS
      res=std::log(M_PI) -std::log(complex_long_double(std::sin(M_PI*x.real())*std::cosh(M_PI*x.imag()),std::cos(M_PI*x.real())*std::sinh(M_PI*x.imag()))) - lngamma(1.-x);
#else
      res=std::log((double) M_PI) -std::log(std::sin((double)M_PI*x)) - lngamma(1.-x);
#endif
#else
      // std::logl?
      res=std::log(M_PIL) -std::log(std::sin(M_PIL*x)) - lngamma(1.L-x);
#endif
    }  else {
#if !defined(HAVE_LONG_DOUBLE) || defined(PNACL)
      x=x-1.;
#else
      x=x-1.L;
#endif
      complex_long_double a = L9[0];
      for (int i = 1; i < 9; ++i) {
	a+= L9[i]/(x+(long_double)(i));
      }
#if !defined(HAVE_LONG_DOUBLE) || defined(PNACL)
      res= (LN_SQRT2PI + std::log(a) - 7.) + (x+.5)*(std::log(x+7.5)-1.);
#else
      res= (LN_SQRT2PI + std::log(a) - 7.L) + (x+.5L)*(std::log(x+7.5L)-1.L);
#endif
    }
    return res;
  }
  
  gen lngamma(const gen & x,GIAC_CONTEXT){
    gen g(x);
#if 0
    if (g.type==_FLOAT_)
      g=evalf_double(g,1,contextptr);
#endif
    if (g.type==_DOUBLE_){
      if (g._DOUBLE_val<0){
	if (g._DOUBLE_val==int(g._DOUBLE_val))
	  return undef;
	gen gg(g._DOUBLE_val,0.1);
	*(gg._CPLXptr+1)=0.0; // convert to complex
	return lngamma(gg,contextptr);
      }
      return lngamma(g._DOUBLE_val);
    }
    if (g.type==_CPLX && (g._CPLXptr->type==_DOUBLE_ || (g._CPLXptr+1)->type==_DOUBLE_ )){
      g=evalf_double(g,1,contextptr);
      complex_long_double z(re(g,contextptr)._DOUBLE_val,im(g,contextptr)._DOUBLE_val);
      z=lngamma(z);
      return gen(double(z.real()),double(z.imag()));
    }
    return ln(Gamma(x,contextptr),contextptr);
  }
  static const char _lgamma_s[]="lgamma";
  static define_unary_function_eval (__lgamma,&lngamma,_lgamma_s);
  define_unary_function_ptr5( at_lgamma ,alias_at_lgamma,&__lgamma,0,T_UNARY_OP);

  // Gamma function
  // lnGamma_minus is ln(Gamma)-(z-1/2)*ln(z)+z which is tractable at +inf
  static gen taylor_lnGamma_minus(const gen & lim_point,const int ordre,const unary_function_ptr & f, int direction,gen & shift_coeff,GIAC_CONTEXT){
    if (ordre<0)
      return 0;
    if (lim_point!=plus_inf)
      return gensizeerr(contextptr);
    shift_coeff=0;
    vecteur v;
    // ln(Gamma(z)) = (z-1/2)*ln(z) - z +
    //                ln(2*pi)/2 + sum(B_2n /((2n)*(2n-1)*z^(2n-1)),n>=1)
    v.push_back(symb_ln(cst_two_pi)/2);
    for (int n=1;2*n<=ordre;++n){
      v.push_back(bernoulli(2*n)/(4*n*n-2*n));
      v.push_back(0);
    }
    v.push_back(undef);
    return v;
  }
  // lnGamma_minus is ln(Gamma)-(z-1/2)*ln(z)+z which is tractable at +inf
  static gen d_lnGamma_minus(const gen & args,GIAC_CONTEXT){
    return Psi(args,0)+1-symb_ln(args)-(args+minus_one_half)/args;
  }
  define_partial_derivative_onearg_genop( D_at_lnGamma_minus," D_at_lnGamma_minus",&d_lnGamma_minus);
  static gen _lnGamma_minus(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (is_inf(g))
      return symb_ln(cst_two_pi)/2;
    return symbolic(at_lnGamma_minus,g);
  }
  static const char _lnGamma_minus_s []="lnGamma_minus";
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval_taylor( __lnGamma_minus,&_lnGamma_minus,(size_t)&D_at_lnGamma_minusunary_function_ptr,&taylor_lnGamma_minus,_lnGamma_minus_s);
#else
  static define_unary_function_eval_taylor( __lnGamma_minus,&_lnGamma_minus,D_at_lnGamma_minus,&taylor_lnGamma_minus,_lnGamma_minus_s);
#endif
  define_unary_function_ptr5( at_lnGamma_minus ,alias_at_lnGamma_minus,&__lnGamma_minus,0,true);
  // ln(Gamma) = lnGamma_minus + (z-1/2)*ln(z)-z which is tractable at +inf
  static gen Gamma_replace(const gen & g,GIAC_CONTEXT){
    return symb_exp((g+minus_one_half)*symb_ln(g)-g)*symb_exp(_lnGamma_minus(g,contextptr));
  }
  static gen taylor_Gamma (const gen & lim_point,const int ordre,const unary_function_ptr & f, int direction,gen & shift_coeff,GIAC_CONTEXT){
    if (ordre<0){
      return 0; // statically handled now
      //limit_tractable_functions().push_back(at_Gamma);
      //limit_tractable_replace().push_back(Gamma_replace);
      //return 1;
    }
    shift_coeff=0;
    if (!is_integer(lim_point) || is_strictly_positive(lim_point,contextptr))
      return taylor(lim_point,ordre,f,0,shift_coeff,contextptr);
    // Laurent series for Gamma
    if (lim_point.type!=_INT_)
      return gensizeerr(contextptr);
    vecteur v;
    identificateur x(" ");
    int n=-lim_point.val;
    gen decal(1);
    for (int i=1;i<=n;++i){
      decal = decal/(x-i);
    }
    taylor(decal,x,zero,ordre,v,contextptr);
    gen Psi1=taylor(1,ordre,f,0,shift_coeff,contextptr);
    shift_coeff=-1;
    if (Psi1.type!=_VECT)
      return gensizeerr(contextptr);
    v=operator_times(v,*Psi1._VECTptr,0);
    v=vecteur(v.begin(),v.begin()+ordre);
    v.push_back(undef);
    return v;
  }
  static gen d_Gamma(const gen & args,GIAC_CONTEXT){
    return Psi(args,0)*Gamma(args,contextptr);
  }
  define_partial_derivative_onearg_genop( D_at_Gamma," D_at_Gamma",&d_Gamma);
  gen Gamma(const gen & x,GIAC_CONTEXT){
    if (x.type==_VECT && x.subtype==_SEQ__VECT && x._VECTptr->size()>=2){
      gen s=x._VECTptr->front(),z=(*x._VECTptr)[1];
      if (s.type==_DOUBLE_)
	z=evalf_double(z,1,contextptr);
      if (z.type==_DOUBLE_)
	s=evalf_double(s,1,contextptr);
      if (s.type==_DOUBLE_ && z.type==_DOUBLE_){
	bool regu=x._VECTptr->size()==3?!is_zero(x._VECTptr->back()):false;
	gen res=upper_incomplete_gammad(s._DOUBLE_val,z._DOUBLE_val,regu);
	if (res==-1){
	  return regu?1:Gamma(s._DOUBLE_val,contextptr)-lower_incomplete_gamma(s._DOUBLE_val,z._DOUBLE_val,regu,contextptr);
	  //return gensizeerr(contextptr);
	}
	return res;
      }
      return symbolic(at_Gamma,x);
    }
    if (x==plus_inf)
      return x;
    if (is_inf(x))
      return undef;
    // return Gamma(get_double(x._FLOAT_val),contextptr);
    if (x.type==_INT_){
      if (x.val<=0)
	return unsigned_inf;
      return factorial(x.val-1);
    }
    if (x.type==_FRAC &&  x._FRACptr->num.type==_INT_){
      if (x._FRACptr->den==2){
	int n=x._FRACptr->num.val;
	// compute Gamma(n/2)
	gen factnum=1,factden=1;
	for (;n>1;n-=2){
	  factnum=(n-2)*factnum;
	  factden=2*factden;
	}
	for (;n<1;n+=2){
	  factnum=2*factnum;
	  factden=n*factden;
	}
	return factnum/factden*sqrt(cst_pi,contextptr);
      }
      // normalize Gamma(n/d) to fractional part ?
      gen xd=evalf_double(x,1,contextptr),X=x;
      if (xd.type==_DOUBLE_){
	double d=std::floor(xd._DOUBLE_val);
	if (d<GAMMA_LIMIT){
	  xd=1;
	  for (int i=int(d);i>0;--i){
	    X-=1;
	    xd=xd*X;
	  }
	  return xd*symbolic(at_Gamma,X);
	}
      }
      // then complement formula if in ]0..1/2[ Gamma(z)=pi/sin(pi*z)/Gamma(1-z) ?
    }
#if 0 // def HAVE_LIBGSL
    if (x.type==_DOUBLE_)
      return gsl_sf_gamma(x._DOUBLE_val);
#endif
#if defined HAVE_LIBMPFI && !defined NO_RTTI
    if (x.type==_REAL){
      if (real_interval * ptr=dynamic_cast<real_interval *>(x._REALptr)){
	mpfr_t l,u; mpfi_t res;
	int nbits=mpfi_get_prec(ptr->infsup);
	mpfr_init2(l,nbits); mpfr_init2(u,nbits); mpfi_init2(res,nbits);
	mpfi_get_left(l,ptr->infsup);
	mpfi_get_right(u,ptr->infsup);
	if (mpfr_cmp_d(l,1.46163214497)>0){
	  mpfr_gamma(l,l,GMP_RNDD);
	  mpfr_gamma(u,u,GMP_RNDU);
	  mpfi_interv_fr(res,l,u);
	  gen tmp=real_interval(res);
	  mpfi_clear(res); mpfr_clear(l); mpfr_clear(u);
	  return tmp;
	}
	// l<=min of Gamma on R^+
	if (mpfr_cmp_d(l,0)>=0){
	  if (mpfr_cmp_d(u,1.46163214496)<0){
	    mpfr_gamma(l,l,GMP_RNDU);
	    mpfr_gamma(u,u,GMP_RNDD);
	    mpfi_interv_fr(res,u,l);
	    gen tmp=real_interval(res);
	    mpfi_clear(res); mpfr_clear(l); mpfr_clear(u);
	    return tmp;
	  }
	  mpfr_gamma(l,l,GMP_RNDU);
	  mpfr_gamma(u,u,GMP_RNDU);
	  if (mpfr_cmp(l,u)>0)
	    mpfr_set(u,l,GMP_RNDU);
	  mpfr_set_d(l,0.88560319441088,GMP_RNDD);
	  mpfi_interv_fr(res,u,l);
	  gen tmp=real_interval(res);
	  mpfi_clear(res); mpfr_clear(l); mpfr_clear(u);
	  return tmp;
	}
	mpfi_clear(res); mpfr_clear(l); 
	// l<0
	if (mpfr_cmp_d(u,0)>=0){
	  mpfr_clear(u);
	  return gen(makevecteur(minus_inf,plus_inf),_INTERVAL__VECT);
	}
	mpfr_clear(u);
	// if l and u<0 handled by reflection formula
	int mode=get_mode_set_radian(contextptr);
	gen tmp=cst_pi / (sin(cst_pi*x,contextptr)*Gamma(1-x,contextptr));
	angle_mode(mode,contextptr);
	return tmp;
      }
    }
#endif
#ifdef HAVE_LIBMPFR
    if (x.type==_REAL && is_positive(x,contextptr)){
      mpfr_t gam;
      int prec=mpfr_get_prec(x._REALptr->inf);
      mpfr_init2(gam,prec);
      mpfr_gamma(gam,x._REALptr->inf,GMP_RNDN);
      real_object res(gam);
      mpfr_clear(gam);
      return res;
    }
#endif
#ifdef HAVE_LIBPARI
    if (x.type==_CPLX)
      return pari_gamma(x);
#endif
    if (x.type==_DOUBLE_ || ( x.type==_CPLX &&  
			      (x._CPLXptr->type==_DOUBLE_ || (x._CPLXptr+1)->type==_DOUBLE_ )
			      )
	) {
#if 1
      if (is_strictly_positive(.5-re(x,contextptr),contextptr)){
  //grad
	int mode=get_mode_set_radian(contextptr);
	gen res=cst_pi / (sin(M_PI*x,contextptr)*Gamma(1-x,contextptr));
	angle_mode(mode,contextptr);

	return res;
      }
      return exp(lngamma(x,contextptr),contextptr);
#else
      static const double p[] = {
	0.99999999999980993, 676.5203681218851, -1259.1392167224028,
	771.32342877765313, -176.61502916214059, 12.507343278686905,
	-0.13857109526572012, 9.9843695780195716e-6, 1.5056327351493116e-7};
      gen z = x-1;
      gen X = p[0];
      int g=7;
      for (int i=1;i<g+2;++i)
	X += gen(p[i])/(z+i);
      gen t = z + g + 0.5;
      return sqrt(2*cst_pi,contextptr) * pow(t,z+0.5,contextptr) * exp(-t,contextptr) * X;   
#endif   
    }
#ifdef GIAC_HAS_STO_38
    return gammatofactorial(x,contextptr);
#else
    // if (x.is_plus() && x._SYMBptr->feuille.type==_VECT && !x._SYMBptr->feuille._VECTptr->empty() && is_one(x._SYMBptr->feuille._VECTptr->back())) return gammatofactorial(x,contextptr);
    return symbolic(at_Gamma,x);
#endif
  }
  gen _Gamma(const gen & args,GIAC_CONTEXT) {
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    return Gamma(args,contextptr);
  }
  static const char _Gamma_s []="Gamma";
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval_taylor( __Gamma,&_Gamma,(size_t)&D_at_Gammaunary_function_ptr,&taylor_Gamma,_Gamma_s);
#else
  static define_unary_function_eval_taylor( __Gamma,&_Gamma,D_at_Gamma,&taylor_Gamma,_Gamma_s);
#endif
  define_unary_function_ptr5( at_Gamma ,alias_at_Gamma,&__Gamma,0,true);

  double upper_incomplete_gammad(double s,double z,bool regularize){
    // returns -1 if continued fraction expansion is not convergent
    // if s is a small integer = poisson_cdf(z,s-1)*Gamma(s)
    if (s==int(s) && s>0)
      return regularize?poisson_cdf(z,int(s-1)):poisson_cdf(z,int(s-1))*std::exp(lngamma(s));
#if 0 // not tested
    // if z large Gamma(s,z) = z^(s-1)*exp(z)*[1 + (s-1)/z + (s-1)*(s-2)/z^2 +...
    if (s>100 && absdouble(z)>1.1*s){
      long_double res=1,pi=1,S=s-1,Z=z;
      for (;pi>1e-17;--S){
	pi *= S/Z;
	res += pi;
      }
      return pi*std::exp(z-(s-1)*std::log(z));
    }
#endif
    if (z<0){
      double l=lower_incomplete_gamma(s,z,regularize,context0)._DOUBLE_val;
      if (regularize) return 1-l;
      return std::exp(lngamma(s))-l;
    }
    // int_z^inf t^(s-1) exp(-t) dt
    // Continued fraction expansion: a1/(b1+a2/(b2+...)))
    // a1=1, a2=1-s, a3=1, a_{m+2}=a_m+1
    // b1=z, b2=1, b_odd=z, b_{even}=1
    // P0=0, P1=a1, Q0=1, Q1=b1
    // j>=2: Pj=bj*Pj-1+aj*Pj-2, Qj=bj*Qj-1+aj*Qj-2
    long_double Pm2=0,Pm1=1,Pm,Qm2=1,Qm1=z,Qm,a2m1=1,a2m=1-s,b2m1=z,b2m=1,pmqm;
    long_double deux=9007199254740992.,invdeux=1/deux;
    for (long_double m=1;m<200;++m){
      // even term
      Pm=b2m*Pm1+a2m*Pm2;
      Qm=b2m*Qm1+a2m*Qm2;
      Pm2=Pm1; Pm1=Pm;
      Qm2=Qm1; Qm1=Qm;
      a2m++;
      // odd term
      Pm=b2m1*Pm1+a2m1*Pm2;
      Qm=b2m1*Qm1+a2m1*Qm2;
      Pm2=Pm1; Pm1=Pm;
      Qm2=Qm1; Qm1=Qm;
      a2m1++;
      pmqm=Pm/Qm;
      if (absdouble(Pm2/Qm2-pmqm)<1e-16){
	long_double coeff=s*std::log(z)-z;
	if (regularize)
	  coeff -= lngamma(s);
	return pmqm*std::exp(coeff);
      }
      // avoid overflow
      if (absdouble(Pm)>deux){
	Pm2 *= invdeux;
	Qm2 *= invdeux;
	Pm1 *= invdeux;
	Qm1 *= invdeux;
      }
    } 
    // alt a1=1, a2=s-1, a3=2*(s-2), a_{m+1}=m*(s-m)
    // b1=1+z-s, b_{m+1}=2+b_{m}
    return -1;
  }

  // lower_incomplete_gamma(a,z)=z^(-a)*gammaetoile(a,z)
  // gammaetoile(a,z)=sum(n=0..inf,(-z)^n/(a+n)/n!)
  gen gammaetoile(const gen & a,const gen &z,GIAC_CONTEXT){
    gen res=0,resr,resi,znsurfact=1,tmp,tmpr,tmpi;
    double eps2=epsilon(contextptr); eps2=eps2*eps2;
    if (eps2<=0)
      eps2=1e-14;
    for (int n=0;;){
      tmp=znsurfact/(a+n);
      reim(tmp,tmpr,tmpi,contextptr);
      reim(res,resr,resi,contextptr);
      if (is_greater(eps2*(resr*resr+resi*resi),tmpr*tmpr+tmpi*tmpi,contextptr))
	break;
      res += tmp;
      ++n;
      znsurfact = znsurfact *(-z)/n;
    }
    return res;
  }

  gen lower_incomplete_gamma(double s,double z,bool regularize,GIAC_CONTEXT){ // regularize=true by default
    // should be fixed if z is large using upper_incomplete_gamma asymptotics
    if (z>0 && -z+s*std::log(z)-lngamma(s+1)<-37)
      return regularize?1:std::exp(lngamma(s));
    if (z<0){
      // FIXME: this does not work if z is large with double precision
      // example igamma(1/3,-216.)
      // multi-precision is required
      gen zs=-std::pow(-z,s)*gammaetoile(s,z,contextptr);
      return regularize?std::exp(-lngamma(s))*zs:zs;
    }
    if (z>=s){
      double res=upper_incomplete_gammad(s,z,regularize);
      if (res>=0){
	if (regularize)
	  return 1-res;
	else
	  return Gamma(s,context0)-res;
      }
    }
    // gamma(s,z) = int(t^s*e^(-t),t=0..z)
    // Continued fraction expansion: a1/(b1+a2/(b2+...)))
    // here a1=1, a2=-s*z, a3=z, then a_{2m}=a_{2m-2}-z and a_{2m+1}=a_{2m-1}+z
    // b1=s, b_{n}=s+n-1
    // P0=0, P1=a1, Q0=1, Q1=b1
    // j>=2: Pj=bj*Pj-1+aj*Pj-2, Qj=bj*Qj-1+aj*Qj-2
    // Here bm=1, am=em, etc.
    long_double Pm2=0,Pm1=1,Pm,Qm2=1,Qm1=s,Qm,a2m=-(s-1)*z,a2m1=0,bm=s;
    long_double deux=9007199254740992.,invdeux=1/deux;
    for (long_double m=1;m<100;++m){
      // even term
      a2m -= z;
      bm++;
      Pm=bm*Pm1+a2m*Pm2;
      Qm=bm*Qm1+a2m*Qm2;
      Pm2=Pm1; Pm1=Pm;
      Qm2=Qm1; Qm1=Qm;
      // odd term
      a2m1 +=z;
      bm++;
      Pm=bm*Pm1+a2m1*Pm2;
      Qm=bm*Qm1+a2m1*Qm2;
      // cerr << Pm/Qm << " " << Pm2/Qm2 << endl;
      if (absdouble(Pm/Qm-Pm2/Qm2)<1e-16){
	double res=Pm/Qm;
	if (regularize)
	  res *= std::exp(-z+s*std::log(z)-lngamma(s));
	else
	  res *= std::exp(-z+s*std::log(z));
	return res;
      }	
      Pm2=Pm1; Pm1=Pm;
      Qm2=Qm1; Qm1=Qm;
      // normalize
#if 1
      if (absdouble(Pm)>deux){
	Pm2 *= invdeux; Qm2 *= invdeux; Pm1 *= invdeux; Qm1 *= invdeux;
      }
#else
      Pm=1/std::sqrt(Pm1*Pm1+Qm1*Qm1);
      Pm2 *= Pm; Qm2 *= Pm; Pm1 *= Pm; Qm1 *= Pm;
#endif
    }
    return undef; //error
  }

  gen _lower_incomplete_gamma(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return gensizeerr(contextptr);
    vecteur v=*args._VECTptr;
    int s=int(v.size());
    if (s>=2 && (v[0].type==_DOUBLE_ || v[1].type==_DOUBLE_)){
      v[0]=evalf_double(v[0],1,contextptr);
      v[1]=evalf_double(v[1],1,contextptr);
    }
    if ( (s==2 || s==3) && v[0].type==_DOUBLE_ && v[1].type==_DOUBLE_ )
      return lower_incomplete_gamma(v[0]._DOUBLE_val,v[1]._DOUBLE_val,s==3?!is_zero(v[2]):false,contextptr);
    if (s<2 || s>3)
      return gendimerr(contextptr);
    if (s==2 && is_zero(v[1],contextptr))
      return 0;
    if (s==2 && v[1]==plus_inf)
      return Gamma(v[0],contextptr);
    if (s==2 && v[0].type==_INT_){
      if (v[0].val<=0)
	return undef;
      int a=v[0].val-1;
      // int(e^(-t)*t^a,t)=-e^(-t)*sum_{b=0}^a(t^b*(a!/(a-b)!))
      gen res=0,t(v[1]),fa(1);
      for (int b=a;;--b){
	res += pow(t,b,contextptr)*fa;
	if (b==0)
	  break;
	fa=b*fa;
      }
      res=-exp(-t,contextptr)*res+fa;
      return res;
    }
    if (abs_calc_mode(contextptr)!=38) // check may be removed if ugamma declared
      return symbolic(at_lower_incomplete_gamma,args);
    if (s==3){
      if (is_zero(v[2]))
	return Gamma(v[0],contextptr)-symbolic(at_Gamma,makesequence(v[0],v[1]));
      return 1-symbolic(at_Gamma,makesequence(v[0],v[1],1));
    }
    return Gamma(v[0],contextptr)-symbolic(at_Gamma,args);
  }
  static const char _lower_incomplete_gamma_s []="igamma"; // "lower_incomplete_gamma"
  static define_unary_function_eval (__lower_incomplete_gamma,&_lower_incomplete_gamma,_lower_incomplete_gamma_s);
  define_unary_function_ptr5( at_lower_incomplete_gamma ,alias_at_lower_incomplete_gamma,&__lower_incomplete_gamma,0,true);

  gen _igamma_exp(const gen & args,GIAC_CONTEXT){
    return symbolic(at_igamma_exp,args);
  }
  static const char _igamma_exp_s []="igamma_exp";
  static define_unary_function_eval (__igamma_exp,&_igamma_exp,_igamma_exp_s);
  define_unary_function_ptr5( at_igamma_exp ,alias_at_igamma_exp,&__igamma_exp,0,true);

  static gen igamma_replace(const gen & g,GIAC_CONTEXT){
    return Gamma(g[0],contextptr)-_igamma_exp(g,contextptr)*exp(-g[1],contextptr);
  }  

#ifdef XLIGHT
  gen Psi(const gen & x,GIAC_CONTEXT){
    return undef;
  }
#else
  // diGamma function
  static gen taylor_Psi_minus_ln(const gen & lim_point,const int ordre,const unary_function_ptr & f, int direction,gen & shift_coeff,GIAC_CONTEXT){
    if (ordre<0)
      return 0;
    if (lim_point!=plus_inf)
      return gensizeerr(contextptr);
    shift_coeff=1;
    vecteur v(1,minus_one_half);
    // Psi(z)=ln(z)-1/(2*z)-sum(B_2n /(2*n*z^(2n)),n>=1)
    for (int n=2;n<=ordre;n+=2){
      v.push_back(-bernoulli(n)/n);
      v.push_back(0);
    }
    v.push_back(undef);
    return v;
  }
  static gen d_Psi_minus_ln(const gen & args,GIAC_CONTEXT){
    return inv(args,contextptr)-Psi(args,1,contextptr);
  }
  define_partial_derivative_onearg_genop( D_at_Psi_minus_ln," D_at_Psi_minus_ln",&d_Psi_minus_ln);
  static gen _Psi_minus_ln(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (is_inf(g))
      return 0;
    return symbolic(at_Psi_minus_ln,g);
  }
  static const char _Psi_minus_ln_s []="Psi_minus_ln";
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval_taylor( __Psi_minus_ln,&_Psi_minus_ln,(size_t)&D_at_Psi_minus_lnunary_function_ptr,&taylor_Psi_minus_ln,_Psi_minus_ln_s);
#else
  static define_unary_function_eval_taylor( __Psi_minus_ln,&_Psi_minus_ln,D_at_Psi_minus_ln,&taylor_Psi_minus_ln,_Psi_minus_ln_s);
#endif
  define_unary_function_ptr5( at_Psi_minus_ln ,alias_at_Psi_minus_ln,&__Psi_minus_ln,0,true);
  static gen Psi_replace(const gen & g,GIAC_CONTEXT){
    return symb_ln(g)+_Psi_minus_ln(g,contextptr);
  }
  static gen taylor_Psi (const gen & lim_point,const int ordre,const unary_function_ptr & f, int direction,gen & shift_coeff,GIAC_CONTEXT){
    if (ordre<0){
      return 0; // statically handled now
      //limit_tractable_functions().push_back(at_Psi);
      //limit_tractable_replace().push_back(Psi_replace);
      //return 1;
    }
    shift_coeff=0;
    if (!is_integer(lim_point) || is_strictly_positive(lim_point,contextptr))
      return taylor(lim_point,ordre,f,0,shift_coeff,contextptr);
    // FIXME Laurent series for Psi
    if (lim_point.type!=_INT_)
      return gensizeerr(contextptr);
    vecteur v;
    identificateur x(" ");
    int n=-lim_point.val;
    gen decal;
    for (int i=0;i<n;++i){
      decal -= inv(x+i,contextptr);
    }
    taylor(decal,x,lim_point,ordre,v,contextptr);
    gen Psi1=taylor(1,ordre,f,0,shift_coeff,contextptr);
    shift_coeff=-1;
    if (Psi1.type!=_VECT)
      return gensizeerr(contextptr);
    v=v+*Psi1._VECTptr;
    v.insert(v.begin(),-1);
    return v;
  }
  static gen d_Psi(const gen & args,GIAC_CONTEXT){
    vecteur v(gen2vecteur(args));
    if (v.size()==1)
      v.push_back(0);
    if (v.size()!=2 || v.back().type!=_INT_)
      return gendimerr(contextptr);
    return Psi(v.front(),v.back().val+1,contextptr);
  }
  define_partial_derivative_onearg_genop( D_at_Psi," D_at_Psi",&d_Psi);

  gen Psi(const gen & x,GIAC_CONTEXT){
#if 0
    if (x.type==_FLOAT_)
      return Psi(get_double(x._FLOAT_val),contextptr);
#endif
    if (is_positive(-x,contextptr)){
      if (is_integer(x))
	return unsigned_inf;
      if (!is_positive(x,contextptr)) // check added for HP for Σ(1/x,x,a,b)
	return Psi(ratnormal(1-x,contextptr),contextptr)-cst_pi/tan(cst_pi*x,contextptr);
    }
    if (x==plus_inf)
      return x;
    if (is_undef(x))
      return x;
    if (is_inf(x))
      return undef;
    if ( (x.type==_INT_) && (x.val<10000) && (x.val>=1)){
      identificateur tt(" t");
      return -cst_euler_gamma+sum_loop(inv(tt,contextptr),tt,1,x.val-1,contextptr);
    }
    if (x.type==_FRAC){
      // Psi(m/k) for 0<m<k
      // Psi(m/k) = -euler_gamma -ln(2k) - pi/2/tan(m*pi/k) +
      //    + 2 sum( cos(2 *pi*n*m/k)*ln(sin(n*pi/k)), n=1..floor (k-1)/2 )
      gen num=x._FRACptr->num,den=x._FRACptr->den;
      if (num.type==_INT_ && den.type==_INT_ && den.val<13){
	int m=num.val,k=den.val;
	gen res;
	int mk=m/k;
	for (int i=mk;i>0;--i){
	  m -= k;
	  res += inv(m,contextptr);
	}
	res = k*res - cst_euler_gamma - ln(2*k,contextptr) - cst_pi/2/tan(m*cst_pi/k,contextptr);
	gen res1 ;
	for (int n=1;n<=(k-1)/2;n++){
	  res1 += cos(2*n*m*cst_pi/k,contextptr)*ln(sin(n*cst_pi/k,contextptr),contextptr);
	}
	return res + 2*res1;
      }
    }
#if 0 // def HAVE_LIBGSL
    if (x.type==_DOUBLE_)
      return gsl_sf_psi(x._DOUBLE_val);
#endif
//#ifdef TARGET_OS_IPHONE
//    if (x.type == _DOUBLE_)
//      return psi(x._DOUBLE_val);
//#endif
    if (x.type==_DOUBLE_){
      double z=x._DOUBLE_val;
      // z<=0 , psi(z)=pi*cotan(pi*z)-psi(1-z)
      // z>0, psi(z)=psi(z+1)-1/z
      // until x>10, 
      double res0=0,res1=0,res2=0;
      bool sub=false;
      if (z<0){
	res0=M_PI/std::tan(M_PI*z);
	z=1-z;
	sub=true;
      }
      for (;z<10;z++){
	res1 -= 1/z;
      }
      // ln(x)-1/2/x-1/12*1/x^2+1/120*1/x^4-1/252*1/x^6+1/240*1/x^8-1/132*1/x^10+691/32760*1/x^12-1/12*1/x^14
      res1 += std::log(z);
      z=1/z;
      res1 -= z/2;
      z=z*z;
      res2 = -z/12;
      res2 *= z;
      res2 += 691./32760.;
      res2 *= z;
      res2 -= 1./132.;
      res2 *= z;
      res2 += 1./240.;
      res2 *= z;
      res2 -= 1./252.;
      res2 *= z;
      res2 += 1./120.;
      res2 *= z;
      res2 -= 1./12.;
      res2 *= z;
      res1 += res2;
      if (sub)
	return res1-res0;
      else
	return res1;
    }
    if (x.type==_CPLX){
      gen c=evalf_double(x,1,contextptr);
      complex<double> z(c._CPLXptr->_DOUBLE_val,(c._CPLXptr+1)->_DOUBLE_val);
      // z<=0 , psi(z)=pi*cotan(pi*z)-psi(1-z)
      // z>0, psi(z)=psi(z+1)-1/z
      // until x>10, 
      complex<double> res0=0,res1=0,res2=0;
      bool sub=false;
      if (c._CPLXptr->_DOUBLE_val<0){
#ifdef GIAC_HAS_STO_38
	res0=(double) M_PI/std::tan((double) M_PI*z);
#else
	res0=M_PI/std::tan(M_PI*z);
#endif
	z=1.0-z;
	sub=true;
      }
      for (;z.real()<10;z+=1){
	res1 -= 1.0/z;
      }
      // ln(x)-1/2/x-1/12*1/x^2+1/120*1/x^4-1/252*1/x^6+1/240*1/x^8-1/132*1/x^10+691/32760*1/x^12-1/12*1/x^14
      res1 += std::log(z);
      z=1.0/z;
      res1 -= z/2.0;
      z=z*z;
      res2 = -z/12.0;
      res2 *= z;
      res2 += 691./32760.;
      res2 *= z;
      res2 -= 1./132.;
      res2 *= z;
      res2 += 1./240.;
      res2 *= z;
      res2 -= 1./252.;
      res2 *= z;
      res2 += 1./120.;
      res2 *= z;
      res2 -= 1./12.;
      res2 *= z;
      res1 += res2;
      if (sub)
	return res1-res0;
      else
	return res1;
    }
#ifdef HAVE_LIBPARI
    // if (x.type==_CPLX || x.type==_REAL)
    if (x.type==_REAL)
      return pari_psi(x);
#endif
    return symbolic(at_Psi,x);
  }

  gen cot_psi_cache(int n,GIAC_CONTEXT){
    static vecteur * ptr=0;
    if (!ptr) ptr=new vecteur;
    vecteur & cot_cache=*ptr;
    if (cot_cache.size()>n)
      return cot_cache[n];
    if (cot_cache.empty())
      cot_cache.push_back(_cot(cst_pi*vx_var(),contextptr));
    while (cot_cache.size()<=n)
      cot_cache.push_back(ratnormal(derive(cot_cache.back(),vx_var(),contextptr),contextptr));
    return cot_cache[n];
  }
  double bernoulli_tab[]={1.000000000000000,-0.50000000000000000,0.1666666666666667,0.0000000000000000,-0.3333333333333333e-1,0.0000000000000000,0.2380952380952381e-1,0.0000000000000000,-0.3333333333333333e-1,0.0000000000000000,0.7575757575757576e-1,0.0000000000000000,-0.2531135531135531,0.0000000000000000,1.166666666666667,0.0000000000000000,-7.092156862745098,0.0000000000000000,0.5497117794486216e2,0.0000000000000000,-0.5291242424242424e3,0.0000000000000000,0.6192123188405797e4,0.0000000000000000,-0.8658025311355311e5,0.0000000000000000,0.1425517166666667e7,0.0000000000000000,-0.2729823106781609e8,0.0000000000000000,0.6015808739006424e9};
  gen evalf_Psi(const gen & x,int n,GIAC_CONTEXT){
    if (n==0)
      return Psi(x,contextptr);
    // |z|<1, Psi(1+z,n)=(-1)^(n+1)*n!*(Zeta(n+1)-(n+1)*Zeta(n+2)*z+(n+1)*(n+2)/2!*Zeta(n+3)*z^2-...)
    // or (-1)^(n+1)*n!*sum((z+k)^(-n-1),k,0,inf)
    // |z|->inf outside R^-: (-1)^(n+1)*((n-1)!/z^n+n!/2/z^(n+1)+sum(bernoulli(2*k)*(2*k+n-1)!/(2*k)!/z^(2k+n),k,1,inf))
    // recurrence Psi(z,n)=Psi(z+1,n)-(-1)^n*n!*z^(-n-1)
    // reflection Psi(1-z,n)+(-1)^(n+1)Psi(z,n)=(-1)^n*pi*cotan(pi*z)^{[n]}
    if (x.type==_DOUBLE_){
      double d=x._DOUBLE_val;
      if (d<=0){
	if (d==int(d))
	  return unsigned_inf;
	gen res=evalf_Psi(1-d,n,contextptr);
	gen tmp=cot_psi_cache(n,contextptr);
	tmp=subst(tmp,vx_var(),d,false,contextptr);
	if (n%2)
	  res=-M_PI*tmp-res;
	else
	  res=res-M_PI*tmp;
	return res;
      }
      // d>0
      double res=0;
      for (;d<10+n;++d){
	res += std::pow(d,-n-1);
      }
      res = n*res;
      double zn=std::pow(d,-n); // (n-1)!/z^n
      double tmp=zn;
      zn=n*zn/(2*d);
      tmp=zn+tmp;
      zn=(n+1)*zn/d;
      for (int k=1;k<15;++k){
	tmp=tmp+bernoulli_tab[2*k]*zn;
	zn=(2*k+n)*(2*k+n+1)/(d*d*(2*k+1)*(2*k+2))*zn;
      }
      double factn=evalf_double(factorial(n-1),1,contextptr)._DOUBLE_val;
      res=factn*(res+tmp);
      if (n%2) return res; else return -res;
    }
    if (x.type==_CPLX){
      gen c=evalf_double(x,1,contextptr);
      double d=c._CPLXptr->_DOUBLE_val,i=(c._CPLXptr+1)->_DOUBLE_val;
      if (d<=0){
	gen res=evalf_Psi(1-x,n,contextptr);
	gen tmp=cot_psi_cache(n,contextptr);
	tmp=subst(tmp,vx_var(),c,false,contextptr);
	if (n%2)
	  res=-M_PI*tmp-res;
	else
	  res=res-M_PI*tmp;
	return res;
      }
      // Re(x)>0
      complex<double> z(d,i),res=0;
      for (;d<10+n;++d,z+=1){
#if defined FXCG || defined TICE
	res += std::pow(z,-n-1.0);
#else
	res += std::pow(z,-n-1);
#endif
      }
      res = double(n)*res;
#if defined FXCG || defined TICE
      complex<double> zn=std::pow(z,double(-n)); // (n-1)!/z^n
#else
     complex<double> zn=std::pow(z,-n); // (n-1)!/z^n
#endif
      complex<double> tmp=zn;
      zn=double(n)*zn/(2.0*z);
      tmp=zn+tmp;
      zn=(n+1.0)*zn/z;
      for (int k=1;k<15;++k){
	tmp=tmp+bernoulli_tab[2*k]*zn;
	zn=(2.0*k+n)*(2.0*k+n+1.0)/(z*z*(2.0*k+1.0)*(2.0*k+2.0))*zn;
      }
      double factn=evalf_double(factorial(n-1),1,contextptr)._DOUBLE_val;
      res=factn*(res+tmp);
      if (n%2) return res; else return -res;
    }
    return undef;
  }
  // n-th derivative of digamma function
  gen Psi(const gen & x,int n,GIAC_CONTEXT){
    if (n<-1)
      return gensizeerr(contextptr);
    if (n==-1)
      return Gamma(x,contextptr);
    if (n==0)
      return Psi(x,contextptr);
    if (is_integer(x) && is_positive(-x,contextptr))
      return unsigned_inf;
    if (is_one(x)){
      if (n%2)
	return Zeta(n+1,contextptr)*factorial(n);
      else
	return -Zeta(n+1,contextptr)*factorial(n);
    }
    if (x==plus_one_half && n>=1){
      gen res=factorial(n);
      if (n%2==0)
	res=-res;
      res=res*(pow(2,n+1,contextptr)-1);
      return res*Zeta(n+1,contextptr);
    }
    if (x==plus_inf)
      return zero;
    if (is_undef(x))
      return x;
    if (is_inf(x))
      return undef;
    if (!n) 
      return Psi(x,contextptr);
    if ( (x.type==_INT_) && (x.val<10000) ){
      identificateur tt(" t");
      if (n%2)
	return factorial(n)*(Zeta(n+1,contextptr)-sum_loop(pow(tt,-n-1),tt,1,x.val-1,contextptr));
      else
	return -factorial(n)*(Zeta(n+1,contextptr)-sum_loop(pow(tt,-n-1),tt,1,x.val-1,contextptr));
    }
    if (x.type==_DOUBLE_ || x.type==_CPLX){
      gen d=evalf_Psi(x,n,contextptr);
      return d;
#if 0 //def HAVE_LIBGSL // for check only
      double val=gsl_sf_psi_n(n,x._DOUBLE_val);
      CERR << d << " " << val << endl;
      return d;
#endif 
    }
    return symbolic(at_Psi,makesequence(x,n));
  }
  gen _Psi(const gen & args,GIAC_CONTEXT) {
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return Psi(args,contextptr);
    if ( args._VECTptr->size()!=2 )
      return symbolic(at_Psi,args);
    gen x(args._VECTptr->front()),n(args._VECTptr->back());
    // if (n.type==_REAL) n=n.evalf_double(1,contextptr);
    if (is_integral(n))
      return Psi(x,n.val,contextptr);
    if (is_integral(x)){
      *logptr(contextptr) << "Warning, please use Psi(x,n), not Psi(n,x)" << endl;
      return Psi(n,x.val,contextptr);
    }
    return gensizeerr(contextptr);
  }
  static const char _Psi_s []="Psi";
#ifdef GIAC_HAS_STO_38
  define_unary_function_eval_taylor (__Psi,&_Psi,(size_t)&D_at_Psiunary_function_ptr,&taylor_Psi,_Psi_s);
#else
  define_unary_function_eval_taylor (__Psi,&_Psi,D_at_Psi,&taylor_Psi,_Psi_s);
#endif
  define_unary_function_ptr5( at_Psi ,alias_at_Psi,&__Psi,0,true);
#endif
  
  string printsommetasnormalmod(const gen & feuille,const char * sommetstr_orig,GIAC_CONTEXT){
    if (python_compat(contextptr))
      return printsommetasoperator(feuille,"mod",contextptr);
    return printsommetasoperator(feuille,sommetstr_orig,contextptr);    
  }
  gen _normalmod(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type!=_VECT || g._VECTptr->size()!=2)
      return gensizeerr(contextptr);
    gen f =g._VECTptr->front();
    if (is_equal(f))
      return symb_equal(_normalmod(makevecteur(f._SYMBptr->feuille[0],g._VECTptr->back()),contextptr),
			_normalmod(makevecteur(f._SYMBptr->feuille[1],g._VECTptr->back()),contextptr));
    if (f.type==_VECT){
      vecteur v=*f._VECTptr;
      for (unsigned i=0;i<v.size();++i)
	v[i]=_normalmod(makevecteur(v[i],g._VECTptr->back()),contextptr);
      return gen(v,f.subtype);
    }
    gen b=g._VECTptr->back();
    static bool warnmod=true;
    if (f.type==_MOD){
      if (warnmod){
	*logptr(contextptr) << "// Warning: a % b returns the class of a in Z/bZ. Use irem(a,b) for remainder" << endl;
	warnmod=false;
      }
      f=*f._MODptr;
      if (b.type==_MOD)
	b=*b._MODptr;
      if (b==0) return f;
      return _irem(makesequence(f,b),contextptr);
    }
    if (b.type==_MOD){
      if (warnmod){
	*logptr(contextptr) << "// Warning: a % b returns the class of a in Z/bZ. Use irem(a,b) for remainder" << endl;
	warnmod=false;
      }
      b=*b._MODptr;
      if (b==0) return f;
      return _irem(makesequence(f,b),contextptr);
    }
    gen res=normal(makemodquoted(f,b),contextptr);
    if (f.type==_VECT && res.type==_VECT)
      res.subtype=f.subtype;
    return res;
  }
#ifdef GIAC_HAS_STO_38
  static const char _normalmod_s []="%%";
#else
  static const char _normalmod_s []="%";
#endif
  static define_unary_function_eval4_index (166,__normalmod,&_normalmod,_normalmod_s,&printsommetasnormalmod,&texprintsommetasoperator);
  define_unary_function_ptr( at_normalmod ,alias_at_normalmod ,&__normalmod);

#ifndef XLIGHT
  // a=expression, x variable, n=number of terms, 
  // compute an approx value of sum((-1)^k*a(k),k,0,+infinity)
  // using Chebychev polynomials
  gen alternate_series(const gen & a,const gen & x,int n,GIAC_CONTEXT){
    gen d=normal((pow(3+2*sqrt(2,contextptr),n)+pow(3-2*sqrt(2,contextptr),n))/2,contextptr);
    gen p=1;
    gen c=d-p;
    gen S=subst(a,x,0,false,contextptr)*c;
    for (int k=1;k<n;k++) {
      p=p*gen(k+n-1)*gen(k-n-1)/gen(k-inv(2,contextptr))/gen(k);
      c=-p-c;
      S=S+subst(a,x,k,false,contextptr)*c;
    }
    return S/d;
  }

  gen Eta(const gen & s,int ndiff,GIAC_CONTEXT){
    if (s.type==_INT_ && !ndiff){
      if (s==1)
	return symb_ln(2);
      if (s%2==0)
	return (1-pow(2,1-s,contextptr))*Zeta(s,contextptr);
    }
    if (s.type==_DOUBLE_ || (s.type==_CPLX)){
      gen rx=re(s,contextptr).evalf_double(1,contextptr);
      if (rx._DOUBLE_val<0.5){
	if (ndiff){
	  identificateur id(" ");
	  gen t(id),zeta;
	  zeta=derive((1-pow(2,1-t,contextptr))*pow(2*cst_pi,t,contextptr)/cst_pi*sin(cst_pi*t/2,contextptr)*symbolic(at_Gamma,1-t)*symbolic(at_Zeta,1-t),t,ndiff,contextptr);
	  zeta=subst(zeta,t,s,false,contextptr);
	  return zeta;
	}
	gen zeta1=Eta(1-s,0,contextptr)/(1-pow(2,s,contextptr));
	gen zetas=pow(2,s,contextptr)*pow(cst_pi,s-1,contextptr)*sin(cst_pi*s/2,contextptr)*Gamma(1-s,contextptr)*zeta1;
	return (1-pow(2,1-s,contextptr))*zetas;
      }
      // find n such that 3*(1+2*|y|)*exp(|y|*pi/2)*10^ndigits < (3+sqrt(8))^n
      gen ix=im(s,contextptr).evalf_double(1,contextptr);
      if (ix.type!=_DOUBLE_)
	return gentypeerr(contextptr);
      double y=absdouble(ix._DOUBLE_val);
      int ndigits=16; // FIXME? use decimal_digits;
      double n=(std::log10(3*(1+2*y)*std::exp(y*M_PI/2))+ndigits)/std::log10(3.+std::sqrt(8.));
      identificateur idx(" ");
      gen x(idx);
      gen res=alternate_series(inv(pow(idx+1,s,contextptr),contextptr)*pow(-ln(idx+1,contextptr),ndiff,contextptr),idx,int(std::ceil(n)),contextptr);
      return res.evalf(1,contextptr);
    }
    else {
      if (ndiff)
	return symbolic(at_Eta,makesequence(s,ndiff));
      else
	return symbolic(at_Eta,s);
    }
  }

  gen Eta(const gen & s0,GIAC_CONTEXT){
    gen s=s0;
    int ndiff=0;
    if (s.type==_VECT){
      if (s._VECTptr->size()!=2)
	return gensizeerr(contextptr);
      gen n=s._VECTptr->back();
      // if (n.type==_REAL) n=n.evalf_double(1,contextptr);
      if (n.type==_DOUBLE_)
	n=int(n._DOUBLE_val);
      if (n.type!=_INT_)
	return gentypeerr(contextptr);
      ndiff=n.val;
      s=s._VECTptr->front();
    }
    return Eta(s,ndiff,contextptr);
  }

  gen Zeta(const gen & x,int ndiff,GIAC_CONTEXT){
    if (!ndiff)
      return Zeta(x,contextptr);
    if (x.type==_DOUBLE_ || (x.type==_CPLX && x.subtype==_DOUBLE_)){
      gen rex=re(x,contextptr).evalf_double(1,contextptr);
      if (rex.type!=_DOUBLE_)
	return gensizeerr(contextptr);
      identificateur id(" ");
      gen t(id),zeta;
      if (rex._DOUBLE_val<0.5){
	// Zeta(x)=2^x*pi^(x-1)*sin(pi*x/2)*Gamma(1-x)*zeta(1-x)
	zeta=derive(pow(2*cst_pi,t,contextptr)/cst_pi*sin(cst_pi*t/2,contextptr)*symbolic(at_Gamma,1-t)*symbolic(at_Zeta,1-t),t,ndiff,contextptr);
	zeta=subst(zeta,t,x,false,contextptr);
      }
      else {
	// Zeta=Eta(x)/(1-2^(1-x))
	zeta=derive(symbolic(at_Eta,t)/(1-pow(2,1-t,contextptr)),t,ndiff,contextptr);
	zeta=subst(zeta,t,x,false,contextptr);
      }
      return zeta;
    }
    return symbolic(at_Zeta,makesequence(x,ndiff));
  }
  gen Zeta(const gen & x,GIAC_CONTEXT){
    if (x.type==_VECT){
      if (x._VECTptr->size()!=2)
	return gensizeerr(contextptr);
      gen n=x._VECTptr->back();
      // if (n.type==_REAL) n=n.evalf_double(1,contextptr);
      if (n.type==_DOUBLE_)
	n=int(n._DOUBLE_val);
      if (n.type!=_INT_)
	return gentypeerr(contextptr);
      int ndiff=n.val;
      return Zeta(x._VECTptr->front(),ndiff,contextptr);
    }
    if ( (x.type==_INT_)){
      int n=x.val;
      if (!n)
	return minus_one_half;
      if (n==1)
	return plus_inf;
      if (n<0){
	if (n%2)
	  return -rdiv(bernoulli(1-n),(1-n),contextptr) ;
	else
	  return zero;
      }
      if (n%2)
	return symbolic(at_Zeta,x);
      else
	return pow(cst_pi,n)*ratnormal(abs(bernoulli(x),contextptr)*rdiv(pow(plus_two,n-1),factorial(n),contextptr),contextptr);
    }
#ifdef HAVE_LIBGSL
    if (x.type==_DOUBLE_)
      return gsl_sf_zeta(x._DOUBLE_val);
#endif // HAVE_LIBGSL
#ifdef HAVE_LIBPARI
    if (x.type==_CPLX && x.subtype!=3)
      return pari_zeta(x);
#endif
#ifdef HAVE_LIBMPFR
    if (x.type==_REAL){
      mpfr_t gam;
      int prec=mpfr_get_prec(x._REALptr->inf);
      mpfr_init2(gam,prec);
      mpfr_zeta(gam,x._REALptr->inf,GMP_RNDN);
      real_object res(gam);
      mpfr_clear(gam);
      return res;
    }
#endif
    if (x.type==_CPLX || x.type==_DOUBLE_)
      return Eta(x,contextptr)/(1-pow(2,1-x,contextptr));
    return symbolic(at_Zeta,x);
  }
  gen _Zeta(const gen & args,GIAC_CONTEXT) {
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    return Zeta(args,contextptr);
  }
  static gen d_Zeta(const gen & args,GIAC_CONTEXT){
    vecteur v(gen2vecteur(args));
    if (v.size()==1)
      v.push_back(0);
    if (v.size()!=2 || v.back().type!=_INT_)
      return gendimerr(contextptr);
    return Zeta(v.front(),v.back().val+1,contextptr);
  }
  static gen taylor_Zeta(const gen & lim_point,const int ordre,const unary_function_ptr & f, int direction,gen & shift_coeff,GIAC_CONTEXT){
    if (ordre<0)
      return 0; // no symbolic preprocessing
    if (is_one(lim_point)){
      shift_coeff=-1;
      identificateur x(" "); vecteur v,w;
      taylor(1-pow(2,1-x,contextptr),x,1,ordre+1,w,contextptr);
      w.erase(w.begin());
      vreverse(w.begin(),w.end());
      if (!w.empty() && is_undef(w.front()))
	w.erase(w.begin());
      gen gw=horner(w,x);
      sparse_poly1 sp=series__SPOL1(symbolic(at_Eta,x+1)/gw,x,0,ordre,0,contextptr); 
      sparse_poly1::const_iterator it=sp.begin(),itend=sp.end();
      for (;it!=itend;++it){
	v.push_back(it->coeff); // assumes all coeffs are non zero...
      }
      return v;
    }
    return taylor(lim_point,ordre,f,direction,shift_coeff,contextptr);
  }
  define_partial_derivative_onearg_genop( D_at_Zeta," D_at_Zeta",&d_Zeta);
  static const char _Zeta_s []="Zeta";
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval_taylor( __Zeta,&_Zeta,(size_t)&D_at_Zetaunary_function_ptr,&taylor_Zeta,_Zeta_s);
#else
  static define_unary_function_eval_taylor( __Zeta,&_Zeta,D_at_Zeta,&taylor_Zeta,_Zeta_s);
#endif
  define_unary_function_ptr5( at_Zeta ,alias_at_Zeta,&__Zeta,0,true);

  static gen d_Eta(const gen & args,GIAC_CONTEXT){
    vecteur v(gen2vecteur(args));
    if (v.size()==1)
      v.push_back(0);
    if (v.size()!=2 || v.back().type!=_INT_)
      return gendimerr(contextptr);
    return Eta(v.front(),v.back().val+1,contextptr);
  }
  define_partial_derivative_onearg_genop( D_at_Eta," D_at_Eta",&d_Eta);
  gen _Eta(const gen & args,GIAC_CONTEXT) {
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    return Eta(args,contextptr);
  }
  static const char _Eta_s []="Eta";
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval3 (__Eta,&_Eta,(size_t)&D_at_Etaunary_function_ptr,_Eta_s);
#else
  static define_unary_function_eval3 (__Eta,&_Eta,D_at_Eta,_Eta_s);
#endif
  define_unary_function_ptr5( at_Eta ,alias_at_Eta,&__Eta,0,true);
  
  // error function
  static gen taylor_erfs(const gen & lim_point,const int ordre,const unary_function_ptr & f, int direction,gen & shift_coeff,GIAC_CONTEXT){
    if (ordre<0)
      return 0;
    if (!is_inf(lim_point))
      return gensizeerr(contextptr);
    shift_coeff=1;
    // erfs(x)=1/sqrt(pi) * 1/x* sum( (2*k)! / (-4)^k / k! * x^(-2k) )
    gen tmp(1);
    vecteur v;
    for (int n=0;n<=ordre;){
      v.push_back(tmp);
      v.push_back(0);
      n +=2 ;
      tmp=gen(n-1)/gen(-2)*tmp;
    }
    v.push_back(undef);
    return multvecteur(inv(sqrt(cst_pi,contextptr),contextptr),v);
  }
  gen _erfs(const gen & g,GIAC_CONTEXT);
  static gen d_erfs(const gen & args,GIAC_CONTEXT){
    return 2*args*_erfs(args,contextptr)-gen(2)/sqrt(cst_pi,contextptr);
  }
  define_partial_derivative_onearg_genop( D_at_erfs," D_at_erfs",&d_erfs);
  gen _erfs(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (is_inf(g))
      return 0;
    return symbolic(at_erfs,g);
  }
  static const char _erfs_s []="erfs";
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval_taylor( __erfs,&_erfs,(size_t)&D_at_erfsunary_function_ptr,&taylor_erfs,_erfs_s);
#else
  static define_unary_function_eval_taylor( __erfs,&_erfs,D_at_erfs,&taylor_erfs,_erfs_s);
#endif
  define_unary_function_ptr5( at_erfs ,alias_at_erfs,&__erfs,0,true);
  static gen erf_replace(const gen & g,GIAC_CONTEXT){
    if (has_i(g))
      return 1-symb_exp(-ratnormal(g*g,contextptr))*_erfs(g,contextptr);
    return symbolic(at_sign,g)*(1-symb_exp(-g*g)*_erfs(symbolic(at_abs,g),contextptr));
  }
  static gen taylor_erf (const gen & lim_point,const int ordre,const unary_function_ptr & f, int direction,gen & shift_coeff,GIAC_CONTEXT){
    if (ordre<0){
      return 0; // statically handled now
      //limit_tractable_functions().push_back(at_erf);
      //limit_tractable_replace().push_back(erf_replace);
      //return 1;
    }
    shift_coeff=0;
    return taylor(lim_point,ordre,f,0,shift_coeff,contextptr);
  }
  static gen d_erf(const gen & e,GIAC_CONTEXT){
    return 2*exp(-pow(e,2),contextptr)/sqrt(cst_pi,contextptr);
  }
  define_partial_derivative_onearg_genop( D_at_erf," D_at_erf",d_erf);

  static gen erf0(const gen & x,gen & erfc,GIAC_CONTEXT){
    // if (x.type==_REAL && is_strictly_positive(-x,contextptr)) return -erf0(-x,erfc,contextptr);
    if (x.type==_DOUBLE_){ 
      double absx=absdouble(x._DOUBLE_val);
      if (absx<=3){
	// numerical computation of int(exp(-t^2),t=0..x) 
	// by series expansion at x=0
	// x*sum( (-1)^n*(x^2)^n/n!/(2*n+1),n=0..inf)
	long_double z=x._DOUBLE_val,z2=z*z,res=0,pi=1;
	for (int n=0;;){
	  res += pi/(2*n+1);
	  ++n;
	  pi = -pi*z2/n;
	  if (pi<1e-17 && pi>-1e-17)
	    break;
	}
	erfc=double(1-2/std::sqrt(M_PI)*z*res);
	return 2/std::sqrt(M_PI)*double(z*res);
      }
      if (absx>=6.5){
	// asymptotic expansion at infinity of int(exp(-t^2),t=x..inf)
	// z=1/x
	// z*exp(-x^2)*(1/2 - 1/4 z^2 +3/8 z^4-15/16 z^6 + ...)
	long_double z=1/absx,z2=z*z/2,res=0,pi=0.5;
	for (int n=0;;++n){
	  res += pi;
	  pi = -pi*(2*n+1)*z2;
	  if (absdouble(pi)<1e-16)
	    break;
	}
	erfc=2/std::sqrt(M_PI)*double(std::exp(-1/z/z)*z*res);
	gen e=1-erfc;
	if (x._DOUBLE_val>=0)
	  return e;
	erfc=2-erfc;
	return -e;
      }
      else { 
	// erf(x)=2*x*exp(-x^2)/sqrt(pi)*sum(2^j*x^(2j)/1/3/5/.../(2j+1),j=0..inf)
	// or continued fraction
	// 2*exp(z^2)*int(exp(-t^2),t=z..inf)=1/(z+1/2/(z+1/(z+3/2/(z+...))))
	long_double z=absx,res=0;
	for (long_double n=40;n>=1;n--){
	  res=n/2/(z+res);
	}
	res=1/(z+res);
	erfc=std::exp(-absx*absx)*double(res)/std::sqrt(M_PI);
	gen e=1-erfc;
	if (x._DOUBLE_val>=0)
	  return e;
	erfc=2-erfc;
	return -e;
      }
#if 0
      // a:=convert(series(erfc(x)*exp(x^2),x=X,24),polynom):; b:=subst(a,x=X+h):;
      if (absx>3 && absx<=5){
	// Digits:=30; evalf(symb2poly(subst(b,X,4),h))
	long_double Zl=absx-4,res=0;
	long_double taberf[]={0.9323573505930262336910814663629e-18,-0.5637770672346891132663122366369e-17,0.3373969923698176600796949171416e-16,-0.1997937342757611758805760309387e-15,0.1170311628709846086671746844320e-14,-0.6779078623355796103927587022047e-14,0.3881943235655598141099274338263e-13,-0.2196789805508621713379735090290e-12,0.1228090799753488475137690971599e-11,-0.6779634525816110746734938098109e-11,0.3694326453071165814527058450923e-10,-0.1986203171147991823844885265211e-9,0.1053084120195192127202221248092e-8,-0.5503368542058483880654875851859e-8,0.2833197888944711586737808090450e-7,-0.1435964425391227330876779173688e-6,0.7160456646037012951391007806358e-6,-0.3510366649840828060143659147374e-5,0.1690564925777814684043808381146e-4,-0.7990888030555549397777128848414e-4,0.3703524689955564311420527395424e-3,-0.1681182076746114476323671722330e-2,0.7465433244975570766528102818814e-2,-0.3238350609502145478059791886069e-1,0.1369994576250613898894451230325};
	unsigned N=sizeof(taberf)/sizeof(long_double);
	for (unsigned i=0;i<N;i++){
	  res *= Zl;
	  res += taberf[i];
	}
	erfc = double(std::exp(-absx*absx)*res);
	return sign(x,contextptr)*(1-erfc);
      }
      if (absx>5 && absx<=6.5){
	// Digits:=30; evalf(symb2poly(subst(b,X,5.75),h))
	long_double Zl=absx-5.75,res=0;
	long_double taberf[]={-0.3899077949952308336341205103240e-12,0.2064555746398182434172952813760e-13,-0.7079917646274828801231710613009e-12,-0.2043006626755557967429543230042e-12,-0.2664588032913413248313045028978e-11,-0.3182230691773937386262907009549e-11,-0.4508687162250923186571888867300e-12,-0.2818971742901571639195611759894e-11,-0.4771270499789446447101554995178e-11,0.2345376254096117543212461524786e-11,-0.6529305258174487397807156793042e-11,0.9817004987916722489154147719630e-12,0.2085292084663647123257426988484e-10,-0.1586500138272075839895787048265e-9,0.1056533982771769784560244626854e-8,-0.6964568016562765632682760517056e-8,0.4530411628438409475101496352516e-7,-0.2918364042864784155554051827879e-6,0.1859299481340192895158490699981e-5,-0.1171241494503672776195474661763e-4,0.7292428889065898343608897828825e-4,-0.4485956983428598110336671805311e-3,0.2725273842847326036320664185043e-2,-0.1634321814380709002113440890281e-1,0.9669877816971385564543076482100e-1};
	unsigned N=sizeof(taberf)/sizeof(long_double);
	for (unsigned i=0;i<N;i++){
	  res *= Zl;
	  res += taberf[i];
	}
	erfc = double(std::exp(-absx*absx)*res);
	return sign(x,contextptr)*(1-erfc);
      }
#endif
    } // end x.type==_DOUBLE_
    gen z=evalf_double(abs(x,contextptr),1,contextptr);
    if (x.type==_CPLX ){
      double absx=z._DOUBLE_val;
      complex_long_double z(evalf_double(re(x,contextptr),1,contextptr)._DOUBLE_val,
			     evalf_double(im(x,contextptr),1,contextptr)._DOUBLE_val);
      if (absx<=3){
	// numerical computation of int(exp(-t^2),t=0..x) 
	// by series expansion at x=0
	// x*sum( (-1)^n*(x^2)^n/n!/(2*n+1),n=0..inf)
	complex_long_double z2=z*z,res=0,pi=1;
	for (long_double n=0;;){
	  res += pi/(2*n+1);
	  ++n;
	  pi = -pi*z2/n;
	  if (complex_long_abs(pi)<1e-17)
	    break;
	}
#if !defined(HAVE_LONG_DOUBLE) || defined(PNACL)
	res=(2.0/std::sqrt(M_PI))*z*res;
#else
	res=(2.0L/std::sqrt(M_PI))*z*res;
#endif
	gen e(double(res.real()),double(res.imag()));
	erfc=1.0-e;
	return e;
      }
      bool neg=z.real()<0;
      if (neg)
	z=-z;
      if (absx>=6.5){
	// asymptotic expansion at infinity of int(exp(-t^2),t=x..inf)
	// z=1/x
	// z*exp(-x^2)*(1/2 - 1/4 z^2 +3/8 z^4-15/16 z^6 + ...)
#if !defined(HAVE_LONG_DOUBLE) || defined(PNACL)
	z=1.0/z;
	complex_long_double z2=z*z/2.0,res=0,pi=0.5;
#else
	z=1.0L/z;
	complex_long_double z2=z*z/2.0L,res=0,pi=0.5;
#endif
	for (long_double n=0;;++n){
	  res += pi;
	  pi = -pi*(2*n+1)*z2;
	  if (complex_long_abs(pi)<1e-16)
	    break;
	}
#if !defined(HAVE_LONG_DOUBLE) || defined(PNACL)
	res=complex_long_double(2.0/std::sqrt(M_PI))*std::exp(-1.0/z/z)*z*res;
#else
	res=complex_long_double(2.0/std::sqrt(M_PI))*std::exp(-1.0L/z/z)*z*res;
#endif
	erfc=gen(double(res.real()),double(res.imag()));
	gen e=1-erfc;
	if (!neg)
	  return e;
	erfc=2-erfc;
	return -e;
      }
      else { 
	// continued fraction
	// 2*exp(z^2)*int(exp(-t^2),t=z..inf)=1/(z+1/2/(z+1/(z+3/2/(z+...))))
	complex_long_double res=0;
	for (long_double n=40;n>=1;n--){
	  res=(n/2)/(z+res);
	}
#if !defined(HAVE_LONG_DOUBLE) || defined(PNACL)
	res=1.0/(z+res);
#else
	res=1.0L/(z+res);
#endif
	res=std::exp(-z*z)*res/complex_long_double(std::sqrt(M_PI));
	erfc=gen(double(res.real()),double(res.imag()));
	gen e=1-erfc;
	if (!neg)
	  return e;
	erfc=2-erfc;
	return -e;
      }
    } // end low precision
    // take account of loss of accuracy
    int prec=decimal_digits(contextptr);
    int newprec,nbitsz=int(z._DOUBLE_val*z._DOUBLE_val/std::log(2.)),prec2=int(prec*std::log(10.0)/std::log(2.0)+.5);
    if (nbitsz>prec2){ 
      // use asymptotic expansion at z=inf
      z = accurate_evalf(inv(x,contextptr),prec2);
      gen z2=z*z/2,res=0,pi=inv(accurate_evalf(plus_two,prec2),contextptr),eps=accurate_evalf(pow(10,-prec,contextptr),prec2)/2;
      for (int n=0;;++n){
	res += pi;
	pi = -(2*n+1)*z2*pi;
	if (is_greater(eps,abs(pi,contextptr),contextptr))
	  break;
      }
      erfc=evalf(2*inv(sqrt(cst_pi,contextptr),contextptr),1,contextptr)*exp(-inv(z*z,contextptr),contextptr)*z*res;
      return 1-erfc;
    }
    if (z._DOUBLE_val>1)
      newprec = prec2+nbitsz+int(std::log(z._DOUBLE_val))+1;
    else
      newprec = prec2+2;
    // numerical computation of int(exp(-t^2),t=0..x) 
    // by series expansion at x=0
    // x*sum( (-1)^n*(x^2)^n/n!/(2*n+1),n=0..inf)
    z=accurate_evalf(x,newprec);
    gen z2=z*z,res=0,pi=1,eps=accurate_evalf(pow(10,-prec,contextptr),prec2)/2;
    for (int n=0;;){
      res += pi/(2*n+1);
      ++n;
      pi = -pi*z2/n;
      if (is_greater(eps,abs(pi,contextptr),contextptr))
	break;
    }
    res = evalf(2*inv(sqrt(cst_pi,contextptr),contextptr),1,contextptr)*z*res;
    erfc=accurate_evalf(1-res,prec2);
    return accurate_evalf(res,prec2);
  }
  gen erf(const gen & x,GIAC_CONTEXT){
    if (is_equal(x))
      return apply_to_equal(x,erf,contextptr);
#if 0
    if (x.type==_FLOAT_)
      return erf(get_double(x._FLOAT_val),contextptr);
#endif
    if (x==plus_inf)
      return plus_one;
    if (x==minus_inf)
      return minus_one;
    if (is_undef(x))
      return x;
    if (is_inf(x))
      return undef;
    if (is_zero(x,contextptr))
      return x;
    gen erfc_;
    if (x.type==_DOUBLE_ || x.type==_CPLX )
      return erf0(x,erfc_,contextptr);
#if 0 // def GIAC_HAS_STO_38
    return 1-2*symbolic(at_UTPN,x*plus_sqrt2);
#else
    return symbolic(at_erf,x);
#endif
#if 0
    gen e=x.evalf(1,contextptr);
#ifdef HAVE_LIBGSL
    if (e.type==_DOUBLE_)
      return gsl_sf_erf(e._DOUBLE_val);
#endif
#ifdef HAVE_LIBMPFR
    if (x.type==_REAL){
      mpfr_t gam;
      int prec=mpfr_get_prec(x._REALptr->inf);
      mpfr_init2(gam,prec);
      mpfr_erf(gam,x._REALptr->inf,GMP_RNDN);
      real_object res(gam);
      mpfr_clear(gam);
      return res;
    }
#endif
#if 0 // def GIAC_HAS_STO_38
    return 1-2*symbolic(at_UTPN,x*plus_sqrt2);
#else
    return symbolic(at_erf,x);
#endif
#endif
  }
  gen _erf(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    return apply(args,erf,contextptr);
  }
  static const char _erf_s []="erf";
#ifdef GIAC_HAS_STO_38
  define_unary_function_eval_taylor( __erf,&_erf,(size_t)&D_at_erfunary_function_ptr,&taylor_erf,_erf_s);
#else
  define_unary_function_eval_taylor( __erf,&_erf,D_at_erf,&taylor_erf,_erf_s);
#endif
  define_unary_function_ptr5( at_erf ,alias_at_erf,&__erf,0,true);

  static gen d_erfc(const gen & e,GIAC_CONTEXT){
    return -d_erf(e,contextptr);
  }
  define_partial_derivative_onearg_genop( D_at_erfc," D_at_erfc",d_erfc);
  gen erfc(const gen & x,GIAC_CONTEXT){
#if 0
    if (x.type==_FLOAT_)
      return erfc(get_double(x._FLOAT_val),contextptr);
#endif
    if (is_equal(x))
      return apply_to_equal(x,erfc,contextptr);
    gen erfc_;
    if (x.type==_DOUBLE_ || x.type==_CPLX ){
      erf0(x,erfc_,contextptr);
      return erfc_;
    }
#if 0 // def GIAC_HAS_STO_38
    return 2*symbolic(at_UTPN,x*plus_sqrt2);
#else
    return 1-erf(x,contextptr); // 1-symbolic(at_erf,x);
#endif
    gen e=x.evalf(1,contextptr);
#ifdef HAVE_LIBGSL
    if (e.type==_DOUBLE_)
      return gsl_sf_erfc(e._DOUBLE_val);
#endif
#if 0 // def GIAC_HAS_STO_38
    return 2*symbolic(at_UTPN,x*plus_sqrt2);
#else
    return 1-symbolic(at_erf,x);
#endif
  }
  gen _erfc(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    return apply(args,erfc,contextptr);
  }
  static const char _erfc_s []="erfc";
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval3 (__erfc,&_erfc,(size_t)&D_at_erfcunary_function_ptr,_erfc_s);
#else
  static define_unary_function_eval3 (__erfc,&_erfc,D_at_erfc,_erfc_s);
#endif
  define_unary_function_ptr5( at_erfc ,alias_at_erfc,&__erfc,0,true);
#endif

  gen _Dirac(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type==_VECT && args.subtype!=_SEQ__VECT)
      return apply(args,_Dirac,contextptr);
    gen f=args;
    if (args.type==_VECT && args.subtype==_SEQ__VECT && !args._VECTptr->empty())
      f=args._VECTptr->front();
    if (is_zero(f,contextptr))
      return unsigned_inf;
    if (f.type<_IDNT)
      return 0;
    return symbolic(at_Dirac,args);
  }
  static gen d_Dirac(const gen & args,GIAC_CONTEXT){
    vecteur v(gen2vecteur(args));
    if (v.size()==1)
      v.push_back(0);
    if (v.size()!=2 || v.back().type!=_INT_)
      return gendimerr(contextptr);
    return _Dirac(makesequence(v.front(),v.back().val+1),contextptr);
  }
  define_partial_derivative_onearg_genop( D_at_Dirac," D_at_Dirac",&d_Dirac);
  static const char _Dirac_s []="Dirac";
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval3 (__Dirac,&_Dirac,(size_t)&D_at_Diracunary_function_ptr,_Dirac_s);
#else
  static define_unary_function_eval3 (__Dirac,&_Dirac,D_at_Dirac,_Dirac_s);
#endif
  define_unary_function_ptr5( at_Dirac ,alias_at_Dirac,&__Dirac,0,true);
  define_partial_derivative_onearg_genop( D_Heaviside," D_Heaviside",&_Dirac);

  gen _Heaviside(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type==_VECT)
      return apply(args,_Heaviside,contextptr);
    if (is_zero(args,contextptr))
      return plus_one;
    gen tmp=_sign(args,contextptr);
    if (tmp.type<=_DOUBLE_)
      return (tmp+1)/2;
    return symbolic(at_Heaviside,args);
  }
  static const char _Heaviside_s []="Heaviside";
#ifdef GIAC_HAS_STO_38
  static define_unary_function_eval3 (__Heaviside,&_Heaviside,(size_t)&D_Heavisideunary_function_ptr,_Heaviside_s);
#else
  static define_unary_function_eval3 (__Heaviside,&_Heaviside,D_Heaviside,_Heaviside_s);
#endif
  define_unary_function_ptr5( at_Heaviside ,alias_at_Heaviside,&__Heaviside,0,true);

  const char _sum_s []="sum";
  static define_unary_function_eval_quoted (__sum,&_sum,_sum_s);
  define_unary_function_ptr5( at_sum ,alias_at_sum,&__sum,_QUOTE_ARGUMENTS,true);

  gen fast_icontent(const gen & g){
    if (g.type==_VECT){
      gen G(0);
      const_iterateur it=g._VECTptr->begin(),itend=g._VECTptr->end();
      for (;it!=itend;++it){
	G=gcd(G,fast_icontent(*it),context0);
      }
      return G;
    }
    if (g.type!=_SYMB)
      return (g.type==_FRAC || (is_integer(g) && g!=0))?abs(g,context0):1;
    if (g._SYMBptr->sommet==at_plus || g._SYMBptr->sommet==at_neg)
      return fast_icontent(g._SYMBptr->feuille);
    if (g._SYMBptr->sommet==at_inv)
      return inv(fast_icontent(g._SYMBptr->feuille),context0);
    if (g._SYMBptr->sommet==at_prod){
      gen G(1);
      const_iterateur it=g._SYMBptr->feuille._VECTptr->begin(),itend=g._SYMBptr->feuille._VECTptr->end();
      for (;it!=itend;++it){
	G=G*fast_icontent(*it);
      }
      return G;
    }
    if (g._SYMBptr->sommet==at_pow){
      if (is_integer(g._SYMBptr->feuille[1]))
	return pow(fast_icontent(g._SYMBptr->feuille[0]),g._SYMBptr->feuille[1],context0);
    }
    return 1;
  }

  gen fast_divide_by_icontent(const gen & g,const gen & z){
    if (g.is_inv() && is_integer(g._SYMBptr->feuille))
      return inv(g._SYMBptr->feuille*z,context0);
    if (z==1)
      return g;
    if (g.type==_VECT){
      vecteur v(*g._VECTptr);
      iterateur it=v.begin(),itend=v.end();
      for (;it!=itend;++it){
	*it=fast_divide_by_icontent(*it,z);
      }
      return gen(v,g.subtype);
    }
    if (g.type!=_SYMB)
      return g/z;
    if (g._SYMBptr->sommet==at_plus || g._SYMBptr->sommet==at_neg)
      return symbolic(g._SYMBptr->sommet,fast_divide_by_icontent(g._SYMBptr->feuille,z));
    if (g._SYMBptr->sommet==at_inv)
      return symbolic(g._SYMBptr->sommet,fast_divide_by_icontent(g._SYMBptr->feuille,inv(z,context0)));
    if (g._SYMBptr->sommet==at_pow && is_integer(g._SYMBptr->feuille[1])){
      gen z1=fast_icontent(g._SYMBptr->feuille[0]);
      gen g1=fast_divide_by_icontent(g._SYMBptr->feuille[0],z1);
      return pow(z1,g._SYMBptr->feuille[1],context0)/z*pow(g1,g._SYMBptr->feuille[1],context0);
    }
    if (g._SYMBptr->sommet==at_prod && g._SYMBptr->feuille.type==_VECT){
      vecteur v(*g._SYMBptr->feuille._VECTptr);
      iterateur it=v.begin(),itend=v.end();
      gen zz(z),z2;
      for (;it!=itend;++it){
	z2=gcd(fast_icontent(*it),zz,context0);
	*it=fast_divide_by_icontent(*it,z2);
	zz=zz/z2;
      }
      return _prod(v,context0)/zz;
    }
    return g/z;
  }

  // vector<unary_function_ptr > solve_fcns_v(solve_fcns,solve_fcns+sizeof(solve_fcns)/sizeof(unary_function_ptr));

  // #ifndef GNUWINCE
  // #ifndef WIN32
  // #endif
#if defined(GIAC_GENERIC_CONSTANTS) // || (defined(VISUALC) && !defined(RTOS_THREADX)) || defined( x86_64)
  const gen zero(0);
  const gen plus_one(1);
  const gen plus_two(2);
  const gen plus_three(3);
  const gen minus_one(-1);
  const gen cst_i(0,1);
#else
  const define_alias_gen(alias_zero,_INT_,0,0);
  const define_alias_gen(alias_plus_one,_INT_,0,1);
  const gen & zero = *(const gen *) & alias_zero;
  const gen & plus_one = *(const gen *) & alias_plus_one;
  define_alias_ref_complex(cst_i_ref,_INT_,0,0,_INT_,0,1);
  const define_alias_gen(alias_cst_i,_CPLX,0,&cst_i_ref);
  static const gen cst_i_storage(0,1);
  const gen & cst_i = cst_i_storage;

  const define_alias_gen(alias_minus_one,_INT_,0,-1);
  const gen & minus_one = *(const gen *) & alias_minus_one;
  const define_alias_gen(alias_plus_two,_INT_,0,2);
  const gen & plus_two = *(const gen *) & alias_plus_two;
  const define_alias_gen(alias_plus_three,_INT_,0,3);
  const gen & plus_three = *(const gen *) & alias_plus_three;
#endif

  //grad 
  const double rad2deg_d(180/M_PI);
  const double deg2rad_d(M_PI/180);
  const double rad2grad_d(200 / M_PI);
  const double grad2rad_d(M_PI / 200);
#if defined(DOUBLEVAL) || defined(GIAC_GENERIC_CONSTANTS) || defined(VISUALC) || defined(x86_64)
  static const gen rad2deg_g_(rad2deg_d);
  const gen & rad2deg_g=rad2deg_g_;
  static const gen deg2rad_g_(deg2rad_d);
  const gen & deg2rad_g=deg2rad_g_;
  //grad
  static const gen rad2grad_g_(rad2grad_d);
  const gen & rad2grad_g = rad2grad_g_;
  static const gen grad2rad_g_(grad2rad_d);
  const gen & grad2rad_g=grad2rad_g_;
#else
#endif

  gen cst_ipi(){
    return gen(0,cst_pi);
  }

#if defined(GIAC_GENERIC_CONSTANTS) // || (defined(VISUALC) && !defined(RTOS_THREADX)) || defined(x86_64)
  gen cst_two_pi(symbolic(at_prod,makevecteur(plus_two,_IDNT_pi())));
  gen cst_pi_over_2(_FRAC2_SYMB(_IDNT_pi(),2));
  gen plus_inf(symbolic(at_plus,_IDNT_infinity()));
  gen minus_inf(symbolic(at_neg,_IDNT_infinity()));
  gen plus_one_half(fraction(1,2));
  gen minus_one_half(symbolic(at_neg,symb_inv(2)));
  gen plus_sqrt3(symbolic(at_pow,gen(makevecteur(3,plus_one_half),_SEQ__VECT)));
  gen plus_sqrt2(symbolic(at_pow,gen(makevecteur(2,plus_one_half),_SEQ__VECT)));
  gen plus_sqrt6(symbolic(at_pow,gen(makevecteur(6,plus_one_half),_SEQ__VECT)));
  gen minus_sqrt6(symbolic(at_neg,plus_sqrt6));
  gen minus_sqrt3(symbolic(at_neg,plus_sqrt3));
  gen minus_sqrt2(symbolic(at_neg,plus_sqrt2));
  gen minus_sqrt3_2(_FRAC2_SYMB(minus_sqrt3,2));
  gen minus_sqrt2_2(_FRAC2_SYMB(minus_sqrt2,2));
  gen minus_sqrt3_3(_FRAC2_SYMB(minus_sqrt3,3));
  gen plus_sqrt3_2(_FRAC2_SYMB(plus_sqrt3,2));
  gen plus_sqrt2_2(_FRAC2_SYMB(plus_sqrt2,2));
  gen plus_sqrt3_3(_FRAC2_SYMB(plus_sqrt3,3));
  gen cos_pi_12(_FRAC2_SYMB(
			    symbolic(at_plus,gen(makevecteur(plus_sqrt6,plus_sqrt2),_SEQ__VECT)),
			    4));
  gen minus_cos_pi_12(_FRAC2_SYMB(
			    symbolic(at_plus,gen(makevecteur(minus_sqrt6,minus_sqrt2),_SEQ__VECT)),
			    4));
  gen sin_pi_12(_FRAC2_SYMB(
			    symbolic(at_plus,gen(makevecteur(plus_sqrt6,minus_sqrt2),_SEQ__VECT)),
			    4));
  gen minus_sin_pi_12(_FRAC2_SYMB(
			    symbolic(at_plus,gen(makevecteur(plus_sqrt2,minus_sqrt6),_SEQ__VECT)),
			    4));
  gen tan_pi_12(symbolic(at_plus,gen(makevecteur(2,minus_sqrt3),_SEQ__VECT)));
  gen tan_5pi_12(symbolic(at_plus,gen(makevecteur(2,plus_sqrt3),_SEQ__VECT)));
  gen minus_tan_pi_12(symbolic(at_neg,tan_pi_12));
  gen minus_tan_5pi_12(symbolic(at_neg,tan_5pi_12));
  gen rad2deg_e(_FRAC2_SYMB(180,_IDNT_pi()));
  gen deg2rad_e(_FRAC2_SYMB(_IDNT_pi(),180));
  //grad
  gen rad2grad_e(_FRAC2_SYMB(200,_IDNT_pi()));
  gen grad2rad_e(_FRAC2_SYMB(_IDNT_pi(),200));
  
  // 0 = -pi, 12=0, 24=pi
  const gen * const table_cos[trig_deno+1]={
    &minus_one,&minus_cos_pi_12,&minus_sqrt3_2,&minus_sqrt2_2,&minus_one_half,&minus_sin_pi_12,
    &zero,&sin_pi_12,&plus_one_half,&plus_sqrt2_2,&plus_sqrt3_2,&cos_pi_12,
    &plus_one,&cos_pi_12,&plus_sqrt3_2,&plus_sqrt2_2,&plus_one_half,&sin_pi_12,
    &zero,&minus_sin_pi_12,&minus_one_half,&minus_sqrt2_2,&minus_sqrt3_2,&minus_cos_pi_12,
    &minus_one
  };
  const gen * const table_tan[trig_deno/2+1]={
    &zero,&tan_pi_12,&plus_sqrt3_3,&plus_one,&plus_sqrt3,&tan_5pi_12,
    &unsigned_inf,&minus_tan_5pi_12,&minus_sqrt3,&minus_one,&minus_sqrt3_3,&minus_tan_pi_12,
    &zero
  };


#else 
  const define_alias_gen(alias_plus_four,_INT_,0,4);
  static const gen gen_plus_four_storage(4);
  const gen & gen_plus_four = gen_plus_four_storage;
  const define_alias_gen(alias_plus_six,_INT_,0,6);
  static const gen gen_plus_six_storage(6);
  const gen & gen_plus_six = gen_plus_six_storage;
  const define_alias_gen(alias_180,_INT_,0,180);
  static const gen gen_180_storage(180);
  const gen & gen_180 = gen_180_storage;

  const define_tab2_alias_gen(alias_cst_two_pi_tab,_INT_,0,2,_IDNT,0,&ref_pi);
  const define_alias_ref_vecteur2(cst_two_pi_refv,alias_cst_two_pi_tab);

  // static const define_alias_gen(cst_two_pi_V,_VECT,_SEQ__VECT,&cst_two_pi_refv);
  const define_alias_ref_symbolic( cst_two_pi_symb ,alias_at_prod,_VECT,_SEQ__VECT,&cst_two_pi_refv);
  const define_alias_gen(alias_cst_two_pi,_SYMB,0,&cst_two_pi_symb);
  static const gen cst_two_pi_storage(2*cst_pi);
  const gen & cst_two_pi = cst_two_pi_storage;

  const define_alias_ref_symbolic( inv_2_symb,alias_at_inv,_INT_,0,2);
  const define_alias_gen(alias_inv_2,_SYMB,0,&inv_2_symb);
  static const gen gen_inv_2_storage(symb_inv(2));
  const gen & gen_inv_2 = gen_inv_2_storage;

  const define_alias_ref_symbolic( inv_3_symb,alias_at_inv,_INT_,0,3)
  const define_alias_gen(alias_inv_3,_SYMB,0,&inv_3_symb);
  static const gen gen_inv_3_storage(symb_inv(3));
  const gen & gen_inv_3 = gen_inv_3_storage;

  const define_alias_ref_symbolic( inv_4_symb,alias_at_inv,_INT_,0,4)
  const define_alias_gen(alias_inv_4,_SYMB,0,&inv_4_symb);
  static const gen gen_inv_4_storage(symb_inv(4));
  const gen & gen_inv_4 = gen_inv_4_storage;

  const define_tab2_alias_gen(alias_cst_pi_over_2_tab,_IDNT,0,&ref_pi,_SYMB,0,&inv_2_symb);
  const define_alias_ref_vecteur2(cst_pi_over_2_refv,alias_cst_pi_over_2_tab);

  const define_alias_ref_symbolic( cst_pi_over_2_symb ,alias_at_prod,_VECT,_SEQ__VECT,&cst_pi_over_2_refv);
  const define_alias_gen(alias_cst_pi_over_2,_SYMB,0,&cst_pi_over_2_symb);
  static const gen cst_pi_over_2_storage(cst_pi*gen_inv_2);
  const gen & cst_pi_over_2 = cst_pi_over_2_storage;

  const define_alias_ref_symbolic( plus_inf_symb ,alias_at_plus,_IDNT,0,&ref_infinity);
  const define_alias_gen(alias_plus_inf,_SYMB,0,&plus_inf_symb);
  static const gen plus_inf_storage(symbolic(at_plus,_IDNT_infinity()));
  const gen & plus_inf = plus_inf_storage;
  const define_alias_ref_symbolic( minus_inf_symb ,alias_at_neg,_IDNT,0,&ref_infinity);
  const define_alias_gen(alias_minus_inf,_SYMB,0,&minus_inf_symb);
  static const gen minus_inf_storage(symbolic(at_neg,_IDNT_infinity()));
  const gen & minus_inf = minus_inf_storage;

  const define_alias_ref_fraction(plus_one_half_ref,_INT_,0,1,_INT_,0,2);
  const define_alias_gen(alias_plus_one_half,_FRAC,0,&plus_one_half_ref);
  static const gen plus_one_half_storage(fraction(1,2));
  const gen & plus_one_half = plus_one_half_storage;
  const define_alias_ref_symbolic( minus_one_half_symb ,alias_at_neg,_SYMB,0,&inv_2_symb);
  const define_alias_gen(alias_minus_one_half,_SYMB,0,&minus_one_half_symb);
  static const gen minus_one_half_storage(symbolic(at_neg,symb_inv(2)));
  const gen & minus_one_half = minus_one_half_storage;
  
  const define_tab2_alias_gen(alias_plus_sqrt3_tab,_INT_,0,3,_FRAC,0,&plus_one_half_ref);
  const define_alias_ref_vecteur2(plus_sqrt3_refv,alias_plus_sqrt3_tab);

  const define_alias_ref_symbolic( plus_sqrt3_symb ,alias_at_pow,_VECT,_SEQ__VECT,&plus_sqrt3_refv);
  const define_alias_gen(alias_plus_sqrt3,_SYMB,0,&plus_sqrt3_symb);
  static const gen plus_sqrt3_storage(symbolic(at_pow,gen(makevecteur(3,plus_one_half),_SEQ__VECT)));
  const gen & plus_sqrt3 = plus_sqrt3_storage;

  const define_tab2_alias_gen(alias_plus_sqrt2_tab,_INT_,0,2,_FRAC,0,&plus_one_half_ref);
  const define_alias_ref_vecteur2(plus_sqrt2_refv,alias_plus_sqrt2_tab);
  const define_alias_ref_symbolic( plus_sqrt2_symb ,alias_at_pow,_VECT,_SEQ__VECT,&plus_sqrt2_refv);
  const define_alias_gen(alias_plus_sqrt2,_SYMB,0,&plus_sqrt2_symb);
  static const gen plus_sqrt2_storage(symbolic(at_pow,gen(makevecteur(2,plus_one_half),_SEQ__VECT)));
  const gen & plus_sqrt2 = plus_sqrt2_storage;

  const define_tab2_alias_gen(alias_plus_sqrt6_tab,_INT_,0,6,_FRAC,0,&plus_one_half_ref);
  const define_alias_ref_vecteur2(plus_sqrt6_refv,alias_plus_sqrt6_tab);
  const define_alias_ref_symbolic( plus_sqrt6_symb ,alias_at_pow,_VECT,_SEQ__VECT,&plus_sqrt6_refv);
  const define_alias_gen(alias_plus_sqrt6,_SYMB,0,&plus_sqrt6_symb);
  static const gen plus_sqrt6_storage(symbolic(at_pow,gen(makevecteur(6,plus_one_half),_SEQ__VECT)));
  const gen & plus_sqrt6 = plus_sqrt6_storage;

  const define_alias_ref_symbolic( minus_sqrt2_symb ,alias_at_neg,_SYMB,0,&plus_sqrt2_symb);
  const define_alias_gen(alias_minus_sqrt2,_SYMB,0,&minus_sqrt2_symb);
  static const gen minus_sqrt2_storage(symbolic(at_neg,plus_sqrt2));
  const gen & minus_sqrt2 = minus_sqrt2_storage;

  const define_alias_ref_symbolic( minus_sqrt3_symb ,alias_at_neg,_SYMB,0,&plus_sqrt3_symb);
  const define_alias_gen(alias_minus_sqrt3,_SYMB,0,&minus_sqrt3_symb);
  static const gen minus_sqrt3_storage(symbolic(at_neg,plus_sqrt3));
  const gen & minus_sqrt3 = minus_sqrt3_storage;

  const define_alias_ref_symbolic( minus_sqrt6_symb ,alias_at_neg,_SYMB,0,&plus_sqrt6_symb);
  const define_alias_gen(alias_minus_sqrt6,_SYMB,0,&minus_sqrt6_symb);
  static const gen minus_sqrt6_storage(symbolic(at_neg,plus_sqrt6));
  const gen & minus_sqrt6 = minus_sqrt6_storage;

  const define_tab2_alias_gen(alias_minus_sqrt3_2_tab,_SYMB,0,&minus_sqrt3_symb,_SYMB,0,&inv_2_symb);
  const define_alias_ref_vecteur2(minus_sqrt3_2_refv,alias_minus_sqrt3_2_tab);
  const define_alias_ref_symbolic( minus_sqrt3_2_symb ,alias_at_prod,_VECT,_SEQ__VECT,&minus_sqrt3_2_refv);
  const define_alias_gen(alias_minus_sqrt3_2,_SYMB,0,&minus_sqrt3_2_symb);
  static const gen minus_sqrt3_2_storage(minus_sqrt3*gen_inv_2);
  const gen & minus_sqrt3_2 = minus_sqrt3_2_storage;

  const define_tab2_alias_gen(alias_minus_sqrt2_2_tab,_SYMB,0,&minus_sqrt2_symb,_SYMB,0,&inv_2_symb);
  const define_alias_ref_vecteur2(minus_sqrt2_2_refv,alias_minus_sqrt2_2_tab);
  const define_alias_ref_symbolic( minus_sqrt2_2_symb ,alias_at_prod,_VECT,_SEQ__VECT,&minus_sqrt2_2_refv);
  const define_alias_gen(alias_minus_sqrt2_2,_SYMB,0,&minus_sqrt2_2_symb);
  static const gen minus_sqrt2_2_storage(minus_sqrt2*gen_inv_2);
  const gen & minus_sqrt2_2 = minus_sqrt2_2_storage;

  const define_tab2_alias_gen(alias_minus_sqrt3_3_tab,_SYMB,0,&minus_sqrt3_symb,_SYMB,0,&inv_3_symb);
  const define_alias_ref_vecteur2(minus_sqrt3_3_refv,alias_minus_sqrt3_3_tab);
  const define_alias_ref_symbolic( minus_sqrt3_3_symb ,alias_at_prod,_VECT,_SEQ__VECT,&minus_sqrt3_3_refv);
  const define_alias_gen(alias_minus_sqrt3_3,_SYMB,0,&minus_sqrt3_3_symb);
  static const gen minus_sqrt3_3_storage(minus_sqrt3*gen_inv_3);
  const gen & minus_sqrt3_3 = minus_sqrt3_3_storage;

  const define_tab2_alias_gen(alias_plus_sqrt3_2_tab,_SYMB,0,&plus_sqrt3_symb,_SYMB,0,&inv_2_symb);
  const define_alias_ref_vecteur2(plus_sqrt3_2_refv,alias_plus_sqrt3_2_tab);
  const define_alias_ref_symbolic( plus_sqrt3_2_symb ,alias_at_prod,_VECT,_SEQ__VECT,&plus_sqrt3_2_refv);
  const define_alias_gen(alias_plus_sqrt3_2,_SYMB,0,&plus_sqrt3_2_symb);
  static const gen plus_sqrt3_2_storage(plus_sqrt3*gen_inv_2);
  const gen & plus_sqrt3_2 = plus_sqrt3_2_storage;

  const define_tab2_alias_gen(alias_plus_sqrt2_2_tab,_SYMB,0,&plus_sqrt2_symb,_SYMB,0,&inv_2_symb);
  const define_alias_ref_vecteur2(plus_sqrt2_2_refv,alias_plus_sqrt2_2_tab);
  const define_alias_ref_symbolic( plus_sqrt2_2_symb ,alias_at_prod,_VECT,_SEQ__VECT,&plus_sqrt2_2_refv);
  const define_alias_gen(alias_plus_sqrt2_2,_SYMB,0,&plus_sqrt2_2_symb);
  static const gen plus_sqrt2_2_storage(plus_sqrt2*gen_inv_2);
  const gen & plus_sqrt2_2 = plus_sqrt2_2_storage;

  const define_tab2_alias_gen(alias_plus_sqrt3_3_tab,_SYMB,0,&plus_sqrt3_symb,_SYMB,0,&inv_3_symb);
  const define_alias_ref_vecteur2(plus_sqrt3_3_refv,alias_plus_sqrt3_3_tab);
  const define_alias_ref_symbolic( plus_sqrt3_3_symb ,alias_at_prod,_VECT,_SEQ__VECT,&plus_sqrt3_3_refv);
  const define_alias_gen(alias_plus_sqrt3_3,_SYMB,0,&plus_sqrt3_3_symb);
  static const gen plus_sqrt3_3_storage(plus_sqrt3*gen_inv_3);
  const gen & plus_sqrt3_3 = plus_sqrt3_3_storage;

  const define_tab2_alias_gen(alias_cos_pi_12_4_tab,_SYMB,0,&plus_sqrt6_symb,_SYMB,0,&plus_sqrt2_symb);
  const define_alias_ref_vecteur2(cos_pi_12_4_refv,alias_cos_pi_12_4_tab);
  const define_alias_ref_symbolic( cos_pi_12_4_symb ,alias_at_plus,_VECT,_SEQ__VECT,&cos_pi_12_4_refv);

  const define_tab2_alias_gen(alias_cos_pi_12_tab,_SYMB,0,&cos_pi_12_4_symb,_SYMB,0,&inv_4_symb);
  const define_alias_ref_vecteur2(cos_pi_12_refv,alias_cos_pi_12_tab);
  const define_alias_ref_symbolic( cos_pi_12_symb ,alias_at_prod,_VECT,_SEQ__VECT,&cos_pi_12_refv);
  const define_alias_gen(alias_cos_pi_12,_SYMB,0,&cos_pi_12_symb);
  static const gen cos_pi_12_storage((plus_sqrt6+plus_sqrt2)*gen_inv_4);
  const gen & cos_pi_12 = cos_pi_12_storage;

  const define_tab2_alias_gen(alias_minus_cos_pi_12_4_tab,_SYMB,0,&minus_sqrt6_symb,_SYMB,0,&minus_sqrt2_symb);
  const define_alias_ref_vecteur2(minus_cos_pi_12_4_refv,alias_minus_cos_pi_12_4_tab);
  const define_alias_ref_symbolic( minus_cos_pi_12_4_symb ,alias_at_plus,_VECT,_SEQ__VECT,&minus_cos_pi_12_4_refv);
  const define_tab2_alias_gen(alias_minus_cos_pi_12_tab,_SYMB,0,&minus_cos_pi_12_4_symb,_SYMB,0,&inv_4_symb);
  const define_alias_ref_vecteur2(minus_cos_pi_12_refv,alias_minus_cos_pi_12_tab);
  const define_alias_ref_symbolic( minus_cos_pi_12_symb ,alias_at_prod,_VECT,_SEQ__VECT,&minus_cos_pi_12_refv);
  const define_alias_gen(alias_minus_cos_pi_12,_SYMB,0,&minus_cos_pi_12_symb);
  static const gen minus_cos_pi_12_storage(-cos_pi_12);
  const gen & minus_cos_pi_12 = minus_cos_pi_12_storage;

  const define_tab2_alias_gen(alias_sin_pi_12_4_tab,_SYMB,0,&plus_sqrt6_symb,_SYMB,0,&minus_sqrt2_symb);
  const define_alias_ref_vecteur2(sin_pi_12_4_refv,alias_sin_pi_12_4_tab);
  const define_alias_ref_symbolic( sin_pi_12_4_symb ,alias_at_plus,_VECT,_SEQ__VECT,&sin_pi_12_4_refv);
  const define_tab2_alias_gen(alias_sin_pi_12_tab,_SYMB,0,&sin_pi_12_4_symb,_SYMB,0,&inv_4_symb);
  const define_alias_ref_vecteur2(sin_pi_12_refv,alias_sin_pi_12_tab);
  const define_alias_ref_symbolic( sin_pi_12_symb ,alias_at_prod,_VECT,_SEQ__VECT,&sin_pi_12_refv);
  const define_alias_gen(alias_sin_pi_12,_SYMB,0,&sin_pi_12_symb);
  static const gen sin_pi_12_storage((plus_sqrt6+minus_sqrt2)*gen_inv_4);
  const gen & sin_pi_12 = sin_pi_12_storage;

  const define_tab2_alias_gen(alias_minus_sin_pi_12_4_tab,_SYMB,0,&plus_sqrt2_symb,_SYMB,0,&minus_sqrt6_symb);
  const define_alias_ref_vecteur2(minus_sin_pi_12_4_refv,alias_minus_sin_pi_12_4_tab);
  const define_alias_ref_symbolic( minus_sin_pi_12_4_symb ,alias_at_plus,_VECT,_SEQ__VECT,&minus_sin_pi_12_4_refv);
  const define_tab2_alias_gen(alias_minus_sin_pi_12_tab,_SYMB,0,&minus_sin_pi_12_4_symb,_SYMB,0,&inv_4_symb);
  const define_alias_ref_vecteur2(minus_sin_pi_12_refv,alias_minus_sin_pi_12_tab);
  const define_alias_ref_symbolic( minus_sin_pi_12_symb ,alias_at_prod,_VECT,_SEQ__VECT,&minus_sin_pi_12_refv);
  const define_alias_gen(alias_minus_sin_pi_12,_SYMB,0,&minus_sin_pi_12_symb);
  static const gen minus_sin_pi_12_storage(-sin_pi_12);
  const gen & minus_sin_pi_12 = minus_sin_pi_12_storage;

  const define_tab2_alias_gen(alias_tan_pi_12_tab,_INT_,0,2,_SYMB,0,&minus_sqrt3_symb);
  const define_alias_ref_vecteur2(tan_pi_12_refv,alias_tan_pi_12_tab);
  const define_alias_ref_symbolic( tan_pi_12_symb ,alias_at_plus,_VECT,_SEQ__VECT,&tan_pi_12_refv);
  const define_alias_gen(alias_tan_pi_12,_SYMB,0,&tan_pi_12_symb);
  static const gen tan_pi_12_storage(2+minus_sqrt3);
  const gen & tan_pi_12 = tan_pi_12_storage;

  const define_tab2_alias_gen(alias_tan_5pi_12_tab,_INT_,0,2,_SYMB,0,&plus_sqrt3_symb);
  const define_alias_ref_vecteur2(tan_5pi_12_refv,alias_tan_5pi_12_tab);
  const define_alias_ref_symbolic( tan_5pi_12_symb ,alias_at_plus,_VECT,_SEQ__VECT,&tan_5pi_12_refv);
  const define_alias_gen(alias_tan_5pi_12,_SYMB,0,&tan_5pi_12_symb);
  static const gen tan_5pi_12_storage(2+plus_sqrt3);
  const gen & tan_5pi_12 = tan_5pi_12_storage;

  const define_alias_ref_symbolic( minus_tan_pi_12_symb ,alias_at_neg,_SYMB,0,&tan_pi_12_symb);
  const define_alias_gen(alias_minus_tan_pi_12,_SYMB,0,&minus_tan_pi_12_symb);
  static const gen minus_tan_pi_12_storage(-tan_pi_12);
  const gen & minus_tan_pi_12 = minus_tan_pi_12_storage;

  const define_alias_ref_symbolic( minus_tan_5pi_12_symb ,alias_at_neg,_SYMB,0,&tan_5pi_12_symb);
  const define_alias_gen(alias_minus_tan_5pi_12,_SYMB,0,&minus_tan_5pi_12_symb);
  static const gen minus_tan_5pi_12_storage(-tan_5pi_12);
  const gen & minus_tan_5pi_12 = minus_tan_5pi_12_storage;

  const define_alias_ref_symbolic( inv_pi_symb,alias_at_inv,_IDNT,0,&ref_pi);
  const define_alias_gen(alias_inv_pi,_SYMB,0,&inv_pi_symb);
  static const gen cst_inv_pi_storage(symb_inv(cst_pi));
  const gen & cst_inv_pi = cst_inv_pi_storage;

  const define_alias_ref_symbolic( inv_180_symb,alias_at_inv,_INT_,0,180);
  const define_alias_gen(alias_inv_180,_SYMB,0,&inv_180_symb);
  static const gen cst_inv_180_storage(symb_inv(180));
  const gen & cst_inv_180 = cst_inv_180_storage;

  const define_tab2_alias_gen(alias_rad2deg_e_tab,_INT_,0,180,_SYMB,0,&inv_pi_symb);
  const define_alias_ref_vecteur2(rad2deg_e_refv,alias_rad2deg_e_tab);
  const define_alias_ref_symbolic( rad2deg_e_symb ,(size_t)&_prod,_VECT,_SEQ__VECT,&rad2deg_e_refv);
  const define_alias_gen(alias_rad2deg_e,_SYMB,0,&rad2deg_e_symb);
  const gen & rad2deg_e = *(const gen *)&alias_rad2deg_e;

  const define_tab2_alias_gen(alias_deg2rad_e_tab,_IDNT,0,&ref_pi,_SYMB,0,&inv_180_symb);
  const define_alias_ref_vecteur2(deg2rad_e_refv,alias_deg2rad_e_tab);
  const define_alias_ref_symbolic( deg2rad_e_symb ,(size_t)&__prod,_VECT,_SEQ__VECT,&deg2rad_e_refv);
  const define_alias_gen(alias_deg2rad_e,_SYMB,0,&deg2rad_e_symb);
  const gen & deg2rad_e = *(const gen *)&alias_deg2rad_e;

  //grad
  const define_tab2_alias_gen(alias_rad2grad_e_tab, _INT_, 0, 200, _SYMB, 0, &inv_pi_symb);
  const define_alias_ref_vecteur2(rad2grad_e_refv, alias_rad2grad_e_tab);
  const define_alias_ref_symbolic(rad2grad_e_symb, (size_t)&_prod, _VECT, _SEQ__VECT, &rad2grad_e_refv);
  const define_alias_gen(alias_rad2grad_e, _SYMB, 0, &rad2grad_e_symb);
  const gen & rad2grad_e = *(const gen *)&alias_rad2grad_e;

  const define_tab2_alias_gen(alias_grad2rad_e_tab, _IDNT, 0, &ref_pi, _SYMB, 0, &inv_180_symb);
  const define_alias_ref_vecteur2(grad2rad_e_refv, alias_grad2rad_e_tab);
  const define_alias_ref_symbolic(grad2rad_e_symb, (size_t)&__prod, _VECT, _SEQ__VECT, &grad2rad_e_refv);
  const define_alias_gen(alias_grad2rad_e, _SYMB, 0, &grad2rad_e_symb);
  const gen & grad2rad_e = *(const gen *)&alias_grad2rad_e;


  // 0 = -pi, 12=0, 24=pi
  static const gen * const table_cos_gen[trig_deno+1]={
    &minus_one,&minus_cos_pi_12,&minus_sqrt3_2,&minus_sqrt2_2,&minus_one_half,&minus_sin_pi_12,
    &zero,&sin_pi_12,&plus_one_half,&plus_sqrt2_2,&plus_sqrt3_2,&cos_pi_12,
    &plus_one,&cos_pi_12,&plus_sqrt3_2,&plus_sqrt2_2,&plus_one_half,&sin_pi_12,
    &zero,&minus_sin_pi_12,&minus_one_half,&minus_sqrt2_2,&minus_sqrt3_2,&minus_cos_pi_12,
    &minus_one
  };
  const gen * const * table_cos = table_cos_gen;
  static const gen * const table_tan_gen[trig_deno/2+1]={
    &zero,&tan_pi_12,&plus_sqrt3_3,&plus_one,&plus_sqrt3,&tan_5pi_12,
    &unsigned_inf,&minus_tan_5pi_12,&minus_sqrt3,&minus_one,&minus_sqrt3_3,&minus_tan_pi_12,
    &zero
  };
  const gen * const * table_tan = table_tan_gen;

#endif // GIAC_GENERIC_CONSTANTS

#ifdef XLIGHT
    const alias_type reim_op_alias[]={(alias_type)&__inv,(alias_type)&__exp,(alias_type)&__cos,(alias_type)&__sin,(alias_type)&__tan,(alias_type)&__cosh,(alias_type)&__sinh,(alias_type)&__tanh,(alias_type)&__atan,(alias_type)&__lnGamma_minus,(alias_type)&__Gamma,(alias_type)&__sign,(alias_type) & __of,0};
#else
  const alias_type reim_op_alias[]={(alias_type)&__inv,(alias_type)&__exp,(alias_type)&__cos,(alias_type)&__sin,(alias_type)&__tan,(alias_type)&__cosh,(alias_type)&__sinh,(alias_type)&__tanh,(alias_type)&__atan,(alias_type)&__lnGamma_minus,(alias_type)&__Gamma,(alias_type)&__Psi_minus_ln,(alias_type)&__Psi,(alias_type)&__Zeta,(alias_type)&__Eta,(alias_type)&__sign,(alias_type)&__erf,(alias_type) & __of,0};
#endif
  const unary_function_ptr * const reim_op=(const unary_function_ptr * const)reim_op_alias;
  // for subst.cc
  const alias_type sincostan_tab_alias[]={(alias_type)&__sin,(alias_type)&__cos,(alias_type)&__tan,0};
  const unary_function_ptr * const sincostan_tab=(const unary_function_ptr * const) sincostan_tab_alias;

  const alias_type asinacosatan_tab_alias[] = {(alias_type)&__asin,(alias_type)&__acos,(alias_type)&__atan,0};
  const unary_function_ptr * const asinacosatan_tab=(const unary_function_ptr * const) asinacosatan_tab_alias;

  const alias_type sinhcoshtanh_tab_alias[]={alias_at_sinh,alias_at_cosh,alias_at_tanh,0};
  const unary_function_ptr * const sinhcoshtanh_tab=(const unary_function_ptr * const)sinhcoshtanh_tab_alias;

  const alias_type sinhcoshtanhinv_tab_alias[]={(alias_type)&__sinh,(alias_type)&__cosh,(alias_type)&__tanh,(alias_type)&__inv,0};
  const unary_function_ptr * const sinhcoshtanhinv_tab=(const unary_function_ptr * const)sinhcoshtanhinv_tab_alias;

  const alias_type sincostansinhcoshtanh_tab_alias[]={(alias_type)&__sin,(alias_type)&__cos,(alias_type)&__tan,(alias_type)&__sinh,(alias_type)&__cosh,(alias_type)&__tanh,0};
  const unary_function_ptr * const sincostansinhcoshtanh_tab=(const unary_function_ptr * const)sincostansinhcoshtanh_tab_alias;

  // vector<unary_function_ptr> sincostan_v(sincostan_tab,sincostan_tab+3);
  // vector<unary_function_ptr> asinacosatan_v(asinacosatan_tab,asinacosatan_tab+3);
  // vector<unary_function_ptr> sinhcoshtanh_v(sinhcoshtanh_tab,sinhcoshtanh_tab+3);
  // vector <unary_function_ptr> sincostansinhcoshtanh_v(merge(sincostan_v,sinhcoshtanh_v));
  const alias_type sign_floor_ceil_round_tab_alias[]={(alias_type)&__sign,(alias_type)&__floor,(alias_type)&__ceil,(alias_type)&__round,(alias_type)&__sum,0};
  const unary_function_ptr * const sign_floor_ceil_round_tab=(const unary_function_ptr *const )sign_floor_ceil_round_tab_alias;

  // vector<unary_function_ptr> sign_floor_ceil_round_v(sign_floor_ceil_round_tab,sign_floor_ceil_round_tab+5);
  const alias_type exp_tab_alias[]={(const alias_type)&__exp,0};
  const unary_function_ptr * const exp_tab=(const unary_function_ptr * const)exp_tab_alias;

  const alias_type tan_tab_alias[]={(const alias_type)&__tan,0};
  const unary_function_ptr * const tan_tab=(const unary_function_ptr * const)tan_tab_alias;

  const alias_type asin_tab_alias[]={(const alias_type)&__asin,0};
  const unary_function_ptr * const asin_tab=(const unary_function_ptr * const)asin_tab_alias;

  const alias_type acos_tab_alias[]={(const alias_type)&__acos,0};
  const unary_function_ptr * const acos_tab=(const unary_function_ptr * const)acos_tab_alias;

  const alias_type atan_tab_alias[]={(const alias_type)&__atan,0};
  const unary_function_ptr * const atan_tab=(const unary_function_ptr * const)atan_tab_alias;

  const alias_type pow_tab_alias[]={(const alias_type)&__pow,0};
  const unary_function_ptr * const pow_tab=(const unary_function_ptr * const)pow_tab_alias;

  const alias_type Heaviside_tab_alias[]={alias_at_Heaviside,0};
  const unary_function_ptr * const Heaviside_tab=(const unary_function_ptr * const)Heaviside_tab_alias;

  const alias_type invpowtan_tab_alias[]={alias_at_inv,alias_at_pow,alias_at_tan,0};
  const unary_function_ptr * const invpowtan_tab=(const unary_function_ptr * const) invpowtan_tab_alias;

  const gen_op_context halftan_tab[]={sin2tan2,cos2tan2,tan2tan2,0};
  const gen_op_context hyp2exp_tab[]={sinh2exp,cosh2exp,tanh2exp,0};
  const gen_op_context hypinv2exp_tab[]={sinh2exp,cosh2exp,tanh2exp,inv_test_exp,0};
  const gen_op_context trig2exp_tab[]={sin2exp,cos2exp,tan2exp,0};
  const gen_op_context atrig2ln_tab[]={asin2ln,acos2ln,atan2ln,0};
  // vector< gen_op_context > halftan_v(halftan_tab,halftan_tab+3);
  // vector< gen_op_context > hyp2exp_v(hyp2exp_tab,hyp2exp_tab+3);
  // vector< gen_op_context > trig2exp_v(trig2exp_tab,trig2exp_tab+3);
  const gen_op_context halftan_hyp2exp_tab[]={sin2tan2,cos2tan2,tan2tan2,sinh2exp,cosh2exp,tanh2exp,0};
  const gen_op_context exp2sincos_tab[]={exp2sincos,0};
  const gen_op_context tan2sincos_tab[]={tantosincos,0};
  const gen_op_context tan2sincos2_tab[]={tantosincos2,0};
  const gen_op_context tan2cossin2_tab[]={tantocossin2,0};
  const gen_op_context asin2acos_tab[]={asintoacos,0};
  const gen_op_context asin2atan_tab[]={asintoatan,0};
  const gen_op_context acos2asin_tab[]={acostoasin,0};
  const gen_op_context acos2atan_tab[]={acostoatan,0};
  const gen_op_context atan2asin_tab[]={atantoasin,0};
  const gen_op_context atan2acos_tab[]={atantoacos,0};
  // vector< gen_op_context > atrig2ln_v(atrig2ln_tab,atrig2ln_tab+3);
  const gen_op_context trigcos_tab[]={trigcospow,0};
  const gen_op_context trigsin_tab[]={trigsinpow,0};
  const gen_op_context trigtan_tab[]={trigtanpow,0};
  const gen_op_context powexpand_tab[]={powtopowexpand,0};
  const gen_op_context powneg2invpow_tab[]={pownegtoinvpow,0};
  const gen_op_context exp2power_tab[]={exptopower,0};
  const alias_type gamma_tab_alias[]={alias_at_Gamma,0};
  const unary_function_ptr * const gamma_tab=(const unary_function_ptr * const)gamma_tab_alias;

  const gen_op_context gamma2factorial_tab[]={gammatofactorial,0};
  const alias_type factorial_tab_alias[]={alias_at_factorial,0};
  const unary_function_ptr * const factorial_tab=(const unary_function_ptr * const)factorial_tab_alias;

  const gen_op_context factorial2gamma_tab[]={factorialtogamma,0};

  // for integration
  const alias_type  primitive_tab_op_alias[]={ (const alias_type)&__sin, (const alias_type)&__cos, (const alias_type)&__tan, (const alias_type)&__exp, (const alias_type)&__sinh, (const alias_type)&__cosh, (const alias_type)&__tanh, (const alias_type)&__asin, (const alias_type)&__acos, (const alias_type)&__atan, (const alias_type)&__ln,(const alias_type)&__asinh, (const alias_type)&__acosh, (const alias_type)&__atanh,0};
  const unary_function_ptr * const primitive_tab_op=(const unary_function_ptr * const)primitive_tab_op_alias;
#ifdef XLIGHT
  const alias_type inverse_tab_op_alias[]={ (const alias_type)&__asin, (const alias_type)&__acos, (const alias_type)&__atan, (const alias_type)&__ln, (const alias_type)&__asinh, (const alias_type)&__acosh, (const alias_type)&__atanh, 0};
#else
  const alias_type inverse_tab_op_alias[]={ (const alias_type)&__asin, (const alias_type)&__acos, (const alias_type)&__atan, (const alias_type)&__ln, (const alias_type)&__asinh, (const alias_type)&__acosh, (const alias_type)&__atanh, (const alias_type)&__erf, (const alias_type)&__erfc,0};
#endif
  const unary_function_ptr * const inverse_tab_op=(const unary_function_ptr * const)inverse_tab_op_alias;

  const alias_type  analytic_sommets_alias[]={ (const alias_type)&__plus, (const alias_type)&__prod, (const alias_type)&__neg, (const alias_type)&__inv, (const alias_type)&__pow, (const alias_type)&__sin, (const alias_type)&__cos, (const alias_type)&__tan, (const alias_type)&__exp, (const alias_type)&__sinh, (const alias_type)&__cosh, (const alias_type)&__tanh, (const alias_type)&__asin, (const alias_type)&__acos, (const alias_type)&__atan, (const alias_type)&__asinh, (const alias_type)&__atanh, (const alias_type)&__acosh, (const alias_type)&__ln, (const alias_type)&__sqrt,0};  
  const unary_function_ptr * const analytic_sommets=(const unary_function_ptr * const)analytic_sommets_alias;
  // test if g is < > <= >=, 
  const alias_type  inequality_tab_alias[]={ (const alias_type)&__equal, (const alias_type)&__inferieur_strict, (const alias_type)&__inferieur_egal, (const alias_type)&__different, (const alias_type)&__superieur_strict, (const alias_type)&__superieur_egal,0};
  const unary_function_ptr * const inequality_tab=(const unary_function_ptr * const)inequality_tab_alias;
  // if you add functions to solve_fcns, modify the second argument of solve_fcns_v to reflect the number of functions in the array
  const alias_type  solve_fcns_tab_alias[]={  (const alias_type)&__exp, (const alias_type)&__ln, (const alias_type)&__sin, (const alias_type)&__cos, (const alias_type)&__tan, (const alias_type)&__asin, (const alias_type)&__acos, (const alias_type)&__atan, (const alias_type)&__sinh, (const alias_type)&__cosh, (const alias_type)&__tanh, (const alias_type)&__asinh, (const alias_type)&__acosh, (const alias_type)&__atanh,0};
  const unary_function_ptr * const solve_fcns_tab = (const unary_function_ptr * const)solve_fcns_tab_alias;

#ifdef XLIGHT
  const alias_type limit_tab_alias[]={(const alias_type)&__Gamma,(const alias_type)&__lower_incomplete_gamma,0};
  const unary_function_ptr * const limit_tab = (const unary_function_ptr * const) limit_tab_alias;
  const gen_op_context limit_replace [] = {Gamma_replace,igamma_replace,0};
#else  
  const alias_type limit_tab_alias[]={(const alias_type)&__Gamma,(const alias_type)&__Psi,(const alias_type)&__erf,(const alias_type)&__lower_incomplete_gamma,0};
  const unary_function_ptr * const limit_tab = (const unary_function_ptr * const) limit_tab_alias;
  const gen_op_context limit_replace [] = {Gamma_replace,Psi_replace,erf_replace,igamma_replace,0};
#endif

  // vector<unary_function_ptr> inequality_sommets(inequality_tab,inequality_tab+sizeof(inequality_tab)/sizeof(unary_function_ptr));
  int is_inequality(const gen & g){
    if (g.type!=_SYMB)
      return false;
    return equalposcomp(inequality_tab,g._SYMBptr->sommet);
  }


  string unquote(const string & s){
    int l=int(s.size());
    if (l>2 && s[0]=='"' && s[l-1]=='"')
      return s.substr(1,l-2);
    else
      return s;
  }

#ifdef NSPIRE
  template<class T>
  nio::ios_base<T> & operator << (nio::ios_base<T> & os,const alias_ref_vecteur & v){
#ifdef IMMEDIATE_VECTOR
    os << &v << ":" << *(gen *)v.begin_immediate_vect << "," << *(gen*) (v.begin_immediate_vect+1);
#else
    os << &v ;
#endif
    return os;
  }
#else
  ostream & operator << (ostream & os,const alias_ref_vecteur & v){
#ifdef IMMEDIATE_VECTOR
    os << (unsigned long) &v << ":" << *(gen *)v.begin_immediate_vect << "," << *(gen*) (v.begin_immediate_vect+1);
#else
    os << (unsigned long) &v ;
#endif
    return os;
  }
#endif

  void fonction_bidon(){
#if !defined GIAC_GENERIC_CONSTANTS && !defined NSPIRE_NEWLIB && !defined FXCG && !defined TICE
    ofstream of("log");
    of << gen_inv_2 << endl;
    of <<  alias_cst_two_pi_tab << " " <<  cst_two_pi_refv << endl;
    of <<  alias_cst_pi_over_2_tab << " " <<  cst_pi_over_2_refv << endl;
    of <<  alias_plus_sqrt3_tab << " " <<  plus_sqrt3_refv << endl;
    of <<  alias_plus_sqrt2_tab << " " <<  plus_sqrt2_refv << endl;
    of <<  alias_plus_sqrt6_tab << " " <<  plus_sqrt6_refv << endl;
    of <<  alias_minus_sqrt3_2_tab << " " <<  minus_sqrt3_2_refv << endl;
    of <<  alias_minus_sqrt2_2_tab << " " <<  minus_sqrt2_2_refv << endl;
    of <<  alias_minus_sqrt3_3_tab << " " <<  minus_sqrt3_3_refv << endl;
    of <<  alias_plus_sqrt3_2_tab << " " <<  plus_sqrt3_2_refv << endl;
    of <<  alias_plus_sqrt2_2_tab << " " <<  plus_sqrt2_2_refv << endl;
    of <<  alias_plus_sqrt3_3_tab << " " <<  plus_sqrt3_3_refv << endl;
    of <<  alias_cos_pi_12_4_tab << " " <<  cos_pi_12_4_refv << endl;
    of <<  alias_cos_pi_12_tab << " " <<  cos_pi_12_refv << endl;
    of <<  alias_minus_cos_pi_12_4_tab << " " <<  minus_cos_pi_12_4_refv << endl;
    of <<  alias_minus_cos_pi_12_tab << " " <<  minus_cos_pi_12_refv << endl;
    of <<  alias_sin_pi_12_4_tab << " " <<  sin_pi_12_4_refv << endl;
    of <<  alias_sin_pi_12_tab << " " <<  sin_pi_12_refv << endl;
    of <<  alias_minus_sin_pi_12_4_tab << " " <<  minus_sin_pi_12_4_refv << endl;
    of <<  alias_minus_sin_pi_12_tab << " " <<  minus_sin_pi_12_refv << endl;
    of <<  alias_tan_pi_12_tab << " " <<  tan_pi_12_refv << endl;
    of <<  alias_tan_5pi_12_tab << " " <<  tan_5pi_12_refv << endl;
    of <<  alias_rad2deg_e_tab << " " <<  rad2deg_e_refv << endl;
    of <<  alias_deg2rad_e_tab << " " <<  deg2rad_e_refv << endl;
    of << plus_inf << " ";
    of << minus_inf << " ";
    of << plus_one_half << " ";
    of << minus_one_half << " ";
    of << plus_sqrt3 << " ";
    of << plus_sqrt2 << " ";
    of << plus_sqrt6 << " ";
    of << minus_sqrt2 << " ";
    of << minus_sqrt3 << " ";
    of << minus_sqrt6 << " ";
    of << minus_sqrt3_2 << " ";
    of << minus_sqrt2_2 << " ";
    of << minus_sqrt3_3 << " ";
    of << plus_sqrt3_2 << " " ;
    of << plus_sqrt2_2 << " ";
    of << plus_sqrt3_3 << " ";
    of << cos_pi_12 << " ";
    of << minus_cos_pi_12 << " ";
    of << sin_pi_12 << " ";
    of << minus_sin_pi_12 << " ";
    of << tan_pi_12 << " ";
    of << tan_5pi_12 << " ";
    of << minus_tan_pi_12 << " ";
    of << minus_tan_5pi_12 << " " << endl;
    of << cst_two_pi << " " ;
    of << cst_pi_over_2 << " ";
    of << cst_inv_pi << " ";
    of << cst_inv_180 << " " << endl;
    of << rad2deg_e << " " ;
    of << deg2rad_e << " " << endl;
#endif
  }

#ifndef NO_NAMESPACE_GIAC
} // namespace giac
#endif // ndef NO_NAMESPACE_GIAC
