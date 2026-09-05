/*
 * Modified for NumOS / NeoCalculator in 2026 for ESP32-S3 and host integration.
 * See ../NUMOS_CHANGES.md for provenance and the file-by-file history.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* -*- mode:C++ ; compile-command: "g++-3.4 -I.. -I../include -g -c threaded.cc -DHAVE_CONFIG_H -DIN_GIAC  -D_I386_" -*- */
#include "giacPCH.h"
/*  Copyright (C) 2000,2007 B. Parisse, Institut Fourier, 38402 St Martin d'Heres
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
#include "threaded.h"
#include "sym2poly.h"
#include "gausspol.h"
#include "usual.h"
#include "monomial.h"
#include "modpoly.h"
#include "giacintl.h"
#include "input_parser.h"
extern "C" void Sleep(int millisecond);
#if defined(TICE) || !defined(FXCG)
#include "k_csdk.h"
#endif
#ifdef TICE
inline void Sleep(int millis){ return os_wait_1ms(millis); }
#endif

#ifndef NO_NAMESPACE_GIAC
namespace giac {
#endif // ndef NO_NAMESPACE_GIAC

  bool gcd(const vector< T_unsigned<gen,hashgcd_U> > & p_orig,const vector< T_unsigned<gen,hashgcd_U> > & q_orig,vector< T_unsigned<gen,hashgcd_U> > & d, vector< T_unsigned<gen,hashgcd_U> > & pcofactor, vector< T_unsigned<gen,hashgcd_U> > & qcofactor,const std::vector<hashgcd_U> & vars, bool compute_cofactors,int nthreads){
    return false;
  }

  // v <- v*k % m
  void mulmod(vector<int> & v,int k,int m){
    if (k==1)
      return;
    vector<int>::iterator it=v.begin(),itend=v.end();
    for (;it!=itend;++it){
      type_operator_times_reduce(*it,k,*it,m);
      // *it = ((*it)*k)%m;
    }
  }

  bool gcd_ext(const vector< T_unsigned<gen,hashgcd_U> > & p_orig,const vector< T_unsigned<gen,hashgcd_U> > & q_orig,vector< T_unsigned<gen,hashgcd_U> > & d, vector< T_unsigned<gen,hashgcd_U> > & pcofactor, vector< T_unsigned<gen,hashgcd_U> > & qcofactor,const std::vector<hashgcd_U> & vars, bool compute_cofactors,int nthreads){
    return false;
  }

#ifndef TICE
  gen _debug_infolevel(const gen & g0,GIAC_CONTEXT){
    if ( g0.type==_STRNG && g0.subtype==-1) return  g0;
    gen g=evalf_double(g0,1,contextptr);
    if (g.type!=_DOUBLE_)
      return debug_infolevel;
    return debug_infolevel=int(g._DOUBLE_val);
  }
  static const char _debug_infolevel_s []="debug_infolevel";
  static define_unary_function_eval (__debug_infolevel,&_debug_infolevel,_debug_infolevel_s);
  define_unary_function_ptr5( at_debug_infolevel ,alias_at_debug_infolevel,&__debug_infolevel,0,T_DIGITS);
#endif

  void wait_1ms(int ms){
    os_wait_1ms(ms); return; 
    // OS_InnerWait_ms(ms);
    int l=0;
    for (int i=0;i<ms;++i){
      for (int j=0;j<1000;++j){
	l += j;
      }
    }
  }
  

  bool is_zero(const vector<int> & v){
    vector<int>::const_iterator it=v.begin(),itend=v.end();
    for (;it!=itend;++it){
      if (*it)
	return false;
    }
    return true;
  }

  inline int make_unit(int ){
    return 1;
  }
  
  inline vector<int> make_unit(const vector<int>){
    vector<int> v(1,1);
    return v;
  }


  bool mod_gcd(const vector< T_unsigned<vector<int>,hashgcd_U> > & p_orig,const vector< T_unsigned<vector<int>,hashgcd_U> > & q_orig,const vector<int> * pminptr,int modulo,const std::vector<hashgcd_U> & vars,
	      vector< T_unsigned<vector<int>,hashgcd_U> > & d,
	      vector< T_unsigned<vector<int>,hashgcd_U> > & pcof,vector< T_unsigned<vector<int>,hashgcd_U> > & qcof,bool compute_cof,
	      int nthreads){
    return false;
  }

  bool mod_gcd(const vector< T_unsigned<int,hashgcd_U> > & p_orig,const vector< T_unsigned<int,hashgcd_U> > & q_orig,const vector<int> * pminptr,int modulo,const std::vector<hashgcd_U> & vars, vector< T_unsigned<int,hashgcd_U> > & d, vector< T_unsigned<int,hashgcd_U> > & pcofactor, vector< T_unsigned<int,hashgcd_U> > & qcofactor,bool compute_cofactor,int nthreads){
    return false;
  }

  bool mod_gcd(const std::vector< T_unsigned<int,hashgcd_U> > & p_orig,const std::vector< T_unsigned<int,hashgcd_U> > & q_orig,int modulo,std::vector< T_unsigned<int,hashgcd_U> > & d, std::vector< T_unsigned<int,hashgcd_U> > & pcofactor, std::vector< T_unsigned<int,hashgcd_U> > & qcofactor,const std::vector<hashgcd_U> & vars, bool compute_pcofactor,bool compute_qcofactor,int nthreads){
    return false;
  }

  bool mod_gcd(const std::vector< T_unsigned<int,hashgcd_U> > & p_orig,const std::vector< T_unsigned<int,hashgcd_U> > & q_orig,int modulo,std::vector< T_unsigned<int,hashgcd_U> > & d, std::vector< T_unsigned<int,hashgcd_U> > & pcofactor, std::vector< T_unsigned<int,hashgcd_U> > & qcofactor,const std::vector<hashgcd_U> & vars, bool compute_cofactors,int nthreads){
    return false;
  }
  
#ifndef NO_NAMESPACE_GIAC
} // namespace giac
#endif // ndef NO_NAMESPACE_GIAC
