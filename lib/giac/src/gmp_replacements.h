/*
 * Modified for NumOS / NeoCalculator in 2026 for ESP32-S3 and host integration.
 * See ../NUMOS_CHANGES.md for provenance and the file-by-file history.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "tommath.h"
#include <limits>
#ifdef NSPIRE
extern "C" {
#include <nspireio/nspireio.h>
}
#endif

extern "C" int ctrl_c_interrupted(int exception);

typedef mp_int mpz_t;

// Compatibility helper expected by some Giac code paths.
inline int alloc_mp_div(const mpz_t *a,const mpz_t *b,mpz_t *q,mpz_t *r,
                        mpz_t *,mpz_t *,mpz_t *,mpz_t *,mpz_t *){
  return mp_div((mp_int *)a,(mp_int *)b,q,r);
}

inline int mpz_init(mpz_t &  a){ return mp_init(&a);}
inline int mpz_init2(mpz_t &  a,int size){ return mp_init_size(&a,size);}
inline int mpz_init_set(mpz_t &  a,const mpz_t & b){ return mp_init_copy(&a,(mp_int *)&b);}
inline void mpz_init_set_si(mpz_t &  a,long b){ mp_init_l(&a,b); }
inline int mpz_init_set_ui(mpz_t &  a,unsigned long b){ return mp_init_ul(&a,b); }
inline void mpz_clear(mpz_t & a){ mp_clear(&a);}
inline int mpz_sgn(const mpz_t & a){ return mp_cmp_d((mp_int *)&a,0);}
inline int mpz_set(mpz_t &  a,const mpz_t & b){ return mp_copy((mp_int *)&b,&a);}
inline void mpz_set_ui(mpz_t &  a,unsigned int ui){ mp_set_u32(&a,ui); }
inline void mpz_set_si(mpz_t &  a,int ui){ mp_set_i32(&a,ui); }
inline int mpz_mod(mpz_t & c,const mpz_t & a,const mpz_t & b){ return mp_mod((mp_int *)&a,(mp_int *)&b,&c); }
inline int mpz_add(mpz_t & c,const mpz_t & a,const mpz_t & b){ return mp_add((mp_int *)&a,(mp_int *)&b,&c); }
inline void mpz_add_ui(mpz_t & c,const mpz_t & a,unsigned int B){ mp_int b; mp_init_u32(&b,B); mp_add((mp_int *)&a,&b,&c); mp_clear(&b); }
inline int mpz_sub(mpz_t & c,const mpz_t & a,const mpz_t & b){ return mp_sub((mp_int *)&a,(mp_int *)&b,&c); }
inline void mpz_sub_ui(mpz_t & c,const mpz_t & a,unsigned int B){ mp_int b; mp_init_u32(&b,B); mp_sub((mp_int *)&a,&b,&c); mp_clear(&b); }
inline int mpz_mul(mpz_t & c,const mpz_t & a,const mpz_t & b){ return mp_mul((mp_int *)&a,(mp_int *)&b,&c); }
inline int mpz_ior(mpz_t & c,const mpz_t & a,const mpz_t & b){ return mp_or((mp_int *)&a,(mp_int *)&b,&c); }
inline int mpz_xor(mpz_t & c,const mpz_t & a,const mpz_t & b){ return mp_xor((mp_int *)&a,(mp_int *)&b,&c); }
inline int mpz_and(mpz_t & c,const mpz_t & a,const mpz_t & b){ return mp_and((mp_int *)&a,(mp_int *)&b,&c); }
inline int mpz_pow_ui(mpz_t & c,const mpz_t & a,unsigned int B){ return mp_expt_n((mp_int *)&a,int(B),&c); }
inline void mpz_ui_pow_ui(mpz_t & c,unsigned int A,unsigned int B){ mp_int a ; mp_init_u32(&a,A); mp_expt_n(&a,int(B),&c); mp_clear(&a);}
inline int mpz_powm(mpz_t & d,const mpz_t &  a,const mpz_t & b,const mpz_t & c){ return mp_exptmod((mp_int *)&a,(mp_int *)&b,(mp_int *)&c,&d); }
inline void mpz_powm_ui(mpz_t & d,const mpz_t &  a,unsigned int B,const mpz_t & c){ mp_int b; mp_init_u32(&b,B); mp_exptmod((mp_int *)&a,&b,(mp_int *)&c,&d); mp_clear(&b);}
inline int mpz_gcd(mpz_t & c,const mpz_t & a,const mpz_t & b){ return mp_gcd((mp_int *)&a,(mp_int *)&b,&c); }
inline int mpz_gcdext(mpz_t & d,mpz_t & u, mpz_t & v,const mpz_t & a,const mpz_t & b){ return mp_exteuclid((mp_int *)&a,(mp_int *)&b,&u,&v,&d); }
inline int gcdint(int a,int b){
  int r;
  while (b){
    r=a%b;
    a=b;
    b=r;
  }
  return a<0?-a:a;
}
inline int mpz_lcm(mpz_t & c,const mpz_t & a,const mpz_t & b){ return mp_lcm((mp_int *)&a,(mp_int *)&b,&c); }
#ifdef TICE
inline void mpz_mul_ui(mpz_t & c,mpz_t & a,unsigned int B){
  mp_int b; mp_init_u32(&b,B); mp_mul((mp_int *)&a,&b,&c); mp_clear(&b);
}
#else
inline void mpz_mul_ui(mpz_t & c,mpz_t & a,unsigned int B){
  mp_mul_d(&a,B,&c);
}
#endif

inline int mpz_mul_2exp(mpz_t & c,const mpz_t & a,unsigned int B){ return mp_mul_2d((mp_int *)&a,B,&c); }
inline void mpz_tdiv_q_2exp(mpz_t & c,const mpz_t & a,unsigned int B){ mp_int d; mp_init(&d); mp_div_2d((mp_int *)&a,B,&c,&d); mp_clear(&d); }
inline void mpz_tdiv_r_2exp(mpz_t & d,const mpz_t & a,unsigned int B){ mp_int c; mp_init(&c); mp_div_2d((mp_int *)&a,B,&c,&d); mp_clear(&c); }
inline void mpz_addmul(mpz_t & c,const mpz_t & a,const mpz_t & b){ mp_int ab; mp_init(&ab); mp_mul((mp_int *)&a,(mp_int *)&b,&ab);  mp_add(&c,&ab,&c); mp_clear(&ab); }
inline void mpz_addmul_ui(mpz_t & c,const mpz_t & a,unsigned int B){ mp_int ab,b; mp_init(&ab); mp_init_u32(&b,B); mp_mul((mp_int *)&a,&b,&ab);  mp_add(&c,&ab,&c); mp_clear(&ab); mp_clear(&b); }
inline void mpz_submul(mpz_t & c,const mpz_t & a,const mpz_t & b){ mp_int ab; mp_init(&ab); mp_mul((mp_int *)&a,(mp_int *)&b,&ab);  mp_sub(&c,&ab,&c); mp_clear(&ab); }
inline void mpz_submul_ui(mpz_t & c,const mpz_t & a,unsigned int B){ mp_int ab; mp_init(&ab); mp_int b; mp_init_u32(&b,B); mp_mul((mp_int *)&a,&b,&ab);  mp_sub(&c,&ab,&c); mp_clear(&ab); mp_clear(&b);}
inline int mpz_fdiv_r(mpz_t & c,const mpz_t & a,const mpz_t & b){ return mp_mod((mp_int *)&a,(mp_int *)&b,&c); }
inline int mpz_tdiv_r(mpz_t & c,const mpz_t & a,const mpz_t & b){ return mp_mod((mp_int *)&a,(mp_int *)&b,&c); }
inline void mpz_fdiv_r_ui(mpz_t & c,const mpz_t & a,unsigned b){ mp_digit C; mp_mod_d((mp_int *)&a,b,&C); mp_set_u32(&c,static_cast<uint32_t>(C)); }
inline void mpz_fdiv_q_ui(mpz_t & c,const mpz_t & a,unsigned b){ mp_digit C; mp_div_d((mp_int *)&a,b,&c,&C);}
inline void mpz_divexact_ui(mpz_t & c,const mpz_t & a,unsigned b){ mp_digit C; mp_div_d((mp_int *)&a,b,&c,&C);}
inline void mpz_fdiv_qr_ui(mpz_t & c,mpz_t& d,const mpz_t & a,unsigned b){ mp_digit D; mp_div_d((mp_int *)&a,b,&c,&D); mp_set_u32(&d,static_cast<uint32_t>(D)); }
inline void mpz_fdiv_q(mpz_t & c,const mpz_t & a,const mpz_t & b){ mp_int d; mp_init(&d); mp_div((mp_int *)&a,(mp_int *)&b,&c,&d); mp_clear(&d); }
inline void mpz_tdiv_q(mpz_t & c,const mpz_t & a,const mpz_t & b){ mp_int d; mp_init(&d); mp_div((mp_int *)&a,(mp_int *)&b,&c,&d); mp_clear(&d); }
inline int mpz_fdiv_qr(mpz_t & c,mpz_t & d,const mpz_t & a,const mpz_t & b){ return mp_div((mp_int *)&a,(mp_int *)&b,&c,&d); }
inline int mpz_tdiv_qr(mpz_t & c,mpz_t & d,const mpz_t & a,const mpz_t & b){ return mp_div((mp_int *)&a,(mp_int *)&b,&c,&d); }
inline int mpz_sqrt(mpz_t &  a,const mpz_t & b){ return mp_sqrt((mp_int *)&b,&a);}
inline int mpz_sqr(mpz_t &  a,const mpz_t & b){ return mp_sqr((mp_int *)&b,&a);}
inline int mpz_neg(mpz_t &  a,const mpz_t & b){ return mp_neg((mp_int *)&b,&a);}
inline int mpz_abs(mpz_t &  a,const mpz_t & b){ return mp_abs((mp_int *)&b,&a);}
inline int mpz_cmp(const mpz_t &  a,const mpz_t & b){ return mp_cmp((mp_int *)&a,(mp_int *)&b);}
inline int mpz_cmp_ui(const mpz_t &  a,unsigned int B){ return mp_cmp_d((mp_int *)&a,B);}
inline int mpz_cmp_si(const mpz_t &  a,int B){ mp_int b; int res; mp_init(&b); mpz_set_si(b,B); res=mp_cmp((mp_int *)&a,&b); mp_clear(&b); return res;}
inline int mpz_sizeinbase(const mpz_t & a,int radix){ size_t size=0; mp_radix_size((mp_int *)&a,radix,&size); return static_cast<int>(size); }
inline int mpz_get_si(const mpz_t & a){
#if 1
  return static_cast<int>(mp_get_l((mp_int *)&a));
#else
  mp_int tmp,res;
  mp_init_set_int(&tmp,1<<16); mp_init(&res);
  mp_mul(&tmp,&tmp,&tmp);
  int neg = a.sign==MP_NEG?-1:1;
  if (neg){
    mp_copy((mp_int*)&a,&res);
    res.sign=MP_ZPOS;
    mp_mod(&res,&tmp,&res);
  }
  else
    mp_mod((mp_int *)&a,&tmp,&res);
  if (res.sign==MP_NEG)
    mp_add(&res,&tmp,&res);
  char s[16];
  mp_toradix(&res,s,10);
  mp_clear(&tmp);
  mp_clear(&res);
  return neg*strtol(s,0,10);
#endif
} // WARNING
inline unsigned mpz_get_ui(const mpz_t & a){
#if 1
  return static_cast<unsigned>(mp_get_ul((mp_int *)&a));
#else
  mp_int tmp,res;
  mp_init_set_int(&tmp,1<<16); mp_init(&res);
  mp_mul(&tmp,&tmp,&tmp);
  mp_mod((mp_int *)&a,&tmp,&res);
  if (res.sign==MP_NEG)
    mp_add(&res,&tmp,&res);
  char s[16];
  mp_toradix(&res,s,10);
  mp_clear(&tmp);
  mp_clear(&res);
  return strtol(s,0,10);
#endif
} // WARNING
inline int mpz_gcd_ui(mpz_t * c,const mpz_t & a,unsigned B){ 
  mpz_t b; mp_init_u32(&b,B); 
  mp_mod((mp_int *)&a,&b,&b);
  int res=mpz_get_ui(b); 
  mp_clear(&b);
  return gcdint(B,res);
}
inline int mpz_set_str(mpz_t &  z,const char * s,int base){return mp_read_radix(&z,s,base);}
inline int mpz_get_str(char * s,int base,const mpz_t &  z){
  size_t size=0;
  if (mp_radix_size((mp_int *)&z,base,&size)!=MP_OKAY)
    return MP_VAL;
  return mp_to_radix((mp_int *)&z,s,size,nullptr,base);
}
inline double mpz_get_d(const mpz_t & z){ 
  if (mp_count_bits((mp_int *)&z)>1023) {
#ifdef TICE
    return 1e38/1e-38;
#else
    return std::numeric_limits<double>::infinity();
#endif
  }
  char s[512]; 
  mp_to_radix((mp_int *)&z,s,sizeof(s),nullptr,10); 
#ifdef NSPIRE
  return Strtod(s,0);
#else
  return strtod(s,0);
#endif
}
inline void mpz_set_d(mpz_t & z,double d){ 
#if 0
  unsigned long long u=*(unsigned long long*)(&d);
  unsigned long long m=u & 0x000fffffffffffffULL;
  long mh=(1<<24)+(m>>28);
  long ml=m&0x0fffffff;
  u >>= 52;
  int e=u%(1<<11);
  e -= 1024+51;
  u >>= 11;
  if (u)
    mh=-mh;
  mpz_set_si(z,mh);
  mpz_mul_2exp(z,z,28);
  if (u)
    mpz_sub_ui(z,z,ml);
  else
    mpz_add_ui(z,z,ml);
  if (e>0)
    mpz_mul_2exp(z,z,e);
  if (e<0)
    mpz_tdiv_q_2exp(z,z,-e);
#else
  char ch[32];
  sprintf(ch,"%.14g",d);
  mp_read_radix(&z,ch,10);
#endif
}
inline void mpz_fac_ui(mpz_t & z,unsigned int i){
  mpz_set_ui(z,1);
  for (unsigned long int j=2;j<=i;j++){
    ctrl_c_interrupted(1);
    mpz_mul_ui(z,z,j);
  }
}
inline int mpz_invert(mpz_t & res,const mpz_t & a,const mpz_t & m){
  mp_int v, d;
  mp_init(&v); mp_init(&d);
  mp_exteuclid((mp_int *)&a,(mp_int *)&m,&res,&v,&d);
  int r=!mpz_cmp_ui(d,1);
  mp_clear(&v); mp_clear(&d);
  return r;
}

inline int mpz_probab_prime_p(const mpz_t & a,int){
  bool result=false;
  mp_prime_is_prime((mp_int *)&a, mp_prime_rabin_miller_trials(mp_count_bits((mp_int *)&a)),&result);
  return result?1:0;
}
inline int mpz_legendre(const mpz_t & a,const mpz_t & n){
  int result;
  mp_kronecker((mp_int *)&a,(mp_int *)&n,&result);
  return result;
}
inline int mpz_jacobi(const mpz_t & a,const mpz_t & n){
  int result;
  mp_kronecker((mp_int *)&a,(mp_int *)&n,&result);
  return result;
}
inline bool mpz_perfect_square_p(const mpz_t & a){
  bool res=false;
  mp_is_square((mp_int *)&a,&res);
  return res;
}
inline int mpz_popcount(const mpz_t & a){
  return 0; // FIXME, should be numbers of bits set to 1
}
inline int mpz_hamdist(const mpz_t & a,const mpz_t & b){
  mpz_t c;
  mp_init(&c);
  mp_xor(&c,(mp_int *) &a,(mp_int *)&b);
  int res=mpz_popcount(c);
  mp_clear(&c);
  return res;
}

#define LONGFLOAT_DOUBLE
typedef double mpf_t;
#define mpf_clear(x) 
#define mpf_init(x) 
#define mpf_init_set(x,y) (x=y)
#define mpf_init_set_d(x,y) (x=y)
#define mpf_init_set_si(x,y) (x=y)
#define mpf_set_z(x,y) 
#define mpf_set(x,y) (x=y)
inline int mpf_set_str(double & x,const char * s,int base){ if (base!=10) return 1; x=strtod(s,0); return 0; } 
#define mpf_get_d(x) (x)
#define mpf_add(x,y,z) (x=y+z)
#define mpf_sub(x,y,z) (x=y-z)
#define mpf_mul(x,y,z) (x=y*z)
#define mpf_neg(x,y) (x=-y)
#define mpf_ui_div(z,x,y) (z=x/y)
#define mpf_sqrt(x,y) (x=sqrt(y))
#define mpf_sgn(x) (x>0?1:-1)
