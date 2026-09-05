/*
 * Modified for NumOS / NeoCalculator in 2026 for ESP32-S3 and host integration.
 * See ../NUMOS_CHANGES.md for provenance and the file-by-file history.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// -*- mode:C++ ; compile-command: "g++-3.4 -I.. -g -c ifactor.cc -DHAVE_CONFIG_H -DIN_GIAC" -*-
#include "giacPCH.h"
#if !defined __MINGW_H && !defined FXCG && !defined TICE && !defined NSPIRE_NEWLIB
#define GIAC_MPQS // define if you want to use giac for sieving 
#endif



#include "path.h"
/*
 *  Copyright (C) 2003,14 R. De Graeve & B. Parisse, 
 *  Institut Fourier, 38402 St Martin d'Heres
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
#ifdef GIAC_HAS_STO_38
//#undef clock
//#undef clock_t
#else
#include <fstream>
//#include <unistd.h> // For reading arguments from file
#include "ifactor.h"
#include "pari.h"
#include "usual.h"
#include "sym2poly.h"
#include "rpn.h"
#include "prog.h"
#include "misc.h"
#include "giacintl.h"
#endif

#ifdef GIAC_HAS_STO_38
#define BESTA_OS
#endif
// Trying to make ifactor(2^128+1) work on ARM
#if defined(RTOS_THREADX) || defined(BESTA_OS) || defined NSPIRE
//#define OLD_AFACT
#define GIAC_ADDITIONAL_PRIMES 16// if defined, additional primes are used in sieve
#else
#define GIAC_ADDITIONAL_PRIMES 32// if defined, additional primes are used in sieve
#endif


#ifndef NO_NAMESPACE_GIAC
namespace giac {
#endif // ndef NO_NAMESPACE_GIAC

  // msieve is optional in upstream Giac; provide a stub when unavailable.
  static bool msieve(const gen &,gen &,GIAC_CONTEXT){
    return false;
  }

  const short int giac_primes[]={2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97,101,103,107,109,113,127,131,137,139,149,151,157,163,167,173,179,181,191,193,197,199,211,223,227,229,233,239,241,251,257,263,269,271,277,281,283,293,307,311,313,317,331,337,347,349,353,359,367,373,379,383,389,397,401,409,419,421,431,433,439,443,449,457,461,463,467,479,487,491,499,503,509,521,523,541,547,557,563,569,571,577,587,593,599,601,607,613,617,619,631,641,643,647,653,659,661,673,677,683,691,701,709,719,727,733,739,743,751,757,761,769,773,787,797,809,811,821,823,827,829,839,853,857,859,863,877,881,883,887,907,911,919,929,937,941,947,953,967,971,977,983,991,997};
  static const int giac_last_prime=giac_primes[sizeof(giac_primes)/sizeof(short)-1];
  int modulo(const mpz_t & a,unsigned b){
    if (mpz_cmp_ui(a,0)<0){
      mpz_neg(*(mpz_t *)&a,a);
      int res=modulo(a,b);
      mpz_neg(*(mpz_t *)&a,a);
      return b-res;
    }
    mp_digit C; 
    mp_mod_d((mp_int *)&a,b,&C);
    return C;
  }

  // Pollard-rho algorithm
  const int POLLARD_GCD=64;
#ifdef GIAC_MPQS 
#if defined(RTOS_THREADX) // !defined(BESTA_OS)
  const int POLLARD_MAXITER=3000;
#else
  const int POLLARD_MAXITER=15000;
#endif
#else
  const int POLLARD_MAXITER=15000;
#endif  

  static gen pollard(gen n, gen k,GIAC_CONTEXT){
    k.uncoerce();
    n.uncoerce();
    int maxiter=POLLARD_MAXITER;
    double nd=evalf_double(n,1,contextptr)._DOUBLE_val;
#if defined(RTOS_THREADX) || defined(BESTA_OS) || defined NSPIRE || defined FXCG || defined TICE
    int nd1=int(2000*(std::log10(nd)-34));
#else
    int nd1=int(1500*std::pow(16.,(std::log10(nd)-40)/10));
#endif
    if (nd1>maxiter)
      maxiter=nd1;
    int m,m1,a,a1,j;
    m1=m=2;
    a1=a=1;
    int c=0;
    mpz_t g,x,x1,x2,x2k,y,y1,p,q,tmpq,alloc1,alloc2,alloc3,alloc4,alloc5;
    mpz_init_set_si(g,1); // ? mp_init_size to specify size
    mpz_init_set_si(x,2);
    mpz_init_set_si(x1,2);
    mpz_init_set_si(y,2);
    mpz_init(y1);
    mpz_init(x2);
    mpz_init(x2k);
    mpz_init_set_si(p,1);
    mpz_init(q);
    mpz_init(tmpq);
    mpz_init(alloc1);
    mpz_init(alloc2);
    mpz_init(alloc3);
    mpz_init(alloc4);
    mpz_init(alloc5);
    while (!ctrl_c && !interrupted && mpz_cmp_si(g,1)==0) {
#ifdef COMPILE_FOR_STABILITY
      control_c();
#endif
      a=2*a+1;//a=2^(e+1)-1=2*l(m)-1 
      while (!ctrl_c && !interrupted && mpz_cmp_si(g,1)==0 && a>m) { // ok
#ifdef COMPILE_FOR_STABILITY
	control_c();
#endif
	// x=f(x,k,n,q);
#ifdef USE_GMP_REPLACEMENTS
	mp_sqr(&x,&x2);
	mpz_add(x2k,x2,*k._ZINTptr);
	if (mpz_cmp(x2k,*n._ZINTptr)>0){
	  mp_grow(&alloc1,x2k.used+2);
	  mpz_set_ui(alloc1,0);
	  alloc1.used = x2k.used +2 ;
	  mpz_set(alloc2,x2k);
	  mpz_set(alloc3,*n._ZINTptr);
	  // mpz_set_si(alloc4,0);
	  // mpz_set_si(alloc5,0);
	  alloc_mp_div(&x2k,n._ZINTptr,&tmpq,&x,&alloc1,&alloc2,&alloc3,&alloc4,&alloc5);
	}
	else
	  mpz_set(x,x2k);
#else 
	mpz_mul(x2,x,x);
	mpz_add(x2k,x2,*k._ZINTptr);
	mpz_tdiv_r(x,x2k,*n._ZINTptr);
#endif
	m += 1;
	if (debug_infolevel && ((m % 
#if defined(RTOS_THREADX) || defined(BESTA_OS) || defined NSPIRE || defined FXCG || defined TICE
				 (1<<10)
#else
				 (1<<18)
#endif
				 )==0))
	  *logptr(contextptr) << CLOCK() << gettext(" Pollard-rho try ") << m << "\n";
	if (m > maxiter ){
	  if (debug_infolevel)	  
	    *logptr(contextptr) << CLOCK() << gettext(" Pollard-rho failure, ntries ") << m << "\n";
	  mpz_clear(alloc5);
	  mpz_clear(alloc4);
	  mpz_clear(alloc3);
	  mpz_clear(alloc2);
	  mpz_clear(alloc1);
	  mpz_clear(tmpq);
	  mpz_clear(x);
	  mpz_clear(x1);
	  mpz_clear(x2);
	  mpz_clear(x2k);
	  mpz_clear(y);
	  mpz_clear(y1);
	  mpz_clear(p);
	  mpz_clear(q);
	  return -1;
	}
	// p=irem(p*(x1-x),n,q);
	mpz_sub(q,x1,x);
	mpz_mul(x2,p,q);
#if 0 // def USE_GMP_REPLACEMENTS
	if (mpz_cmp(x2,*n._ZINTptr)>0){
	  mp_grow(&alloc1,x2.used+2);
	  mpz_set_ui(alloc1,0);
	  alloc1.used = x2.used +2 ;
	  mpz_set(alloc2,x2);
	  mpz_set(alloc3,*n._ZINTptr);
	  // mpz_set_si(alloc4,0);
	  // mpz_set_si(alloc5,0);
	  alloc_mp_div(&x2,n._ZINTptr,&tmpq,&p,&alloc1,&alloc2,&alloc3,&alloc4,&alloc5);
	}
	else 
	  mpz_set(p,x2);
#else
	mpz_tdiv_r(p,x2,*n._ZINTptr);
#endif
	c += 1;
	if (c==POLLARD_GCD) {
	  // g=gcd(abs(p,context0),n); 
	  mpz_abs(q,p);
	  my_mpz_gcd(g,q,*n._ZINTptr);
	  if (mpz_cmp_si(g,1)==0) {
	    mpz_set(y,x); // y=x;
	    mpz_set(y1,x1); // y1=x1;
	    mpz_set_si(p,1); // p=1;
	    a1=a;
	    m1=m;
	    c=0;
	  }
	}
      }//m=a=2^e-1=l(m)
      if (mpz_cmp_si(g,1)==0) {
	mpz_set(x1,x); // x1=x;//x1=x_m=x_l(m)-1
	j=3*(a+1)/2; // j=3*iquo(a+1,2);
	for (long i=m+1;i<=j;i++){
	  // x=f(x,k,n,q);
	  mpz_mul(x2,x,x);
	  mpz_add(x2k,x2,*k._ZINTptr);
#if 0 // def USE_GMP_REPLACEMENTS
	  if (mpz_cmp(x2k,*n._ZINTptr)>0){
	    mp_grow(&alloc1,x2k.used+2);
	    mpz_set_ui(alloc1,0);
	    alloc1.used = x2k.used +2 ;
	    mpz_set(alloc2,x2k);
	    mpz_set(alloc3,*n._ZINTptr);
	    // mpz_set_si(alloc4,0);
	    // mpz_set_si(alloc5,0);
	    alloc_mp_div(&x2k,n._ZINTptr,&tmpq,&x,&alloc1,&alloc2,&alloc3,&alloc4,&alloc5);
	  }
	  else 
	    mpz_set(x,x2);
#else
	  mpz_tdiv_r(x,x2k,*n._ZINTptr);
#endif
	}
	m=j;
      }
    }
    //g<>1 ds le paquet de POLLARD_GCD
    if (debug_infolevel>5)
      CERR << CLOCK() << " Pollard-rho nloops " << m << "\n";
    mpz_set(x,y); // x=y;
    mpz_set(x1,y1); // x1=y1;
    mpz_set_si(g,1); // g=1;
    a=(a1-1)/2; // a=iquo(a1-1,2);
    m=m1;
    while (!ctrl_c && !interrupted && mpz_cmp_si(g,1)==0) {
#ifdef COMPILE_FOR_STABILITY
      control_c();
#endif
      a=2*a+1;
      while (!ctrl_c && !interrupted && mpz_cmp_si(g,1)==0 && a>m) { // ok
#ifdef COMPILE_FOR_STABILITY
	control_c();
#endif
	// x=f(x,k,n,q);
	mpz_mul(x2,x,x);
	mpz_add(x2k,x2,*k._ZINTptr);
#if 0 // def USE_GMP_REPLACEMENTS
	if (mpz_cmp(x2k,*n._ZINTptr)>0){
	  mp_grow(&alloc1,x2k.used+2);
	  mpz_set_ui(alloc1,0);
	  alloc1.used = x2k.used +2 ;
	  mpz_set(alloc2,x2k);
	  mpz_set(alloc3,*n._ZINTptr);
	  alloc_mp_div(&x2k,n._ZINTptr,&tmpq,&x,&alloc1,&alloc2,&alloc3,&alloc4,&alloc5);
	}
	else
	  mpz_set(x,x2k);	  
#else
	mpz_tdiv_r(x,x2k,*n._ZINTptr);
#endif
	m += 1;
	if (m > maxiter ){
	  mpz_clear(alloc5);
	  mpz_clear(alloc4);
	  mpz_clear(alloc3);
	  mpz_clear(alloc2);
	  mpz_clear(alloc1);
	  mpz_clear(tmpq);
	  mpz_clear(x);
	  mpz_clear(x1);
	  mpz_clear(x2);
	  mpz_clear(x2k);
	  mpz_clear(y);
	  mpz_clear(y1);
	  mpz_clear(p);
	  mpz_clear(q);
	  return -1;
	}
	// p=irem(x1-x,n,q);
	mpz_sub(q,x1,x);
#if 0 // def USE_GMP_REPLACEMENTS
	if (mpz_cmp(q,*n._ZINTptr)>0){
	  mp_grow(&alloc1,q.used+2);
	  mpz_set_ui(alloc1,0);
	  alloc1.used = q.used +2 ;
	  mpz_set(alloc2,q);
	  mpz_set(alloc3,*n._ZINTptr);
	  // mpz_set_si(alloc4,0);
	  // mpz_set_si(alloc5,0);
	  alloc_mp_div(&q,n._ZINTptr,&tmpq,&p,&alloc1,&alloc2,&alloc3,&alloc4,&alloc5);
	}
	else 
	  mpz_set(p,q);
#else
	mpz_tdiv_r(p,q,*n._ZINTptr);
#endif
	// g=gcd(abs(p,context0),n);  // ok
	mpz_abs(q,p);
	my_mpz_gcd(g,q,*n._ZINTptr);
      }
      if (mpz_cmp_si(g,1)==0) {
	mpz_set(x1,x); // x1=x;
	j=3*(a+1)/2; // j=3*iquo(a+1,2);
	for (long i=m+1;j>=i;i++){
	  // x=f(x,k,n,q);
	  mpz_mul(x2,x,x);
	  mpz_add(x2k,x2,*k._ZINTptr);
	  mpz_tdiv_qr(tmpq,x,x2k,*n._ZINTptr);
	}
	m=j;
      }
    }
    mpz_clear(alloc5);
    mpz_clear(alloc4);
    mpz_clear(alloc3);
    mpz_clear(alloc2);
    mpz_clear(alloc1);
    mpz_clear(tmpq);
    mpz_clear(x);
    mpz_clear(x1);
    mpz_clear(x2);
    mpz_clear(x2k);
    mpz_clear(y);
    mpz_clear(y1);
    mpz_clear(p);
    mpz_clear(q);
#ifdef COMPILE_FOR_STABILITY
    control_c();
#endif
    if (ctrl_c || interrupted){
      mpz_clear(g);
      return 0;
    }
    if (mpz_cmp(g,*n._ZINTptr)==0) {
      if (k==1) {
	mpz_clear(g);
	return(pollard(n,-1,contextptr)); 
      }
      else {
	if (k*k==1){
	  mpz_clear(g);
	  return(pollard(n,3,contextptr));
	}
	else {
	  if (is_greater(k,50,contextptr)){
#if 1
	    return -1;
#else
	    ref_mpz_t * ptr=new ref_mpz_t;
	    mpz_init_set(ptr->z,g);
	    mpz_clear(g);
	    return ptr;
#endif
	  }
	  else {
	    mpz_clear(g);
	    return(pollard(n,k+2,contextptr));
	  }
	} 
      }
    } 
    ref_mpz_t * ptr=new ref_mpz_t;
    mpz_init_set(ptr->z,g);
    mpz_clear(g);
    return ptr;
  }

  // const short int giac_primes[]={2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97,101,103,107,109,113,127,131,137,139,149,151,157,163,167,173,179,181,191,193,197,199,211,223,227,229,233,239,241,251,257,263,269,271,277,281,283,293,307,311,313,317,331,337,347,349,353,359,367,373,379,383,389,397,401,409,419,421,431,433,439,443,449,457,461,463,467,479,487,491,499,503,509,521,523,541,547,557,563,569,571,577,587,593,599,601,607,613,617,619,631,641,643,647,653,659,661,673,677,683,691,701,709,719,727,733,739,743,751,757,761,769,773,787,797,809,811,821,823,827,829,839,853,857,859,863,877,881,883,887,907,911,919,929,937,941,947,953,967,971,977,983,991,997};

  bool eratosthene(double n,vector<bool> * & v){
    static vector<bool> *ptr=0;
    if (!ptr)
      ptr=new vector<bool>;
    vector<bool> &erato=*ptr;
    v=ptr;
    if (n+1>erato.size()){
      unsigned N=int(n);
      ++N;
#if defined BESTA_OS 
      if (N>2e6)
	return false;
#else
      if (N>2e9)
	return false;
#endif
      N = (N*11)/10;
      erato=vector<bool>(N+1,true); 
      // insure that we won't recompute all again from start for ithprime(i+1)
      for (unsigned p=2;;++p){
	while (!erato[p]) // find next prime
	  ++p;
	if (p*p>N) // finished
	  return true;
	for (unsigned i=2*p;i<=N;i+=p) 
	  erato[i]=false; // remove p multiples
      }
    }
    return true;
  }

  bool eratosthene2(double n,vector<bool> * & v){
    static vector<bool> *ptr=0;
    if (!ptr)
      ptr=new vector<bool>;
    vector<bool> &erato=*ptr;
    v=ptr;
    if (n/2>=erato.size()){
      unsigned N=int(n);
      ++N;
#if defined BESTA_OS 
      if (N>4e6)
	return false;
#else
      if (N>2e9)
	return false;
#endif
      // 11/20 insures that we won't recompute all again from start for ithprime(i+1)
      N = (N*11)/20; // keep only odd numbers in sieve
      erato=vector<bool>(N+1,true); //erato[i] stands for 2*i+1 <-> n corresponds to erato[n/2]
      for (unsigned p=3;;p+=2){
	while (!erato[p/2]) // find next prime (first one is p==3)
	  p+=2;
	if (p*p>2*N+1) // finished
	  return true;
	// p is prime, set p*p, (p+2)*p, etc. to be non prime
	for (unsigned i=(p*p)/2;i<=N;i+=p) 
	  erato[i]=false; // remove p multiples
      }
    }
    return true;
  }

  // ithprime(n) is approx invli(n)+invli(sqrt(n))/4 where invli is reciproc.
  // of Li(x)=Ei(ln(x))
  // For fast code, cf. https://github.com/kimwalisch/primecount
  static const char _ithprime_s []="ithprime";
  static symbolic symb_ithprime(const gen & args){
    return symbolic(at_ithprime,args);
  }
  static gen ithprime(const gen & g_,GIAC_CONTEXT){
    gen g(g_);
    if (!is_integral(g))
      return gentypeerr(contextptr);
    if (g.type!=_INT_)
      return gensizeerr(contextptr); // symb_ithprime(g);
    int i=g.val;
    if (i<0)
      return gensizeerr(contextptr);
    if (i==0)
      return 1;
    if (i<=int(sizeof(giac_primes)/sizeof(short int)))
      return giac_primes[i-1];
    vector<bool> * vptr=0;
#if 1
    if (!eratosthene2(i*std::log(double(i))*1.1,vptr))
      return gensizeerr(contextptr);
    unsigned count=2;
    unsigned s=unsigned(vptr->size());
    for (unsigned k=2;k<s;++k){
      if ((*vptr)[k]){
	++count;
	if (i==count)
	  return int(2*k+1);
      }
    }
    return undef;
#else
    if (!eratosthene(i*std::log(double(i))*1.1,vptr))
      return gensizeerr(contextptr);
    unsigned count=2;
    unsigned s=vptr->size();
    for (unsigned k=4;k<s;++k){
      if ((*vptr)[k]){
	++count;
	if (i==count)
	  return int(k);
      }
    }
    return undef;
#endif
  }
  gen _ithprime(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type==_VECT)
      return apply(args,_ithprime,contextptr);
    return ithprime(args,contextptr);
  }
  static define_unary_function_eval (__ithprime,&_ithprime,_ithprime_s);
  define_unary_function_ptr5( at_ithprime ,alias_at_ithprime,&__ithprime,0,true);

  static const char _nprimes_s []="nprimes";
  static gen nprimes(const gen & g_,GIAC_CONTEXT){
    gen g(g_);
    if (!is_integral(g))
      return gentypeerr(contextptr);
    if (g.type!=_INT_)
      return gensizeerr(contextptr); // symb_ithprime(g);
    int i=g.val;
    if (i<0)
      return gensizeerr(contextptr);
    if (i<2)
      return 0;
    vector<bool> * vptr=0;
    if (!eratosthene2(i+2,vptr))
      return gensizeerr(contextptr);
    unsigned count=1; // 2 is prime, then count odd primes
    i=(i-1)/2;
    for (int k=1;k<=i;++k){
      if ((*vptr)[k])
	++count;
    }
    return int(count);
  }
  gen _nprimes(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type==_VECT)
      return apply(args,_nprimes,contextptr);
    return nprimes(args,contextptr);
  }
  static define_unary_function_eval (__nprimes,&_nprimes,_nprimes_s);
  define_unary_function_ptr5( at_nprimes ,alias_at_nprimes,&__nprimes,0,true);

  bool is_divisible_by(const gen & n,unsigned long a){
    if (n.type==_ZINT){
#ifdef USE_GMP_REPLACEMENTS
      mp_digit c;
      mp_mod_d(n._ZINTptr, a, &c);
      return c==0;
#else
      return mpz_divisible_ui_p(*n._ZINTptr,a);
#endif
    }
    return n.val%a==0;
  }

  // find trivial factors of n, 
  // if add_last is true the remainder is put in the vecteur,
  // otherwise n contains the remainder
  vecteur pfacprem(gen & n,bool add_last,GIAC_CONTEXT){
    gen a;
    gen q;
    int p,i,prime;
    vecteur v(2);
    vecteur u;
    if (is_zero(n))
      return u;
    if (n.type==_ZINT){
      ref_mpz_t * cur = new ref_mpz_t;
      mpz_t div,q,r,alloc1,alloc2,alloc3,alloc4,alloc5;
      mpz_set(cur->z,*n._ZINTptr);
      mpz_init_set(q,*n._ZINTptr);
      mpz_init(r);
      mpz_init(div);
      mpz_init(alloc1);
      mpz_init(alloc2);
      mpz_init(alloc3);
      mpz_init(alloc4);
      mpz_init(alloc5);
      for (i=0;i<int(sizeof(giac_primes)/sizeof(short int));++i){
	if (mpz_cmp_si(cur->z,1)==0) 
	  break;
	prime=giac_primes[i];
	mpz_set_ui(div,prime);
#ifdef USE_GMP_REPLACEMENTS
	for (p=0;;p++){
	  mp_grow(&alloc1,cur->z.used+2);
	  mpz_set_ui(alloc1,0);
	  alloc1.used = cur->z.used +2 ;
	  mpz_set(alloc2,cur->z);
	  mpz_set(alloc3,div);
	  alloc_mp_div(&cur->z,&div,&q,&r,&alloc1,&alloc2,&alloc3,&alloc4,&alloc5);
	  // mpz_tdiv_qr(q,r,cur->z,div);
	  if (mpz_cmp_si(r,0))
	    break;
	  mp_exch(&cur->z,&q);
	}
	// *logptr(contextptr) << "Factor " << prime << " " << p << "\n";
	if (p){
	  u.push_back(prime);
	  u.push_back(p);
	}
#else
	if (mpz_divisible_ui_p(cur->z,prime)){
	  mpz_set_ui(div,prime);
	  for (p=0;;p++){
	    mpz_tdiv_qr(q,r,cur->z,div);
	    if (mpz_cmp_si(r,0))
	      break;
	    mpz_swap(cur->z,q);
	  }
	  // *logptr(contextptr) << "Factor " << prime << " " << p << "\n";
	  u.push_back(prime);
	  u.push_back(p);
	}
#endif
      } // end for on smal primes
      mpz_clear(alloc5);
      mpz_clear(alloc4);
      mpz_clear(alloc3);
      mpz_clear(alloc2);
      mpz_clear(alloc1);
      mpz_clear(div); mpz_clear(r); mpz_clear(q);
      n=cur;
    }
    else {
      for (i=0;i<int(sizeof(giac_primes)/sizeof(short int));++i){
	if (n==1) 
	  break;
	a.val=giac_primes[i];
	p=0;
	while (is_divisible_by(n,a.val)){ // while (irem(n,a,q)==0){
	  n=iquo(n,a); 
	  p=p+1;
	}
	if (p!=0){
	  // *logptr(contextptr) << "Factor " << a << " " << p << "\n";
	  u.push_back(a);
	  u.push_back(p);
	}
      }
    }
    if (add_last && i==int(sizeof(giac_primes)/sizeof(short int)) && !is_one(n)){
      // hack: check if n is a perfect square
      double nf=evalf_double(n,1,contextptr)._DOUBLE_val;
      nf=std::sqrt(nf);
      gen n2=_round(nf,contextptr);
      if (n2*n2==n){
	u.push_back(n2);
	u.push_back(2);	
      }
      else {
	u.push_back(n);
	u.push_back(1);
      }
      n=1;
    }
    //v[0]=n;
    //v[1]=u;
    
    return(u);
  }

#ifdef USE_GMP_REPLACEMENTS
  static gen inpollardsieve(const gen &a,gen k,bool & do_pollard,GIAC_CONTEXT){
    gen b=do_pollard?pollard(a,k,contextptr):-1;
#ifdef COMPILE_FOR_STABILITY
    control_c();
#endif
#ifdef GIAC_MPQS
    if (b==-1 && !ctrl_c && !interrupted){ 
      do_pollard=false;
      if (msieve(a,b,contextptr)) return b; else return -1; }
#endif
    return b;
  }
  static gen pollardsieve(const gen &a,gen k,bool & do_pollard,GIAC_CONTEXT){
#if defined( GIAC_HAS_STO_38) || defined(EMCC) || defined NSPIRE
    int debug_infolevel_=debug_infolevel;
#if defined RTOS_THREADX || defined NSPIRE || defined FXCG || defined TICE
    debug_infolevel=2;
    if (do_pollard)
      *logptr(contextptr) << gettext("Pollard-rho on ") << a << "\n"; 
#else
    debug_infolevel=0;
#endif
#endif
    gen res=inpollardsieve(a,k,do_pollard,contextptr);
#if defined( GIAC_HAS_STO_38) || defined(EMCC) || defined NSPIRE
    debug_infolevel=debug_infolevel_;
#ifdef GIAC_HAS_STO_38
    Calc->Terminal.MakeUnvisible();
#endif
#endif
    return res;
  }
#else // USE_GMP_REPLACEMENTS
  static gen pollardsieve(const gen &a,gen k,bool & do_pollard,GIAC_CONTEXT){
    gen b=do_pollard?pollard(a,k,contextptr):-1;
#ifdef COMPILE_FOR_STABILITY
    control_c();
#endif
#ifdef GIAC_MPQS
    if (b==-1 && !ctrl_c && !interrupted){ 
      do_pollard=false;
      if (msieve(a,b,contextptr)) return b; else return -1; }
#endif
    if (b==-1)
      b=a;
    return b;
  }
#endif // USE_GMP_REPLACEMENTS

  static gen ifactor2(const gen & n,vecteur & v,bool & do_pollard,GIAC_CONTEXT){
    if (is_greater(giac_last_prime*giac_last_prime,n,contextptr) || is_probab_prime_p(n) ){
      v.push_back(n);
      return 1;
    }
    // Check for power of integer: arg must be > 1e4, n*ln(arg)=d => n<d/ln(1e4)
    double d=evalf_double(n,1,contextptr)._DOUBLE_val;
    int maxpow=int(std::ceil(std::log(d)/std::log(1e4)));
    for (int i=2;i<=maxpow;++i){
      if ( (i>2 && i%2==0) ||
	   (i>3 && i%3==0) ||
	   (i>5 && i%5==0) ||
	   (i>7 && i%7==0) )
	continue;
      gen u;
      if (i==2)
	u=isqrt(n);
      else {
	double x=std::pow(d,1./i);
	u=gen(longlong(x));
      }
      if (pow(u,i,contextptr)==n){
	vecteur w;
	do_pollard=true;
	ifactor2(u,w,do_pollard,contextptr);
	for (int j=0;j<i;j++)
	  v=mergevecteur(v,w);
	return v;
      }
    }
    gen a=pollardsieve(n,1,do_pollard,contextptr);
    if (a==-1)
      return a;
#ifdef COMPILE_FOR_STABILITY
    control_c();
#endif
    if (ctrl_c || interrupted)
      return gensizeerr("Interrupted");
    gen ba=n/a;
    if (a!=n)
      a=ifactor2(a,v,do_pollard,contextptr);
    else {
      a=1;
      v.push_back(n);
    }
    if (is_strictly_greater(ba,1,contextptr))
      a=ifactor2(ba,v,do_pollard,contextptr);
    return a;
  }

  static vecteur facprem(gen & n,GIAC_CONTEXT){
    vecteur v;    
    if (n==1) { return v; }
    if ( (n.type==_INT_ && n.val<giac_last_prime*giac_last_prime) || is_probab_prime_p(n)) {
      v.push_back(n);
      n=1;
      return v;
    }
    if (debug_infolevel>5)
      CERR << "Pollard begin " << CLOCK() << "\n";
    bool do_pollard=true;
    gen a=ifactor2(n,v,do_pollard,contextptr);
    if (a==-1)
      return makevecteur(gensizeerr(gettext("Quadratic sieve failure, perhaps number too large")));
    if (is_zero(a))
      return makevecteur(gensizeerr(gettext("Stopped by user interruption")));
    n=1;
    return v;
  }

  void mergeifactors(const vecteur & f,const vecteur &g,vecteur & h){
    h=f;
    for (unsigned i=0;i<g.size();i+=2){
      unsigned j=0;
      for (;j<f.size();j+=2){
	if (f[j]==g[i])
	  break;
      }
      if (j<f.size())
	h[j+1] += g[i+1];
      else {
	h.push_back(g[i]);
	h.push_back(g[i+1]);
      }
    }
  }

  static vecteur giac_ifactors(const gen & n0,GIAC_CONTEXT){
    if (!is_integer(n0) || is_zero(n0))
      return vecteur(1,gensizeerr(gettext("ifactors")));
    if (is_one(n0))
      return vecteur(0);
    gen n(n0);
    vecteur f;
    vecteur g;
    vecteur u;
    // First find if |n-k^d|<=1 for d = 2, 3, 5 or 7
    double nd=evalf_double(n,1,contextptr)._DOUBLE_val;
    double nd2=std::floor(std::sqrt(nd)+.5);
    if (absdouble(1-nd2*nd2/nd)<1e-10){
      gen n2=isqrt(n+1);
      if (n==n2*n2){
	f=ifactors(n2,contextptr);
	iterateur it=f.begin(),itend=f.end();
	for (;it!=itend;++it){
	  ++it;
	  *it = 2 * *it;
	}
	return f;
      }
      if (n==n2*n2-1){
	f=ifactors(n2-1,contextptr);
	g=ifactors(n2+1,contextptr);
	mergeifactors(f,g,u);
	return u;
      }
    }
    for (int k=3;;){
      nd2=std::floor(std::pow(nd,1./k)+.5);
      if (absdouble(1-std::pow(nd2,k)/nd)<1e-10){
	gen n2=_floor(nd2,contextptr),nf=n2*n2;
	for (int j=2;j<k;j++)
	  nf=nf*n2;
	if (n==nf){
	  f=ifactors(n2,contextptr);
	  iterateur it=f.begin(),itend=f.end();
	  for (;it!=itend;++it){
	    ++it;
	    *it = k * *it;
	  }
	  return f;
	}
	if (n==nf-1){ // n2^k-1
	  f=ifactors(n2-1,contextptr);
	  g=ifactors(n/(n2-1),contextptr);
	  mergeifactors(f,g,u);
	  return u;
	}
	if (n==nf+1){ // n2^k+1
	  f=ifactors(n2+1,contextptr);
	  g=ifactors(n/(n2+1),contextptr);
	  mergeifactors(f,g,u);
	  return u;
	}
      }
      if (k==11) break;
      if (k==7) break; // k=11;
      if (k==5) k=7;
      if (k==3) k=5;
    }
    //f=pfacprem(n,false,contextptr);
    //cout<<n<<" "<<f<<"\n";
    while (n!=1) {
      g=facprem(n,contextptr);
      if (is_undef(g))
	return g;
      islesscomplexthanf_sort(g.begin(),g.end());
      gen last=0; int p=0;
      for (unsigned i=0;i<g.size();++i){
	if (g[i]==last)
	  ++p;
	else {
	  if (last!=0){
	    u.push_back(last);
	    u.push_back(p);
	  }
	  last=g[i];
	  p=1;
	}
      }
      u.push_back(last);
      u.push_back(p);
    }   
    g=mergevecteur(f,u);
    return g;
  }

  static vecteur ifactors1(const gen & n0,GIAC_CONTEXT){
    if (is_greater(1e71,n0,contextptr))
      return giac_ifactors(n0,contextptr);
    if (n0.type==_VECT && !n0._VECTptr->empty())
      return giac_ifactors(n0._VECTptr->front(),contextptr);
#ifdef HAVE_LIBPARI
#ifdef __APPLE__
    return vecteur(1,gensizeerr(gettext("(Mac OS) Large number, you can try pari(); pari_factor(")+n0.print(contextptr)+")"));
#endif
    if (!is_integer(n0) || is_zero(n0))
      return vecteur(1,gensizeerr(gettext("ifactors")));
    if (is_one(n0))
      return vecteur(0);
    gen g(pari_ifactor(n0),contextptr); 
    if (g.type==_VECT){
      matrice m(mtran(*g._VECTptr));
      vecteur res;
      const_iterateur it=m.begin(),itend=m.end();
      for (;it!=itend;++it){
	if (it->type!=_VECT) return vecteur(1,gensizeerr(gettext("ifactor.cc/ifactors")));
	res.push_back(it->_VECTptr->front());
	res.push_back(it->_VECTptr->back());
      }
      return res;
    }
#endif // LIBPARI
    return giac_ifactors(n0,contextptr);
  }

  vecteur ifactors(const gen & n0,GIAC_CONTEXT){
    gen n(n0);
    vecteur f=pfacprem(n,false,contextptr);
    if (is_undef(f))
      return f;
    vecteur g=ifactors1(n,contextptr);
    if (is_undef(g))
      return g;
    return mergevecteur(f,g);
  }


  gen ifactors(const gen & args,int maplemode,GIAC_CONTEXT){
    if ( (args.type==_INT_) || (args.type==_ZINT)){
      if (is_zero(args)){
	if (maplemode==1)
	  return makevecteur(args,vecteur(0));
	else
	  return makevecteur(args);
      }
      vecteur v(ifactors(abs(args,contextptr),contextptr)); // ok
      if (!v.empty() && is_undef(v.front()))
	return v.front();
      if (maplemode!=1){
	if (is_positive(args,context0))
	  return v;
	return mergevecteur(makevecteur(minus_one,plus_one),v);
      }
      vecteur res;
      const_iterateur it=v.begin(),itend=v.end();
      for (;it!=itend;it+=2){
	res.push_back(makevecteur(*it,*(it+1)));
      }
      if (is_positive(args,context0))
	return makevecteur(plus_one,res);
      else
	return makevecteur(minus_one,res);	
    }
    return gentypeerr(gettext("ifactors"));
  }

  gen _ifactors(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type==_VECT)
      return apply(args,_ifactors,contextptr);
    gen g(args);
    if (!is_integral(g))
      return gensizeerr(contextptr);
    if (calc_mode(contextptr)==1){ // ggb returns factors repeted instead of multiplicites
      vecteur res;
      gen in=ifactors(g,0,contextptr);
      if (in.type==_VECT){
	for (unsigned i=0;i<in._VECTptr->size();i+=2){
	  gen f=in[i],m=in[i+1];
	  if (m.type==_INT_){
	    for (int j=0;j<m.val;++j)
	      res.push_back(f);
	  }
	}
	return res;
      }
    }
    return ifactors(g,0,contextptr);
  }
  static const char _ifactors_s []="ifactors";
  static define_unary_function_eval (__ifactors,&_ifactors,_ifactors_s);
  define_unary_function_ptr5( at_ifactors ,alias_at_ifactors,&__ifactors,0,true);

  static const char _facteurs_premiers_s []="facteurs_premiers";
  static define_unary_function_eval (__facteurs_premiers,&_ifactors,_facteurs_premiers_s);
  define_unary_function_ptr5( at_facteurs_premiers ,alias_at_facteurs_premiers,&__facteurs_premiers,0,true);

  static const char _maple_ifactors_s []="maple_ifactors";
  gen _maple_ifactors(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type==_VECT)
      return apply(args,_maple_ifactors,contextptr);
    return ifactors(args,1,contextptr);
  }
  static define_unary_function_eval (__maple_ifactors,&_maple_ifactors,_maple_ifactors_s);
  define_unary_function_ptr5( at_maple_ifactors ,alias_at_maple_ifactors,&__maple_ifactors,0,true);

  static vecteur in_factors(const gen & gf,GIAC_CONTEXT){
    if (gf.type!=_SYMB)
      return makevecteur(gf,plus_one);
    unary_function_ptr & u=gf._SYMBptr->sommet;
    if (u==at_inv){
      vecteur v=in_factors(gf._SYMBptr->feuille,contextptr);
      iterateur it=v.begin(),itend=v.end();
      for (;it!=itend;it+=2)
	*(it+1)=-*(it+1);
      return v;
    }
    if (u==at_neg){
      vecteur v=in_factors(gf._SYMBptr->feuille,contextptr);
      v.push_back(minus_one);
      v.push_back(plus_one);
      return v;
    }
    if ( (u==at_pow) && (gf._SYMBptr->feuille._VECTptr->back().type==_INT_) ){
      vecteur v=in_factors(gf._SYMBptr->feuille._VECTptr->front(),contextptr);
      gen k=gf._SYMBptr->feuille._VECTptr->back();
      iterateur it=v.begin(),itend=v.end();
      for (;it!=itend;it+=2)
	*(it+1)=k* *(it+1);
      return v;
    }
    if (u!=at_prod)
      return makevecteur(gf,plus_one);
    vecteur res;
    const_iterateur it=gf._SYMBptr->feuille._VECTptr->begin(),itend=gf._SYMBptr->feuille._VECTptr->end();
    for (;it!=itend;++it){
      res=mergevecteur(res,in_factors(*it,contextptr));
    }
    return res;
  }
  static vecteur in_factors1(const vecteur & res,GIAC_CONTEXT){
    gen coeff(1);
    vecteur v;
    const_iterateur it=res.begin(),itend=res.end();
    for (;it!=itend;it+=2){
      if (lidnt(*it).empty())
	coeff=coeff*(pow(*it,*(it+1),contextptr));
      else
	v.push_back(makevecteur(*it,*(it+1)));
    }
    return makevecteur(coeff,v);
  }
  vecteur factors(const gen & g,const gen & x,GIAC_CONTEXT){
    gen gf=factor(g,x,false,contextptr);
    vecteur res=in_factors(gf,contextptr);
    if (xcas_mode(contextptr)!=1)
      return res;
    return in_factors1(res,contextptr);
  }
  vecteur sqff_factors(const gen & g,GIAC_CONTEXT){
    gen gf=_sqrfree(g,contextptr);
    return in_factors(gf,contextptr);
  }
  static const char _factors_s []="factors";
  gen _factors(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type==_VECT && args.subtype==_SEQ__VECT && args._VECTptr->size()==2){
      gen j=args._VECTptr->back();
      gen res=_factors(args._VECTptr->front()*j,contextptr);
      if (res.type==_VECT && xcas_mode(contextptr)!=1)
	res=in_factors1(*res._VECTptr,contextptr);
      if (res.type==_VECT && res._VECTptr->size()==2){
	res._VECTptr->front()=recursive_normal(res._VECTptr->front()/j,contextptr);
	if (xcas_mode(contextptr)!=1){
	  if (is_one(res._VECTptr->front()))
	    res=res._VECTptr->back();
	  else {
	    j=res._VECTptr->front();
	    res=res._VECTptr->back();
	    if (res.type==_VECT)
	      res=mergevecteur(makevecteur(j,1),*res._VECTptr);
	  }
	  vecteur v;
	  aplatir(*res._VECTptr,v,contextptr);
	  res=v;
	}
      }
      return res;
    }
    if (args.type==_VECT)
      return apply(args,_factors,contextptr);
    return factors(args,vx_var(),contextptr);
  }
  static define_unary_function_eval (__factors,&_factors,_factors_s);
  define_unary_function_ptr5( at_factors ,alias_at_factors,&__factors,0,true);

  static gen ifactors2ifactor(const vecteur & l,bool quote){
    int s;
    s=int(l.size());
    gen r;
    vecteur v(s/2);
    for (int j=0;j<s;j=j+2){
      if (!is_one(l[j+1]))
	v[j/2]=symb_pow(l[j],l[j+1]);
      else
	v[j/2]=l[j];
    }
    if (v.size()==1){
#if defined(GIAC_HAS_STO_38) && defined(CAS38_DISABLED)
      return symb_quote(v.front());
#else
      if (quote)
	return symb_quote(v.front());
      return v.front();
#endif
    }
    r=symb_prod(v);
#if defined(GIAC_HAS_STO_38) && defined(CAS38_DISABLED)
    r=symb_quote(r);
#endif
    if (quote)
      return symb_quote(r);
    return r;
  }
  gen ifactor(const gen & n,GIAC_CONTEXT){
    vecteur l;
    l=ifactors(n,contextptr);
    if (!l.empty() && is_undef(l.front())) return l.front();
    return ifactors2ifactor(l,calc_mode(contextptr)==1);
  }
  gen _ifactor(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    gen n=args;
    if (n.type==_VECT && n._VECTptr->size()==1 && is_integer(n._VECTptr->front()))
      return ifactor(n,contextptr);
    if (n.type==_VECT)
      return apply(n,_ifactor,contextptr);
    if (!is_integral(n))
      return gensizeerr(contextptr);
    if (is_strictly_positive(-n,0))
      return -_ifactor(-n,contextptr);
    if (n.type==_INT_ && n.val<=3)
      return n;
    return ifactor(n,contextptr);
  }
  static const char _ifactor_s []="ifactor";
  static define_unary_function_eval (__ifactor,&_ifactor,_ifactor_s);
  define_unary_function_ptr5( at_ifactor ,alias_at_ifactor,&__ifactor,0,true);

  static const char _factoriser_entier_s []="factoriser_entier";
  static define_unary_function_eval (__factoriser_entier,&_ifactor,_factoriser_entier_s);
  define_unary_function_ptr5( at_factoriser_entier ,alias_at_factoriser_entier,&__factoriser_entier,0,true);

  static vecteur divis(const vecteur & l3,GIAC_CONTEXT){
    vecteur l1(1);
    gen d,e;
    int s=int(l3.size());
    gen taille=1;
    for (int k=0;k<s;k+=2){
      taille=taille*(l3[k+1]+1);
    }
    if (taille.type!=_INT_ || taille.val>LIST_SIZE_LIMIT)
      return vecteur(1,gendimerr(contextptr));
    l1.reserve(taille.val);
    l1[0]=1;//l3.push_back(..);
    for (int k=0;k<s;k=k+2) {
      vecteur l2;
      l2.reserve(taille.val);
      int s1;
      s1=int(l1.size());
      vecteur l4(s1);
      d=l3[k];
      e=l3[k+1];
      int ei;
      if (e.type==_INT_){
	ei=e.val;
      }
      else
	return vecteur(1,gensizeerr(gettext("Integer too large")));
      for (int j=1;j<=ei;j++){
	gen dj=pow(d,j);
	for (int l=0;l<s1;l++){ 
	  l4[l]=l1[l]*dj;
	}
	// l2=mergevecteur(l2,l4);
	iterateur it=l4.begin(),itend=l4.end();
	for (;it!=itend;++it)
	  l2.push_back(*it);
      }
      // l1=mergevecteur(l1,l2);
      iterateur it=l2.begin(),itend=l2.end();
      for (;it!=itend;++it)
	l1.push_back(*it);
    }
    return(l1); 
  }
  gen idivis(const gen & n,GIAC_CONTEXT){
    vecteur l3(ifactors(n,contextptr));
    if (!l3.empty() && is_undef(l3.front())) return l3.front();
    return divis(l3,contextptr);
  }
  gen _idivis(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type==_VECT)
      return apply(args,_idivis,contextptr);
    gen n=args;
    if (is_zero(n) || (!is_integral(n) && !is_integer(n)) || n.type==_CPLX) 
      return gentypeerr(contextptr);
    return _sort(idivis(abs(n,contextptr),contextptr),contextptr);
  }
  static const char _idivis_s []="idivis";
  static define_unary_function_eval (__idivis,&_idivis,_idivis_s);
  define_unary_function_ptr5( at_idivis ,alias_at_idivis,&__idivis,0,true);

  gen _divis(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type==_VECT)
      return apply(args,_divis,contextptr);
    return divis(factors(args,vx_var(),contextptr),contextptr);
  }
  static const char _divis_s []="divis";
  static define_unary_function_eval (__divis,&_divis,_divis_s);
  define_unary_function_ptr5( at_divis ,alias_at_divis,&__divis,0,true);

  /*
  gen ichinreme(const vecteur & a,const vecteur & b){
    vecteur r(2);
    gen p=a[1],q=b[1],u,v,d;
    egcd(p,q,u,v,d);
    if (d!=1)  return gensizeerr(contextptr);
    r[0]=(u*p*b[0]+v*q*a[0]%p*q);
    r[1]=p*q;
    return(r);
  }
  gen _ichinreme(const gen & args){
  if ( args){
    if ( (args.type!=_VECT) || (args._VECTptr->size()!=4) )
      return gensizeerr(contextptr);
    vecteur a(2).type==_STRNG && args.subtype==-1{
    if ( (args.type!=_VECT) || (args._VECTptr->size()!=4) )
      return gensizeerr(contextptr);
    vecteur a(2))) return  args){
    if ( (args.type!=_VECT) || (args._VECTptr->size()!=4) )
      return gensizeerr(contextptr);
    vecteur a(2);
    if ( (args.type!=_VECT) || (args._VECTptr->size()!=4) )
      return gensizeerr(contextptr);
    vecteur a(2),b(2);
    a[0]=args[0];
    a[1]=args[1];
    b[0]=args[2];
    b[1]=args[3];
    //gen a=args[0],p=args[1], b=args[2],q=args[3];
    return ichinreme(a,b);
  }
  static const char _ichinreme_s []="ichinreme";
  static define_unary_function_eval (__ichinreme,&_ichinreme,_ichinreme_s);
  define_unary_function_ptr5( at_ichinreme ,alias_at_ichinreme,&__ichinreme,0,true); 
  */

  gen euler(const gen & e,GIAC_CONTEXT){
    if (e==0)
      return e;
    vecteur v(ifactors(e,contextptr));
    if (!v.empty() && is_undef(v.front())) return v.front();
    const_iterateur it=v.begin(),itend=v.end();
    for (gen res(plus_one);;){
      if (it==itend)
	return res;
      gen p=*it;
      ++it;
      int n=it->val;
      res = res * (p-plus_one)*pow(p,n-1);
      ++it;
    }
  }
  static const char _euler_s []="euler";
  gen _euler(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type==_VECT)
      return apply(args,_euler,contextptr);
    if ( is_integer(args) && is_positive(args,contextptr))
      return euler(args,contextptr);
    return gentypeerr(contextptr);
  }
  static define_unary_function_eval (__euler,&_euler,_euler_s);
  define_unary_function_ptr5( at_euler ,alias_at_euler,&__euler,0,true);

  gen _propfrac(const gen & arg,GIAC_CONTEXT){
    if ( arg.type==_STRNG && arg.subtype==-1) return  arg;
    gen args(arg);
    vecteur v;
    if (arg.type==_VECT && arg._VECTptr->size()==2){
      v=vecteur(1,arg._VECTptr->back());
      args=arg._VECTptr->front();
      lvar(args,v);
    }
    else
      v=lvar(arg);
    gen g=e2r(args,v,contextptr);
    gen a,b;
    fxnd(g,a,b);
    {
      gen d=r2e(b,v,contextptr);
      g=_quorem(makesequence(r2e(a,v,contextptr),d,v.front()),contextptr);
      if (is_undef(g)) return g;
      vecteur &v=*g._VECTptr;
      return v[0]+rdiv(v[1],d,contextptr);
    }
  }
  static const char _propfrac_s []="propfrac";
  static define_unary_function_eval (__propfrac,&_propfrac,_propfrac_s);
  define_unary_function_ptr5( at_propfrac ,alias_at_propfrac,&__propfrac,0,true);
  
  gen iabcuv(const gen & a,const gen & b,const gen & c,GIAC_CONTEXT){
    gen d=gcd(a,b);
    if (c%d!=0)  return gensizeerr(gettext("No solution in ring"));
    gen a1=a/d,b1=b/d,c1=c/d;
    gen u,v,w;
    egcd(a1,b1,u,v,w);
    vecteur r(2);
    r[0]=smod(u*c1,b);
    r[1]=iquo(c-r[0]*a,b);
    return r;
  }
  gen _iabcuv(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if ( (args.type!=_VECT) || (args._VECTptr->size()!=3) )
      return gensizeerr(contextptr);
    gen a=args[0],b=args[1],c=args[2];
    return iabcuv(a,b,c,contextptr);
  }
  static const char _iabcuv_s []="iabcuv";
  static define_unary_function_eval (__iabcuv,&_iabcuv,_iabcuv_s);
  define_unary_function_ptr5( at_iabcuv ,alias_at_iabcuv,&__iabcuv,0,true);

  gen abcuv(const gen & a,const gen & b,const gen & c,const gen & x,GIAC_CONTEXT){
    gen g=_egcd(makesequence(a,b,x),contextptr);
    if (is_undef(g)) return g;
    vecteur & v=*g._VECTptr;
    gen h=_quorem(makesequence(c,v[2],x),contextptr);
    if (is_undef(h)) return h;
    vecteur & w=*h._VECTptr;
    if (!is_zero(w[1]))
      return gensizeerr(gettext("No solution in ring"));
    gen U=v[0]*w[0],V=v[1]*w[0];
    if (_degree(makesequence(c,x),contextptr).val<_degree(makesequence(a,x),contextptr).val+_degree(makesequence(b,x),contextptr).val ){
      U=_rem(makesequence(U,b,x),contextptr);
      V=_rem(makesequence(V,a,x),contextptr);
    }
    return makevecteur(U,V);
  }
  gen _abcuv(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if ( (args.type!=_VECT) || (args._VECTptr->size()<3) )
      return gensizeerr(contextptr);
    vecteur & v =*args._VECTptr;
    if (v.size()>3)
      return abcuv(v[0],v[1],v[2],v[3],contextptr);
    return abcuv(v[0],v[1],v[2],vx_var(),contextptr);
  }
  static const char _abcuv_s []="abcuv";
  static define_unary_function_eval (__abcuv,&_abcuv,_abcuv_s);
  define_unary_function_ptr5( at_abcuv ,alias_at_abcuv,&__abcuv,0,true);

  gen simp2(const gen & a,const gen & b,GIAC_CONTEXT){
    vecteur r(2);
    gen d=gcd(a,b);
    r[0]=normal(a/d,contextptr);
    r[1]=normal(b/d,contextptr);
    return r;
  }
  gen _simp2(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if ( (args.type!=_VECT) || (args._VECTptr->size()!=2) )
      return gensizeerr(contextptr);
    gen a=args[0],b=args[1];
    if ( (a.type==_VECT) || (b.type==_VECT) )
      return gensizeerr(contextptr);
    return simp2(a,b,contextptr);
  }
  static const char _simp2_s []="simp2";
  static define_unary_function_eval (__simp2,&_simp2,_simp2_s);
  define_unary_function_ptr5( at_simp2 ,alias_at_simp2,&__simp2,0,true);
 
  gen fxnd(const gen & a){
    vecteur v(lvar(a));
    gen g=e2r(a,v,context0); // ok
    gen n,d;
    fxnd(g,n,d);
    return makevecteur(r2e(n,v,context0),r2e(d,v,context0)); // ok
  }
  gen _fxnd(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type==_VECT)
      return apply(args,fxnd);
    return fxnd(args);
  }
  static const char _fxnd_s []="fxnd";
  static define_unary_function_eval (__fxnd,&_fxnd,_fxnd_s);
  define_unary_function_ptr5( at_fxnd ,alias_at_fxnd,&__fxnd,0,true); 

#ifndef NO_NAMESPACE_GIAC
} // namespace giac
#endif // ndef NO_NAMESPACE_GIAC
