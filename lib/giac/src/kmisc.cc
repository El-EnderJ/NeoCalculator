/*
 * Modified for NumOS / NeoCalculator in 2026 for ESP32-S3 and host integration.
 * See ../NUMOS_CHANGES.md for provenance and the file-by-file history.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* -*- mode:C++ ; compile-command: "g++ -I.. -I../include -DHAVE_CONFIG_H -DIN_GIAC -DGIAC_GENERIC_CONSTANTS -fno-strict-aliasing -g -c misc.cc -Wall" -*- */
#include "giacPCH.h"
/*
 *  Copyright (C) 2001, 2007 R. De Graeve, B. Parisse, Institut Fourier, 38402 St Martin d'Heres
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
#include <fstream>
#include <string>
#include "misc.h"
#include "usual.h"
#include "sym2poly.h"
#include "rpn.h"
#include "prog.h"
#include "derive.h"
#include "subst.h"
#include "intg.h"
#include "vecteur.h"
#include "ifactor.h"
#include "solve.h"
#include "modpoly.h"
#include "permu.h"
#include "sym2poly.h"
#include "plot.h"
#include "lin.h"
#include "modpoly.h"
#include "desolve.h"
#include "alg_ext.h"
#include "input_parser.h"
#include "input_lexer.h"
#include "maple.h"
#include "quater.h"
#include "sparse.h"
#include "giacintl.h"
#include "console.h"
#include "menuGUI.h"
#ifndef FXCG
#include "k_csdk.h"
#endif
#include "main.h"

extern int line_start;
#ifdef TICE
const char * select_var();
const char * keytostring(int key,int keyflag,bool py);
int chartab();
int print_msg12(const char * msg1,const char * msg2,int textY);
int confirm(const char * msg1,const char * msg2,bool acexit);
int do_confirm(const char * s);
bool confirm_overwrite();
void invalid_varname();
int inputline(const char * msg1,const char * msg2,string & s,bool numeric,int ypos);
void print_alpha_shift(int keyflag);
void insert(string & s,int pos,const char * add);
#endif

#ifndef NO_NAMESPACE_GIAC
namespace giac {
#endif // ndef NO_NAMESPACE_GIAC

  gen _addr(const gen & g,GIAC_CONTEXT){
    if (g.type==_VECT && g.subtype==_SEQ__VECT && g._VECTptr->size()==2){
      gen & obj=g._VECTptr->front();
      vecteur & ptr=*obj._VECTptr;
      return makevecteur((longlong)(size_t)(&ptr),(int) taille(obj,RAND_MAX),tailles(obj));
    }
    vecteur & ptr=*g._VECTptr;
    return (longlong)(size_t)(&ptr);
  }
  static const char _addr_s []="addr";
  static define_unary_function_eval (__addr,&_addr,_addr_s);
  define_unary_function_ptr5( at_addr ,alias_at_addr,&__addr,0,true);

#ifdef WITH_PLOT
  static vecteur listplot(const gen & g,vecteur & attributs,GIAC_CONTEXT){
    if (g.type!=_VECT || g._VECTptr->empty())
      return vecteur(1,gensizeerr(contextptr));
    int s=read_attributs(*g._VECTptr,attributs,contextptr);
    vecteur v;
    if (g.subtype==_SEQ__VECT && s>=4 && g[1].type==_IDNT)
      return listplot(_seq(g,contextptr),attributs,contextptr);
    if (s>=2 && g._VECTptr->front().type<=_DOUBLE_ && g[1].type==_VECT){
      int l=int(g[1]._VECTptr->size());
      v=*g._VECTptr;
      v[0]=vecteur(l);
      double d=evalf_double(g._VECTptr->front(),1,contextptr)._DOUBLE_val;
      for (int j=0;j<l;++j){
	(*v[0]._VECTptr)[j]=j+d;
      }
      if (!ckmatrix(v))
	return vecteur(1,gendimerr(contextptr));
      v=mtran(v);
    }
    else {
      if (g._VECTptr->front().type==_VECT){
	vecteur & v0 = *g._VECTptr->front()._VECTptr;
	int v0s=int(v0.size());
	if (s==1)
	  v=v0;
	else {
	  if (v0s==1 && ckmatrix(g))
	    v=*mtran(*g._VECTptr).front()._VECTptr;
	  else
	    v=*g._VECTptr;
	}
      }
      else
	v=*g._VECTptr;
    }
    s=int(v.size());
    vecteur res;
    res.reserve(s);
    for (int i=0;i<s;++i){
      gen tmp=v[i];
      if (tmp.type==_VECT){
	if (tmp._VECTptr->size()==2)
	  res.push_back(vect2c(tmp));
	else
	  return vecteur(1,gendimerr(contextptr));
      }
      else
	res.push_back(i+(xcas_mode(contextptr)?1:0)+cst_i*tmp);
    }
    return res;
  }

  gen _listplot(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    vecteur attributs(1,FL_BLACK);
    vecteur res=listplot(g,attributs,contextptr);
    if (is_undef(res) && !res.empty())
      return res.front();
    if (attributs.size()>1)
      return symb_pnt_name(gen(res,_GROUP__VECT),attributs[0],attributs[1],contextptr);
    else
      return symb_pnt(gen(res,_GROUP__VECT),attributs[0],contextptr);
  }
  static const char _plotlist_s []="plotlist";
  static define_unary_function_eval (__plotlist,&_listplot,_plotlist_s);
  define_unary_function_ptr5( at_plotlist ,alias_at_plotlist,&__plotlist,0,true);
  // [[x1 y1] [x2 y2] ...]
  static gen scatterplot(const gen & g,int mode,GIAC_CONTEXT){
    bool polygone=bool(mode&1),scatter=bool(mode&2),bar=bool(mode &4);
    vecteur v(gen2vecteur(g));
    vecteur attr(1,FL_BLACK);
    int s=read_attributs(v,attr,contextptr);
    if (s==1)
      v=*v.front()._VECTptr;
    else
      v=vecteur(v.begin(),v.begin()+s);
    if (s>2 && v.back().type==_INT_){
      // discard size
      --s;
      v.pop_back();
    }
    if (g.type==_VECT && s==2 && g.subtype==_SEQ__VECT){
      if (!ckmatrix(v))
	return gensizeerr(contextptr); 
      v=mtran(v);
    }
    unsigned ncol=0;
    const gen & vf=v.front();
    if (vf.type!=_VECT){
#if 0
      if (polygone)
	return _listplot(g,contextptr);
      vecteur attributs(1,FL_BLACK);
      vecteur res=listplot(g,attributs,contextptr);
      int s=int(res.size());
      for (int i=0;i<s;++i){
	res[i]=symb_pnt(res[i],attributs[0],contextptr);
      }
      return gen(res,_SEQ__VECT);
#else
      v=gen2vecteur(g);
      attr=vecteur(1,FL_BLACK);
      s=read_attributs(v,attr,contextptr);
      vecteur res;
      if (s==1 && v[0].type==_VECT){
        v=*v[0]._VECTptr;
        s=v.size();
      }
      for (int i=0;i<s-1;i+=2){
        res.push_back(makevecteur(v[i],v[i+1]));
      }
      v.swap(res);
#endif
    }
    if (!ckmatrix(v)||v.empty() || (ncol=unsigned(v[0]._VECTptr->size()))<2)
      return gensizeerr(contextptr);
    if (v[0]._VECTptr->front().type==_STRNG){
      if (attr.size()==1)
	attr.push_back(vecteur(v[0]._VECTptr->begin()+1,v[0]._VECTptr->end()));
      v.erase(v.begin());
    }
    v=gen2vecteur(_sort(v,contextptr));
    const_iterateur it=v.begin(),itend=v.end();
    vecteur res;
    string nullstr;
    vecteur vres;
    for (unsigned j=1;j<ncol;++j){
      vecteur attributs(1,int(j<=FL_WHITE?j-1:j));
      attributs.push_back(string2gen("",false));
      if (!attr.empty()){ 
	if (ncol==2)
	  attributs[0]=attr[0];
	if (attr[0].type==_VECT && attr[0]._VECTptr->size()>=j)
	  attributs[0]=(*attr[0]._VECTptr)[j-1];
	if (attr.size()>1){
	  if (ncol==2)
	    attributs[1]=attr[1];
	  if (attr[1].type==_VECT && attr[1]._VECTptr->size()>=j)
	    attributs[1]=(*attr[1]._VECTptr)[j-1];
	}
      }
      res.clear();
      for (it=v.begin();it!=itend;++it){
	gen tmp=(*it->_VECTptr)[j];
	if (tmp.type==_STRNG && attributs[1].type==_STRNG && *attributs[1]._STRNGptr==nullstr)
	  attributs[1]=gen(*tmp._STRNGptr,contextptr);
	else {
	  if (is_equal(tmp))
	    read_attributs(vecteur(1,tmp),attributs,contextptr);
	  else {
	    tmp=it->_VECTptr->front()+cst_i*tmp;
	    if (polygone)
	      res.push_back(tmp);
	    if (scatter)
	      vres.push_back(symb_pnt_name(tmp,attributs[0],string2gen(( (it==v.begin() && !polygone) ?gen2string(attributs[1]):""),false),contextptr));
	    if (bar)
	      vres.push_back(symb_segment(it->_VECTptr->front(),tmp,attributs,_GROUP__VECT,contextptr));
	  }
	}
      }
      if (polygone)
	vres.push_back(symb_pnt_name(res,attributs[0],attributs[1],contextptr));
    }
    if (polygone && !scatter && ncol==2)
      return vres.front();
    return gen(vres,_SEQ__VECT);
  }
  gen _scatterplot(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    // dbg_printf("scatterplot %s\n",g.print().c_str());
    return scatterplot(g,2,contextptr);
  }
  static const char _scatterplot_s []="scatterplot";
  static define_unary_function_eval (__scatterplot,&_scatterplot,_scatterplot_s);
  define_unary_function_ptr5( at_scatterplot ,alias_at_scatterplot,&__scatterplot,0,true);

  gen _polygonplot(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    return scatterplot(g,1,contextptr);
  }
  static const char _polygonplot_s []="polygonplot";
  static define_unary_function_eval (__polygonplot,&_polygonplot,_polygonplot_s);
  define_unary_function_ptr5( at_polygonplot ,alias_at_polygonplot,&__polygonplot,0,true);

  static const char _ligne_polygonale_s []="ligne_polygonale";
  static define_unary_function_eval (__ligne_polygonale,&_polygonplot,_ligne_polygonale_s);
  define_unary_function_ptr5( at_ligne_polygonale ,alias_at_ligne_polygonale,&__ligne_polygonale,0,true);

  gen _polygonscatterplot(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    return scatterplot(g,3,contextptr);
  }
  static const char _polygonscatterplot_s []="polygonscatterplot";
  static define_unary_function_eval (__polygonscatterplot,&_polygonscatterplot,_polygonscatterplot_s);
  define_unary_function_ptr5( at_polygonscatterplot ,alias_at_polygonscatterplot,&__polygonscatterplot,0,true);
#endif

  static const char _set_language_s []="set_language";
  gen _set_language(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    return 1;
  }
static define_unary_function_eval (__set_language,&_set_language,_set_language_s);
  define_unary_function_ptr5( at_set_language ,alias_at_set_language,&__set_language,0,true);

  gen _scalar_product(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT || args._VECTptr->size()!=2)
      return gensizeerr(contextptr);
    vecteur & v=*args._VECTptr;
    return scalar_product(v[0],v[1],contextptr);
  }    
  static const char _scalar_product_s []="scalar_product";
  static define_unary_function_eval (__scalar_product,&_scalar_product,_scalar_product_s);
  define_unary_function_ptr5( at_scalar_product ,alias_at_scalar_product,&__scalar_product,0,true);

  static const char _dot_s []="dot";
  static define_unary_function_eval (__dot,&_scalar_product,_dot_s);
  define_unary_function_ptr5( at_dot ,alias_at_dot,&__dot,0,true);

  gen _compare(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT || args._VECTptr->size()!=2)
      return gensizeerr(contextptr);
    vecteur & v=*args._VECTptr;
    return v[0].islesscomplexthan(v[1]);
  }    
  static const char _compare_s []="compare";
  static define_unary_function_eval (__compare,&_compare,_compare_s);
  define_unary_function_ptr5( at_compare ,alias_at_compare,&__compare,0,true);

  gen _preval(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return symbolic(at_preval,args);
    vecteur & v=*args._VECTptr;
    int s=int(v.size());
    if (s<3)
      return gentoofewargs("");
    gen f(v[0]),x,a,b;
    a=v[1];
    b=v[2];
    if (s==3)
      x=vx_var();
    else 
      x=v[3];
    if (x.type!=_IDNT)
      return gentypeerr(contextptr);
    return preval(f,x,a,b,contextptr);
  }    
  static const char _preval_s []="preval";
  static define_unary_function_eval (__preval,&_preval,_preval_s);
  define_unary_function_ptr5( at_preval ,alias_at_preval,&__preval,0,true);

  // return suitable gen for interpolation
  // if possible return j, if j is too large, return a GF element
  gen interpolate_xi(int j,const gen &coeff){
    if (coeff.type==_MOD){
    }
    if (coeff.type!=_USER)
      return j;
#if 0 // ndef NO_RTTI
    if (galois_field * gf=dynamic_cast<galois_field *>(coeff._USERptr)){
      if (j<gf->p.val)
	return j;
      galois_field g(*gf); // copy
      g.a=_revlist(_convert(makesequence(j,change_subtype(_BASE,_INT_MAPLECONVERSION),gf->p),context0),context0);
      return g;
    }
#endif
    return j;
  }
  // characteristic must be large enough to interpolate the resultant
  // d1+1 evaluations + there is a probab. of 2/p of bad evaluation
  // (d1+1)*p/(p-2)<p -> p>d1+3 + we add some more for safety
  // on Galois fields comparison should be (d+1)*p/(p-2)<p^m
  // assuming interpolation is done with all fields elements
  bool interpolable_resultant(const polynome & P,int d1,gen & coefft,bool extend,GIAC_CONTEXT){
    int tt=coefft.type;
    if (tt!=_USER)
      tt=coefftype(P,coefft);
    return interpolable(d1,coefft,extend,contextptr);
  }

  bool interpolable(int d1,gen & coefft,bool extend,GIAC_CONTEXT){
    int tt=coefft.type;
    if (tt==_USER){
#if 0 // ndef NO_RTTI
      if (galois_field * gf=dynamic_cast<galois_field *>(coefft._USERptr)){
	gen m=gf->p;
	if (!is_integer(m))
	  return false;
	return is_greater(pow(m,gf->P._VECTptr->size()-1,contextptr),d1+20,contextptr);
      }
      return true;
#endif
    }
    if (tt==_MOD){
      gen m=*(coefft._MODptr+1);
      if (!is_integer(m))
	return false;
      if (is_greater(m,d1+20,contextptr))
	return true;
      if (!extend || !_isprime(m,contextptr).val)
	return false;
      // build a suitable field extension...
      int n=int(std::ceil(std::log(d1+20.0)/std::log(evalf_double(m,1,contextptr)._DOUBLE_val)));
#if 1 // def NO_RTTI
      return false;
#else
      coefft=_galois_field(makesequence(m,n),contextptr);
      return true;
#endif
    }
    return true;
  }
  vecteur divided_differences(const vecteur & x,const vecteur & y){
    vecteur res(y);
    int s=int(x.size());
    for (int k=1;k<s;++k){
      for (int j=s-1;j>=k;--j){
	res[j]=(res[j]-res[j-1])/(x[j]-x[j-k]);
      }
      //CERR << k << res << "\n";
    }
    return res;
  }
  gen _lagrange(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return symbolic(at_lagrange,args);
    vecteur & v=*args._VECTptr;
    int s=int(v.size());
    if (s<2)
      return gentoofewargs("");
    gen v0(v[0]),v1(v[1]),x=vx_var();
    if (ckmatrix(v0) && v0._VECTptr->size()==2){
      x=v1;
      v1=v0._VECTptr->back();
      v0=v0._VECTptr->front();
    }
    if (s>=3)
      x=v[2];
    if (v0.type!=_VECT && v1.type==_VECT){
      gen tmp=v1;
      v1=_apply(makesequence(v0,v1),contextptr);
      v0=tmp;
    }
    if (v1.type!=_VECT && v0.type==_VECT)
      v1=_apply(makesequence(v1,v0),contextptr);
    if ( (v0.type!=_VECT) || (v1.type!=_VECT) )
      return gensizeerr(contextptr);
    vecteur & vx =*v0._VECTptr;
    vecteur & vy=*v1._VECTptr;
    s=int(vx.size());
    if (!s || vy.size()!=unsigned(s))
      return gendimerr(contextptr);
    // Using divided difference instead of the theoretical formula
    if (x.type==_VECT && x._VECTptr->empty()){
      vecteur res;
      interpolate(vx,vy,res,0);
      return res;
    }
    vecteur w=divided_differences(vx,vy);
    if (x==at_lagrange)
      return w;
    gen pi(1),res(w[s-1]);
    for (int i=s-2;i>=0;--i){
      res = res*(x-vx[i])+w[i];
      if (i%100==99) // otherwise segfault 
	res=ratnormal(res,contextptr);
    }
    return res;
    /*
    gen res(zero);
    for (int i=0;i<s;++i){
      gen pix(plus_one),pix0(plus_one),x0(vx[i]);
      for (int j=0;j<s;++j){
	if (j==i)
	  continue;
	pix=pix*(x-vx[j]);
	pix0=pix0*(x0-vx[j]);
      }
      res=res+vy[i]*rdiv(pix,pix0);
    }
    return res;
    */
  }    
  static const char _lagrange_s []="lagrange";
  static define_unary_function_eval (__lagrange,&_lagrange,_lagrange_s);
  define_unary_function_ptr5( at_lagrange ,alias_at_lagrange,&__lagrange,0,true);

  gen _reorder(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return symbolic(at_reorder,args);
    vecteur & v=*args._VECTptr;
    int s=int(v.size());
    if (s<2)
      return gentoofewargs("");
    gen e(v[0]),l(v[1]);
    if (e.type<=_POLY) return e;
    if (l.type!=_VECT)
      return gensizeerr(contextptr);
    vecteur w(*l._VECTptr);
    lvar(e,w);
    e=e2r(e,w,contextptr);
    return r2e(e,w,contextptr);
  }    
  static const char _reorder_s []="reorder";
  static define_unary_function_eval (__reorder,&_reorder,_reorder_s);
  define_unary_function_ptr5( at_reorder ,alias_at_reorder,&__reorder,0,true);

  gen _adjoint_matrix(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return symbolic(at_adjoint_matrix,args);
    matrice mr(*args._VECTptr);
    if (!is_squarematrix(mr))
      return gensizeerr(contextptr);
    matrice m_adj;
    vecteur p_car;
    p_car=mpcar(mr,m_adj,true,true,contextptr);
    return makevecteur(p_car,m_adj);
  }    
  static const char _adjoint_matrix_s []="adjoint_matrix";
  static define_unary_function_eval (__adjoint_matrix,&_adjoint_matrix,_adjoint_matrix_s);
  define_unary_function_ptr5( at_adjoint_matrix ,alias_at_adjoint_matrix,&__adjoint_matrix,0,true);

  gen _equal2diff(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    return apply(args,equal2diff);
  }    
  static const char _equal2diff_s []="equal2diff";
  static define_unary_function_eval (__equal2diff,&_equal2diff,_equal2diff_s);
  define_unary_function_ptr5( at_equal2diff ,alias_at_equal2diff,&__equal2diff,0,true);

  static gen equal2list(const gen & arg){
    if ( !is_equal(arg))
      return makevecteur(arg,zero);
    return arg._SYMBptr->feuille;
  }
  gen _equal2list(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    return apply(args,equal2list);
  }    
  static const char _equal2list_s []="equal2list";
  static define_unary_function_eval (__equal2list,&_equal2list,_equal2list_s);
  define_unary_function_ptr5( at_equal2list ,alias_at_equal2list,&__equal2list,0,true);

  gen _rank(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return gentypeerr(contextptr); // return symbolic(at_adjoint_matrix,args);
    matrice mr(*args._VECTptr);
    if (!ckmatrix(mr))
      return gensizeerr(contextptr);
    mr=mrref(mr,contextptr);
    int r=int(mr.size());
    for (;r;--r){
      if (!is_zero(mr[r-1]))
	break;
    }
    return r;
  }    
  static const char _rank_s []="rank";
  static define_unary_function_eval (__rank,&_rank,_rank_s);
  define_unary_function_ptr5( at_rank ,alias_at_rank,&__rank,0,true);

  gen _sec(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    return inv(cos(args,contextptr),contextptr);
  }    
  static const char _sec_s []="sec";
  static define_unary_function_eval (__sec,&_sec,_sec_s);
  define_unary_function_ptr5( at_sec ,alias_at_sec,&__sec,0,true);

  gen _csc(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    return inv(sin(args,contextptr),contextptr);
  }    
  static const char _csc_s []="csc";
  static define_unary_function_eval (__csc,&_csc,_csc_s);
  define_unary_function_ptr5( at_csc ,alias_at_csc,&__csc,0,true);

  gen _cot(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    return rdiv(cos(args,contextptr),sin(args,contextptr),contextptr);
  }    
  static const char _cot_s []="cot";
  static define_unary_function_eval (__cot,&_cot,_cot_s);
  define_unary_function_ptr5( at_cot ,alias_at_cot,&__cot,0,true);

  gen _asec(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    return acos(inv(args,contextptr),contextptr);
  }    
  static const char _asec_s []="asec";
  static define_unary_function_eval (__asec,&_asec,_asec_s);
  define_unary_function_ptr5( at_asec ,alias_at_asec,&__asec,0,true);

  gen _acsc(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    return asin(inv(args,contextptr),contextptr);
  }    
  static const char _acsc_s []="acsc";
  static define_unary_function_eval (__acsc,&_acsc,_acsc_s);
  define_unary_function_ptr5( at_acsc ,alias_at_acsc,&__acsc,0,true);

  gen _acot(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (is_zero(args))
      //grad
      return angle_radian(contextptr)?cst_pi_over_2:(angle_degree(contextptr)?90:100);
#if 0
    if (abs_calc_mode(contextptr)==38)
      return cst_pi_over_2-atan(args,contextptr);
#endif
    return atan(inv(args,contextptr),contextptr);
  }    
  static const char _acot_s []="acot";
  static define_unary_function_eval (__acot,&_acot,_acot_s);
  define_unary_function_ptr5( at_acot ,alias_at_acot,&__acot,0,true);

  // args=[u'*v,v] or [[F,u'*v],v] -> [F+u*v,-u*v']
  // a third argument would be the integration var
  // if v=0 returns F+integrate(u'*v,x)
  gen _ibpu(const gen & args,GIAC_CONTEXT) {
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if ( (args.type!=_VECT) || (args._VECTptr->size()<2) )
      return symbolic(at_ibpu,args);
    vecteur & w=*args._VECTptr;
    gen X(vx_var()),x(vx_var()),a,b;
    bool bound=false;
    if (w.size()>=3)
      x=X=w[2];
    if (is_equal(x))
      x=x._SYMBptr->feuille[0];
    if (w.size()>=5)
      X=symb_equal(x,symb_interval(w[3],w[4]));
    if (is_equal(X) && X._SYMBptr->feuille[1].is_interval()){
      a=X._SYMBptr->feuille[1]._SYMBptr->feuille[0];
      b=X._SYMBptr->feuille[1]._SYMBptr->feuille[1];
      bound=true;
    }
    gen u,v(w[1]),uprimev,F;
    if (w.front().type==_VECT){
      vecteur & ww=*w.front()._VECTptr;
      if (ww.size()!=2)
	return gensizeerr(contextptr);
      F=ww.front();
      uprimev=ww.back();
    }
    else 
      uprimev=w.front();
    if (is_zero(v) || is_one(v)){
      gen tmp=integrate_gen(uprimev,x,contextptr);
      if (is_undef(tmp)) return tmp;
      if (bound)
	tmp=preval(tmp,x,a,b,contextptr);
      return tmp+F;
    }
    gen uprime(normal(rdiv(uprimev,v,contextptr),contextptr));
    u=integrate_gen(uprime,x,contextptr);
    if (is_undef(u)) return u;
    if (bound)
      F += preval(u*v,x,a,b,contextptr);
    else
      F += u*v;      
    return makevecteur(F,normal(-u*derive(v,x,contextptr),contextptr));
  }
  static const char _ibpu_s []="ibpu";
  static define_unary_function_eval (__ibpu,&_ibpu,_ibpu_s);
  define_unary_function_ptr5( at_ibpu ,alias_at_ibpu,&__ibpu,0,true);

  gen _changebase(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return symbolic(at_changebase,args);
    vecteur & v=*args._VECTptr;
    if (v.size()!=2)
      return gentypeerr(contextptr);
    gen a=v.front(),p=v.back();
    if (!is_squarematrix(p))
      return gensizeerr(contextptr);
    return minv(*p._VECTptr,contextptr)*a*p;
  }    
  static const char _changebase_s []="changebase";
  static define_unary_function_eval (__changebase,&_changebase,_changebase_s);
  define_unary_function_ptr5( at_changebase ,alias_at_changebase,&__changebase,0,true);

  static gen epsilon2zero(const gen & g,GIAC_CONTEXT){
    switch (g.type){
    case _DOUBLE_:
      if (fabs(g._DOUBLE_val)<epsilon(contextptr))
	return zero;
      else
	return g;
    case _CPLX:
      return epsilon2zero(re(g,contextptr),contextptr)+cst_i*epsilon2zero(im(g,contextptr),contextptr);
    case _SYMB:
      return symbolic(g._SYMBptr->sommet,epsilon2zero(g._SYMBptr->feuille,contextptr));
    case _VECT:
      return apply(g,epsilon2zero,contextptr);
    default:
      return g;
    }
  }
  gen _epsilon2zero(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type==_VECT && args.subtype==_SEQ__VECT && args._VECTptr->size()==2){
      gen p=evalf_double(args._VECTptr->back(),1,contextptr);
      if (p.type==_DOUBLE_ && p._DOUBLE_val>0){
	double eps=epsilon(contextptr);
	epsilon(p._DOUBLE_val,contextptr);
	gen res=epsilon2zero(args._VECTptr->front(),contextptr);
	epsilon(eps,contextptr);
	return res;
      }
    }
    return epsilon2zero(args,contextptr);
  }    
  static const char _epsilon2zero_s []="epsilon2zero";
  static define_unary_function_eval (__epsilon2zero,&_epsilon2zero,_epsilon2zero_s);
  define_unary_function_ptr5( at_epsilon2zero ,alias_at_epsilon2zero,&__epsilon2zero,0,true);

  gen _suppress(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return symbolic(at_suppress,args);
    vecteur & v=*args._VECTptr;
    if (v.size()==3 && v[1].type==_INT_ && v[2].type==_INT_){
      int i1=v[1].val-array_start(contextptr); //(xcas_mode(contextptr)!=0 || abs_calc_mode(contextptr)==38);
      int i2=v[2].val-array_start(contextptr); //(xcas_mode(contextptr)!=0 || abs_calc_mode(contextptr)==38);
      if (i1 >i2 || i1<0 || i2 < 0)
	return gendimerr(contextptr);
      if (v[0].type==_VECT){
	vecteur w=*v[0]._VECTptr;
	if (i1>=int(w.size()) || i2>=int(w.size()))
	  return gendimerr(contextptr);
	return gen(mergevecteur(vecteur(w.begin(),w.begin()+i1),vecteur(w.begin()+i2+1,w.end())),v[0].subtype);
      }
      if (v[0].type==_STRNG){
	string s=*v[0]._STRNGptr;
	if (i1>=int(s.size()) || i2>=int(s.size()))
	  return gendimerr(contextptr);
	return string2gen(s.substr(0,i1)+s.substr(i2+1,s.size()-i2-1),false);
      }
      return gensizeerr(contextptr);
    }
    if (v.size()!=2)
      return gentypeerr(contextptr);
    gen l=v.front(),i=v.back();
    int ii=0;
    if (i.type==_VECT){
      i=sortad(*i._VECTptr,false,contextptr);
      if (i.type==_VECT){
	const_iterateur it=i._VECTptr->begin(),itend=i._VECTptr->end();
	for (;it!=itend;++it){
	  l=_suppress(makesequence(l,*it),contextptr);
	}
	return l;
      }
    }
    if (i.type==_INT_ )
      ii=i.val-array_start(contextptr); //(xcas_mode(contextptr)!=0 || abs_calc_mode(contextptr)==38);
    if (l.type==_STRNG){
      string res;
      string & s=*l._STRNGptr;
      int n=int(s.size());
      if (i.type==_INT_ && ii>=0 && ii<n)
	res=s.substr(0,ii)+s.substr(ii+1,n-ii-1);
      if (i.type==_STRNG){
	string & remove=*i._STRNGptr;
	int removen=int(remove.size());
	for (int j=0;j<n;++j){
	  int k=int(remove.find(s[j]));
	  if (k<0 || k>=removen)
	    res += s[j];
	}
      }
      return string2gen(res,false);
    }
    if ( (l.type!=_VECT) || (i.type!=_INT_) )
      return gensizeerr(contextptr);
    const_iterateur it=l._VECTptr->begin(),itend=l._VECTptr->end();
    vecteur res;
    res.reserve(itend-it);
    for (int j=0;it!=itend;++it,++j){
      if (j!=ii)
	res.push_back(*it);
    }
    return gen(res,l.subtype);
  }    
  static const char _suppress_s []="suppress";
  static define_unary_function_eval (__suppress,&_suppress,_suppress_s);
  define_unary_function_ptr5( at_suppress ,alias_at_suppress,&__suppress,0,true);

  gen _clear(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type==_VECT && args._VECTptr->empty()){
      Bdisp_AllClr_VRAM();
      return 1;
    }
    gen g=eval(args,1,contextptr);
    if (g.type==_STRNG) 
      g=string2gen("",false);
    else {
      if (g.type!=_VECT)
	return gensizeerr(contextptr);
      g=gen(vecteur(0),args.subtype);
    }
    if (args.type==_STRNG || args.type==_VECT)
      return g;
    return sto(g,args,contextptr);
  }
  static const char _clear_s []="clear";
  static define_unary_function_eval (__clear,&_clear,_clear_s);
  define_unary_function_ptr5( at_clear ,alias_at_clear,&__clear,_QUOTE_ARGUMENTS,true);

  gen _insert(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return gensizeerr(contextptr);
    vecteur & v=*args._VECTptr;
    if (v.size()!=3)
      return gensizeerr(contextptr);
    gen i=v[1];
    if (!is_integral(i) || i.type!=_INT_) 
      return gensizeerr(contextptr);
    int ii=i.val-array_start(contextptr); //(xcas_mode(contextptr)!=0 || abs_calc_mode(contextptr)==38);
    if (v[0].type==_VECT){
      vecteur w=*v[0]._VECTptr;
      if (ii<0 || ii>int(w.size()))
	return gendimerr(contextptr);
      w.insert(w.begin()+ii,v[2]);
      return gen(w,v[0].subtype);
    }
    if (v[0].type==_STRNG){
      string s=*v[0]._STRNGptr;
      if (ii<0 || ii>int(s.size()))
	return gendimerr(contextptr);
      string add=(v[2].type==_STRNG)?*v[2]._STRNGptr:v[2].print(contextptr);
      s=s.substr(0,ii)+add+s.substr(ii,s.size()-ii);
      return string2gen(s,false);
    }
    return gensizeerr(contextptr);
  }    
  static const char _insert_s []="insert";
  static define_unary_function_eval (__insert,&_insert,_insert_s);
  define_unary_function_ptr5( at_insert ,alias_at_insert,&__insert,0,true);

#ifndef XLIGHT
  gen _pop(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type==_VECT && args.subtype==_SEQ__VECT && args._VECTptr->size()==2 ){
      if (args._VECTptr->front().type==_MAP){
	const gen & m=args._VECTptr->front();
	const gen & indice=args._VECTptr->back();
	gen_map::iterator it=m._MAPptr->find(indice),itend=m._MAPptr->end();
	if (it==itend)
	  return gensizeerr(gettext("Bad index")+indice.print(contextptr));
	m._MAPptr->erase(it);
	return 1;
      }
      if (args._VECTptr->back().type==_INT_){
	int pos=args._VECTptr->back().val;
	gen g=args._VECTptr->front();
	if (pos>=0 && g.type==_VECT && g._VECTptr->size()>pos){
	  gen res=(*g._VECTptr)[pos];
	  g._VECTptr->erase(g._VECTptr->begin()+pos);
	  return res;
	}
      }
    }
    if (args.type!=_VECT || args._VECTptr->empty()) 
      return gensizeerr(contextptr);
    gen res=args._VECTptr->back();
    args._VECTptr->pop_back();
    return res;
  }
  static const char _pop_s []="pop";
  static define_unary_function_eval (__pop,&_pop,_pop_s);
  define_unary_function_ptr5( at_pop ,alias_at_pop,&__pop,0,true);
#endif

  static int valuation(const polynome & p){
    if (p.coord.empty())
      return -1;
    return p.coord.back().index.front();
  }
  gen _valuation(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    gen p,x;
    if (args.type!=_VECT){
      x=vx_var();
      p=args;
    }
    else {
      vecteur & v=*args._VECTptr;
      int s=int(v.size());
      if (!s)
	return minus_inf;
      if ( (args.subtype==_POLY1__VECT) || (s!=2) || (v[1].type!=_IDNT) ){
	int j=s;
	for (;j;--j){
	  if (!is_zero(v[j-1]))
	    break;
	}
	return s-j;
      }
      x=v.back();
      p=v.front();
    }
    vecteur lv(1,x);
    lvar(p,lv);
    gen aa=e2r(p,lv,contextptr),aan,aad;
    if (is_zero(aa))
      return minus_inf;
    fxnd(aa,aan,aad);
    if ( (aad.type==_POLY) && (aad._POLYptr->lexsorted_degree() ) )
      return gensizeerr(contextptr);
    if (aan.type!=_POLY)
      return zero;
    int res=valuation(*aan._POLYptr);
    if (res==-1)
      return minus_inf;
    else
      return res;
  }    
  static const char _valuation_s []="valuation";
  static define_unary_function_eval (__valuation,&_valuation,_valuation_s);
  define_unary_function_ptr5( at_valuation ,alias_at_valuation,&__valuation,0,true);

  static const char _ldegree_s []="ldegree";
  static define_unary_function_eval (__ldegree,&_valuation,_ldegree_s);
  define_unary_function_ptr5( at_ldegree ,alias_at_ldegree,&__ldegree,0,true);

  int sum_degree(const index_m & v1,int vars){
    int i=0;
    for (index_t::const_iterator it=v1.begin();it!=v1.end() && it!=v1.begin()+vars;++it)
      i=i+(*it);
    return(i);
  }

  int total_degree(const polynome & p,int vars) {
    std::vector< monomial<gen> >::const_iterator it=p.coord.begin();
    std::vector< monomial<gen> >::const_iterator it_end=p.coord.end();
    int res=0;
    for (;it!=it_end;++it){
      int temp=sum_degree(it->index,vars);
      if (res<temp)
	res=temp;
    }
    return res;
  }


  gen _degree_(const gen & args,bool total,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    gen p,x;
    if (args.type!=_VECT){
      p=args;
      if (calc_mode(contextptr)==1)
	x=ggb_var(p);
      else
	x=vx_var();
    }
    else {
      vecteur & v=*args._VECTptr;
      int s=int(v.size());
      if ( (args.subtype==_POLY1__VECT) || (s!=2) || (v[1].type!=_IDNT && v[1].type!=_VECT) )
	return s-1;
      x=v.back();
      p=v.front();
    }
    if (p.type==_POLY){
      if (x.type==_INT_ && x.val>=0 && x.val<p._POLYptr->dim)
	return p._POLYptr->degree(x.val);
      else {
	vecteur res(p._POLYptr->dim);
	index_t idx(p._POLYptr->degree());
	for (int i=0;i<p._POLYptr->dim;++i)
	  res[i]=idx[i];
	return res;
      }
    }
    vecteur lv(1,x);
    if (x.type==_VECT)
      lv=*x._VECTptr;
    lvar(p,lv);
    gen aa=e2r(p,lv,contextptr),aan,aad;
    if (is_zero(aa))
      return zero;
    fxnd(aa,aan,aad);
    if (x.type==_VECT){
      if (total){
	int deg=0;
	if (aad.type==_POLY)
	  deg -= total_degree(*aad._POLYptr,int(x._VECTptr->size()));
	if (aan.type==_POLY)
	  deg += total_degree(*aan._POLYptr,int(x._VECTptr->size()));
	return deg;
      }
      int s=int(x._VECTptr->size());
      vecteur res(s);
      for (int i=0;i<s;++i){
	int deg=0;
	if (aad.type==_POLY)
	  deg -= aad._POLYptr->degree(i);;
	if (aan.type!=_POLY)
	  res[i]=deg;
	else
	  res[i]=deg+aan._POLYptr->degree(i);
      }
      return res;
    }
    int deg=0;
    if ( (aad.type==_POLY) && (aad._POLYptr->lexsorted_degree() ) )
      deg -= aad._POLYptr->lexsorted_degree();;
    if (aan.type!=_POLY)
      return deg;
    return deg+aan._POLYptr->lexsorted_degree();
  }    
  gen _degree(const gen & args,GIAC_CONTEXT){
    return _degree_(args,false,contextptr);
  }
  static const char _degree_s []="degree";
  static define_unary_function_eval (__degree,&_degree,_degree_s);
  define_unary_function_ptr5( at_degree ,alias_at_degree,&__degree,0,true);

  gen _total_degree(const gen & args,GIAC_CONTEXT){
    return _degree_(args,true,contextptr);
  }
  static const char _total_degree_s []="total_degree";
  static define_unary_function_eval (__total_degree,&_total_degree,_total_degree_s);
  define_unary_function_ptr5( at_total_degree ,alias_at_total_degree,&__total_degree,0,true);

  gen _lcoeff(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    gen x,p,order;
    int s=2;
    if (args.type!=_VECT){
      x=vx_var();
      p=args;
    }
    else {
      vecteur & v=*args._VECTptr;
      s=int(v.size());
      if (!s)
	return args;
      if ( (args.subtype!=_SEQ__VECT) || (s<2) )
	return v.front();
      x=v[1];
      p=v[0];
      if (s>2)
	order=v[2];
    }
    gen g=_e2r(makesequence(p,x),contextptr),n,d;
    fxnd(g,n,d);
    if (n.type!=_VECT){
      if (n.type==_POLY){
	polynome nlcoeff(*n._POLYptr);
	if (!nlcoeff.coord.empty()){
	  if (order.type==_INT_)
	    change_monomial_order(nlcoeff,order);
	  nlcoeff.coord.erase(nlcoeff.coord.begin()+1,nlcoeff.coord.end());
	}
	n=nlcoeff;
      }
      return _r2e(makesequence(n/d,x),contextptr);
    }
    return n._VECTptr->front()/d;
  }
  static const char _lcoeff_s []="lcoeff";
  static define_unary_function_eval (__lcoeff,&_lcoeff,_lcoeff_s);
  define_unary_function_ptr5( at_lcoeff ,alias_at_lcoeff,&__lcoeff,0,true);

  static gen tcoeff(const vecteur & v){
    int s=int(v.size());
    gen g;
    for (;s;--s){
      g=v[s-1];
      if (!is_zero(g))
	return g;
    }
    return zero;
  }
  gen _tcoeff(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    gen x,p;
    if (args.type!=_VECT){
      x=vx_var();
      p=args;
    }
    else {
      vecteur& v=*args._VECTptr;
      int s=int(v.size());
      if ( (args.subtype!=_SEQ__VECT) || (s!=2) || (v[1].type!=_IDNT) )
	return tcoeff(v);
      x=v[1];
      p=v[0];
    }
    gen g=_e2r(makesequence(p,x),contextptr),n,d;
    fxnd(g,n,d);
    if (n.type!=_VECT)
      return zero;
    return tcoeff(*n._VECTptr)/d;
  }
  static const char _tcoeff_s []="tcoeff";
  static define_unary_function_eval (__tcoeff,&_tcoeff,_tcoeff_s);
  define_unary_function_ptr5( at_tcoeff ,alias_at_tcoeff,&__tcoeff,0,true);

  static gen sqrfree(const gen & g,const vecteur & l,GIAC_CONTEXT){
    if (g.type!=_POLY)
      return r2sym(g,l,contextptr);
    factorization f(sqff(*g._POLYptr));
    factorization::const_iterator it=f.begin(),itend=f.end();
    gen res(plus_one);
    for (;it!=itend;++it)
      res=res*pow(r2e(it->fact,l,contextptr),it->mult);
    return res;
  }
  static vecteur sqrfree(const gen & g,const vecteur & l,int mult,GIAC_CONTEXT){
    vecteur res;
    if (g.type!=_POLY){
      if (is_one(g))
	return res;
      return vecteur(1,makevecteur(r2sym(g,l,contextptr),mult));
    }
    factorization f(sqff(*g._POLYptr));
    factorization::const_iterator it=f.begin(),itend=f.end();
    for (;it!=itend;++it){
      const polynome & p=it->fact;
      gen pg=r2e(p,l,contextptr);
      if (!is_one(pg))
	res.push_back(makevecteur(pg,mult*it->mult));
    }
    return res;
  }
  gen _sqrfree(const gen & args_,GIAC_CONTEXT){
    gen args(args_);
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    bool factors=false;
    if (args.type==_VECT){
      vecteur argv=*args._VECTptr;
      if (!argv.empty() && argv.back()==at_factors){
	factors=true;
	argv.pop_back();
	if (argv.size()==1)
	  args=argv.front();
	else
	  args=gen(argv,args.subtype);
      }
    }
    if (args.type==_VECT) // fixme take care of factors
      return apply(args,_sqrfree,contextptr);
    if (args.type!=_SYMB)
      return factors?makevecteur(args,1):args;
    gen a,b;
    if (is_algebraic_program(args,a,b)) // fixme take care of factors
      return symb_prog3(a,0,_sqrfree(b,contextptr));
    vecteur l(alg_lvar(args));
    gen g=e2r(args,l,contextptr);
    if (g.type==_FRAC){
      fraction f=*g._FRACptr;
      if (factors)
	return mergevecteur(sqrfree(f.num,l,1,contextptr),sqrfree(f.den,l,-1,contextptr));
      return sqrfree(f.num,l,contextptr)/sqrfree(f.den,l,contextptr);
    }
    else {
      if (factors)
	return sqrfree(g,l,1,contextptr);
      return sqrfree(g,l,contextptr);
    }
  }
  static const char _sqrfree_s []="sqrfree";
  static define_unary_function_eval (__sqrfree,&_sqrfree,_sqrfree_s);
  define_unary_function_ptr5( at_sqrfree ,alias_at_sqrfree,&__sqrfree,0,true);

  gen _truncate(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    gen e(args);
    int n,s=1;
    vecteur w(1,vx_var());
    gen gn(5);
    if (args.type==_VECT){
      vecteur & v=*args._VECTptr;
      s=int(v.size());
      if (s==0)
	return gensizeerr(contextptr);
      e=v[0];
      if (s==3){
	w=gen2vecteur(v[1]);
	gn=v[2];
      }
      else {
	if (s==2)
	  gn=v[1];
      }
    }
    if (gn.type!=_INT_)
      return gensizeerr(contextptr);
    n=gn.val;
    int nvar=int(w.size()); // number of var w.r.t. which we truncate
    vecteur l(lop(e,at_order_size));
    vecteur lp(l.size(),zero);
    e=subst(e,l,lp,false,contextptr);
    // FIXME if l not empty, adjust order of truncation using arg of order_size
    lvar(e,w);
    e=e2r(e,w,contextptr);
    gen num,den;
    fxnd(e,num,den);
    if ( (den.type==_POLY) && (den._POLYptr->lexsorted_degree() ) )
      return gensizeerr(contextptr);
    if (num.type==_POLY){
      vector< monomial<gen> >::const_iterator it=num._POLYptr->coord.begin(),itend=num._POLYptr->coord.end();
      vector< monomial<gen> > res;
      for (;it!=itend;++it){
	index_t::const_iterator i=it->index.begin();
	int deg=0;
	for (int j=0;j<nvar;++j,++i)
	  deg=deg+(*i);
	if (deg<=n)
	  res.push_back(*it);
      }
      num._POLYptr->coord=res;
    }
    return r2e(rdiv(num,den,contextptr),w,contextptr);
  }    
  static const char _truncate_s []="truncate";
  static define_unary_function_eval (__truncate,&_truncate,_truncate_s);
  define_unary_function_ptr5( at_truncate ,alias_at_truncate,&__truncate,0,true);

  gen _canonical_form(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    gen p,x,a,b,c;
    if (is_algebraic_program(args,a,b))
      return symb_prog3(a,0,_canonical_form(makesequence(b,a[0]),contextptr));
    if (args.type!=_VECT){
      p=args;
      x=ggb_var(p);
    }
    else {
      vecteur & v=*args._VECTptr;
      if (v.size()!=2)
	return gentypeerr(contextptr);
      p=v.front();
      x=v.back();
    }
    if (x.type!=_IDNT)
      return gentypeerr(contextptr);
    if (!is_quadratic_wrt(p,x,a,b,c,contextptr))
      return gensizeerr(contextptr);
    if (is_zero(a))
      return b*x+c;
    // a*x^2+b*x+c -> a*(x+b/(2*a))^2+(b^2-4*a*c)/(4*a)
    return a*pow(x+b/(2*a),2)+(4*a*c-pow(b,2))/(4*a);
  }    
  static const char _canonical_form_s []="canonical_form";
  static define_unary_function_eval (__canonical_form,&_canonical_form,_canonical_form_s);
  define_unary_function_ptr5( at_canonical_form ,alias_at_canonical_form,&__canonical_form,0,true);

  gen _taux_accroissement(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    gen p,x,a,b,c;
    if (args.type!=_VECT || args._VECTptr->size()<3)
      return gensizeerr(contextptr);
    vecteur v = *args._VECTptr;
    if (is_algebraic_program(v.front(),a,b)){
      return _taux_accroissement(makesequence(b,a[0],v[1],v[2]),contextptr);
      // return symbolic(at_program,makevecteur(v[1],0,_taux_accroissement(gen(makevecteur(b,a[0],v[1],v[2]),_SEQ__VECT),contextptr)));
    }
    if (v.size()<4)
      v.insert(v.begin()+1,vx_var());
    if (v[1].type!=_IDNT)
      return gentypeerr(contextptr);
    return (subst(v.front(),v[1],v[3],false,contextptr)-subst(v.front(),v[1],v[2],false,contextptr))/(v[3]-v[2]);
  }    
  static const char _taux_accroissement_s []="taux_accroissement";
  static define_unary_function_eval (__taux_accroissement,&_taux_accroissement,_taux_accroissement_s);
  define_unary_function_ptr5( at_taux_accroissement ,alias_at_taux_accroissement,&__taux_accroissement,0,true);

  gen _fcoeff(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    gen x;
    vecteur p;
    if (args.type!=_VECT)
      return symbolic(at_fcoeff,args);
    vecteur & v=*args._VECTptr;
    if ( (v.size()!=2) || (v.front().type!=_VECT) ){
      p=v;
      x=vx_var();
    }
    else {
      p=*v.front()._VECTptr;
      x=v.back();
    }
    if (x.type!=_IDNT)
      return gentypeerr(contextptr);    
    const_iterateur it=p.begin(),itend=p.end();
    if ( (itend-it)%2 )
      return gensizeerr(contextptr);
    gen res(plus_one);
    for (;it!=itend;it+=2){
      res=res*pow(x-*it,*(it+1),contextptr);
    }
    return res;
  }    
  static const char _fcoeff_s []="fcoeff";
  static define_unary_function_eval (__fcoeff,&_fcoeff,_fcoeff_s);
  define_unary_function_ptr5( at_fcoeff ,alias_at_fcoeff,&__fcoeff,0,true);

  static void addfactors(const gen & p,const gen & x,int mult,vecteur & res,GIAC_CONTEXT){
    vecteur v=sqff_factors(p,contextptr);
    const_iterateur it=v.begin(),itend=v.end();
    for (;it!=itend;){
      vecteur w=solve(*it,x,1,contextptr);
      ++it;
      int n=it->val;
      ++it;
      const_iterateur jt=w.begin(),jtend=w.end();
      for (;jt!=jtend;++jt){
	res.push_back(*jt);
	res.push_back(n*mult);
      }
    }
  }    

  gen _froot(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    gen p,x;
    if (args.type!=_VECT){
      x=vx_var();
      p=args;
    }
    else {
      vecteur & v=*args._VECTptr;
      if (v.size()!=2)
	return gensizeerr(contextptr);
      x=v.back();
      if (x.type!=_IDNT)
	return gensizeerr(gettext("2nd arg"));
      p=v.front();
    }
    vecteur lv(lvar(p));
    gen aa=e2r(p,lv,contextptr),aan,aad;
    fxnd(aa,aan,aad);
    vecteur res;
    addfactors(r2e(aan,lv,contextptr),x,1,res,contextptr);
    addfactors(r2e(aad,lv,contextptr),x,-1,res,contextptr);
    return res;
  }

  static const char _froot_s []="froot";
  static define_unary_function_eval (__froot,&_froot,_froot_s);
  define_unary_function_ptr5( at_froot ,alias_at_froot,&__froot,0,true);

  gen _roots(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    gen r=_froot(g,contextptr);
    if (r.type!=_VECT || (r._VECTptr->size() % 2) )
      return gensizeerr(contextptr);
    vecteur & v = *r._VECTptr;
    vecteur res;
    int s=int(v.size()/2);
    for (int i=0;i<s;++i){
      if (v[2*i+1].val>0)
	res.push_back(makevecteur(v[2*i],v[2*i+1]));
    }
    return res;
  }
  static const char _roots_s []="roots";
  static define_unary_function_eval (__roots,&_roots,_roots_s);
  define_unary_function_ptr5( at_roots ,alias_at_roots,&__roots,0,true);

  gen _divpc(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    gen p,q,x;
    if (args.type!=_VECT)
      return symbolic(at_divpc,args);
    vecteur & v=*args._VECTptr;
    int s=int(v.size());
    if (s<3)
      return gensizeerr(contextptr);
    p=v.front();
    q=v[1];
    if (v[2].type!=_INT_)
      return gensizeerr(contextptr);
    if (s==3)
      x=vx_var();
    else 
      x=v.back();
    vecteur lv(1,x);
    lvar(p,lv);
    lvar(q,lv);
    gen aa=e2r(p,lv,contextptr),aan,aad;
    fxnd(aa,aan,aad);
    gen ba=e2r(q,lv,contextptr),ban,bad;
    fxnd(ba,ban,bad);
    if ( ( aad.type==_POLY && aad._POLYptr->lexsorted_degree())
	 || (bad.type==_POLY && bad._POLYptr->lexsorted_degree())
	 )
      return gensizeerr(contextptr);
    if (ban.type!=_POLY)
      return r2e(rdiv(aan*bad,ban*aad,contextptr),lv,contextptr);
    vecteur a;
    if (aan.type==_POLY)
      a=polynome2poly1(*aan._POLYptr,1);
    else
      a=vecteur(1,aan);
    vecteur b=polynome2poly1(*ban._POLYptr,1);
    if (is_zero(b.back()))
      divisionby0err(q);
    vreverse(a.begin(),a.end());
    vreverse(b.begin(),b.end());
    int n=int(b.size()-a.size())+v[2].val;
    for (int i=0;i<n;++i)
      a.push_back(zero);
    vecteur quo,rem;
    environment * env=new environment;
    DivRem(a,b,env,quo,rem);
    delete env;
    vreverse(quo.begin(),quo.end());
    gen res(vecteur2polynome(quo,int(lv.size())));
    res=rdiv(res*bad,aad,contextptr);
    return r2e(res,lv,contextptr);
  }

  static const char _divpc_s []="divpc";
  static define_unary_function_eval (__divpc,&_divpc,_divpc_s);
  define_unary_function_ptr5( at_divpc ,alias_at_divpc,&__divpc,0,true);

  gen _ptayl(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    gen p,q,x;
    if (args.type!=_VECT){
      p=_POLY1__VECT;
      p.subtype=_INT_MAPLECONVERSION;
      return _series(makesequence(args,p),contextptr);
    }
    vecteur v=*args._VECTptr;
    int s=int(v.size());
    if (s<2)
      return gensizeerr(contextptr);
    if (s>3 || v[1].is_equal() || (s==3 && v[2].type==_INT_)){
      p=_POLY1__VECT;
      p.subtype=_INT_MAPLECONVERSION;
      v.push_back(p);
      return _series(gen(v,_SEQ__VECT),contextptr);
    }
    p=v.front();
    q=v[1];
    if (p.type==_VECT)
      return taylor(*p._VECTptr,q,0);
    if (s==2)
      x=vx_var();
    else 
      x=v.back();
    if (is_integral(x)){
      p=_POLY1__VECT;
      p.subtype=_INT_MAPLECONVERSION;
      v.push_back(p);
      return _series(makesequence(gen(v,_SEQ__VECT)),contextptr);
    }
    if (!is_zero(derive(q,x,contextptr)))
      return gensizeerr(contextptr);
    vecteur lv(1,x);
    lvar(p,lv);
    lvar(q,lv);
    gen aa=e2r(p,lv,contextptr),aan,aad;
    fxnd(aa,aan,aad);
    if ( ( (aad.type==_POLY)&&(aad._POLYptr->lexsorted_degree()) )
	 )
      return gensizeerr(contextptr);
    if (aan.type!=_POLY)
      return p;
    gen ba=e2r(q,vecteur(lv.begin()+1,lv.end()),contextptr);
    vecteur a(polynome2poly1(*aan._POLYptr,1));
    vecteur res=taylor(a,ba,0);
    return r2e(vecteur2polynome(res,int(lv.size())),lv,contextptr)/r2e(aad,lv,contextptr);
  }

  static const char _ptayl_s []="ptayl";
  static define_unary_function_eval (__ptayl,&_ptayl,_ptayl_s);
  define_unary_function_ptr5( at_ptayl ,alias_at_ptayl,&__ptayl,0,true);
  
  vecteur gen2continued_fraction(const gen & g,int n,GIAC_CONTEXT){
    // Compute a vector of size n+1 with last element=remainder
    vecteur res,remain;
    gen tmp(g),f;
#ifndef HAVE_LIBMPFR
    if (!alg_lvar(tmp).empty())
      tmp=evalf_double(tmp,1,contextptr);
#endif
    int i=0,j;
    for (;i<n;++i){
      if ( (j=equalposcomp(remain,tmp)) ){
	// int s=remain.size();
	res.push_back(vecteur(res.begin()+j-1,res.end()));
	return res;
      }
      else
	remain.push_back(tmp);
      f=_floor(tmp,0);
      res.push_back(f);
      if (is_zero(tmp-f))
	return res;
      tmp=normal(inv(tmp-f,contextptr),contextptr);
    }
    res.push_back(tmp);
    return res;
  }
  gen _dfc(const gen & g_orig,GIAC_CONTEXT){
    if ( g_orig.type==_STRNG && g_orig.subtype==-1) return  g_orig;
    gen g=g_orig;
    if (g.type==_FRAC){
      gen tmp=_floor(g,contextptr);
      vecteur res(1,tmp);
      g -= tmp;
      for (;!is_zero(g);){
	g = inv(g,contextptr);
	tmp = _floor(g,contextptr);
	res.push_back(tmp);
	g -=tmp;
      }
      return res;
    }
    double eps=epsilon(contextptr);
    if (g.type==_VECT && g._VECTptr->size()==2){
      gen gf=evalf_double(g._VECTptr->back(),1,contextptr);
      if (is_integral(gf))
	return gen2continued_fraction(g._VECTptr->front(),gf.val,contextptr);
      if (gf.type==_DOUBLE_){
	eps=gf._DOUBLE_val;
	g=evalf_double(g._VECTptr->front(),1,contextptr);
      }
    }
    g=evalf_double(g,1,contextptr);
    if (g.type!=_DOUBLE_)
      return gensizeerr(contextptr);
    return vector_int_2_vecteur(float2continued_frac(g._DOUBLE_val,eps));
  }
  static const char _dfc_s []="dfc";
  static define_unary_function_eval (__dfc,&_dfc,_dfc_s);
  define_unary_function_ptr5( at_dfc ,alias_at_dfc,&__dfc,0,true);

  gen _dfc2f(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type!=_VECT || g._VECTptr->empty())
      return gensizeerr(contextptr);
    vecteur v =(*g._VECTptr);
    gen res(v.back());
    if (v.back().type==_VECT){
      // represent a quadratic x=[... x], find equation
      identificateur tmp(" x");
      gen eq(tmp);
      const_iterateur it=v.back()._VECTptr->end()-1,itend=v.back()._VECTptr->begin()-1;
      for (;it!=itend;--it)
	eq=inv(eq,contextptr)+(*it);
      vecteur w=solve(eq-tmp,tmp,0,contextptr);
      gen ws=_sort(w,0);
      if (ws.type!=_VECT || ws._VECTptr->empty())
	return gensizeerr(contextptr);
      res=ws._VECTptr->back();
    }
    for (;;){
      v.pop_back();
      if (v.empty())
	return res;
      res=inv(res,contextptr);
      res=res+v.back();
    }
    // return continued_frac2gen(vecteur_2_vector_int(*g._VECTptr),nan(),epsilon);
  }
  static const char _dfc2f_s []="dfc2f";
  static define_unary_function_eval (__dfc2f,&_dfc2f,_dfc2f_s);
  define_unary_function_ptr5( at_dfc2f ,alias_at_dfc2f,&__dfc2f,0,true);

  gen float2rational(double d_orig,double eps,GIAC_CONTEXT){
    double d=d_orig;
    if (d<0)
      return -float2rational(-d,eps,contextptr);
    if (d>RAND_MAX)
      return d;    // reconstruct
    vector<int> v(float2continued_frac(d,eps));
    return continued_frac2gen(v,d_orig,eps,contextptr);
  }
  gen _float2rational(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    switch (g.type){
    case _DOUBLE_:
      return float2rational(g._DOUBLE_val,epsilon(contextptr),contextptr);
      // case _REAL:      return float2rational(evalf_double(g,1,contextptr)._DOUBLE_val,epsilon(contextptr),contextptr);
    case _CPLX:
      return _float2rational(re(g,contextptr),contextptr)+cst_i*_float2rational(im(g,contextptr),contextptr);
    case _SYMB:
      return symbolic(g._SYMBptr->sommet,_float2rational(g._SYMBptr->feuille,contextptr));
    case _VECT:
      return apply(g,_float2rational,contextptr);
    default:
      return g;
    }
  }
  static const char _float2rational_s []="float2rational";
  static define_unary_function_eval (__float2rational,&_float2rational,_float2rational_s);
  define_unary_function_ptr5( at_float2rational ,alias_at_float2rational,&__float2rational,0,true);

  gen _fmod(const gen & g,GIAC_CONTEXT){
    if (g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type!=_VECT || g.subtype!=_SEQ__VECT || g._VECTptr->size()!=2) 
      return gensizeerr(contextptr);
    const gen & a=g._VECTptr->front(),b=g._VECTptr->back();
    if (a.type==_DOUBLE_ && b.type==_DOUBLE_)
      return a._DOUBLE_val-std::floor(a._DOUBLE_val/b._DOUBLE_val)*b._DOUBLE_val;
    return a-_floor(a/b,contextptr)*b;
  }
  static const char _fmod_s []="fmod";
  static define_unary_function_eval (__fmod,&_fmod,_fmod_s);
  define_unary_function_ptr5( at_fmod ,alias_at_fmod,&__fmod,0,true);

#ifndef XLIGHT
  gen _gramschmidt(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type!=_VECT)
      return symbolic(at_gramschmidt,g);
    vecteur & v(*g._VECTptr);
    if (ckmatrix(v))
      return gramschmidt(v,true,contextptr);
    if (v.size()==2){
      gen lvect=v[0];
      gen scalaire=v[1];
      if (scalaire.type==_INT_ && ckmatrix(lvect))
	return gramschmidt(*lvect._VECTptr,scalaire.val,contextptr);
      if (lvect.type!=_VECT)
	return gensizeerr(contextptr);
      vecteur lv=*lvect._VECTptr;
      int s=int(lv.size());
      if (!s)
	return lv;
      vecteur sc(1,scalaire(makesequence(lv[0],lv[0]),contextptr));
      for (int i=1;i<s;++i){
	gen cl;
	for (int j=0;j<i;++j)
	  cl=cl+rdiv(scalaire(makesequence(lv[i],lv[j]),contextptr),sc[j],contextptr)*lv[j];
	lv[i]=lv[i]-cl;
	sc.push_back(scalaire(makesequence(lv[i],lv[i]),contextptr));
      }
      for (int i=0;i<s;++i)
	lv[i]=rdiv(lv[i],sqrt(sc[i],contextptr),contextptr);
      return lv;
    }
    return gensizeerr(contextptr);
  }
  static const char _gramschmidt_s []="gramschmidt";
  static define_unary_function_eval (__gramschmidt,&_gramschmidt,_gramschmidt_s);
  define_unary_function_ptr5( at_gramschmidt ,alias_at_gramschmidt,&__gramschmidt,0,true);
#endif

  void aplatir(const matrice & m,vecteur & v,bool full){
    int s=int(m.size());
    if (!full){
      v.clear();
      v.reserve(2*s);
    }
    const_iterateur it=m.begin(),itend=m.end(),jt,jtend;
    for (;it!=itend;++it){
      if (it->type!=_VECT || it->subtype==_GGB__VECT)
	v.push_back(*it);
      else {
	if (full){
	  aplatir(*it->_VECTptr,v,full);
	  continue;
	}
	jt=it->_VECTptr->begin(),jtend=it->_VECTptr->end();
	for (;jt!=jtend;++jt)
	  v.push_back(*jt);
      }
    }
  }

  static void change_scale2(vecteur & v,const gen & g){
    gen l(g);
    for (unsigned i=1;i<v.size();++i){
      v[i]=v[i]/l;
      l=g*l;
    }
  }

  /*
  gen exptorootof(const gen & g,GIAC_CONTEXT){
    gen h=ratnormal(g/cst_two_pi/cst_i,contextptr);
    if (h.type!=_FRAC || h._FRACptr->num.type!=_INT_ || h._FRACptr->den.type!=_INT_)
      return symbolic(at_exp,g);
    int n=h._FRACptr->num.val,d=h._FRACptr->den.val;
    n=n%d;
    if (d<0){ d=-d; n=-n; }
    vecteur v=cyclotomic(d);
    vecteur w(absint(n)+1);
    w[0]=1;
    w=w%v;
    h=symbolic(at_rootof,makesequence(w,v));
    if (n>0)
      return h;
    return inv(h,contextptr);
  }
  const gen_op_context exp2rootof_tab[]={exptorootof,0};
  gen exp2rootof(const gen & g,GIAC_CONTEXT){
    return subst(g,exp_tab,exp2rootof_tab,false,contextptr);
  }
  gen _exp2rootof(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    gen var,res;
    if (is_algebraic_program(args,var,res))
      return symbolic(at_program,makesequence(var,0,_exp2rootof(res,contextptr)));
    if (is_equal(args))
      return apply_to_equal(args,_exp2rootof,contextptr);
    return exp2rootof(args,contextptr);
  }
  static const char _exp2rootof_s []="exp2rootof";
  static define_unary_function_eval (__exp2rootof,&_exp2rootof,_exp2rootof_s);
  define_unary_function_ptr5( at_exp2rootof ,alias_at_exp2rootof,&__exp2rootof,0,true);
  */

#ifndef XLIGHT
  static gen pmin(const matrice & m,GIAC_CONTEXT){
    int s=int(m.size());
    matrice mpow(midn(s));
    matrice res;
    vecteur v;
    for (int i=0;i<=s;++i){
      if (is_zero(mpow)){
	vecteur w(i+1);
	w[0]=1;
	return w;
      }
      aplatir(mpow,v);
      v.push_back(pow(vx_var(),i));
      res.push_back(v);
      mpow=mmult(mpow,m);
    }
    matrice r;
    gen det;
    mrref(res,r,v,det,0,s+1,0,s*s,
	  /* fullreduction */0,1,true,1,0,
	  contextptr);
    // find 1st line with zeros (except in the last col)
    const_iterateur it=r.begin(),itend=r.end();
    for (;it!=itend;++it){
      if (is_zero(vecteur(it->_VECTptr->begin(),it->_VECTptr->end()-1)))
	break;
    }
    if (it==itend)
      return gensizeerr(contextptr);
    gen t= _e2r(makesequence(it->_VECTptr->back(),vx_var()),contextptr);
    if (t.type==_VECT)
      return gen(t/lgcd(*t._VECTptr),_POLY1__VECT);
    else
      return t;
  }
  gen _pmin(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (is_squarematrix(g)){
      matrice &m =*g._VECTptr;
      vecteur w;
      gen p=m[0][0];
      if (p.type==_USER){
	std_matrix<gen> M;
	matrice2std_matrix_gen(m,M);
	mod_pcar(M,w,true);
	return gen(w,_POLY1__VECT);
      }
      if (p.type==_MOD && (p._MODptr+1)->type==_INT_){
	gen mg=unmod(m);
	if (mg.type==_VECT){
	  matrice M=*mg._VECTptr;
	  vector< vector<int> > N;
	  int modulo=(p._MODptr+1)->val;
	  bool krylov=true;
	  vector<int> res;
	  if (mod_pcar(M,N,modulo,krylov,res,contextptr,true)){
	    vector_int2vecteur(res,w);
	    return makemod(gen(w,_POLY1__VECT),modulo);
	    // environment env; w=modularize(w,modulo,&env);
	    // return gen(w,_POLY1__VECT);
	  }
	}
      }
#ifndef XLIGHT
      if (is_integer_matrice(m)){
	w=mpcar_int(m,true,contextptr,true);
	return gen(w,_POLY1__VECT);
      }
#endif
      if (poly_pcar_interp(m,w,true,contextptr))
	return gen(w,_POLY1__VECT);
      if (proba_epsilon(contextptr) && probabilistic_pmin(m,w,true,contextptr))
	return gen(w,_POLY1__VECT);
      return pmin(m,contextptr);
    }
    if (is_integer(g) || g.type==_MOD)
      return gen(makevecteur(1,-g),_POLY1__VECT);
    // if (g.type==_FRAC) return gen(makevecteur(g._FRACptr->den,-g._FRACptr->num),_POLY1__VECT);
    if (is_cinteger(g) && g.type==_CPLX){
      gen a=*g._CPLXptr,b=*(g._CPLXptr+1);
      // z=(a+i*b), (z-a)^2=-b^2
      return gen(makevecteur(1,-2*a,a*a+b*b),_POLY1__VECT);
    }
    if (g.type==_EXT)
      return minimal_polynomial(g,true,contextptr);
    if (g.type!=_VECT){
      gen g_(g);
      //if (!lop(g_,at_exp).empty())
      g_=cossinexp2rootof(g_,contextptr);
      vecteur v=alg_lvar(g_);
      if (v.size()==1 && v.front().type==_VECT && v.front()._VECTptr->empty()){
	gen tmp=e2r(g_,v,contextptr);
	gen d=1;
	if (tmp.type==_FRAC){
	  d=tmp._FRACptr->den;
	  tmp=tmp._FRACptr->num;
	  if (d.type==_CPLX){
	    tmp=tmp*conj(d,contextptr);
	    d=d*conj(d,contextptr);
	  }
	}
	if (tmp.type==_POLY && tmp._POLYptr->dim==0)
	  tmp=tmp._POLYptr->coord.front().value;
	if (tmp.type==_EXT){
	  if (has_i(*tmp._EXTptr)){
	    gen r,i;
	    reim(tmp,r,i,contextptr);
	    tmp=r+algebraic_EXTension(makevecteur(1,0),makevecteur(1,0,1))*i;
	    while (tmp.type==_FRAC){
	      d=d*tmp._FRACptr->den;
	      tmp=tmp._FRACptr->num;
	    }
	  }
	  tmp=minimal_polynomial(tmp,true,contextptr);
	  if (tmp.type!=_VECT)
	    return gensizeerr(contextptr);
	  vecteur v=*tmp._VECTptr;
	  change_scale2(v,d);
	  return gen(v,_POLY1__VECT);
	}
      }
    }
    if (g.type!=_VECT || g._VECTptr->size()!=2)
      return symbolic(at_pmin,g);
    vecteur & v(*g._VECTptr);
    if (!is_squarematrix(v.front())){
      gen res=_pmin(v.front(),contextptr);
      if (res.type==_VECT)
	return symb_horner(*res._VECTptr,v.back());
      return gensizeerr(contextptr);
    }
    matrice &m=*v.front()._VECTptr;
    // probabilistic minimal polynomial
    vecteur w;
    if (proba_epsilon(contextptr) &&probabilistic_pmin(m,w,true,contextptr))
      return symb_horner(w,v.back());
    else
      return _r2e(makesequence(_pmin(m,contextptr),v.back()),contextptr);
  }
  static const char _pmin_s []="pmin";
  static define_unary_function_eval (__pmin,&_pmin,_pmin_s);
  define_unary_function_ptr5( at_pmin ,alias_at_pmin,&__pmin,0,true);


  // a faire: vpotential, signtab
  gen _potential(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if ( (g.type!=_VECT) || (g._VECTptr->size()!=2) )
      return symbolic(at_potential,g);
    vecteur v(plotpreprocess(g,contextptr));
    if (is_undef(v))
      return v;
    gen f=v[0];
    gen x=v[1];
    if ( (f.type!=_VECT) || (x.type!=_VECT) )
      return gensizeerr(contextptr);
    vecteur & fv=*f._VECTptr;
    vecteur & xv=*x._VECTptr;
    int s=int(fv.size());
    if (unsigned(s)!=xv.size())
      return gendimerr(contextptr);
    for (int i=0;i<s;++i){
      for (int j=i+1;j<s;++j){
	if (!is_zero(simplify(derive(fv[i],xv[j],contextptr)-derive(fv[j],xv[i],contextptr),contextptr)))
	  return gensizeerr(gettext("Not a potential"));
      }
    }
    gen res;
    for (int i=0;i<s;++i){
      res=res+integrate_gen(simplify(fv[i]-derive(res,xv[i],contextptr),contextptr),xv[i],contextptr);
    }
    return res;
  }
  static const char _potential_s []="potential";
  static define_unary_function_eval_quoted (__potential,&_potential,_potential_s);
  define_unary_function_ptr5( at_potential ,alias_at_potential,&__potential,_QUOTE_ARGUMENTS,true);

  gen _vpotential(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if ( (g.type!=_VECT) || (g._VECTptr->size()!=2) )
      return symbolic(at_vpotential,g);
    vecteur v(plotpreprocess(g,contextptr));
    if (is_undef(v))
      return v;
    gen f=v[0];
    gen x=v[1];
    if ( (f.type!=_VECT) || (x.type!=_VECT) )
      return gensizeerr(contextptr);
    vecteur & fv=*f._VECTptr;
    vecteur & xv=*x._VECTptr;
    unsigned int s=unsigned(fv.size());
    if ( (s!=3) || (s!=xv.size()) )
      return gendimerr(contextptr);
    if (!is_zero(simplify(_divergence(g,contextptr),contextptr)))
      return gensizeerr(gettext("Not a vector potential"));
    vecteur res(3);
    /* return A0=0, A1=int[B_2,x0], A2=-int[B_1,x0]+F(x1,x2)
     * where F=int[B0+d_2[int[B_2,x0]]+d_1[int[B_1,x0]],x1]
     * F does not depend on x0 since divergence[B]=0 */
    res[1]=integrate_gen(fv[2],xv[0],contextptr);
    res[2]=integrate_gen(fv[1],xv[0],contextptr);
    gen F=simplify(fv[0]+derive(res[1],xv[2],contextptr)+derive(res[2],xv[1],contextptr),contextptr);
    F=integrate_gen(F,xv[1],contextptr);
    res[2]=F-res[2];
    return res;
  }
  static const char _vpotential_s []="vpotential";
  static define_unary_function_eval_quoted (__vpotential,&_vpotential,_vpotential_s);
  define_unary_function_ptr5( at_vpotential ,alias_at_vpotential,&__vpotential,_QUOTE_ARGUMENTS,true);
#endif
  
  gen _poly2symb(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type==_VECT && g.subtype!=_SEQ__VECT)
      return _r2e(makesequence(g,vx_var()),contextptr);      
    return _r2e(g,contextptr);
  }
  static const char _poly2symb_s []="poly2symb";
  static define_unary_function_eval (__poly2symb,&_poly2symb,_poly2symb_s);
  define_unary_function_ptr5( at_poly2symb ,alias_at_poly2symb,&__poly2symb,0,true);

  gen _symb2poly(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    return _e2r(g,contextptr);
  }
  static const char _symb2poly_s []="symb2poly";
  static define_unary_function_eval (__symb2poly,&_symb2poly,_symb2poly_s);
  define_unary_function_ptr5( at_symb2poly ,alias_at_symb2poly,&__symb2poly,0,true);

  gen _exp2trig(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    return _sincos(g,contextptr);
  }
  static const char _exp2trig_s []="exp2trig";
  static define_unary_function_eval (__exp2trig,&_exp2trig,_exp2trig_s);
  define_unary_function_ptr5( at_exp2trig ,alias_at_exp2trig,&__exp2trig,0,true);

  gen _nrows(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (!ckmatrix(g))
      return gensizeerr(contextptr);
    return int(g._VECTptr->size());
  }
  static const char _nrows_s []="nrows";
  static define_unary_function_eval (__nrows,&_nrows,_nrows_s);
  define_unary_function_ptr5( at_nrows ,alias_at_nrows,&__nrows,0,true);

  gen _ncols(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (!ckmatrix(g))
      return gensizeerr(contextptr);
    if (g._VECTptr->empty())
      return zero;
    return int(g._VECTptr->front()._VECTptr->size());
  }
  static const char _ncols_s []="ncols";
  static define_unary_function_eval (__ncols,&_ncols,_ncols_s);
  define_unary_function_ptr5( at_ncols ,alias_at_ncols,&__ncols,0,true);

  gen _l2norm(const gen & g0,GIAC_CONTEXT){
    if ( g0.type==_STRNG && g0.subtype==-1) return  g0;
    gen g(g0);
    if (g.type==_VECT && g.subtype==_VECTOR__VECT && g._VECTptr->size()==2)
      g=g._VECTptr->back()-g._VECTptr->front();
    if (g.type!=_VECT)
      return abs(g,contextptr);
    vecteur v;
    if (g._VECTptr->size()==2 && g._VECTptr->front().type==_VECT && g._VECTptr->back()==at_vector){
      aplatir(*g._VECTptr->front()._VECTptr,v);
      return l2norm(v,contextptr);      
    }
    if (ckmatrix(g)){
      gen tmp=_SVL(g,contextptr);
      if (tmp.type==_VECT && tmp._VECTptr->size()==2 && tmp._VECTptr->back().type==_VECT) 
	tmp=tmp._VECTptr->back();
      return _max(tmp,contextptr);
    }
    v=*g._VECTptr;
    return l2norm(v,contextptr);
  }
  static const char _l2norm_s []="l2norm";
  static define_unary_function_eval (__l2norm,&_l2norm,_l2norm_s);
  define_unary_function_ptr5( at_l2norm ,alias_at_l2norm,&__l2norm,0,true);

  static const char _norm_s []="norm";
  static define_unary_function_eval (__norm,&_l2norm,_norm_s);
  define_unary_function_ptr5( at_norm ,alias_at_norm,&__norm,0,true);

  gen _normalize(const gen & a,GIAC_CONTEXT){
    if ( a.type==_STRNG && a.subtype==-1) return  a;
    return a/_l2norm(a,contextptr);
  }
  static const char _normalize_s []="normalize";
  static define_unary_function_eval (__normalize,&_normalize,_normalize_s);
  define_unary_function_ptr5( at_normalize ,alias_at_normalize,&__normalize,0,true);

  static const char _randmatrix_s []="randmatrix";
  static define_unary_function_eval (__randmatrix,&_ranm,_randmatrix_s);
  define_unary_function_ptr5( at_randmatrix ,alias_at_randmatrix,&__randmatrix,0,true);

  extern const unary_function_ptr * const  at_lgcd;
  gen _lgcd(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return symbolic(at_lgcd,args);
    return lgcd(*args._VECTptr);
  }
  static const char _lgcd_s []="lgcd";
  static define_unary_function_eval (__lgcd,&_lgcd,_lgcd_s);
  define_unary_function_ptr5( at_lgcd ,alias_at_lgcd,&__lgcd,0,true);

  // synonyms
  gen _float(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    gen g_=g;
    if (g.type==_STRNG)
      g_=gen(*g._STRNGptr,contextptr);
    return evalf(g_,1,contextptr);
  }
  static const char _float_s []="float";
  static define_unary_function_eval (__float,&_float,_float_s);
  define_unary_function_ptr5( at_float ,alias_at_float,&__float,0,true);

  gen _build_complex(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type==_VECT && g._VECTptr->size()==2)
      return gen(g._VECTptr->front(),g._VECTptr->back());
    if (g.type==_STRNG)
      return gen(*g._STRNGptr,contextptr);
    return g;
  }
  static const char _build_complex_s []="complex";
  static define_unary_function_eval (__build_complex,&_build_complex,_build_complex_s);
  define_unary_function_ptr5( at_complex ,alias_at_build_complex,&__build_complex,0,true);

  gen _hold(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    return g;
  }
  static const char _hold_s []="hold";
  static define_unary_function_eval_quoted (__hold,&_hold,_hold_s);
  define_unary_function_ptr5( at_hold ,alias_at_hold,&__hold,_QUOTE_ARGUMENTS,true);

  gen _eigenvals(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (!is_squarematrix(g))
      return gendimerr(contextptr);
    bool b=complex_mode(contextptr);
    complex_mode(true,contextptr);
    matrice m;
    vecteur d;
    if (!egv(*g._VECTptr,m,d,contextptr,false,false,true))
      *logptr(contextptr) << gettext("Low accuracy") << "\n";
    complex_mode(b,contextptr);
    return gen(d,_SEQ__VECT);
  }
  static const char _eigenvals_s []="eigenvals";
  static define_unary_function_eval (__eigenvals,&_eigenvals,_eigenvals_s);
  define_unary_function_ptr5( at_eigenvals ,alias_at_eigenvals,&__eigenvals,0,true);

  static const char _giackernel_s []="kernel";
  static define_unary_function_eval (__giackernel,&_ker,_giackernel_s);
  define_unary_function_ptr5( at_kernel ,alias_at_kernel,&__giackernel,0,true);

  gen _eigenvects(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    bool b=complex_mode(contextptr);
    complex_mode(true,contextptr);
    gen res=_egv(g,contextptr);
    complex_mode(b,contextptr);
    return res;
  }
  static const char _eigenvects_s []="eigenvects";
  static define_unary_function_eval (__eigenvects,&_eigenvects,_eigenvects_s);
  define_unary_function_ptr5( at_eigenvects ,alias_at_eigenvects,&__eigenvects,0,true);

  static const char _eigenvalues_s []="eigenvalues";
  static define_unary_function_eval (__eigenvalues,&_eigenvals,_eigenvalues_s);
  define_unary_function_ptr5( at_eigenvalues ,alias_at_eigenvalues,&__eigenvalues,0,true);

  static const char _charpoly_s []="charpoly";
  static define_unary_function_eval (__charpoly,&_pcar,_charpoly_s);
  define_unary_function_ptr5( at_charpoly ,alias_at_charpoly,&__charpoly,0,true);

  static const char _eigenvectors_s []="eigenvectors";
  static define_unary_function_eval (__eigenvectors,&_eigenvects,_eigenvectors_s);
  define_unary_function_ptr5( at_eigenvectors ,alias_at_eigenvectors,&__eigenvectors,0,true);

  static const char _rowdim_s []="rowdim";
  static define_unary_function_eval (__rowdim,&_nrows,_rowdim_s);
  define_unary_function_ptr5( at_rowdim ,alias_at_rowdim,&__rowdim,0,true);

  static const char _coldim_s []="coldim";
  static define_unary_function_eval (__coldim,&_ncols,_coldim_s);
  define_unary_function_ptr5( at_coldim ,alias_at_coldim,&__coldim,0,true);

  static const char _multiply_s []="multiply";
  static define_unary_function_eval (__multiply,&_prod,_multiply_s);
  define_unary_function_ptr5( at_multiply ,alias_at_multiply,&__multiply,0,true);


  gen _divisors(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    gen res=_idivis(g,contextptr);
    if (res.type==_VECT) res.subtype=_SET__VECT;
    return res;
  }
  static const char _divisors_s []="divisors";
  static define_unary_function_eval (__divisors,&_divisors,_divisors_s);
  define_unary_function_ptr5( at_divisors ,alias_at_divisors,&__divisors,0,true);

  gen _maxnorm(const gen & g0,GIAC_CONTEXT){
    if ( g0.type==_STRNG && g0.subtype==-1) return  g0;
    gen g(g0);
    if (g.type==_VECT && g.subtype==_VECTOR__VECT)
      g=vector2vecteur(*g._VECTptr);
    return linfnorm(g,contextptr);
  }
  static const char _maxnorm_s []="maxnorm";
  static define_unary_function_eval (__maxnorm,&_maxnorm,_maxnorm_s);
  define_unary_function_ptr5( at_maxnorm ,alias_at_maxnorm,&__maxnorm,0,true);

  gen l1norm(const vecteur & v,GIAC_CONTEXT){
    gen res;
    const_iterateur it=v.begin(),itend=v.end();
    for (;it!=itend;++it)
      res=res+linfnorm(*it,contextptr);
    return res;
  }

  gen _l1norm(const gen & g0,GIAC_CONTEXT){
    if ( g0.type==_STRNG && g0.subtype==-1) return  g0;
    gen g(g0);
    if (g.type==_VECT && g.subtype==_VECTOR__VECT)
      g=vector2vecteur(*g._VECTptr);
    if (g.type!=_VECT)
      return linfnorm(g,contextptr);
    if (g._VECTptr->size()==2 && g._VECTptr->front().type==_VECT && g._VECTptr->back()==at_vector){
      vecteur v;
      aplatir(*g._VECTptr->front()._VECTptr,v);
      return l1norm(v,contextptr);      
    }
    if (ckmatrix(g))
      return _rowNorm(mtran(*g._VECTptr),contextptr);
    return l1norm(*g._VECTptr,contextptr);
  }
  static const char _l1norm_s []="l1norm";
  static define_unary_function_eval (__l1norm,&_l1norm,_l1norm_s);
  define_unary_function_ptr5( at_l1norm ,alias_at_l1norm,&__l1norm,0,true);

  gen _linfnorm(const gen & g0,GIAC_CONTEXT){
    if ( g0.type==_STRNG && g0.subtype==-1) return  g0;
    gen g(g0);
    if (g.type==_VECT && g.subtype==_VECTOR__VECT)
      g=vector2vecteur(*g._VECTptr);
    if (g.type!=_VECT)
      return linfnorm(g,contextptr);
    if (g._VECTptr->size()==2 && g._VECTptr->front().type==_VECT && g._VECTptr->back()==at_vector){
      vecteur v;
      aplatir(*g._VECTptr->front()._VECTptr,v);
      return linfnorm(v,contextptr);      
    }
    if (ckmatrix(g))
      return _rowNorm(g,contextptr);
    return linfnorm(*g._VECTptr,contextptr);
  }
  static const char _linfnorm_s []="linfnorm";
  static define_unary_function_eval (__linfnorm,&_linfnorm,_linfnorm_s);
  define_unary_function_ptr5( at_linfnorm ,alias_at_linfnorm,&__linfnorm,0,true);

  gen _frobenius_norm(const gen & g0,GIAC_CONTEXT){
    if ( g0.type==_STRNG && g0.subtype==-1) return  g0;
    gen g(g0);
    if (g.type==_VECT && g.subtype==_VECTOR__VECT)
      g=vector2vecteur(*g._VECTptr);
    vecteur v;
    if (ckmatrix(g))
      aplatir(*g._VECTptr,v);
    else
      v=*g._VECTptr;
    return l2norm(v,contextptr);
  }
  static const char _frobenius_norm_s []="frobenius_norm";
  static define_unary_function_eval (__frobenius_norm,&_frobenius_norm,_frobenius_norm_s);
  define_unary_function_ptr5( at_frobenius_norm ,alias_at_frobenius_norm,&__frobenius_norm,0,true);

  gen _matrix_norm(const gen & g0,GIAC_CONTEXT){
    if ( g0.type==_STRNG && g0.subtype==-1) return  g0;
    if (g0.type!=_VECT || g0._VECTptr->empty())
      return gentypeerr(contextptr);
    if (g0._VECTptr->back()==0){
      gen g=g0._VECTptr->front();
      if (!ckmatrix(g))
	return _linfnorm(g,contextptr);
      vecteur & v =*g._VECTptr;
      gen res=0;
      for (unsigned i=0;i<v.size();++i){
	res=max(res,linfnorm(v[i],contextptr),contextptr);
      }
      return res;
    }
    if (g0._VECTptr->back()==1)
      return _l1norm(g0._VECTptr->front(),contextptr);
    if (g0._VECTptr->back()==2)
      return _l2norm(g0._VECTptr->front(),contextptr);
    if (is_inf(g0._VECTptr->back()))
      return _linfnorm(g0._VECTptr->front(),contextptr);
    return _frobenius_norm(g0,contextptr);
  }
  static const char _matrix_norm_s []="matrix_norm";
  static define_unary_function_eval (__matrix_norm,&_matrix_norm,_matrix_norm_s);
  define_unary_function_ptr5( at_matrix_norm ,alias_at_matrix_norm,&__matrix_norm,0,true);

  gen _dotprod(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if ( (g.type!=_VECT) || (g._VECTptr->size()!=2))
      return gentypeerr(contextptr);
    vecteur v=*g._VECTptr;
    if (v[0].type==_VECT && v[1].type==_VECT)
      return scalarproduct(*v[0]._VECTptr,*v[1]._VECTptr,contextptr);
    return dotvecteur(v[0],v[1]);
  }
  static const char _dotprod_s []="dotprod";
  static define_unary_function_eval (__dotprod,&_dotprod,_dotprod_s);
  define_unary_function_ptr5( at_dotprod ,alias_at_dotprod,&__dotprod,0,true);

  static const char _crossproduct_s []="crossproduct";
  static define_unary_function_eval (__crossproduct,&_cross,_crossproduct_s);
  define_unary_function_ptr5( at_crossproduct ,alias_at_crossproduct,&__crossproduct,0,true);

  gen _diag(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type!=_VECT || g._VECTptr->empty())
      return gensizeerr(contextptr);
    vecteur v=*g._VECTptr;
    int l=int(v.size());
#ifndef XLIGHT
    if (l==2){
      if (ckmatrix(v[0])){
	if (v[1]==at_left){
	  matrice m=*v[0]._VECTptr,res;
	  int n=int(m.size());
	  res.reserve(n);
	  for (int i=0;i<n;++i){
	    vecteur v=*m[i]._VECTptr;
	    int s=int(v.size());
	    for (int j=i+1;j<s;++j)
	      v[j]=0;
	    res.push_back(v);
	  }
	  return res;
	}
	if (v[1]==at_right){
	  matrice m=*v[0]._VECTptr,res;
	  int n=int(m.size());
	  res.reserve(n);
	  for (int i=0;i<n;++i){
	    vecteur v=*m[i]._VECTptr;
	    for (int j=0;j<i;++j)
	      v[j]=0;
	    res.push_back(v);
	  }
	  return res;
	}
	if (v[1]==at_lu){
	  matrice m=*v[0]._VECTptr,resl,resu,diag;
	  int n=int(m.size());
	  resl.reserve(n); resu.reserve(n);
	  for (int i=0;i<n;++i){
	    vecteur v=*m[i]._VECTptr;
	    diag.push_back(v[i]);
	    for (int j=0;j<=i;++j)
	      v[j]=0;
	    resu.push_back(v);
	    v=*m[i]._VECTptr;
	    int s=int(v.size());
	    for (int j=i;j<s;++j)
	      v[j]=0;
	    resl.push_back(v);
	  }
	  return makesequence(resl,diag,resu);
	}
	if (is_integral(v[1]) && v[1].type==_INT_){
	  // sub diagonal extraction
	  int shift=v[1].val;
	  const vecteur & V = *v[0]._VECTptr;
	  const_iterateur it=V.begin();
	  int vs=int(V.size());
	  vecteur res;
	  for (int i=giacmax(0,-shift);i<vs;++i){
	    const vecteur & ligne=*V[i]._VECTptr;
	    if (i+shift>=ligne.size())
	      break;
	    res.push_back(ligne[i+shift]);
	  }
	  return res;
	}
      }// if (ckmatrix(v[0])
      else {
	if (v[1].is_equal())
	  v[1]=v[1]._SYMBptr->feuille[1];
	if (v[0].type==_VECT &&is_integral(v[1]) && v[1].type==_INT_){
	  int shift=v[1].val;
	  const vecteur & V = *v[0]._VECTptr;
	  const_iterateur it=V.begin();
	  int vs=int(V.size());
	  int ts=vs+absint(shift);
	  vecteur res(ts);
	  for (int i=0;i<ts;++i){
	    vecteur ligne(ts);
	    int j=i+shift;
	    if (j>=0 && j<ts){
	      ligne[j]=*it;
	      ++it;
	    }
	    res[i]=ligne;
	  }
	  return gen(res,_MATRIX__VECT);
	}
      }
    }
    if (l==3 && v[0].type==_VECT && v[1].type==_VECT && v[2].type==_VECT && v[0]._VECTptr->size()+1==v[1]._VECTptr->size() && v[0]._VECTptr->size()==v[2]._VECTptr->size() ){
      vecteur & l=*v[0]._VECTptr;
      vecteur & d=*v[1]._VECTptr;
      vecteur & u=*v[2]._VECTptr;
      int n=int(d.size());
      matrice res(n);
      for (int i=0;i<n;++i){
	vecteur w(n);
	if (i)
	  w[i-1]=l[i-1];
	w[i]=d[i];
	if (i<n-1)
	  w[i+1]=u[i];
	res[i]=w;
      }
      return res;
    }
#endif
    if (is_squarematrix(v)){
      vecteur res(l);
      for (int i=0;i<l;++i)
	res[i]=v[i][i];
      return res;
    }
    if (ckmatrix(v)){
      if (l==1 && v[0].type==_VECT){
	v=*v[0]._VECTptr;
      }
      else
	v=*mtran(v)[0]._VECTptr;
    }
    l=int(v.size());
    matrice res;
    if (l && ckmatrix(v.front()) ){
      int s=0,r=0;
      for (int i=0;i<l;++i){
	if (!is_squarematrix(v[i]))
	  return gentypeerr(contextptr);
	s += int(v[i]._VECTptr->size());
      }
      for (int i=0;i<l;++i){
	vecteur & current=*v[i]._VECTptr;
	int c=int(current.size());
	for (int j=0;j<c;++j){
	  vecteur tmp(r);
	  vecteur & currentj=*current[j]._VECTptr;
	  for (int k=0;k<c;++k){
	    tmp.push_back(currentj[k]);
	  }
	  for (int k=c+r;k<s;++k)
	    tmp.push_back(zero);
	  res.push_back(tmp);
	}
	r += c;
      }
      return res;
    }
    for (int i=0;i<l;++i){
      vecteur tmp(i);
      tmp.push_back(v[i]);
      res.push_back(mergevecteur(tmp,vecteur(l-1-i)));
    }
    return res;
  }
  static const char _diag_s []="diag";
  static define_unary_function_eval (__diag,&_diag,_diag_s);
  define_unary_function_ptr5( at_diag ,alias_at_diag,&__diag,0,true);

  static const char _BlockDiagonal_s []="BlockDiagonal";
  static define_unary_function_eval (__BlockDiagonal,&_diag,_BlockDiagonal_s);
  define_unary_function_ptr5( at_BlockDiagonal ,alias_at_BlockDiagonal,&__BlockDiagonal,0,true);
  
  gen _giacinput(const gen & args,GIAC_CONTEXT){
    //return undef; // GIAC FIXME
#if 1
    string s;
    clear_abort();
    if (!freeze){
      dConsoleRedraw();
    }
#if 0
    s=(char *)Console_GetLine();
    set_abort();
#else
    int res=inputline("enter: ok, annul: interrompt",args.type==_STRNG?(args._STRNGptr->c_str()):"input ?",s,false,40);
    cout << (args.type==_STRNG?*args._STRNGptr:"input ?") << s << "\n";
    if (!freeze)
      dConsoleRedraw();
    set_abort();
    if (res==5){
      ctrl_c=interrupted=true;
      s="";
    }
#endif
    return string2gen(s,false);
#else
    char buf[1024];
    clear_abort();
    if (args.type==_STRNG)
      cout << *args._STRNGptr ;
    else
      cout <<"input?" ;
    cout.flush();
    int i;
    for (i=0;i<1023;++i){
      int c=getchar();
      cout << char(c);
      cout.flush();
      if (c==0x8 && i){
	buf[i-1]=0;
	cout << '\n' << buf;
	i-=2;
	continue;
      }
      if (c==0x18){
	ctrl_c=interrupted=true;
	break;
      }
      if (c=='\n')
	break;
      buf[i]=c;
      if (i & c==0)
	i -=2;
    }
    buf[i]=0;
    set_abort();
    return string2gen(buf,false);
#endif
  }
  static const char _input_s []="input";
  static define_unary_function_eval (__giacinput,&_giacinput,_input_s);
  define_unary_function_ptr5( at_input ,alias_at_input,&__giacinput,0,true);

  static const char _f2nd_s []="f2nd";
  static define_unary_function_eval (__f2nd,&_fxnd,_f2nd_s);
  define_unary_function_ptr5( at_f2nd ,alias_at_f2nd,&__f2nd,0,true);

#if 1 // ndef XLIGHT
  // service=-3 for content -2 for primpart -1 for coeff, degree for coeff
  static gen primpartcontent(const gen& g,int service,GIAC_CONTEXT){
    vecteur v;
    if (g.type==_VECT && g.subtype !=_SEQ__VECT){
      if (calc_mode(contextptr)==1)
	v=makevecteur(g,ggb_var(g));
      else
	v=makevecteur(g,vx_var());
    }
    else
      v=gen2vecteur(g);
    if (!v.empty() && v[0].type==_POLY)
      v.insert(v.begin()+1,vecteur(0));
    int s=int(v.size());
    if (s==2 && v[1].is_pow()){
      gen & f = v[1]._SYMBptr->feuille;
      if (f.type==_VECT && f._VECTptr->size()==2 && f._VECTptr->back().type==_INT_){
	v[1]=f._VECTptr->front();
	service=f._VECTptr->back().val;
      }
    }
    if (s>=2 && v[1].type==_VECT){
      vecteur l(*v[1]._VECTptr);
      int outerdim=int(l.size());
      lvar(v[0],l);
      int innerdim=int(l.size())-outerdim;
      fraction f(1);
      if (v[0].type==_POLY)
	f.num=v[0];
      else
	f=sym2r(v[0],l,contextptr);
      vecteur ll(l.begin()+outerdim,l.end());
      if (f.num.type!=_POLY){
	if (service==-1){
	  gen res=r2e(v[0],l,contextptr);
	  if (s==3){
	    if (is_zero(v[2]))
	      return res;
	    else
	      return zero;
	  }
	  return makevecteur(res);
	}
	if (service==-2)
	  return r2e(inv(f.den,contextptr),l,contextptr);
	if (service==-3)
	  return r2e(f.num,l,contextptr);
	if (service==-4)
	  return is_integer(f.num)?f.num:plus_one;
	return gensizeerr(contextptr);
      }
      if (service==-4)
	return Tcontent(*f.num._POLYptr);
      polynome & p_aplati=*f.num._POLYptr;
      polynome p=splitmultivarpoly(p_aplati,innerdim); 
      vector< monomial<gen> >::const_iterator it=p.coord.begin(),itend=p.coord.end();
      vecteur coeffs;
      coeffs.reserve(itend-it);
      for (;it!=itend;++it)
	coeffs.push_back(it->value);
      if (service==-1){
	gen gden=r2e(f.den,l,contextptr);
	if (s==3 && v[2].type==_VECT){
	  index_t ind;
	  if (!vecteur2index(*v[2]._VECTptr,ind))
	    return zero;
	  index_m i(ind);
	  it=p.coord.begin();
	  for (;it!=itend;++it){
	    if (it->index==i)
	      return r2e(it->value,ll,contextptr)/gden;
	  }
	  return zero;
	}
	return r2e(coeffs,ll,contextptr)/gden;
      }
      if (service==-2){
	p=p/_lgcd(coeffs,contextptr);
	p=unsplitmultivarpoly(p,innerdim);
	return r2e(p/f.den,l,contextptr);
      }
      if (service==-3)
	return r2e(_lgcd(coeffs,contextptr),ll,contextptr);
      return gensizeerr(contextptr);
    }
    if (s!=1 && s!=2)
      return gensizeerr(contextptr);
    gen x(vx_var());
    if (calc_mode(contextptr)==1)
      x=ggb_var(v[0]);
    if (s==2)
      x=v[1];
    gen f(_e2r(makesequence(v[0],x),contextptr));
    gen deno(1);
    if (f.type==_FRAC){
      deno=f._FRACptr->den;
      f=f._FRACptr->num;
    }
    if (f.type!=_VECT){
      switch(service){
      case -1:
	return makevecteur(f)/deno;
      case -2:
	return plus_one;
      case -3:
	return f;
      case -4:
	return (is_integer(f)?f:plus_one)/(is_integer(deno)?deno:plus_one);
      default:
	if (service>0)
	  return zero;
	else
	  return f;
      }
    }
    switch (service){
    case -1:
      return f/deno;
    case -2:
      return symb_horner(*f._VECTptr/_lgcd(f,contextptr),x);
    case -3: 
      return _lgcd(f,contextptr)/deno;
    case -4:
      f=_lgcd(f,contextptr);
      return _icontent(makesequence(f,lvar(f)),contextptr)/(is_integer(deno)?deno:plus_one);
    }
    vecteur & w=*f._VECTptr;
    int ss=int(w.size());
    if (service>=ss)
      return zero;
    return w[ss-service-1]/deno;
  }
  gen _primpart(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    return primpartcontent(g,-2,contextptr);
  }
  static const char _primpart_s []="primpart";
  static define_unary_function_eval (__primpart,&_primpart,_primpart_s);
  define_unary_function_ptr5( at_primpart ,alias_at_primpart,&__primpart,0,true);

  gen _content(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    return primpartcontent(g,-3,contextptr);
  }
  static const char _content_s []="content";
  static define_unary_function_eval (__content,&_content,_content_s);
  define_unary_function_ptr5( at_content ,alias_at_content,&__content,0,true);

  gen _icontent(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    return primpartcontent(g,-4,contextptr);
  }
  static const char _icontent_s []="icontent";
  static define_unary_function_eval (__icontent,&_icontent,_icontent_s);
  define_unary_function_ptr5( at_icontent ,alias_at_icontent,&__icontent,0,true);

  gen _coeff(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type==_VECT && !g._VECTptr->empty() && 
	(g._VECTptr->back().type==_INT_ || g._VECTptr->back().type==_DOUBLE_ || g._VECTptr->back().type==_FRAC)){
      vecteur v=*g._VECTptr;
      if (v.size()==2 && v.front().type==_SPOL1){
	const sparse_poly1 & s=*v.front()._SPOL1ptr;
	sparse_poly1::const_iterator it=s.begin(),itend=s.end();
	gen n=v.back();
	for (;it!=itend;++it){
	  if (it->exponent==n)
	    return it->coeff;
	  if (is_greater(it->exponent,n,contextptr))
	    return 0;
	}
	return undef;
      }
      is_integral(v.back());
      if (v.back().val<0)
	return gendimerr(contextptr);
      int n=absint(v.back().val);
      v.pop_back();
      if (v.size()==1 && v.front().type==_USER){
#if 0 // ndef NO_RTTI
	if (galois_field * gptr=dynamic_cast<galois_field *>(v.front()._USERptr)){
	  gen ga=gptr->a;
	  if (ga.type==_VECT){
	    int s=ga._VECTptr->size();
	    if (n>=s)
	      return 0;
	    n=s-1-n;
	    if (n>=0 && n<s)
	      return ga[n];
	  }
	  return gendimerr(contextptr);
	}
#endif
      }
      return primpartcontent(gen(v,g.subtype),n,contextptr);
    }
    if (xcas_mode(contextptr)==1 && g.type==_VECT && g._VECTptr->size()==2 && g._VECTptr->back().type==_IDNT){
      return primpartcontent(g,1,contextptr);
    }
    return primpartcontent(g,-1,contextptr);
  }
  static const char _coeff_s []="coeff";
  static define_unary_function_eval (__coeff,&_coeff,_coeff_s);
  define_unary_function_ptr5( at_coeff ,alias_at_coeff,&__coeff,0,true);

  static const char _coeffs_s []="coeffs";
  static define_unary_function_eval (__coeffs,&_coeff,_coeffs_s);
  define_unary_function_ptr5( at_coeffs ,alias_at_coeffs,&__coeffs,0,true);
#endif

  static const char _ichrem_s []="ichrem";
  static define_unary_function_eval (__ichrem,&_ichinrem,_ichrem_s);
  define_unary_function_ptr5( at_ichrem ,alias_at_ichrem,&__ichrem,0,true);

  gen _chrem(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (!ckmatrix(g) || g._VECTptr->size()!=2)
      return gensizeerr(contextptr);
    matrice m=mtran(*g._VECTptr);
    const_iterateur it=m.begin(),itend=m.end();
    if (it==itend)
      return gensizeerr(contextptr);
    gen res=*it;
    for (++it;it!=itend;++it){
      res=_ichinrem(makesequence(res,*it),contextptr);
    }
    return res;
  }
  static const char _chrem_s []="chrem";
  static define_unary_function_eval (__chrem,&_chrem,_chrem_s);
  define_unary_function_ptr5( at_chrem ,alias_at_chrem,&__chrem,0,true);

  gen _genpoly(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type!=_VECT || g._VECTptr->size()!=3)
      return gentypeerr(contextptr);
    vecteur & v=*g._VECTptr;
    gen n=v[0],b=v[1],x=v[2];
    if (b.type!=_INT_ && b.type!=_ZINT)
      return gentypeerr(contextptr);
    b=abs(b,contextptr);
    if (is_zero(b)||is_one(b))
      return gensizeerr(contextptr);
    vecteur l(lvar(n));
    fraction f(e2r(n,l,contextptr));
    if (is_integer(f.num))
      f.num=pzadic(polynome(f.num,0),b);
    else {
      if (f.num.type==_POLY)
	f.num=pzadic(*f.num._POLYptr,b);
    }
    if (is_integer(f.den))
      f.den=pzadic(polynome(f.den,0),b);
    else {
      if (f.den.type==_POLY)
	f.den=pzadic(*f.den._POLYptr,b);
    }
    l.insert(l.begin(),x);
    return r2e(f,l,contextptr);
  }
  static const char _genpoly_s []="genpoly";
  static define_unary_function_eval (__genpoly,&_genpoly,_genpoly_s);
  define_unary_function_ptr5( at_genpoly ,alias_at_genpoly,&__genpoly,0,true);

  gen _flatten(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT) return gensizeerr(contextptr);
    vecteur res;
    aplatir(*args._VECTptr,res,true);
    return res;
  }
  static const char _flatten_s []="flatten";
  static define_unary_function_eval (__flatten,&_flatten,_flatten_s);
  define_unary_function_ptr5( at_flatten ,alias_at_flatten,&__flatten,0,true);

  gen _flatten1(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT) return gensizeerr(contextptr);
    vecteur res;
    aplatir(*args._VECTptr,res,false);
    return res;
  }
  static const char _flatten1_s []="flatten1";
  static define_unary_function_eval (__flatten1,&_flatten1,_flatten1_s);
  define_unary_function_ptr5( at_flatten1 ,alias_at_flatten1,&__flatten1,0,true);

  bool has_undef_stringerr(const gen & g,std::string & err){
    if (g.type==_STRNG && g.subtype==-1){
      err=*g._STRNGptr;
      return true;
    }
    if (g.type==_VECT){
      unsigned s=unsigned(g._VECTptr->size());
      for (unsigned i=0;i<s;++i){
	if (has_undef_stringerr((*g._VECTptr)[i],err))
	  return true;
      }
      return false;
    }
    if (g.type==_POLY){
      unsigned s=unsigned(g._POLYptr->coord.size());
      for (unsigned i=0;i<s;++i){
	if (has_undef_stringerr(g._POLYptr->coord[i].value,err))
	  return true;
      }
      return false;
    }
    if (g.type==_SYMB)
      return has_undef_stringerr(g._SYMBptr->feuille,err);
    return false;
  }


  gen scalarproduct(const vecteur & a,const vecteur & b,GIAC_CONTEXT){
    vecteur::const_iterator ita=a.begin(), itaend=a.end();
    vecteur::const_iterator itb=b.begin(), itbend=b.end();
    gen res,tmp;
    for (;(ita!=itaend)&&(itb!=itbend);++ita,++itb){
      type_operator_times(conj(*ita,contextptr),(*itb),tmp);
      res += tmp;
    }
    return res;
  }

  gen _subtype(const gen & args,GIAC_CONTEXT){
    if (args.type==_INT_ && args.subtype==0)
      return change_subtype(0,_INT_TYPE);
    if (args.type==_ZINT && args.subtype==0)
      return change_subtype(2,_INT_TYPE);
    if (args.type==_DOUBLE_)
      return change_subtype(1,_INT_TYPE);
    if (args.type==_REAL)
      return change_subtype(3,_INT_TYPE);
    return args.subtype;
  }
  static const char _subtype_s []="subtype";
  static define_unary_function_eval (__subtype,&_subtype,_subtype_s);
  define_unary_function_ptr5( at_subtype ,alias_at_subtype,&__subtype,0,true);


  gen _is_polynomial(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    vecteur v;
    if (args.type==_VECT && args.subtype!=_SEQ__VECT)
      v=vecteur(1,args);
    else
      v=gen2vecteur(args);
    if (v.empty())
      return gensizeerr(contextptr);
    if (v.size()==1)
      v.push_back(ggb_var(args));
    gen tmp=giac::apply(gen(v),equal2diff);
    vecteur lv=lvarxwithinv(tmp,v[1],contextptr);
    gen res=lv.size()<2?1:0;
    res.subtype=_INT_BOOLEAN;
    return res;
  }
  static const char _is_polynomial_s []="is_polynomial";
  static define_unary_function_eval (__is_polynomial,&_is_polynomial,_is_polynomial_s);
  define_unary_function_ptr5( at_is_polynomial ,alias_at_is_polynomial,&__is_polynomial,0,true);

  // find positions of object in list or first position of substring in string
  gen _find(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    vecteur v = gen2vecteur(args);
    if (v.size()!=2 && v.size()!=3)
      return gensizeerr(contextptr);
    const gen a=v.front();
    int pos=0;
    if (v.size()==3){
      if (v[2].type!=_INT_)
	return gensizeerr(contextptr);
      pos=v[2].val;
    }
    int shift=array_start(contextptr); //xcas_mode(contextptr)>0 || abs_calc_mode(contextptr)==38;
    bool py=python_compat(contextptr);
    if (a.type==_STRNG && v[1].type!=_VECT){
      if (v[1].type!=_STRNG)
	return gensizeerr(contextptr);
      const string s=*v[1]._STRNGptr;
      vecteur res;
      for (;;++pos){
	pos=int(a._STRNGptr->find(s,pos));
	if (py)
	  return pos;
	if (pos<0 || pos>=int(a._STRNGptr->size()))
	  break;
	res.push_back(pos+shift);
      }
      return res;
    }
    if (v[1].type!=_VECT)
      return gensizeerr(contextptr);
    const vecteur & w =*v[1]._VECTptr;
    int s=int(w.size());
    vecteur res;
    for (int i=pos;i<s;++i){
      if (a==w[i]){
	if (py)
	  return i;
	res.push_back(i+shift);
      }
    }
    return res;
  }
  static const char _find_s []="find";
  static define_unary_function_eval (__find,&_find,_find_s);
  define_unary_function_ptr5( at_find ,alias_at_find,&__find,0,true);

#ifndef XLIGHT
  gen _linspace(const gen & args,GIAC_CONTEXT){
    if (args.type!=_VECT || args._VECTptr->size()<2) return gensizeerr(contextptr);
    int n=100;
    vecteur v = *args._VECTptr;
    gen start=v[0],stop=v[1];
    if (v.size()>2){
      gen N=v[2];
      if (!is_integral(N) || N.val<2)
	return gendimerr(contextptr);
      n=N.val;
    }
    gen step=(stop-start)/(n-1);
    vecteur w(n);
    for (int i=0;i<n;++i){
      w[i]=start+i*step;
    }
    return w;
  }
  static const char _linspace_s []="linspace";
  static define_unary_function_eval (__linspace,&_linspace,_linspace_s);
  define_unary_function_ptr5( at_linspace ,alias_at_linspace,&__linspace,0,true);
#endif
  gen _atan2(const gen & args,GIAC_CONTEXT){
    if (args.type!=_VECT)
      return gensizeerr(contextptr);
    if (//&& args.subtype==_SEQ__VECT 
	args._VECTptr->size()==2)
      return arg(args._VECTptr->back()+cst_i*args._VECTptr->front(),contextptr);
    return gensizeerr(contextptr); //apply(args,_atan2,contextptr);
  }
  static const char _atan2_s []="atan2";
  static define_unary_function_eval (__atan2,&_atan2,_atan2_s);
  define_unary_function_ptr5( at_atan2 ,alias_at_atan2,&__atan2,0,true);

  gen _add_autosimplify(const gen & args,GIAC_CONTEXT){
    return eval(add_autosimplify(args,contextptr),eval_level(contextptr),contextptr);
  }
  static const char _add_autosimplify_s []="add_autosimplify";
  static define_unary_function_eval (__add_autosimplify,&_add_autosimplify,_add_autosimplify_s);
  define_unary_function_ptr5( at_add_autosimplify ,alias_at_add_autosimplify,&__add_autosimplify,_QUOTE_ARGUMENTS,true);



  // step by step utilities

  bool is_periodic(const gen & f,const gen & x,gen & periode,GIAC_CONTEXT){
    periode=0;
    vecteur vx=lvarx(f,x);
    for (unsigned i=0;i<vx.size();++i){
      if (vx[i].type!=_SYMB || (vx[i]._SYMBptr->sommet!=at_exp && vx[i]._SYMBptr->sommet!=at_sin && vx[i]._SYMBptr->sommet!=at_cos && vx[i]._SYMBptr->sommet!=at_tan)){
	if (f.type==_SYMB)
	  return is_periodic(f._SYMBptr->feuille,x,periode,contextptr);
	return false;
      }
    }
    gen g=_lin(trig2exp(f,contextptr),contextptr);
    vecteur v;
    rlvarx(g,x,v);
    islesscomplexthanf_sort(v.begin(),v.end());
    int i,s=int(v.size());
    if (s<2)
      return false;
    gen a,b,v0,alpha,beta,alphacur,betacur,gof,periodecur;
    for (i=1;i<s;++i){
      if (!v[i].is_exp()){
	if (!is_periodic(v[i]._SYMBptr->feuille,x,periodecur,contextptr))
	  return false;
	periode=gcd(periode,periodecur,contextptr);
	continue;
      }
      v0=v[i];
      gen v0arg=v0._SYMBptr->feuille;
      if (is_linear_wrt(v0arg,x,alphacur,betacur,contextptr)){ 
	periodecur=normal(alphacur/cst_i,contextptr);
	if (!is_zero(im(periodecur,contextptr)))
	  return false;
	periode=gcd(periode,periodecur,contextptr);
      }
      else
	return false;
    }
    periode=ratnormal(cst_two_pi/periode);
    return !is_zero(periode);
  }

#ifdef WITH_TABVAR
  
  bool in_domain(const gen & df,const gen &x,const gen & x0,GIAC_CONTEXT){
    if (df==x)
      return true;
    if (df.type==_VECT){
      const vecteur v=*df._VECTptr;
      for (int i=0;i<int(v.size());++i){
	if (in_domain(v[i],x,x0,contextptr))
	  return true;
      }
      return false;
    }
    gen g=eval(subst(df,x,x0,false,contextptr),1,contextptr);
    return is_one(g);
  }

  // convert series expansion f at x=x0 to polynomial Taylor expansion
  // a is set to the predominant non constant monomial coefficient
  // (i.e. start from end first non 0)
  bool convert_polynom(const gen & f,const gen & x,const gen & x0,vecteur & v,gen & a,int &order,GIAC_CONTEXT){
    v.clear();
    vecteur l(lop(f,at_order_size));
    vecteur lp(l.size(),zero);
    gen g=subst(f,l,lp,false,contextptr);
    l=vecteur(1,x);
    lp=vecteur(1,x+x0);
    g=subst(g,l,lp,false,contextptr);
    lvar(g,l);
    gen temp=e2r(g,l,contextptr);
    if (is_zero(temp))
      return true;
    l.erase(l.begin());
    gen res;
    gen tmp2(polynome2poly1(temp,1));
    res=l.empty()?tmp2:((tmp2.type==_FRAC && tmp2._FRACptr->den.type==_VECT && tmp2._FRACptr->den._VECTptr->size()>1)?gen(fraction(r2e(tmp2._FRACptr->num,l,contextptr),r2e(tmp2._FRACptr->den,l,contextptr))):r2e(tmp2,l,contextptr));
    if (res.type==_FRAC && res._FRACptr->num.type==_VECT && res._FRACptr->den.type<_POLY){
      res=inv(res._FRACptr->den,contextptr)*res._FRACptr->num;
    }
    if (res.type!=_VECT)
      return false;
    v=*res._VECTptr;
    order=0;
    for (int i=int(v.size())-2;i>=0;--i){
      if (v[i]!=0){
	a=v[i];
	order=int(v.size())-i-1;
	break;
      }
    }
    return true;
  }

  static gen crunch_rootof(const gen & g,GIAC_CONTEXT){
    return has_op(g,*at_rootof)?evalf(g,1,contextptr):g;
  }

  gen try_limit_undef(const gen & f,const identificateur & x,const gen & x0,int direction,GIAC_CONTEXT){
    gen res;
#ifdef NO_STDEXCEPT
    res=limit(f,x,x0,direction,contextptr);
    if (res.type==_STRNG)
      res=undef; // message too long
#else
    try {
      res=limit(f,x,x0,direction,contextptr);
    } catch (std::runtime_error & err){
      res=undef;
    }
#endif    
    return res;
  }

  static gen write_legende(const gen & g,bool exactlegende,GIAC_CONTEXT){
    if (exactlegende)
      return symb_equal(at_legende,g);
    int digits=decimal_digits(contextptr);
    decimal_digits(3,contextptr);
    gen res=evalf(g,1,contextptr);
    res=string2gen(res.print(contextptr),false);
    res=symb_equal(at_legende,res);
    decimal_digits(digits,contextptr);
    return res;
  }
  
  vecteur endpoints(const gen & g){
    vecteur res;
    if (g.type==_VECT){
      const_iterateur it=g._VECTptr->begin(),itend=g._VECTptr->end();
      for (;it!=itend;++it)
	res=mergevecteur(res,endpoints(*it));
      return res;
    }
    if (g.type!=_SYMB)
      return res;
    if (g._SYMBptr->sommet==at_and || g._SYMBptr->sommet==at_ou)
      return endpoints(g._SYMBptr->feuille);
    if (is_inequation(g) || g._SYMBptr->sommet==at_different || g._SYMBptr->sommet==at_equal)
      return vecteur(1,g._SYMBptr->feuille[1]);
    return res;
  }

  int step_param_(const gen & f,const gen & g,const gen & t,gen & tmin,gen&tmax,vecteur & poi,vecteur & tvi,bool printtvi,bool exactlegende,GIAC_CONTEXT,bool do_inflex){
#if 0 // ndef WITH_TABVAR2
    return 0; // this spares about 30K
#endif
    if (t.type!=_IDNT)
      return 0;
    gprintf(gettext("====================\nParametric plot (%gen,%gen), variable %gen"),makevecteur(f,g,t),1,contextptr);
    gen periodef,periodeg,periode;
    if (is_periodic(f,t,periodef,contextptr) && is_periodic(g,t,periodeg,contextptr)){
      periode=gcd(periodef,periodeg,contextptr);
      if (is_greater(tmax-tmin,periode,contextptr)){
	tmin=normal(-periode/2,contextptr);
	tmax=normal(periode/2,contextptr);
      }
    }
    int eof=0,eog=0;
    if (tmin==-tmax && (eof=is_even_odd(f,t,contextptr)) && (eog=is_even_odd(g,t,contextptr))){
      if (eof==1){
	if (eog==1)
	  gprintf(gettext("Even functions."),vecteur(0),1,contextptr);
	else
	  gprintf(gettext("Even function %gen, odd function %gen. Reflection Ox"),makevecteur(f,g),1,contextptr);
      }
      else {
	if (eog==1)
	  gprintf(gettext("Odd function %gen, even function %gen. Reflection Oy"),makevecteur(f,g),1,contextptr);
	else
	  gprintf(gettext("Odd functions. Center O"),vecteur(0),1,contextptr);
      }
      tmin=0;
    }
    gen tmin0=ratnormal(tmin,contextptr),tmax0=ratnormal(tmax,contextptr);
    vecteur lv=lidnt(evalf(f,1,contextptr));
    if (lv.empty())
      return 1;
    if (lv.size()!=1 || lv.front()!=t)
      return 0;
    gen fg=symbolic(at_nop,makesequence(f,g));
    gen df=domain(fg,t,0,contextptr);
    gprintf(gettext("Domain %gen"),vecteur(1,df),1,contextptr);
    gen df1=domain(fg,t,1,contextptr); // singular values only
    if (df1.type!=_VECT){
      gensizeerr(gettext("Unable to find singular points"));
      return 0;
    }
    // Singularities
    vecteur sing,crit;
    identificateur xid=*t._IDNTptr;
    iterateur it=df1._VECTptr->begin(),itend=df1._VECTptr->end();
    for (;it!=itend;++it){
      if (is_greater(*it,tmin,contextptr) && is_greater(tmax,*it,contextptr)){
	sing.push_back(*it);
      }
    }
    // Extremas
    int st=step_infolevel(contextptr); 
    step_infolevel(0,contextptr);
    gen f1=_factor(derive(f,t,contextptr),contextptr),g1=_factor(derive(g,t,contextptr),contextptr);
    gen f2=derive(f1,t,contextptr),g2=derive(g1,t,contextptr);
    gen conv=do_inflex?f1*g2-f2*g1:1;
    gen tval=eval(t,1,contextptr);
    giac_assume(symb_and(symb_superieur_egal(t,tmin),symb_inferieur_egal(t,tmax)),contextptr);
    int cm=calc_mode(contextptr);
    calc_mode(-38,contextptr); // avoid rootof
    gen cx=recursive_normal(solve(f1,t,periode==0?2:0,contextptr),contextptr);
    gen cy=recursive_normal(solve(g1,t,periode==0?2:0,contextptr),contextptr);
    gen cc=vecteur(0);
    if (do_inflex)
      cc=recursive_normal(solve(conv,t,periode==0?2:0,contextptr),contextptr);
    calc_mode(cm,contextptr); // avoid rootof
    if (t!=tval)
      sto(tval,t,contextptr);
    step_infolevel(st,contextptr);
    if (cx.type!=_VECT || cy.type!=_VECT){
      *logptr(contextptr) << gettext("Unable to find critical points") << "\n";
      purgenoassume(t,contextptr);
      return 0;
    }
    vecteur c=mergevecteur(*cx._VECTptr,*cy._VECTptr),infl;
    if (cc.type==_VECT){
      infl=*cc._VECTptr;
      c=mergevecteur(c,infl);
    }
    else
      *logptr(contextptr) << gettext("Unable to find inflection points") << "\n";
    for (int i=0;i<int(infl.size());++i)
      infl[i]=ratnormal(infl[i],contextptr);
    for (int i=0;i<int(c.size());++i)
      c[i]=ratnormal(c[i],contextptr);
    comprim(c);
    if (!lidnt(evalf(c,1,contextptr)).empty()){
      *logptr(contextptr) << gettext("Infinite number of critical points. Try with optional argument ") << t << "=tmin..tmax" << "\n";
      purgenoassume(t,contextptr);
      return 0;
    }
    it=c.begin();itend=c.end();
    for (;it!=itend;++it){
      if (!lop(*it,at_rootof).empty())
	*it=re(evalf(*it,1,contextptr),contextptr);
      *it=recursive_normal(*it,contextptr);
      if (in_domain(df,t,*it,contextptr) && is_greater(*it,tmin,contextptr) && is_greater(tmax,*it,contextptr)){
	crit.push_back(*it);
	gen fx=try_limit_undef(f,xid,*it,0,contextptr);
	fx=recursive_normal(fx,contextptr);
	gen gx=try_limit_undef(g,xid,*it,0,contextptr);
	gx=recursive_normal(gx,contextptr);
	gen ax,ay;
	bool singp=equalposcomp(*cx._VECTptr,*it) && equalposcomp(*cy._VECTptr,*it);
	if (singp){
	  // singular point, find tangent (and kind?)
	  /* ax=try_limit_undef(f2,xid,*it,0,contextptr);
	  ax=recursive_normal(ax,contextptr);
	  ay=try_limit_undef(g2,xid,*it,0,contextptr);
	  ay=recursive_normal(ay,contextptr); */
	  int ordre=5;
	  vecteur vx,vy;
	  int ox=0,oy=0,o1=0,o2=0;
	  while (ordre<=20 && o1==0){
	    // series expansion
	    if (!convert_polynom(series(f,xid,*it,ordre,contextptr),xid,*it,vx,ax,ox,contextptr))
	      break;
	    if (!convert_polynom(series(g,xid,*it,ordre,contextptr),xid,*it,vy,ay,oy,contextptr))
	      break;
	    o1=ox;
	    if (ox<oy)
	      ay=0;
	    if (oy<ox){
	      ax=0;
	      o1=oy;
	    }
	    if (o1){
	      // find cusp kind / type de rebroussement
	      vreverse(vx.begin(),vx.end());
	      vreverse(vy.begin(),vy.end());
	      while (vx.size()<vy.size())
		vx.push_back(0);
	      while (vy.size()<vx.size())
		vy.push_back(0);
	      o2=o1+1;
	      int vs=int(vx.size());
	      for (;o2<vs;++o2){
		gen determinant=simplify(vx[o1]*vy[o2]-vx[o2]*vy[o1],contextptr);
		if (!is_zero(determinant))
		  break;
	      }
	      if (o2==vs)
		o1=0;
	    }
	    ordre *= 2;
	  }
	  gprintf(gettext("Singular point %gen, point %gen direction %gen kind (%gen,%gen)\nTaylor expansions %gen"),makevecteur(symb_equal(t__IDNT_e,*it),makevecteur(fx,gx),makevecteur(ax,ay),o1,o2,makevecteur(vx,vy)),1,contextptr);
	  gprintf(" \n",vecteur(0),1,contextptr);
	}
	else {
	  ax=try_limit_undef(f1,xid,*it,0,contextptr);
	  ay=try_limit_undef(g1,xid,*it,0,contextptr);
	  ax=recursive_normal(ax,contextptr);
	  ay=recursive_normal(ay,contextptr);
	}
	gen n=sqrt(ax*evalf(ax,1,contextptr)+ay*ay,contextptr);
	if (!is_undef(fx) && !is_inf(fx) && !is_undef(gx) && !is_inf(gx)){
	  gen pnt=_point(makesequence(fx,gx,write_legende(makevecteur(fx,gx),exactlegende,contextptr),symb_equal(at_couleur,equalposcomp(infl,*it)?_RED:_MAGENTA)),contextptr);
	  poi.push_back(pnt);	
	  if (singp){
	    vecteur ve=makevecteur(_point(makesequence(fx,gx),contextptr),makevecteur(ax/n,ay/n),symb_equal(at_couleur,_BLUE));
	    ve.push_back(write_legende(makevecteur(ax,ay),exactlegende,contextptr));
	    gen vv=_vector(gen(ve,_SEQ__VECT),contextptr);
	    poi.push_back(vv);
	  }
	}	
      }
    }
    if (tmin==minus_inf && !equalposcomp(sing,minus_inf)){
      if (in_domain(df,t,tmin,contextptr))
	sing.push_back(tmin);
      tmin=plus_inf;
    }
    if (tmax==plus_inf && !equalposcomp(sing,plus_inf)){
      if (in_domain(df,t,tmax,contextptr))
	sing.push_back(tmax);
      tmax=minus_inf;
    }
    it=crit.begin();itend=crit.end();
    for (;it!=itend;++it){
      if (!is_inf(*it)){ 
	if (is_greater(tmin,*it,contextptr))
	  tmin=*it;
	if (is_greater(*it,tmax,contextptr))
	  tmax=*it;
      }
    }
    it=infl.begin();itend=infl.end();
    for (;it!=itend;++it){
      if (!is_inf(*it)){ 
	if (is_greater(tmin,*it,contextptr))
	  tmin=*it;
	if (is_greater(*it,tmax,contextptr))
	  tmax=*it;
      }
    }
    // asymptotes
    gen xmin(plus_inf),xmax(minus_inf),ymin(plus_inf),ymax(minus_inf);
    it=sing.begin();itend=sing.end();
    for (;it!=itend;++it){
      if (!is_inf(*it)){
	if (is_greater(tmin,*it,contextptr))
	  tmin=*it;
	if (is_greater(*it,tmax,contextptr))
	  tmax=*it;	
      }
      gen fx=try_limit_undef(f,xid,*it,0,contextptr);
      fx=recursive_normal(fx,contextptr);
      if (!is_inf(fx) && !lidnt(evalf(fx,1,contextptr)).empty()) continue;
      gen fy=try_limit_undef(g,xid,*it,0,contextptr);
      fy=recursive_normal(fy,contextptr);
      if (!is_inf(fy) && !lidnt(evalf(fy,1,contextptr)).empty()) continue;
      if (is_inf(fx)){
	if (!is_inf(fy)){
	  gen equ=symb_equal(y__IDNT_e,fy);
	  if (is_greater(ymin,fy,contextptr))
	    ymin=fy;
	  if (is_greater(fy,ymax,contextptr))
	    ymax=fy;
	  gprintf(gettext("Horizontal asymptote at %gen : %gen"),makevecteur(*it,equ),1,contextptr);
	  continue;
	}
	gen a=try_limit_undef(g/f,xid,*it,0,contextptr);
	a=recursive_normal(a,contextptr);
	if (is_undef(a)) continue;
	if (is_inf(a)){
	  gprintf(gettext("Vertical parabolic asymptote at %gen"),vecteur(1,*it),1,contextptr);
	  continue;
	}
	else
	  if (!lidnt(evalf(a,1,contextptr)).empty()) continue;
	if (is_zero(a)){
	  gprintf(gettext("Horizontal parabolic asymptote at %gen"),vecteur(1,*it),1,contextptr);
	  continue;
	}
	gen b=try_limit_undef(g-a*f,xid,*it,0,contextptr);
	b=recursive_normal(b,contextptr);
	if (is_undef(b)) continue;
	if (is_inf(b)){
	  gprintf(gettext("Parabolic asymptote direction at %gen: %gen"),makevecteur(*it,symb_equal(y__IDNT_e,a*x__IDNT_e)),1,contextptr);
	  continue;
	}
	else
	  if (!lidnt(evalf(b,1,contextptr)).empty()) continue;
	gen equ=symb_equal(y__IDNT_e,a*x__IDNT_e+b);
	gprintf(gettext("Asymptote at %gen: %gen"),makevecteur(*it,equ),1,contextptr);
	continue;
      }
      if (is_inf(fy)){
	gen equ=symb_equal(x__IDNT_e,fx);
	if (is_greater(xmin,fx,contextptr))
	  xmin=fx;
	if (is_greater(fx,xmax,contextptr))
	  xmax=fx;
	gprintf(gettext("Vertical asymptote at %gen: %gen"),makevecteur(*it,equ),1,contextptr);
	continue;
      }
    }
    for (int i=0;i<int(sing.size());++i)
      sing[i]=ratnormal(sing[i],contextptr);
    for (int i=0;i<int(crit.size());++i)
      crit[i]=ratnormal(crit[i],contextptr);
    vecteur tvx=mergevecteur(sing,crit);
    if (in_domain(df,t,tmin0,contextptr))
      tvx.insert(tvx.begin(),tmin0);
    if (in_domain(df,t,tmax0,contextptr))
      tvx.push_back(tmax0);
    // add endpoints of df
    vecteur ep=endpoints(df);
    for (size_t i=0;i<ep.size();++i){
      if (is_greater(ep[i],tmin0,contextptr) && is_greater(tmax0,ep[i],contextptr) && in_domain(df,t,ep[i],contextptr))
	tvx.push_back(ep[i]);
    }
    comprim(tvx);
    gen tmp=_sort(tvx,contextptr);
    if (tmp.type!=_VECT){
      purgenoassume(t,contextptr);
      return 0;
    }
    tvx=*tmp._VECTptr;
    int pos=equalposcomp(tvx,minus_inf);
    if (pos){
      tvx.erase(tvx.begin()+pos-1);
      tvx.insert(tvx.begin(),minus_inf);
    }
    pos=equalposcomp(tvx,plus_inf);
    if (pos){
      tvx.erase(tvx.begin()+pos-1);
      tvx.push_back(plus_inf);
    }
    gen nextt=tvx.front();
    vecteur tvit=makevecteur(t,nextt);
    gen x=try_limit_undef(f,xid,nextt,1,contextptr);
    if (!has_inf_or_undef(x) && is_greater(xmin,x,contextptr))
      xmin=x;
    if (!has_inf_or_undef(x) && is_greater(x,xmax,contextptr))
      xmax=x;
    gen y=try_limit_undef(g,xid,nextt,1,contextptr);
    if (!has_inf_or_undef(y) && is_greater(ymin,y,contextptr))
      ymin=y;
    if (!has_inf_or_undef(y) && is_greater(y,ymax,contextptr))
      ymax=y;
    vecteur tvif=makevecteur(symb_equal(x__IDNT_e,f),x);
    vecteur tvig=makevecteur(symb_equal(y__IDNT_e,g),y);
    gen nothing=string2gen(" ",false);
    vecteur tvidf=makevecteur(symb_equal(symb_derive(x__IDNT_e),f1),try_limit_undef(f1,xid,nextt,1,contextptr));
    vecteur tvidg=makevecteur(symb_equal(symb_derive(y__IDNT_e),g1),try_limit_undef(g1,xid,nextt,1,contextptr));
    vecteur tviconv;
    if (do_inflex)
      tviconv=makevecteur(symb_derive(x__IDNT_e)*symb_derive(symb_derive(y__IDNT_e))-symb_derive(y__IDNT_e)*symb_derive(symb_derive(x__IDNT_e)),try_limit_undef(conv,xid,nextt,1,contextptr));
    int tvs=int(tvx.size());
    for (int i=1;i<tvs;++i){
      gen curt=nextt,dfx,dgx,convt;
      nextt=tvx[i];
      tvit.push_back(nothing);
      if (is_inf(nextt) && is_inf(curt)){
	dfx=try_limit_undef(f1,xid,0,0,contextptr);
	dgx=try_limit_undef(g1,xid,0,0,contextptr);
	convt=try_limit_undef(conv,xid,0,0,contextptr);	
      }
      else {
	if (curt==minus_inf){
	  dfx=try_limit_undef(f1,xid,nextt-1,0,contextptr);
	  dgx=try_limit_undef(g1,xid,nextt-1,0,contextptr);
	  convt=try_limit_undef(conv,xid,nextt-1,0,contextptr);
	}
	else {
	  if (nextt==plus_inf){
	    dfx=try_limit_undef(f1,xid,curt+1,0,contextptr);
	    dgx=try_limit_undef(g1,xid,curt+1,0,contextptr);
	    convt=try_limit_undef(conv,xid,curt+1,0,contextptr);
	  }
	  else {
	    gen milieut=(curt+nextt)/2;
	    gen curxd=evalf_double(curt,1,contextptr);
	    gen nextxd=evalf_double(nextt,1,contextptr);
	    if (curxd.type==_DOUBLE_ && nextxd.type==_DOUBLE_){
	      double cd=curxd._DOUBLE_val,nd=nextxd._DOUBLE_val;
	      if (nd-cd>1e-6*(absdouble(cd)+absdouble(nd))){
		milieut=exact((cd+nd)/2,contextptr);
	      }
	    }
	    dfx=try_limit_undef(f1,xid,milieut,0,contextptr);
	    dgx=try_limit_undef(g1,xid,milieut,0,contextptr);
	    convt=try_limit_undef(conv,xid,(curt+nextt)/2,0,contextptr);
	  }
	}
      }
      if (is_zero(dfx) || is_zero(dgx)){
	purgenoassume(t,contextptr);
	return 0;
      }
      if (is_strictly_positive(dfx,contextptr)){
#if 1 // defined NSPIRE || defined NSPIRE_NEWLIB || defined HAVE_WINT_T
	tvif.push_back(string2gen("/",false));
#else
	tvif.push_back(string2gen("↗",false));
#endif
	tvidf.push_back(string2gen("+",false));
      }
      else {
#if 1 //defined NSPIRE || defined NSPIRE_NEWLIB || defined HAVE_WINT_T
	tvif.push_back(string2gen("\\",false));
#else
	tvif.push_back(string2gen("↘",false));
#endif
	tvidf.push_back(string2gen("-",false));
      }
      if (is_strictly_positive(convt,contextptr))
	tviconv.push_back(string2gen(abs_calc_mode(contextptr)==38?"∪":"+ (U)",false));
      else
	tviconv.push_back(string2gen(abs_calc_mode(contextptr)==38?"∩":"- (^)",false));
      if (is_strictly_positive(dgx,contextptr)){
#if 1 // defined NSPIRE || defined NSPIRE_NEWLIB || defined HAVE_WINT_T
	tvig.push_back(string2gen("/",false));
#else
	tvig.push_back(string2gen("↗",false));
#endif
	tvidg.push_back(string2gen("+",false));
      }
      else {
#if 1 // defined NSPIRE || defined NSPIRE_NEWLIB || defined HAVE_WINT_T
	tvig.push_back(string2gen("\\",false));
#else
	tvig.push_back(string2gen("↘",false));
#endif
	tvidg.push_back(string2gen("-",false));
      }
      if (i<tvs-1 && equalposcomp(sing,nextt)){
	x=try_limit_undef(f,xid,nextt,-1,contextptr);
	x=recursive_normal(x,contextptr);
	if (!has_inf_or_undef(x) && is_greater(xmin,x,contextptr))
	  xmin=x;
	if (!has_inf_or_undef(x) && is_greater(x,xmax,contextptr))
	  xmax=x;
	y=try_limit_undef(g,xid,nextt,-1,contextptr);
	y=recursive_normal(y,contextptr);
	if (!has_inf_or_undef(y) && is_greater(ymin,y,contextptr))
	  ymin=y;
	if (!has_inf_or_undef(y) && is_greater(y,ymax,contextptr))
	  ymax=y;
	tvit.push_back(nextt);
	tvif.push_back(x);
	tvig.push_back(crunch_rootof(y,contextptr));
	tvidf.push_back(nothing);
	tvidg.push_back(nothing);
	tviconv.push_back(nothing);
	gen x=try_limit_undef(f,xid,nextt,1,contextptr);
	x=recursive_normal(x,contextptr);
	if (!has_inf_or_undef(x) && is_greater(xmin,x,contextptr))
	  xmin=x;
	if (!has_inf_or_undef(x) && is_greater(x,xmax,contextptr))
	  xmax=x;
	y=try_limit_undef(g,xid,nextt,1,contextptr);
	y=recursive_normal(y,contextptr);
	if (!has_inf_or_undef(y) && is_greater(ymin,y,contextptr))
	  ymin=y;
	if (!has_inf_or_undef(y) && is_greater(y,ymax,contextptr))
	  ymax=y;
	tvit.push_back(nextt);
	tvif.push_back(x);
	tvig.push_back(crunch_rootof(y,contextptr));
	tvidf.push_back(nothing);
	tvidg.push_back(nothing);
	tviconv.push_back(nothing);
      }
      else {
	gen x=try_limit_undef(f,xid,nextt,-1,contextptr);
	x=recursive_normal(x,contextptr);
	if (!has_inf_or_undef(x) && is_greater(xmin,x,contextptr))
	  xmin=x;
	if (!has_inf_or_undef(x) && is_greater(x,xmax,contextptr))
	  xmax=x;
	y=try_limit_undef(g,xid,nextt,-1,contextptr);
	y=recursive_normal(y,contextptr);
	if (!has_inf_or_undef(y) && is_greater(ymin,y,contextptr))
	  ymin=y;
	if (!has_inf_or_undef(y) && is_greater(y,ymax,contextptr))
	  ymax=y;
	tvit.push_back(nextt);
	tvif.push_back(x);
	tvig.push_back(y);
	y=try_limit_undef(f1,xid,nextt,-1,contextptr);
	y=recursive_normal(y,contextptr);
	tvidf.push_back(crunch_rootof(y,contextptr));
	y=try_limit_undef(g1,xid,nextt,-1,contextptr);
	y=recursive_normal(y,contextptr);
	tvidg.push_back(crunch_rootof(y,contextptr));
	if (equalposcomp(infl,nextt)) y=0;
	else {
	  y=try_limit_undef(conv,xid,nextt,-1,contextptr);
	  y=recursive_normal(y,contextptr);
	}
	tviconv.push_back(crunch_rootof(y,contextptr));
      }
    }
    tvi=makevecteur(tvit,tvif,tvidf,tvig,tvidg,tviconv);
    if (!do_inflex)
      tvi.pop_back();
    gen xscale=xmax-xmin;
    if (is_inf(xscale) || xscale==0)
      xscale=gnuplot_xmax-gnuplot_xmin;
    if (eof==2){
      xmax=max(xmax,-xmin,contextptr);
      xmin=-xmax;
    }
    if (eog==2){
      ymax=max(ymax,-ymin,contextptr);
      ymin=-ymax;
    }
    if (eof && eog)
      tmin=-tmax;
    if (periode==0){
      gen tscale=tmax-tmin;
      tmax += tscale/2;
      tmin -= tscale/2;
    }
    if (tmax==tmin){
      tmin=gnuplot_tmin;
      tmax=gnuplot_tmax;
    }
    gen glx(_GL_X);
    glx.subtype=_INT_PLOT;
    glx=symb_equal(glx,symb_interval(xmin-xscale/2,xmax+xscale/2));
    poi.insert(poi.begin(),glx);
    gen yscale=ymax-ymin;
    if (is_inf(yscale) || yscale==0){
      yscale=gnuplot_ymax-gnuplot_ymin;
      ymax=gnuplot_ymax;
      ymin=gnuplot_ymin;
    }
    if (eog==2){
      ymax=max(ymax,-ymin,contextptr);
      ymin=-ymax;
    }
    gen gly(_GL_Y);
    gly.subtype=_INT_PLOT;
    gly=symb_equal(gly,symb_interval(ymin-yscale/2,ymax+yscale/2));
    poi.insert(poi.begin(),gly);
    gprintf(gettext("Variations (%gen,%gen)\n%gen"),makevecteur(f,g,tvi),1,contextptr);
#ifndef EMCC
    if (printtvi && step_infolevel(contextptr)==0)
      *logptr(contextptr) << tvi << "\n";
#endif
    // finished!
    purgenoassume(t,contextptr);
    return 1 + (periode!=1);
  }

  int step_param(const gen & f,const gen & g,const gen & t,gen & tmin,gen&tmax,vecteur & poi,vecteur & tvi,bool printtvi,bool exactlegende,GIAC_CONTEXT,bool do_inflex){
    bool c=complex_mode(contextptr); int st=step_infolevel(contextptr),s=0;
    if (t==x__IDNT_e || t==y__IDNT_e)
      *logptr(contextptr) << gettext("Warning, using x or y as variable in parametric plot may lead to confusion!") << "\n";
    step_infolevel(0,contextptr);
#ifdef NO_STDEXCEPT
    s=step_param_(f,g,t,tmin,tmax,poi,tvi,printtvi,exactlegende,contextptr,do_inflex);
#else
    try {
      s=step_param_(f,g,t,tmin,tmax,poi,tvi,printtvi,exactlegende,contextptr,do_inflex);
    } catch(std::runtime_error & e){ 
      last_evaled_argptr(contextptr)=NULL;
      s=0;
    }
#endif
    complex_mode(c,contextptr);
    step_infolevel(st,contextptr);
    return s;
  }

#else
  int step_param(const gen & f,const gen & g,const gen & t,gen & tmin,gen&tmax,vecteur & poi,vecteur & tvi,bool printtvi,bool exactlegende,GIAC_CONTEXT,bool do_inflex){
    return 0;
  }

#endif

#ifdef WITH_TABVAR
  gen strict2large(const gen & g){
    if (g.type==_VECT){
      vecteur v(*g._VECTptr);
      for (size_t i=0;i<v.size();++i)
	v[i]=strict2large(v[i]);
      return gen(v,g.subtype);
    }
    if (g.type!=_SYMB)
      return g;
    if (g._SYMBptr->sommet==at_superieur_strict)
      return symb_superieur_egal(g._SYMBptr->feuille);
    if (g._SYMBptr->sommet==at_inferieur_strict)
      return symb_inferieur_egal(g._SYMBptr->feuille);
    if (g._SYMBptr->sommet==at_different)
      return 1;
    return symbolic(g._SYMBptr->sommet,strict2large(g._SYMBptr->feuille));
  }

  // x->f in xmin..xmax
  // pass -inf and inf by default.
  // poi will contain point of interest: asymptotes and extremas
  // xmin and xmax will be set to values containing all points in poi
  int step_func_(const gen & f,const gen & x,gen & xmin,gen&xmax,vecteur & poi,vecteur & tvi,gen& periode,vecteur & asym,vecteur & parab,vecteur & crit,vecteur & infl,bool printtvi,bool exactlegende,GIAC_CONTEXT,bool do_inflex){
    if (x.type!=_IDNT)
      return 0;
    gprintf(gettext("====================\nFunction plot %gen, variable %gen"),makevecteur(f,x),1,contextptr);
    if (is_periodic(f,x,periode,contextptr)){
      gprintf(gettext("Periodic function T=%gen"),vecteur(1,periode),1,contextptr);
      if (is_greater(xmax-xmin,periode,contextptr)){
	xmin=normal(-periode/2,contextptr);
	xmax=normal(periode/2,contextptr);
      }
    }
    int eo=0;
    if (xmin==-xmax && (eo=is_even_odd(f,x,contextptr))){
      if (eo==1)
	gprintf(gettext("Even function %gen. Reflection Oy"),vecteur(1,f),1,contextptr);
      else
	gprintf(gettext("Odd function %gen. Center O"),vecteur(1,f),1,contextptr);
      xmin=0;
    }
    gen xmin0=ratnormal(xmin,contextptr),xmax0=ratnormal(xmax,contextptr);
    vecteur lv=lidnt(evalf(f,1,contextptr));
    if (lv.empty())
      lv=lidnt(f);
    if (lv.empty())
      return 1;
    if (lv.size()!=1 || lv.front()!=x)
      return 0;
    gen xval=eval(x,1,contextptr);
    giac_assume(symb_and(symb_superieur_egal(x,xmin),symb_inferieur_egal(x,xmax)),contextptr);
    gen df=domain(f,x,0,contextptr);
    gen dflarge(strict2large(df));
    gprintf(gettext("Domain %gen"),vecteur(1,df),1,contextptr);
    gen df1=domain(f,x,1,contextptr); // singular values only
    if (df1.type!=_VECT){
      gensizeerr(gettext("Unable to find singular points"));
      return 0;
    }
    // Asymptotes
    vecteur sing;
    identificateur xid=*x._IDNTptr;
    iterateur it=df1._VECTptr->begin(),itend=df1._VECTptr->end();
    for (;it!=itend;++it){
      if (in_domain(dflarge,x,*it,contextptr) && is_greater(*it,xmin,contextptr) && is_greater(xmax,*it,contextptr)){
	sing.push_back(*it);
      }
    }
    // Extremas
    int st=step_infolevel(contextptr);
    step_infolevel(0,contextptr);
    gen f1=_factor(derive(f,x,contextptr),contextptr);
    gen f2=derive(f1,x,contextptr);
#if 1
    int cm=calc_mode(contextptr);
    calc_mode(-38,contextptr); // avoid rootof
    gen c1=solve(f1,x,periode==0?2:0,contextptr);
    gen c2=(!do_inflex || is_zero(f2))?gen(vecteur(0)):solve(_numer(f2,contextptr),x,periode==0?2:0,contextptr),c(c1);
    //gen c2=gen(vecteur(0)),c(c1);
    calc_mode(cm,contextptr);
    step_infolevel(st,contextptr);
    if (x!=xval)
      sto(xval,x,contextptr);
    if (c1.type!=_VECT){
      *logptr(contextptr) << gettext("Unable to find critical points") << "\n";
      return 0;
    }
    if (c2.type==_VECT){
      infl=*c2._VECTptr;
      c=gen(mergevecteur(gen2vecteur(c1),infl));
    }
    else
      *logptr(contextptr) << gettext("Unable to find convexity") << "\n";
    // if (c.type==_VECT && c._VECTptr->empty()) c=_fsolve(makesequence(f,x),contextptr);
#else
    gen c=critical(makesequence(f,x),false,contextptr);
    step_infolevel(st,contextptr);
    if (c.type!=_VECT){
      *logptr(contextptr) << gettext("Unable to find critical points") << "\n";
      purgenoassume(x,contextptr);
      return 0;
    }
#endif
    if (!lidnt(evalf(c,1,contextptr)).empty()){
      *logptr(contextptr) << gettext("Infinite number of critical points. Try with optional argument ") << x << "=xmin..xmax" << "\n";
      purgenoassume(x,contextptr);
      return 0;
    }
    it=c._VECTptr->begin();itend=c._VECTptr->end();
    for (;it!=itend;++it){
      if (!lop(*it,at_rootof).empty())
	*it=re(evalf(*it,1,contextptr),contextptr);
      if (in_domain(df,x,*it,contextptr) && is_greater(*it,xmin,contextptr) && is_greater(xmax,*it,contextptr)){
	crit.push_back(*it);
	gen fx=try_limit_undef(f,xid,*it,0,contextptr);
	fx=recursive_normal(fx,contextptr);
	if (!is_undef(fx) && !is_inf(fx)){
	  if (1 || exactlegende)
	    poi.push_back(_point(makesequence(*it,fx,write_legende(makevecteur(*it,fx),exactlegende,contextptr),symb_equal(at_couleur,equalposcomp(infl,*it)?_GREEN:_MAGENTA)),contextptr));
	  else {
	    gen abscisse=evalf_double(*it,1,contextptr);
	    gen ordonnee=evalf_double(fx,1,contextptr);
	    if (abscisse.type==_DOUBLE_ && ordonnee.type==_DOUBLE_)
	      poi.push_back(_point(makesequence(*it,fx,write_legende(string2gen(print_DOUBLE_(abscisse._DOUBLE_val,3)+","+print_DOUBLE_(ordonnee._DOUBLE_val,3),false),exactlegende,contextptr),symb_equal(at_couleur,_MAGENTA)),contextptr));
	  }
	}
      }
    }
    if (xmin==minus_inf && !equalposcomp(sing,minus_inf)){
      if (in_domain(df,x,xmin,contextptr))
	sing.push_back(xmin);
      xmin=plus_inf;
    }
    if (xmax==plus_inf && !equalposcomp(sing,plus_inf)){
      if (in_domain(df,x,xmax,contextptr))
	sing.push_back(xmax);
      xmax=minus_inf;
    }
    it=crit.begin();itend=crit.end();
    for (;it!=itend;++it){
      if (!has_inf_or_undef(*it)){ 
	if (is_greater(xmin,*it,contextptr))
	  xmin=*it;
	if (is_greater(*it,xmax,contextptr))
	  xmax=*it;
      }
    }
    it=sing.begin();itend=sing.end();
    for (;it!=itend;++it){
      gen equ;
      if (!has_inf_or_undef(*it)){ // vertical
	if (is_greater(xmin,*it,contextptr))
	  xmin=*it;
	if (is_greater(*it,xmax,contextptr))
	  xmax=*it;
	gen l=try_limit_undef(f,xid,*it,1,contextptr);
	l=recursive_normal(l,contextptr);
	if (is_inf(l)){
	  equ=symb_equal(x__IDNT_e,*it);
	  asym.push_back(makevecteur(*it,equ));
	  gprintf(gettext("Vertical asymptote %gen"),vecteur(1,equ),1,contextptr);
	  if (eo && *it!=0){
	    equ=symb_equal(x__IDNT_e,-*it);
	    asym.push_back(makevecteur(-*it,equ));
	    gprintf(gettext("Symmetric vertical asymptote %gen"),vecteur(1,equ),1,contextptr);
	  }
	}
	continue;
      }
      gen l=try_limit_undef(f,xid,*it,0,contextptr);
      l=recursive_normal(l,contextptr);
      if (is_undef(l)) continue;
      if (!is_inf(l)){
	if (!lidnt(evalf(l,1,contextptr)).empty()) continue;
	equ=symb_equal(y__IDNT_e,l);
	asym.push_back(makevecteur(*it,equ));
	gprintf(gettext("Horizontal asymptote %gen"),vecteur(1,equ),1,contextptr);
	if (eo==2 && *it!=0 && l!=0){
	  equ=symb_equal(y__IDNT_e,l);
	  asym.push_back(makevecteur(-*it,-equ));
	  gprintf(gettext("Symmetric horizontal asymptote %gen"),vecteur(1,-equ),1,contextptr);
	}
	continue;
      }
      gen a=try_limit_undef(f/x,xid,*it,0,contextptr);
      a=recursive_normal(a,contextptr);
      if (is_undef(a)) continue;
      if (is_inf(a)){
	parab.push_back(makevecteur(*it,a));
	gprintf(gettext("Vertical parabolic asymptote at %gen"),vecteur(1,*it),1,contextptr);
	continue;
      }
      else
	if (!lidnt(evalf(a,1,contextptr)).empty()) continue;
      if (is_zero(a)){
	parab.push_back(makevecteur(*it,0));
	gprintf(gettext("Horizontal parabolic asymptote at %gen"),vecteur(1,*it),1,contextptr);
	continue;
      }
      gen b=try_limit_undef(f-a*x,xid,*it,0,contextptr);
      b=recursive_normal(b,contextptr);
      if (is_undef(b)) continue;
      // avoid bounded_function
      if (is_inf(b)){
	parab.push_back(makevecteur(*it,a));
	gprintf(gettext("Parabolic asymptote direction %gen at infinity"),vecteur(1,symb_equal(y__IDNT_e,a*x__IDNT_e)),1,contextptr);
	continue;
      }
      else
	if (!lidnt(evalf(b,1,contextptr)).empty()) continue;
      equ=symb_equal(y__IDNT_e,a*x__IDNT_e+b);
      asym.push_back(makevecteur(*it,equ));
      gprintf(gettext("Asymptote %gen"),vecteur(1,equ),1,contextptr);
      if (eo && *it!=0){
	if (eo==1)
	  equ=symb_equal(y__IDNT_e,-a*x__IDNT_e+b);
	else
	  equ=symb_equal(y__IDNT_e,a*x__IDNT_e-b);
	asym.push_back(makevecteur(*it,equ));
	gprintf(gettext("Symmetric asymptote %gen"),vecteur(1,equ),1,contextptr);
      }
    }
    // merge sing and crit, add xmin0, xmax0, build variation matrix
    for (int i=0;i<int(sing.size());++i)
      sing[i]=ratnormal(sing[i],contextptr);
    for (int i=0;i<int(crit.size());++i)
      crit[i]=ratnormal(crit[i],contextptr);
    vecteur tvx=mergevecteur(sing,crit);
    if (in_domain(df,x,xmin0,contextptr))
      tvx.insert(tvx.begin(),xmin0);
    if (in_domain(df,x,xmax0,contextptr))
      tvx.push_back(xmax0);
    // add endpoints of df
    vecteur ep=endpoints(df);
    for (size_t i=0;i<ep.size();++i){
      if (is_greater(ep[i],xmin0,contextptr) && is_greater(xmax0,ep[i],contextptr) && in_domain(df,x,ep[i],contextptr))
	tvx.push_back(ep[i]);
    }
    // add sign/abs
    vecteur lsignabs(mergevecteur(lop(f,at_sign),lop(f,at_abs)));
    if (!lsignabs.empty()){
      lsignabs=lvarx(lsignabs,x);
      for (size_t i=0;i<lsignabs.size();++i){
	tvx=mergevecteur(tvx,solve(lsignabs[i]._SYMBptr->feuille,x,periode==0?2:0,contextptr));
      }
    }
    comprim(tvx);
    gen tmp=_sort(tvx,contextptr);
    if (tmp.type!=_VECT){
      purgenoassume(x,contextptr);
      return 0;
    }
    tvx=*tmp._VECTptr;
    int pos=equalposcomp(tvx,minus_inf);
    if (pos){
      tvx.erase(tvx.begin()+pos-1);
      tvx.insert(tvx.begin(),minus_inf);
    }
    pos=equalposcomp(tvx,plus_inf);
    if (pos){
      tvx.erase(tvx.begin()+pos-1);
      tvx.push_back(plus_inf);
    }
    gen nextx=tvx.front();
    if (!lop(nextx,at_rootof).empty())
      nextx=re(evalf(nextx,1,contextptr),contextptr);
    vecteur tvix=makevecteur(x,nextx);
    gen y=try_limit_undef(f,xid,nextx,1,contextptr),ymin(plus_inf),ymax(minus_inf);
    if (!has_inf_or_undef(y) && is_greater(ymin,y,contextptr))
      ymin=y;
    if (!has_inf_or_undef(y) && is_greater(y,ymax,contextptr))
      ymax=y;
    gen yof=y__IDNT_e; // symb_of(y__IDNT_e,x); // 
    vecteur tvif=makevecteur(symb_equal(yof,f),y);
    gen nothing=string2gen(" ",false);
    vecteur tvidf=makevecteur(symb_equal(symb_derive(yof),f1),try_limit_undef(f1,xid,nextx,1,contextptr));
    vecteur tvidf2;
    if (do_inflex)
      tvidf2=makevecteur(string2gen("y''",false),try_limit_undef(f2,xid,nextx,1,contextptr));
    int tvs=int(tvx.size());
    for (int i=1;i<tvs;++i){
      gen curx=nextx,dfx,df2;
      nextx=tvx[i];
      if (!lop(nextx,at_rootof).empty())
	nextx=re(evalf(nextx,1,contextptr),contextptr);
      tvix.push_back(nothing);
      if (is_inf(nextx) && is_inf(curx)){
	dfx=try_limit_undef(f1,xid,0,0,contextptr);
	if (do_inflex) df2=try_limit_undef(f2,xid,0,0,contextptr);
      }
      else {
	if (curx==minus_inf){
	  dfx=try_limit_undef(f1,xid,nextx-1,0,contextptr);
	  if (do_inflex) df2=try_limit_undef(f2,xid,nextx-1,0,contextptr);
	}
	else {
	  if (nextx==plus_inf){
	    dfx=try_limit_undef(f1,xid,curx+1,0,contextptr);
	    if (do_inflex) df2=try_limit_undef(f2,xid,curx+1,0,contextptr);
	  }
	  else {
	    gen m=(curx+nextx)/2;
	    gen curxd=evalf_double(curx,1,contextptr);
	    gen nextxd=evalf_double(nextx,1,contextptr);
	    if (curxd.type==_DOUBLE_ && nextxd.type==_DOUBLE_){
	      double cd=curxd._DOUBLE_val,nd=nextxd._DOUBLE_val;
	      if (nd-cd>1e-6*(absdouble(cd)+absdouble(nd))){
		m=exact((cd+nd)/2,contextptr);
	      }
	    }
	    if (in_domain(df,x,m,contextptr)){
	      dfx=try_limit_undef(f1,xid,m,0,contextptr);
	      if (do_inflex) df2=try_limit_undef(f2,xid,m,0,contextptr);
	    }
	    else dfx=df2=undef;
	  }
	}
      }
      if (is_zero(dfx)){
	purgenoassume(x,contextptr);
	return 0;
      }
      if (is_undef(dfx)){
	tvif.push_back(string2gen("X",false));
	tvidf.push_back(string2gen("X",false));
      }
      else {
	if (is_strictly_positive(dfx,contextptr)){
#if 1//defined NSPIRE || defined NSPIRE_NEWLIB || defined HAVE_WINT_T
	  tvif.push_back(string2gen("/",false));
	  // tvif.push_back(string2gen("↑",false));
#else
	  tvif.push_back(string2gen("↗",false));
#endif
	  tvidf.push_back(string2gen("+",false));
	}
	else {
#if 1 //defined NSPIRE || defined NSPIRE_NEWLIB || defined HAVE_WINT_T
	  tvif.push_back(string2gen("\\",false));
	  // tvif.push_back(string2gen("↓",false));
#else
	  tvif.push_back(string2gen("dec.",false));
#endif
	  tvidf.push_back(string2gen("-",false));
	}
      }
      if (do_inflex){
	if (is_undef(df2))
	  tvidf2.push_back(string2gen("X",false));
	else {
	  if (is_strictly_positive(df2,contextptr)){
	    tvidf2.push_back(string2gen("+ U",false));
	  }
	  else {
	    tvidf2.push_back(string2gen("- ^",false)); 
	  }
	}
      }
      if (i<tvs-1 && equalposcomp(sing,nextx)){
	y=try_limit_undef(f,xid,nextx,-1,contextptr);
	y=recursive_normal(y,contextptr);
	if (!has_inf_or_undef(y) && is_greater(ymin,y,contextptr))
	  ymin=y;
	if (!has_inf_or_undef(y) && is_greater(y,ymax,contextptr))
	  ymax=y;
	tvix.push_back(nextx);
	tvif.push_back(crunch_rootof(y,contextptr));
	tvidf.push_back(string2gen("||",false));
	if (do_inflex) tvidf2.push_back(string2gen("||",false));
	y=try_limit_undef(f,xid,nextx,1,contextptr);
	y=recursive_normal(y,contextptr);
	if (!has_inf_or_undef(y) && is_greater(ymin,y,contextptr))
	  ymin=y;
	if (!has_inf_or_undef(y) && is_greater(y,ymax,contextptr))
	  ymax=y;
	tvix.push_back(nextx);
	tvif.push_back(crunch_rootof(y,contextptr));
	tvidf.push_back(string2gen("||",false));
	if (do_inflex) tvidf2.push_back(string2gen("||",false));
      }
      else {
	y=try_limit_undef(f,xid,nextx,-1,contextptr); 
	if (0 && !is_inf(nextx) && !is_zero(recursive_normal(y-try_limit_undef(f,xid,nextx,1,contextptr),contextptr))) // should not happen
	  y=undef;
	y=recursive_normal(y,contextptr);
	if (!has_inf_or_undef(y) && is_greater(ymin,y,contextptr))
	  ymin=y;
	if (!has_inf_or_undef(y) && is_greater(y,ymax,contextptr))
	  ymax=y;
	tvix.push_back(nextx);
	tvif.push_back(crunch_rootof(y,contextptr));
	y=try_limit_undef(f1,xid,nextx,-1,contextptr); 
	// additional check for same bidirectional limit
	gen ysecond;
	if (!is_inf(nextx) && !is_zero(recursive_normal(y-(ysecond=try_limit_undef(f1,xid,nextx,1,contextptr)),contextptr)))
	  y=makevecteur(y,ysecond);
	y=recursive_normal(y,contextptr);
	tvidf.push_back(crunch_rootof(y,contextptr));
	if (do_inflex){
	  y=try_limit_undef(f2,xid,nextx,0,contextptr);
	  y=recursive_normal(y,contextptr);
	  tvidf2.push_back(crunch_rootof(y,contextptr));
	}
      }
    }
    tvi=makevecteur(tvix,tvidf,tvif);
    if (do_inflex) tvi.push_back(tvidf2);
    // suppress double entries
    vecteur tvit(mtran(tvi));
    for (size_t i=1;i<tvit.size();++i){
      if (tvit[i]==tvit[i-1])
	tvit.erase(tvit.begin()+i);
    }
    tvi=mtran(tvit);
    gen yscale=ymax-ymin;
    if (is_inf(yscale) || yscale==0){
      yscale=xmax-xmin;
      ymax=gnuplot_ymax;
      ymin=gnuplot_ymin;
    }
    if (is_inf(yscale) || yscale==0){
      yscale=gnuplot_ymax-gnuplot_ymin;
      ymax=gnuplot_ymax;
      ymin=gnuplot_ymin;
    }
    if (eo){
      xmax=max(xmax,-xmin,contextptr);
      xmin=-xmax;
      if (eo==2){
	ymax=max(ymax,-ymin,contextptr);
	ymin=-ymax;
      }
    }
    gen gly(_GL_Y);
    gly.subtype=_INT_PLOT;
    gly=symb_equal(gly,symb_interval(ymin-yscale/2,ymax+yscale/2));
    poi.insert(poi.begin(),gly);
    // finished!
    purgenoassume(x,contextptr);
    return 1 + (periode!=0);
  }

  int step_func(const gen & f,const gen & x,gen & xmin,gen&xmax,vecteur & poi,vecteur & tvi,gen & periode,vecteur & asym,vecteur & parab,vecteur & crit,vecteur & inflex,bool printtvi,bool exactlegende,GIAC_CONTEXT,bool do_inflex){
    bool c=complex_mode(contextptr); int st=step_infolevel(contextptr),s=0;
    step_infolevel(0,contextptr);
#ifdef NO_STDEXCEPT
    s=step_func_(f,x,xmin,xmax,poi,tvi,periode,asym,parab,crit,inflex,printtvi,exactlegende,contextptr,do_inflex);
#else
    try {
      s=step_func_(f,x,xmin,xmax,poi,tvi,periode,asym,parab,crit,inflex,printtvi,exactlegende,contextptr,do_inflex);
    } catch (std::runtime_error & e){
      last_evaled_argptr(contextptr)=NULL;
      s=0;
    }
#endif
    complex_mode(c,contextptr);
    step_infolevel(st,contextptr);
    return s;
  }

  gen _tabvar(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    vecteur v(g.type==_VECT && g.subtype==_SEQ__VECT?*g._VECTptr:vecteur(1,g));
    int s=int(v.size());
#ifdef EMCC
    int plot=1;
#else
    int plot=0;
#endif
    bool return_tabvar=false,return_equation=false,return_coordonnees=false,do_inflex=false;
    for (int i=0;i<s;++i){
      if (v[i]==at_tabvar){
	return_tabvar=true;
	v.erase(v.begin()+i);
	--s; --i; continue;
      }
      if (v[i]==at_coordonnees){
	return_coordonnees=true;
	v.erase(v.begin()+i);
	--s; --i; continue;
      }
      if (v[i].is_equal()){
	gen & f=v[i]._SYMBptr->feuille;
	if (f.type==_VECT && f._VECTptr->size()==2 && f._VECTptr->front()==at_derive){
	  if (f._VECTptr->back()==2)
	    do_inflex=true;
	  v.erase(v.begin()+i);
	  --s; --i; continue;
	}
      }
    }
    bool exactlegende=false;
    if (s>1 && v[s-1]==at_exact){
      exactlegende=true;
      v.pop_back();
      --s;
    }
    if (s==2 && v[1].type==_SYMB && v[1]._SYMBptr->sommet!=at_equal)
      v=makevecteur(v,ggb_var(v));
    if (s==1){
      v.push_back(ggb_var(g));
      ++s;
    }
    if (s<2)
      return gensizeerr(contextptr);
    gen f=exact(v[0],contextptr);
    gen x=v[1];
    int s0=2;
    gen xmin(minus_inf),xmax(plus_inf);
    bool default_interval=true;
    if (x.is_equal()){
      gen g=x._SYMBptr->feuille;
      if (g.type!=_VECT || g._VECTptr->size()!=2)
	return gensizeerr(contextptr);
      x=g._VECTptr->front();
      g=g._VECTptr->back();
      if (g.is_interval()){
	xmin=g._SYMBptr->feuille[0];
	xmax=g._SYMBptr->feuille[1];
	default_interval=(xmin==minus_inf && xmax==plus_inf);
      }
    }
    else {
      if (s>=4){
	xmin=v[2];
	xmax=v[3];
	default_interval=(xmin==minus_inf && xmax==plus_inf);
	s0=4;
      }
      if (s==2 && x.type!=_IDNT)
	return _tabvar(makevecteur(f,x),contextptr);
    }
    vecteur tvi,poi;
    bool param=f.type==_VECT && f._VECTptr->size()==2;
    int periodic=0;
    if (param)
      periodic=step_param(f._VECTptr->front(),f._VECTptr->back(),x,xmin,xmax,poi,tvi,false,exactlegende,contextptr,do_inflex);
    else {
      gen periode; vecteur asym,parab,crit,inflex;
      periodic=step_func(f,x,xmin,xmax,poi,tvi,periode,asym,parab,crit,inflex,false,exactlegende,contextptr,do_inflex);
    }
    // round floats in tvi
    for (int i=0;i<int(tvi.size());++i){
      gen tmp=tvi[i];
      if (tmp.type==_VECT){
	vecteur v=*tmp._VECTptr;
	for (int j=0;j<int(v.size());++j){
	  if (v[j].type==_DOUBLE_)
	    v[j]=_round(makesequence(v[j],3),contextptr);
	}
	tvi[i]=gen(v,tmp.subtype);
      }
    }
    if (periodic==0)
      return undef;
    if (return_tabvar)
      return tvi;
    if (return_coordonnees)
      return _coordonnees(poi,contextptr);
    gen scale=(gnuplot_xmax-gnuplot_xmin)/5.0;
    gen m=xmin,M=xmax;
    if (is_inf(m))
      m=gnuplot_xmin;
    if (is_inf(M))
      M=gnuplot_xmax;
    if (m!=M)
      scale=(M-m)/3.0;
    if (xmin!=xmax && (periodic==2 || !default_interval) ){
      m=m-0.009*scale; M=M+0.01*scale;
    }
    else {
      m=m-0.973456*scale; M=M+1.018546*scale;
    }
    x=symb_equal(x,symb_interval(m,M));
    vecteur w=makevecteur(f,x);
    for (;s0<s;++s0){
      w.push_back(v[s0]);
    }
    gen p;
    if (plot){
      poi=mergevecteur(poi,gen2vecteur(p));
      if (plot==2)
	return gen(poi,_SEQ__VECT);
      if (plot==1)
	return tvi; // gprintf("%gen",makevecteur(gen(poi,_SEQ__VECT)),1,contextptr);
    }
    return tvi;
  }
  static const char _tabvar_s []="tabvar";
  static define_unary_function_eval (__tabvar,&_tabvar,_tabvar_s);
  define_unary_function_ptr5( at_tabvar ,alias_at_tabvar,&__tabvar,0,true);

  gen _printf(const gen & args,GIAC_CONTEXT){
    if (args.type!=_VECT || args.subtype!=_SEQ__VECT){
      int st=step_infolevel(contextptr);
      step_infolevel(1,contextptr);
      gprintf("%gen",vecteur(1,args),contextptr);
      step_infolevel(st,contextptr);
      return 1;
    }
    vecteur v=*args._VECTptr;
    if (v.empty() || v.front().type!=_STRNG)
      return 0;
    string s=*v.front()._STRNGptr;
    v.erase(v.begin());
    int st=step_infolevel(contextptr);
    step_infolevel(1,contextptr);
    gprintf(s,v,contextptr);
    step_infolevel(st,contextptr);
    return 1;
  }
  static const char _printf_s []="printf";
  static define_unary_function_eval (__printf,&_printf,_printf_s);
  define_unary_function_ptr5( at_printf ,alias_at_printf,&__printf,0,true);
#endif

  int charx2int(char c){
    if (c>='0' && c<='9') return c-'0';
    if (c>='a' && c<='z') return c-'a'+10;
    if (c>='A' && c<='Z') return c-'A'+10;
    return -1;
  }


  gen _range(const gen & args,GIAC_CONTEXT){
    gen g(args);
    if (is_integral(g) && g.type==_INT_ && g.val>=0){
      int n=g.val;
      vecteur v(n);
      for (int i=0;i<n;++i)
	v[i]=i;
      return v;
    }
    if (g.type==_VECT && g._VECTptr->size()>=2){
      gen a=g._VECTptr->front(),b=(*g._VECTptr)[1],c=1;
      if (g._VECTptr->size()==3)
	c=g._VECTptr->back();
      if (is_integral(a) && is_integral(b) && is_integral(c)){
	int A=a.val,B=b.val,C=c.val;
	if ( (A<=B && C>0) || (A>=B && C<0)){
	  int s=std::ceil(double(B-A)/C);
	  vecteur w(s);
	  for (int i=0;i<s;++i)
	    w[i]=A+i*C;
	  return w;
	}
      }
      a=evalf_double(a,1,contextptr);
      b=evalf_double(b,1,contextptr);
      c=evalf_double(c,1,contextptr);
      if (a.type==_DOUBLE_ && b.type==_DOUBLE_ && c.type==_DOUBLE_){
	double A=a._DOUBLE_val,B=b._DOUBLE_val,C=c._DOUBLE_val;
	if ( (A<=B && C>0) || (A>=B && C<0)){
	  int s=std::ceil((B-A)/C);
	  vecteur w(s);
	  for (int i=0;i<s;++i)
	    w[i]=A+i*C;
	  return w;
	}
      }
    }
    return gensizeerr(contextptr);
  }
  static const char _range_s []="range";
  static define_unary_function_eval (__range,&_range,_range_s);
  define_unary_function_ptr5( at_range ,alias_at_range,&__range,0,true);

#ifndef XLIGHT
  string strip(const string & s,const string &chars){
    int ss=int(s.size()),cs=int(chars.size()),i,j;
    for (i=0;i<ss;++i){
      int pos=chars.find(s[i]);
      if (pos<0 || pos>=cs)
	break;
    }
    for (j=ss-1;j>=i;--j){
      int pos=chars.find(s[j]);
      if (pos<0 || pos>=cs)
	break;
    }
    return s.substr(i,j-i+1);
  }
  
  gen _strip(const gen & args,GIAC_CONTEXT){
    if (args.type==_STRNG)
      return string2gen(strip(*args._STRNGptr," "),false);
    if (args.type==_VECT && args._VECTptr->size()==2 && args._VECTptr->front().type==_STRNG && args._VECTptr->back().type==_STRNG)
      return string2gen(strip(*args._VECTptr->front()._STRNGptr,*args._VECTptr->back()._STRNGptr),false);
    return gensizeerr(contextptr);
  }
  static const char _strip_s []="strip";
  static define_unary_function_eval (__strip,&_strip,_strip_s);
  define_unary_function_ptr5( at_strip ,alias_at_strip,&__strip,0,true);

  gen _lower(const gen & args,GIAC_CONTEXT){
    if (ckmatrix(args)){
      vecteur res(*args._VECTptr);
      int l=int(res.size());
      for (int i=0;i<l;++i){
	vecteur ligne=*res[i]._VECTptr;
	int c=int(ligne.size());
	for (int j=i+1;j<c;++j)
	  ligne[j]=0;
	res[i]=ligne;
      }
      return gen(res,_MATRIX__VECT);
    }
    if (args.type!=_STRNG)
      return gensizeerr(contextptr);
    string s(*args._STRNGptr);
    int ss=s.size();
    for (int i=0;i<ss;++i)
      s[i]=tolower(s[i]);
    return string2gen(s,false);
  }
  static const char _lower_s []="lower";
  static define_unary_function_eval (__lower,&_lower,_lower_s);
  define_unary_function_ptr5( at_lower ,alias_at_lower,&__lower,0,true);

  gen _upper(const gen & args,GIAC_CONTEXT){
    if (ckmatrix(args)){
      vecteur res(*args._VECTptr);
      int l=int(res.size());
      for (int i=0;i<l;++i){
	vecteur ligne=*res[i]._VECTptr;
	int c=int(ligne.size());
	for (int j=0;j<i;++j)
	  ligne[j]=0;
	res[i]=ligne;
      }
      return gen(res,_MATRIX__VECT);
    }
    if (args.type!=_STRNG)
      return gensizeerr(contextptr);
    string s(*args._STRNGptr);
    int ss=s.size();
    for (int i=0;i<ss;++i)
      s[i]=toupper(s[i]);
    return string2gen(s,false);
  }
  static const char _upper_s []="upper";
  static define_unary_function_eval (__upper,&_upper,_upper_s);
  define_unary_function_ptr5( at_upper ,alias_at_upper,&__upper,0,true);
#endif

  gen _isinf(const gen & a,GIAC_CONTEXT){
    if (a.type==_STRNG && a.subtype==-1) return  a;
    return change_subtype(is_inf(a),_INT_BOOLEAN);
  }
  static const char _isinf_s []="isinf";
  static define_unary_function_eval (__isinf,&_isinf,_isinf_s);
  define_unary_function_ptr5( at_isinf ,alias_at_isinf,&__isinf,0,true);

  gen _isnan(const gen & a,GIAC_CONTEXT){
    if (a.type==_STRNG && a.subtype==-1) return  a;
    return change_subtype(is_undef(a),_INT_BOOLEAN);
  }
  static const char _isnan_s []="isnan";
  static define_unary_function_eval (__isnan,&_isnan,_isnan_s);
  define_unary_function_ptr5( at_isnan ,alias_at_isnan,&__isnan,0,true);

  gen _isfinite(const gen & a,GIAC_CONTEXT){
    if (a.type==_STRNG && a.subtype==-1) return  a;
    return change_subtype(!is_inf(a) && !is_undef(a),_INT_BOOLEAN);
  }
  static const char _isfinite_s []="isfinite";
  static define_unary_function_eval (__isfinite,&_isfinite,_isfinite_s);
  define_unary_function_ptr5( at_isfinite ,alias_at_isfinite,&__isfinite,0,true);
  
#ifdef WITH_SHEET
  static gen stddevmean(const vecteur & v,int withstddev,int xcol,int freqcol,GIAC_CONTEXT){
    int sv=int(v.size());
    if (xcol>=sv || freqcol>=sv)
      return gendimerr(contextptr);
    if (v[xcol].type!=_VECT || v[freqcol].type!=_VECT)
      return gensizeerr(contextptr);
    vecteur v1(*v[xcol]._VECTptr),v2(*v[freqcol]._VECTptr);
    // if v1 is made of intervals replace by the center of these intervals
    iterateur it=v1.begin(),itend=v1.end();
    for (;it!=itend;++it){
      if (it->is_interval()){
	gen & f=it->_SYMBptr->feuille;
	if (f.type==_VECT && f._VECTptr->size()==2)
	  *it=(f._VECTptr->front()+f._VECTptr->back())/2;
      }
    }
    if (ckmatrix(v1) ^ ckmatrix(v2))
      return gensizeerr(contextptr);
    int n=int(v1.size());
    if (unsigned(n)!=v2.size())
      return gensizeerr(contextptr);
    gen m,m2,s;
    for (int i=0;i<n;++i){
      s += v2[i];
      m += apply(v2[i],v1[i],prod);
      if (withstddev)
	m2 += apply(v2[i],apply(v1[i],v1[i],prod),prod);
    }
    m = apply(m,s,contextptr,rdiv);
    if (withstddev){
      m2 -= apply(s,apply(m,m,prod),prod);
      if (s.type!=_VECT && is_greater(1,s,contextptr) && withstddev==2)
	*logptr(contextptr) << "stddevp called with N<=1, perhaps you are misusing this command with frequencies" << "\n";
      m2=apply(m2,s-(withstddev==2),contextptr,rdiv);
      if (withstddev==3)
	return m2;
      return apply(m2,sqrt,contextptr);
    }
    else
      return m;
  }
  // withstddev=0 (mean), 1 (stddev divided by n), 2 (by n-1), 3 (variance)
  static gen stddevmean(const gen & g,int withstddev,GIAC_CONTEXT){
    vecteur & v=*g._VECTptr;
    int s=int(v.size());
    if (s<2)
      return gensizeerr(contextptr);
    if (v[1].type!=_INT_)
      return stddevmean(v,withstddev,0,1,contextptr);
    if (v[0].type!=_VECT)
      return gensizeerr(contextptr);
    int xcol=v[1].val;
    int freqcol=xcol+1;
    if (s>2 && v[2].type==_INT_)
      freqcol=v[2].val;
    return stddevmean(mtran(*v[0]._VECTptr),withstddev,xcol,freqcol,contextptr);
  }

  gen _mean(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type==_VECT && g.subtype==_SEQ__VECT)
      return stddevmean(g,0,contextptr);
    vecteur v(gen2vecteur(g));
    if (!ckmatrix(v))
      return mean(mtran(vecteur(1,v)),true)[0];
    else
      v=mean(v,true);
    return v;
  }
  static const char _mean_s []="mean";
  static define_unary_function_eval (__mean,&_mean,_mean_s);
  define_unary_function_ptr5( at_mean ,alias_at_mean,&__mean,0,true);

  gen _stddev(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type==_VECT && g.subtype==_SEQ__VECT)
      return stddevmean(g,1,contextptr);
    vecteur v(gen2vecteur(g));
    if (!ckmatrix(v))
      return stddev(mtran(vecteur(1,v)),true,1)[0];
    else
      v=stddev(v,true,1);
    return v;
  }
  static const char _stddev_s []="stddev";
  static define_unary_function_eval (__stddev,&_stddev,_stddev_s);
  define_unary_function_ptr5( at_stddev ,alias_at_stddev,&__stddev,0,true);

  gen _stdDev(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type==_VECT && g.subtype==_SEQ__VECT)
      return stddevmean(g,2,contextptr);
    vecteur v(gen2vecteur(g));
    if (!ckmatrix(v))
      return stddev(mtran(vecteur(1,v)),true,2)[0];
    else
      v=stddev(v,true,2);
    return v;
  }
  static const char _stdDev_s []="stdDev";
  static define_unary_function_eval (__stdDev,&_stdDev,_stdDev_s);
  define_unary_function_ptr5( at_stdDev ,alias_at_stdDev,&__stdDev,0,true);  

  gen _variance(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type==_VECT && g.subtype==_SEQ__VECT)
      return stddevmean(g,3,contextptr);
    vecteur v(gen2vecteur(g));
    if (!ckmatrix(v))
      return stddev(mtran(vecteur(1,v)),true,3)[0];
    else
      v=stddev(v,true,3);
    return v;
  }
  static const char _variance_s []="variance";
static define_unary_function_eval (__variance,&_variance,_variance_s);
  define_unary_function_ptr5( at_variance ,alias_at_variance,&__variance,0,true);

  static gen freq_quantile(const matrice & v,double d,GIAC_CONTEXT){
    if (!ckmatrix(v))
      return undef;
    matrice w;
    if (v.size()==2)
      w=mtran(v);
    else
      w=v;
    if (w.front()._VECTptr->size()!=2)
      return undef;
    // Row Sort (using row 1)
    gen_sort_f(w.begin(),w.end(),first_ascend_sort);
    w=mtran(w);
    // w[0]=data, w[1]=frequencies
    vecteur data=*w[0]._VECTptr;
    vecteur freq=*w[1]._VECTptr;
    gen sigma=d*prodsum(freq,false);
    if (is_undef(sigma)) return sigma;
    int s=int(freq.size());
    gen partial_sum;
    for (int i=0;i<s;++i){
      partial_sum=partial_sum+freq[i];
      if (!is_zero(partial_sum) && is_strictly_greater(partial_sum,sigma,contextptr))
	return data[i];
      if (partial_sum==sigma && i<s)
	return (i==s-1 || (calc_mode(contextptr)!=1 && abs_calc_mode(contextptr)!=38) )?data[i]:(data[i]+data[i+1])/2;
    }
    return undef;
  }
  gen quartile123(const gen &g,double d,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    vecteur v(gen2vecteur(g));
    if (g.type==_VECT && g.subtype==_SEQ__VECT && v.size()==2)
      return freq_quantile(v,d,contextptr);
    if (!ckmatrix(v)){
      if (!is_fully_numeric(evalf(v,1,contextptr))){
	islesscomplexthanf_sort(v.begin(),v.end());
	return v[int(std::ceil(v.size()*d))-1]; // v[(v.size()-1)/4];
      }
      matrice mt=mtran(ascsort(mtran(vecteur(1,v)),true));
      if (d==0.5 && !v.empty() && !(v.size()%2))
	return (mt[v.size()/2][0]+mt[v.size()/2-1][0])/2;
      return mt[int(std::ceil(v.size()*d))-1][0];
    }
    else
      v=ascsort(v,true);
    v=mtran(v);
    if (d==0.5 && !v.empty() && !(v.size()%2))
      return (v[v.size()/2]+v[v.size()/2-1])/2;
    return v[int(std::ceil(v.size()*d))-1]; // v[(v.size()-1)/4];
  }
  gen _median(const gen & g,GIAC_CONTEXT){
    return quartile123(g,0.5,contextptr);
  }
  static const char _median_s []="median";
  static define_unary_function_eval(unary_median,&_median,_median_s);
  define_unary_function_ptr5( at_median ,alias_at_median,&unary_median,0,true);

  gen _quartile1(const gen & g,GIAC_CONTEXT){
    return quartile123(g,0.25,contextptr);
  }
  static const char _quartile1_s []="quartile1";
  static define_unary_function_eval(unary_quartile1,&_quartile1,_quartile1_s);
  define_unary_function_ptr5( at_quartile1 ,alias_at_quartile1,&unary_quartile1,0,true);

  gen _quartile3(const gen & g,GIAC_CONTEXT){
    return quartile123(g,0.75,contextptr);
  }
  static const char _quartile3_s []="quartile3";
  static define_unary_function_eval(unary_quartile3,&_quartile3,_quartile3_s);
  define_unary_function_ptr5( at_quartile3 ,alias_at_quartile3,&unary_quartile3,0,true);

  gen _quartiles(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    vecteur v(gen2vecteur(g));
    if (g.type==_VECT && g.subtype==_SEQ__VECT && v.size()==2)
      return makevecteur(freq_quantile(v,0.0,contextptr),freq_quantile(v,0.25,contextptr),freq_quantile(v,0.5,contextptr),freq_quantile(v,0.75,contextptr),freq_quantile(v,1.0,contextptr));
    if (!ckmatrix(v)){
      if (!is_fully_numeric(evalf(v,1,contextptr))){
	islesscomplexthanf_sort(v.begin(),v.end());
	int s=int(v.size());
	return makevecteur(v[0],v[int(std::ceil(s/4.))-1],v[int(std::ceil(s/2.))-1],v[int(std::ceil(3*s/4.))-1],v[s-1]);
      }
      v=ascsort(mtran(vecteur(1,v)),true);
    }
    else
      v=ascsort(v,true);
    v=mtran(v);
    int s=int(v.size());
    if (s==0)
      return gensizeerr(contextptr);
    return makevecteur(v[0],v[int(std::ceil(s/4.))-1],v[int(std::ceil(s/2.))-1],v[int(std::ceil(3*s/4.))-1],v[s-1]);
  }
  static const char _quartiles_s []="quartiles";
  static define_unary_function_eval(unary_quartiles,&_quartiles,_quartiles_s);
  define_unary_function_ptr5( at_quartiles ,alias_at_quartiles,&unary_quartiles,0,true);

  gen _moustache(const gen & g_orig,GIAC_CONTEXT){
    if ( g_orig.type==_STRNG && g_orig.subtype==-1) return  g_orig;
    vecteur attributs(1,FL_BLACK);
    gen g(g_orig);
    bool horizontal=true,myaxes=true;
    double ymin=global_window_ymin;
    double ymax=global_window_ymax;
    if (g.type==_VECT && g.subtype==_SEQ__VECT){
      vecteur v(*g._VECTptr);
      if (!v.empty() && v.back().is_integer() && v.back().subtype==_INT_PLOT && v.back().val==_AXES) {
        myaxes=false;
        v.pop_back();
      }
      int s=read_attributs(v,attributs,contextptr);
      if (s>1){
	gen tmp=v[s-1];
	if (is_equal(tmp)){
	  if (tmp._SYMBptr->feuille[0]==x__IDNT_e)
	    horizontal=false;
	  tmp=tmp._SYMBptr->feuille[1];
	}
	if (tmp.is_symb_of_sommet(at_interval)){
	  ymin=evalf_double(tmp._SYMBptr->feuille[0],1,contextptr)._DOUBLE_val;
	  ymax=evalf_double(tmp._SYMBptr->feuille[1],1,contextptr)._DOUBLE_val;
	  --s;
	}
      }
      if (s==1)
	g=v.front();
      else
	g=gen(vecteur(v.begin(),v.begin()+s),g_orig.subtype);
    }
    gen tmpp=_quartiles(g,contextptr);
    if (tmpp.type!=_VECT)
      return tmpp;
    vecteur v0(*tmpp._VECTptr),v;
    if (!ckmatrix(v0))
      v=vecteur(1,v0);
    else
      v=mtran(v0);
    // _Pictsize(0);
    int s=int(v.size());
    vecteur res;
    double xmin=gnuplot_xmin,xmax=gnuplot_xmax;
    gen tmpx=_min(v0[0],contextptr),tmpxx=_max(v0[4],contextptr);
    if (tmpx.type==_DOUBLE_)
      xmin=tmpx._DOUBLE_val;
    if (tmpxx.type==_DOUBLE_)
      xmax=tmpxx._DOUBLE_val;
    vecteur attr=gen2vecteur(attributs[0]);
    vecteur legendes(1,string2gen("",false));
    if (attributs.size()>=2)
      legendes=gen2vecteur(attributs[1]);
    int ls=int(legendes.size());
    vecteur affichages(gen2vecteur(attributs[0]));
    int as=int(affichages.size()),attrn=attr.size();
    if (horizontal){
      if (myaxes) res.push_back(symb_equal(change_subtype(_AXES,_INT_PLOT),4));
      double y_scale=(ymax-ymin)/(4*s);
      for (int i=0;i<s;++i){
	double y_up=ymax-(4*i+1)*y_scale;
	double y_middle=ymax-(4*i+2)*y_scale;
	double y_down=ymax-(4*i+3)*y_scale;
	vecteur current=gen2vecteur(v[i]);
	if (current.size()!=5)
	  continue;
	// trait min -> 1er quartile
	res.push_back(_segment(makesequence(current[0]+y_middle*cst_i,current[1]+y_middle*cst_i),contextptr));
	// rectangle
        gen col=change_subtype(attrn>0?attr[i%attrn]:52,_INT_COLOR);
        if (col!=FL_BLACK)
	  res.push_back(pnt_attrib(gen(makevecteur(current[3]+y_down*cst_i,current[1]+y_down*cst_i,current[1]+y_up*cst_i,current[3]+y_up*cst_i,current[3]+y_down*cst_i),_GROUP__VECT),vecteur(1,col+_FILL_POLYGON),contextptr));
        res.push_back(pnt_attrib(gen(makevecteur(current[3]+y_down*cst_i,current[1]+y_down*cst_i,current[1]+y_up*cst_i,current[3]+y_up*cst_i,current[3]+y_down*cst_i),_GROUP__VECT),vecteur(1,FL_BLACK),contextptr));
	// mediane
	res.push_back(_segment(makesequence(current[2]+y_down*cst_i,current[2]+y_up*cst_i),contextptr));
	// trait 3eme quartile -> fin
	res.push_back(_segment(makesequence(current[3]+y_middle*cst_i,current[4]+y_middle*cst_i),contextptr));
        if (i<ls)
          res.push_back(_legende(makesequence(current[4]+y_middle*cst_i,legendes[i]),contextptr));
      }
    } else { // vertical picture
      if (myaxes) res.push_back(symb_equal(change_subtype(_AXES,_INT_PLOT),2));
      swapdouble(xmin,ymin);
      swapdouble(xmax,ymax);
      double x_scale=(xmax-xmin)/(4*s);
      for (int i=0;i<s;++i){
	double x_up=xmin+(4*i+1)*x_scale;
	double x_middle=xmin+(4*i+2)*x_scale;
	double x_down=xmin+(4*i+3)*x_scale;
	vecteur current=gen2vecteur(v[i]);
	if (current.size()!=5)
	  continue;
	// trait min -> 1er quartile
	res.push_back(_segment(makesequence(x_middle+current[0]*cst_i,current[1]*cst_i+x_middle),contextptr));
	// rectangle
        gen col=change_subtype(attrn>0?attr[i%attrn]:52,_INT_COLOR);
        if (col!=FL_BLACK)
          res.push_back(pnt_attrib(gen(makevecteur(current[3]*cst_i+x_down,current[1]*cst_i+x_down,current[1]*cst_i+x_up,current[3]*cst_i+x_up,current[3]*cst_i+x_down),_GROUP__VECT),vecteur(1,col+_FILL_POLYGON),contextptr));
        res.push_back(pnt_attrib(gen(makevecteur(current[3]*cst_i+x_down,current[1]*cst_i+x_down,current[1]*cst_i+x_up,current[3]*cst_i+x_up,current[3]*cst_i+x_down),_GROUP__VECT),vecteur(1,FL_BLACK),contextptr));
	// mediane
	res.push_back(_segment(makesequence(current[2]*cst_i+x_down,current[2]*cst_i+x_up),contextptr));
	// trait 3eme quartile -> fin
	res.push_back(_segment(makesequence(current[3]*cst_i+x_middle,current[4]*cst_i+x_middle),contextptr));
        if (i<ls)
          res.push_back(_legende(makesequence(current[4]*cst_i+x_middle,legendes[i],change_subtype(_QUADRANT2,_INT_COLOR)),contextptr));
      }
    }
    return gen(res,_SEQ__VECT);
  }
  static const char _moustache_s []="moustache";
  static define_unary_function_eval(unary_moustache,&_moustache,_moustache_s);
  define_unary_function_ptr5( at_moustache ,alias_at_moustache,&unary_moustache,0,true);

  static const char _boxwhisker_s []="boxwhisker";
  static define_unary_function_eval(unary_boxwhisker,&_moustache,_boxwhisker_s);
  define_unary_function_ptr5( at_boxwhisker ,alias_at_boxwhisker,&unary_boxwhisker,0,true);

  static vector<double> prepare_effectifs(const vecteur & v,GIAC_CONTEXT){
    if (v.empty())
      return vector<double>(0);
    vecteur w;
    if (ckmatrix(v)){
      int s=int(v.front()._VECTptr->size());
      if (s==1)
	w=*evalf_double(mtran(v)[0],1,contextptr)._VECTptr;
      else
	return vector<double>(0);
    }
    else
      w=*evalf_double(v,1,contextptr)._VECTptr;
    // vector will be sorted keeping only DOUBLE data
    int s=int(w.size());
    vector<double> w1;
    w1.reserve(s);
    for (int i=0;i<s;++i){
      if (w[i].type==_DOUBLE_)
	w1.push_back(w[i]._DOUBLE_val);
    }
    sort(w1.begin(),w1.end());
    s=int(w1.size());
    if (!s)
      return vector<double>(0);
    return w1;
  }

  static vecteur centres2intervalles(const vecteur & centres,double class_min,bool with_class_min,GIAC_CONTEXT){
    if (centres.size()<2)
      return vecteur(1,gensizeerr(contextptr));
    double d0=evalf_double(centres[0],1,contextptr)._DOUBLE_val,d1=evalf_double(centres[1],1,contextptr)._DOUBLE_val;
    double debut=class_min;
    if (!with_class_min || debut<=-1e38)
      debut=d0+(d0-d1)/2;
    vecteur res;
    const_iterateur it=centres.begin(),itend=centres.end();
    res.reserve(itend-it);
    for (;it!=itend;++it){
      gen g=evalf_double(*it,1,contextptr);
      if (g.type!=_DOUBLE_)
	return vecteur(1,gensizeerr(contextptr));
      double milieu=g._DOUBLE_val;
      double fin=milieu+(milieu-debut);
      if (it+1!=itend){
	g=evalf_double(*(it+1),1,contextptr);
	if (g.type!=_DOUBLE_)
	  return vecteur(1,gensizeerr(contextptr));
	fin=(milieu+g._DOUBLE_val)/2;
      }
      if (fin<=debut)
	return vecteur(1,gensizeerr(contextptr));
      res.push_back(symb_interval(debut,fin));
      debut=fin;
    }
    return res;
  }

  static gen histogram(const vecteur & v,double class_minimum,double class_size,const vecteur & attributs,GIAC_CONTEXT){
    if (class_size<=0){
      // find class_minimum and class_size from data and number of classes
      int nc=int(class_minimum); // arg passed is the number of classes
      vector<double> w=prepare_effectifs(v,contextptr);
      if (w.size()<2)
	return gensizeerr(contextptr);
      class_minimum=w.front();
      class_size=((w.back()-w.front())*(1+1e-12))/nc;
    }
    if (ckmatrix(v) && !v.empty() && v.front()._VECTptr->size()==2){
      // matrix format is 2 columns 1st column=interval, 2nd column=frequency
      // OR value/frequencies
      // get total of population
      const_iterateur it=v.begin(),itend=v.end();
      double n=0;
      for (;it!=itend;++it)
	n += evalf_double(it->_VECTptr->back(),1,contextptr)._DOUBLE_val;
      // get surface
      gen g=v.front()._VECTptr->front();
      if (g.is_interval()){
	g=g._SYMBptr->feuille;
	if (g.type!=_VECT || g._VECTptr->size()!=2)
	  return gentypeerr(contextptr);
	g=evalf_double(g._VECTptr->front(),1,contextptr);
      }
      else
	g=g-class_size/2;
      gen h=(itend-1)->_VECTptr->front();
      if (h.is_interval()){
	h=h._SYMBptr->feuille;
	if (h.type!=_VECT || h._VECTptr->size()!=2)
	  return gentypeerr(contextptr);
	h=evalf_double(h._VECTptr->back(),1,contextptr);
      }
      else
	h=h+class_size/2;
      if (g.type!=_DOUBLE_ || h.type!=_DOUBLE_ || g._DOUBLE_val>=h._DOUBLE_val)
	return gensizeerr(contextptr);
      double inf,sup; // delta=h._DOUBLE_val-g._DOUBLE_val;
      it=v.begin();
      //  int nclass=itend-it;
      vecteur res;
      for (;it!=itend;++it){
	gen current=it->_VECTptr->front();
	if (current.is_interval()){
	  if (!chk_double_interval(current,inf,sup,contextptr))
	    return gentypeerr(contextptr);
	}
	else {
	  gen tmp=evalf_double(current,1,contextptr);
	  if (tmp.type!=_DOUBLE_)
	    return gentypeerr(contextptr);
	  inf = tmp._DOUBLE_val -class_size/2;
	  sup = tmp._DOUBLE_val + class_size/2;
	}
	double height=1/(sup-inf);
	height=height*evalf_double(it->_VECTptr->back(),1,contextptr)._DOUBLE_val/n;
	gen mini(inf,height),maxi(sup,height);
	gen rectan(makevecteur(inf,sup,maxi,mini,inf),_LINE__VECT);
	res.push_back(pnt_attrib(rectan,attributs,contextptr));
	// res.push_back(_segment(makevecteur(inf,mini),contextptr));
	// res.push_back(_segment(makevecteur(mini,maxi),contextptr));
	// res.push_back(_segment(makevecteur(maxi,sup),contextptr));	    
      }
      return res;
    }
    vector<double>  w1=prepare_effectifs(v,contextptr);
    int s=int(w1.size());
    if (!s)
      return gendimerr(contextptr);
    // class_min + k*class_size <= mini hence k
    double kbegin=std::floor((w1.front()-class_minimum)/class_size);
    double kend=std::floor((w1.back()-class_minimum)/class_size);
    vector<double>::const_iterator it=w1.begin(),itend=w1.end();
    vecteur res;
    for (;kbegin<=kend;++kbegin){
      // count in this class
      double min_class=kbegin*class_size+class_minimum;
      double max_class=min_class+class_size;
      double effectif=0;
      for (;it!=itend;++it,++effectif){
	if (*it>=max_class)
	  break;
      }
      effectif /= s*class_size; // height of the class
      gen ming=min_class+gen(0.0,effectif);
      gen maxg=max_class+gen(0.0,effectif);
      gen rectan(makevecteur(min_class,max_class,maxg,ming,min_class),_LINE__VECT);
      res.push_back(pnt_attrib(rectan,attributs,contextptr));
      // res.push_back(_segment(makevecteur(min_class,ming),contextptr));
      // res.push_back(_segment(makevecteur(ming,maxg),contextptr));
      // res.push_back(_segment(makevecteur(maxg,max_class),contextptr));
    }
    return res; // gen(res,_SEQ__VECT);
  }
  gen _histogram(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type==_SYMB && is_distribution(g)){
      vecteur v(gen2vecteur(g._SYMBptr->feuille));
      v.insert(v.begin(),g._SYMBptr->sommet);
      return _histogram(gen(v,_SEQ__VECT),contextptr);
    }
    if (g.type!=_VECT)
      return gensizeerr(contextptr);
    vecteur args;
    if (g.subtype==_SEQ__VECT)
      args=*g._VECTptr;
    vecteur attributs(1,FL_BLACK);
    int s=read_attributs(args,attributs,contextptr);
    args=vecteur(args.begin(),args.begin()+s);
    if (s>=2){
      if (args[0].type!=_VECT)
	return gensizeerr(contextptr);
      vecteur data=*args[0]._VECTptr;
      if (data.empty())
	return gensizeerr(contextptr);
      if (data.front().type==_VECT && data.front()._VECTptr->size()==1 && ckmatrix(data))
	data=*mtran(data).front()._VECTptr;
      gen arg1=evalf_double(args[1],1,contextptr);
      if (ckmatrix(data)&&arg1.type==_DOUBLE_){ // [ [center, effectif] ... ], min
	data=mtran(data); // 1st line = all centers
	if (data.size()!=2)
	  return gensizeerr(contextptr);
	data[0]=centres2intervalles(*data[0]._VECTptr,arg1._DOUBLE_val,true,contextptr);
	if (is_undef(data[0]))
	  return gensizeerr(contextptr);
	data=mtran(data);
	gen g=data[0][0];
	if (g.is_interval() && g._SYMBptr->feuille.type==_VECT && g._SYMBptr->feuille._VECTptr->size()==2){
	  gen g1=g._SYMBptr->feuille._VECTptr->front();
	  g1=evalf_double(g1,1,contextptr);
	  gen g2=g._SYMBptr->feuille._VECTptr->back();
	  g2=evalf_double(g2,1,contextptr);
	  if (g1.type==_DOUBLE_ && g2.type==_DOUBLE_)
	    return histogram(data,g1._DOUBLE_val,(g2-g1)._DOUBLE_val,attributs,contextptr);
	}
	return histogram(data,0.0,0.0,attributs,contextptr);
      }
      if (s==3){
	gen arg2=evalf_double(args[2],1,contextptr);
	if (arg1.type==_DOUBLE_ && arg2.type==_DOUBLE_)
	  return histogram(data,arg1._DOUBLE_val,arg2._DOUBLE_val,attributs,contextptr);
      }
      if (s==2 && is_integral(arg1) && arg1.type==_INT_ && arg1.val>0)
	return histogram(data,arg1.val,0.0,attributs,contextptr);
      if (s==2 && args[1].type==_VECT)
	return _histogram(makesequence(mtran(args),-1.1e38),contextptr);
      return gensizeerr(contextptr);
    }
    if (s==1 && args.front().type==_VECT)
      args=*args.front()._VECTptr;
    else
      args=gen2vecteur(g);
    if (ckmatrix(args)){
      gen tmp=args[0];
      if (tmp._VECTptr->size()==2 && !tmp._VECTptr->front().is_interval()){
	vecteur data=mtran(args); // 1st line = all centers
	if (data.size()!=2)
	  return gensizeerr(contextptr);
	data[0]=centres2intervalles(*data[0]._VECTptr,0,false,contextptr);
	if (is_undef(data[0]))
	  return gensizeerr(contextptr);
	data=mtran(data);
	return histogram(data,0.0,1e-14,attributs,contextptr);
      }
    }
    return histogram(args,class_minimum,class_size,attributs,contextptr);
  }
  static const char _histogram_s []="histogram";
  static define_unary_function_eval (__histogram,&_histogram,_histogram_s);
  define_unary_function_ptr5( at_histogram ,alias_at_histogram,&__histogram,0,true);

  vecteur genpoint2vecteur(const gen & g,GIAC_CONTEXT){
    vecteur v(gen2vecteur(g));
    for (unsigned i=0;i<v.size();++i){
      gen & tmp = v[i];
      if (tmp.is_symb_of_sommet(at_pnt))
	tmp=complex2vecteur(remove_at_pnt(tmp),contextptr);
    }
    return v;
  }

  static vecteur covariance_correlation(const gen & g,const gen & u1,const gen & u2,int xcol,int ycol,int freqcol,GIAC_CONTEXT){
    if (is_undef(g))
      return makevecteur(g,g);
    vecteur v(genpoint2vecteur(g,contextptr));
    if (!ckmatrix(v) || v.empty() || v.front()._VECTptr->size()<2)
      return makevecteur(undef,undef);
    gen sigmax,sigmay,sigmaxy,sigmax2,sigmay2,tmpx,tmpy,n,freq;
    if (freqcol<-1){
      // g is interpreted as a double-entry table with 1st col = x-values
      // 1st line=y-values, line/col is a frequency for this value of x/y
      int r,c;
      mdims(v,r,c);
      if (r<2 || c<2)
	return makevecteur(gendimerr(contextptr),gendimerr(contextptr));
      vecteur & vy=*v[0]._VECTptr;
      for (int i=1;i<r;++i){
	vecteur & w=*v[i]._VECTptr;
	gen & currentx=w[0];
	for (int j=1;j<c;++j){
	  gen & currenty=vy[j];
	  freq=w[j];
	  n += freq;
	  gen cf(currentx*freq);
	  sigmax += cf;
	  sigmax2 += currentx*cf;
	  cf=currenty*freq;
	  sigmay += cf;
	  sigmay2 += currenty*cf;
	  sigmaxy += currentx*cf;
	}
      }
    }
    else {
      const_iterateur it=v.begin(),itend=v.end();
      int s=int(it->_VECTptr->size());
      if (xcol>=s || ycol>=s || freqcol >=s)
	return makevecteur(gendimerr(contextptr),gendimerr(contextptr));
      for (;it!=itend;++it){
	vecteur & w=*it->_VECTptr;
	if (u1.type==_FUNC)
	  tmpx=u1(w[xcol],contextptr);
	else
	  tmpx=w[xcol];
	if (u2.type==_FUNC)
	  tmpy=u2(w[ycol],contextptr);
	else
	  tmpy=w[ycol];
	if (freqcol>=0)
	  freq=w[freqcol];
	else
	  freq=plus_one;
	n += freq;
	gen cf(tmpx*freq);
	sigmax += cf;
	sigmax2 += tmpx*cf;
	cf=tmpy*freq;
	sigmay += cf;
	sigmay2 += tmpy*cf;
	sigmaxy += tmpx*cf;
      }
    }
    gen tmp(n*sigmaxy-sigmax*sigmay);
    gen covariance=tmp/(n*n);
    gen correlation=tmp/sqrt((n*sigmax2-sigmax*sigmax)*(n*sigmay2-sigmay*sigmay),contextptr);
    return makevecteur(covariance,correlation);
  }
  
  static void find_xyfreq(const gen & g,gen & gv,int & xcol,int & ycol,int &freqcol,GIAC_CONTEXT){
    xcol=0;
    ycol=1;
    freqcol=-1;
    if (g.type==_VECT && g.subtype==_SEQ__VECT && !g._VECTptr->empty()){
      vecteur v=*g._VECTptr;
      if (v[0].type!=_VECT){
	gv=gensizeerr(contextptr);
	return;
      }
      int s=int(v.size());
      if (s==3 && v[1].type==_VECT){
	if (!ckmatrix(v[2]))
	  v[2]=_diag(v[2],contextptr);
	int n,c;
	mdims(*v[2]._VECTptr,n,c);
	if (unsigned(n)==v[0]._VECTptr->size() && unsigned(c)==v[1]._VECTptr->size()){
	  vecteur v0(*v[1]._VECTptr);
	  v0.insert(v0.begin(),zero);
	  matrice m(mtran(*v[2]._VECTptr));
	  m.insert(m.begin(),v[0]);
	  m=mtran(m);
	  m.insert(m.begin(),v0);
	  gv=m;
	  freqcol=-2;
	  return;
	}
      }
      if (s>1) {
	if (v[1].type==_INT_){
	  xcol=v[1].val;
	  if (xcol<0)
	    freqcol=-2;
	}
	else {
	  if (!ckmatrix(v))
	    gv=gensizeerr(contextptr);
	  else
	    gv=mtran(v);
	  return;
	}
      }
      if (s>2 && v[2].type==_INT_)
	ycol=v[2].val;
      if (s>3 && v[3].type==_INT_)
	freqcol=v[3].val;
      gv=v[0];
    }
    else {
      gv=genpoint2vecteur(g,contextptr);
      if (!ckmatrix(gv) || gv._VECTptr->empty()){
	gv=gensizeerr(contextptr);
	return;
      }
      if (gv._VECTptr->front()._VECTptr->size()>2)
	freqcol=2;
      if (gv._VECTptr->front()._VECTptr->front().type==_STRNG)
	freqcol=-2;
    }
  }
  gen _covariance(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    int xcol,ycol,freqcol;
    gen gv;
    find_xyfreq(g,gv,xcol,ycol,freqcol,contextptr);
    if (is_undef(gv)) return gv;
    return covariance_correlation(gv,zero,zero,xcol,ycol,freqcol,contextptr)[0];
  }
  static const char _covariance_s []="covariance";
  static define_unary_function_eval (__covariance,&_covariance,_covariance_s);
  define_unary_function_ptr5( at_covariance ,alias_at_covariance,&__covariance,0,true);

  gen _correlation(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    int xcol,ycol,freqcol;
    gen gv;
    find_xyfreq(g,gv,xcol,ycol,freqcol,contextptr);
    if (is_undef(gv)) return gv;
    return covariance_correlation(gv,zero,zero,xcol,ycol,freqcol,contextptr)[1];
  }
  static const char _correlation_s []="correlation";
  static define_unary_function_eval (__correlation,&_correlation,_correlation_s);
  define_unary_function_ptr5( at_correlation ,alias_at_correlation,&__correlation,0,true);

  gen _interval2center(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type==_VECT)
      return apply(g,_interval2center,contextptr);
    if (g.is_interval()){
      gen & tmp=g._SYMBptr->feuille;
      if (tmp.type!=_VECT || tmp._VECTptr->size()!=2)
	return gensizeerr(contextptr);
      vecteur & v=*tmp._VECTptr;
      return (v.front()+v.back())/2;
    }
    return g;
  }
  static const char _interval2center_s []="interval2center";
  static define_unary_function_eval (__interval2center,&_interval2center,_interval2center_s);
  define_unary_function_ptr5( at_interval2center ,alias_at_interval2center,&__interval2center,0,true);

  gen function_regression(const gen & g,const gen & u1,const gen & u2,gen & a,gen &b,double & xmin,double & xmax,gen & correl2,GIAC_CONTEXT){
    gen gv,freq;
    int xcol,ycol,freqcol;
    xmin=1e300;
    xmax=-xmin;
    find_xyfreq(g,gv,xcol,ycol,freqcol,contextptr);
    if (!ckmatrix(gv))
      return gensizeerr(contextptr);
    vecteur & v = *gv._VECTptr;
    gen n;
    gen sigmax,sigmay,sigmaxy,sigmax2,sigmay2,tmpx,tmpy;
    if (freqcol<-1){
      int r,c;
      mdims(v,r,c);
      if (r<2 || c<2)
	return gendimerr(contextptr);
      vecteur & vy=*v[0]._VECTptr;
      gen currentx,currenty;
      for (int i=1;i<r;++i){
	vecteur & w=*v[i]._VECTptr;
	gen tmpg=evalf_double(w[0],1,contextptr);
	if (tmpg.type==_DOUBLE_){
	  double tmp=tmpg._DOUBLE_val;
	  if (tmp<xmin)
	    xmin=tmp;
	  if (tmp>xmax)
	    xmax=tmp;
	}
	if (u1.type==_FUNC)
	  currentx=u1(w[0],contextptr);
	else
	  currentx=w[0];
	for (int j=1;j<c;++j){
	  if (u2.type==_FUNC)
	    currenty=u2(vy[j],contextptr);
	  else
	    currenty=vy[j];
	  currenty=_interval2center(currenty,contextptr);
	  if (is_undef(currenty))
	    return currenty;
	  freq=w[j];
	  n += freq;
	  gen cf(currentx*freq);
	  sigmax += cf;
	  sigmax2 += currentx*cf;
	  cf=currenty*freq;
	  sigmay += cf;
	  sigmay2 += currenty*cf;
	  sigmaxy += currentx*cf;
	}
      }
    }
    else {
      const_iterateur it=v.begin(),itend=v.end();
      for (;it!=itend;++it){
	vecteur & w=*it->_VECTptr;
	gen tmpg=evalf_double(w[xcol],1,contextptr);
	if (tmpg.type==_DOUBLE_){
	  double tmp=tmpg._DOUBLE_val;
	  if (tmp<xmin)
	    xmin=tmp;
	  if (tmp>xmax)
	    xmax=tmp;
	}
	if (u1.type==_FUNC)
	  tmpx=u1(w[xcol],contextptr);
	else
	  tmpx=w[xcol];
	tmpx=_interval2center(tmpx,contextptr);
	if (is_undef(tmpx))
	  return tmpx;
	if (u2.type==_FUNC)
	  tmpy=u2(w[ycol],contextptr);
	else
	  tmpy=w[ycol];
	tmpy=_interval2center(tmpy,contextptr);
	if (is_undef(tmpy))
	  return tmpy;
	if (freqcol<0)
	  freq=plus_one;
	else
	  freq=w[freqcol];
	n += freq;
	gen cf(tmpx*freq);
	sigmax += cf;
	sigmax2 += tmpx*cf;
	cf=tmpy*freq;
	sigmay += cf;
	sigmay2 += tmpy*cf;
	sigmaxy += tmpx*cf;
      }
    }
    gen tmp(n*sigmaxy-sigmax*sigmay),tmp2(n*sigmax2-sigmax*sigmax);
    a=tmp/tmp2;
    b=(sigmay-a*sigmax)/n;
    correl2=(tmp*tmp)/tmp2/(n*sigmay2-sigmay*sigmay);
    return makevecteur(sigmax,sigmay,n,sigmax2,sigmay2);
    // cerr << sigmax << " "<< sigmay << " " << sigmaxy << " " << n << " " << sigmax2 << " " << sigmay2 << "\n";
  }

  static gen function_regression(const gen & g,const gen & u1,const gen & u2,GIAC_CONTEXT){
    gen a,b,correl2;
    double xmin,xmax;
    gen errcode=function_regression(g,u1,u2,a,b,xmin,xmax,correl2,contextptr);
    if (is_undef(errcode)) return errcode;
    return makesequence(a,b);
  }

  gen _linear_regression(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    return function_regression(g,zero,zero,contextptr);
  }
  static const char _linear_regression_s []="linear_regression";
  static define_unary_function_eval (__linear_regression,&_linear_regression,_linear_regression_s);
  define_unary_function_ptr5( at_linear_regression ,alias_at_linear_regression,&__linear_regression,0,true);


  gen _exponential_regression(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    return exp(function_regression(g,zero,at_ln,contextptr),contextptr);
  }
  static const char _exponential_regression_s []="exponential_regression";
static define_unary_function_eval (__exponential_regression,&_exponential_regression,_exponential_regression_s);
  define_unary_function_ptr5( at_exponential_regression ,alias_at_exponential_regression,&__exponential_regression,0,true);

  gen _logarithmic_regression(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    return function_regression(g,at_ln,zero,contextptr);
  }
  static const char _logarithmic_regression_s []="logarithmic_regression";
  static define_unary_function_eval (__logarithmic_regression,&_logarithmic_regression,_logarithmic_regression_s);
  define_unary_function_ptr5( at_logarithmic_regression ,alias_at_logarithmic_regression,&__logarithmic_regression,0,true);

  gen _power_regression(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    gen res= function_regression(evalf(g,1,contextptr),at_ln,at_ln,contextptr);
    if (res.type==_VECT && res._VECTptr->size()==2){
      vecteur v(*res._VECTptr);
      v[1]=exp(v[1],contextptr);
      return gen(v,_SEQ__VECT);
    }
    return res;
  }
  static const char _power_regression_s []="power_regression";
  static define_unary_function_eval (__power_regression,&_power_regression,_power_regression_s);
  define_unary_function_ptr5( at_power_regression ,alias_at_power_regression,&__power_regression,0,true);

  gen regression_plot_attributs(const gen & g,vecteur & attributs,bool & eq,bool & r,GIAC_CONTEXT){
    gen res=g;
    r=false; eq=false;
    if (g.type==_VECT && g.subtype==_SEQ__VECT){
      int n=read_attributs(*g._VECTptr,attributs,contextptr);
      vecteur v=vecteur(g._VECTptr->begin(),g._VECTptr->begin()+n);
      vecteur & w=*g._VECTptr;
      int ws=int(w.size());
      for (int i=0;i<ws;++i){
	if (w[i]==at_equation){
	  eq=true;
	  if (i<n){
	    v.erase(v.begin()+i);
	    --n;
	    --i;
	  }
	}
	if (w[i]==at_correlation){
	  r=true;
	  if (i<n){
	    v.erase(v.begin()+i);
	    --n;
	    --i;
	  }
	}
      }
      if (n==1)
	res=g._VECTptr->front();
      else
	res=gen(v,_SEQ__VECT);
    }
    else
      attributs=vecteur(1,FL_BLACK);
    if (res.type==_VECT && res._VECTptr->front().type!=_VECT){
      vecteur v=*res._VECTptr;
      int s=v.size();
      if (s%2==0){
        vecteur v1(s/2),v2(s/2);
        for (int i=0;i<s/2;i++){
          v1[i]=v[2*i];
          v2[i]=v[2*i+1];
        }
        res=makesequence(v1,v2);
      }
    }
    return res;
  }

  gen _linear_regression_plot(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    gen a,b,correl2;
    double xmin,xmax;
    vecteur attributs;
    bool eq,r;
    gen G=regression_plot_attributs(g,attributs,eq,r,contextptr);
    // dbg_printf("linear regression plot %s\n",G.print().c_str());
    gen errcode=function_regression(G,zero,zero,a,b,xmin,xmax,correl2,contextptr);
    if (is_undef(errcode)) return errcode;
    xmax += (xmax-xmin); 
    gen ad(evalf_double(a,1,contextptr)),bd(evalf_double(b,1,contextptr)),cd(evalf_double(correl2,1,contextptr));
    if (ad.type==_DOUBLE_ && bd.type==_DOUBLE_ && cd.type==_DOUBLE_){
      string eqs="y="+print_DOUBLE_(ad._DOUBLE_val,3)+"*x+"+print_DOUBLE_(bd._DOUBLE_val,3);
      string R2s=" , R2="+print_DOUBLE_(cd._DOUBLE_val,3);
      *logptr(contextptr) << eqs << R2s << "\n";
      string s;
      if (eq)
	s += eqs;
      if (r)
	s += R2s;
      attributs.push_back(string2gen(s,false));
    }
    return put_attributs(_droite(makesequence(b*cst_i,1+(b+a)*cst_i),contextptr),attributs,contextptr);
  }
  static const char _linear_regression_plot_s []="linear_regression_plot";
  static define_unary_function_eval (__linear_regression_plot,&_linear_regression_plot,_linear_regression_plot_s);
  define_unary_function_ptr5( at_linear_regression_plot ,alias_at_linear_regression_plot,&__linear_regression_plot,0,true);

  gen _exponential_regression_plot(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    gen a,b,correl2;
    double xmin,xmax;
    vecteur attributs;
    bool eq,r;
    gen G=regression_plot_attributs(g,attributs,eq,r,contextptr);
    gen errcode=function_regression(G,zero,at_ln,a,b,xmin,xmax,correl2,contextptr);
    if (is_undef(errcode)) return errcode;
    gen ad(evalf_double(a,1,contextptr)),bd(evalf_double(b,1,contextptr)),cd(evalf_double(correl2,1,contextptr));
    if (ad.type==_DOUBLE_ && bd.type==_DOUBLE_ && cd.type==_DOUBLE_){
      string eqs="y="+print_DOUBLE_(std::exp(ad._DOUBLE_val),3)+"^x*"+print_DOUBLE_(std::exp(bd._DOUBLE_val),3);
      string R2s=" , R2="+print_DOUBLE_(cd._DOUBLE_val,3);
      *logptr(contextptr) << eqs << R2s << "\n";
      string s;
      if (eq)
	s += eqs;
      if (r)
	s += R2s;
      attributs.push_back(string2gen(s,false));
    }
    return put_attributs(_plotfunc(makesequence(evalf(exp(b,contextptr),1,contextptr)*exp(a*vx_var(),contextptr),symb_equal(vx_var(),symb_interval(xmin,xmax))),contextptr),attributs,contextptr);
  }
  static const char _exponential_regression_plot_s []="exponential_regression_plot";
  static define_unary_function_eval (__exponential_regression_plot,&_exponential_regression_plot,_exponential_regression_plot_s);
  define_unary_function_ptr5( at_exponential_regression_plot ,alias_at_exponential_regression_plot,&__exponential_regression_plot,0,true);

  gen _logarithmic_regression_plot(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    gen a,b,correl2;
    double xmin,xmax;
    vecteur attributs;
    bool eq,r;
    gen G=regression_plot_attributs(g,attributs,eq,r,contextptr);
    gen errcode=function_regression(G,at_ln,zero,a,b,xmin,xmax,correl2,contextptr);
    if (is_undef(errcode)) return errcode;
    xmax += (xmax-xmin); 
    gen ad(evalf_double(a,1,contextptr)),bd(evalf_double(b,1,contextptr)),cd(evalf_double(correl2,1,contextptr));
    if (ad.type==_DOUBLE_ && bd.type==_DOUBLE_ && cd.type==_DOUBLE_){
      string eqs="y="+print_DOUBLE_(ad._DOUBLE_val,3)+"*ln(x)+"+print_DOUBLE_(bd._DOUBLE_val,3);
      string R2s=" , R2="+print_DOUBLE_(cd._DOUBLE_val,3);
      *logptr(contextptr) << eqs << R2s << "\n";
      string s;
      if (eq)
	s += eqs;
      if (r)
	s += R2s;
      attributs.push_back(string2gen(s,false));
    }
    return put_attributs(_plotfunc(makesequence(a*ln(vx_var(),contextptr)+b,symb_equal(vx_var(),symb_interval(xmin,xmax))),contextptr),attributs,contextptr);
  }
  static const char _logarithmic_regression_plot_s []="logarithmic_regression_plot";
  static define_unary_function_eval (__logarithmic_regression_plot,&_logarithmic_regression_plot,_logarithmic_regression_plot_s);
  define_unary_function_ptr5( at_logarithmic_regression_plot ,alias_at_logarithmic_regression_plot,&__logarithmic_regression_plot,0,true);

  gen _power_regression_plot(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    gen a,b,correl2;
    double xmin,xmax;
    vecteur attributs;
    bool eq,r;
    gen G=regression_plot_attributs(g,attributs,eq,r,contextptr);
    gen errcode=function_regression(G,at_ln,at_ln,a,b,xmin,xmax,correl2,contextptr);
    if (is_undef(errcode)) return errcode;
    xmax += (xmax-xmin); 
    gen ad(evalf_double(a,1,contextptr)),bd(evalf_double(b,1,contextptr)),cd(evalf_double(correl2,1,contextptr));
    if (ad.type==_DOUBLE_ && bd.type==_DOUBLE_ && cd.type==_DOUBLE_){
      string eqs="y="+print_DOUBLE_(exp(bd,contextptr)._DOUBLE_val,3)+"*x^"+print_DOUBLE_(ad._DOUBLE_val,3);
      string R2s=" , R2="+print_DOUBLE_(cd._DOUBLE_val,3);
      *logptr(contextptr) << eqs << R2s << "\n";
      string s;
      if (eq)
	s += eqs;
      if (r)
	s += R2s;
      attributs.push_back(string2gen(s,false));
    }
    return put_attributs(_plotfunc(makesequence(exp(b,contextptr)*pow(vx_var(),a,contextptr),symb_equal(vx_var(),symb_interval(xmin,xmax))),contextptr),attributs,contextptr);
  }
  static const char _power_regression_plot_s []="power_regression_plot";
  static define_unary_function_eval (__power_regression_plot,&_power_regression_plot,_power_regression_plot_s);
  define_unary_function_ptr5( at_power_regression_plot ,alias_at_power_regression_plot,&__power_regression_plot,0,true);

  static gen polynomial_regression(const gen & g,int d,const gen & u1, const gen & u2,double & xmin, double & xmax,GIAC_CONTEXT){
    xmin=1e300,xmax=-xmin;
    vecteur v(genpoint2vecteur(g,contextptr));
    if (!ckmatrix(v) || v.empty() || v.front()._VECTptr->size()<2)
      return undef;
    // use first and second column
    const_iterateur it=v.begin(),itend=v.end();
    // int n(itend-it);
    gen sigmax,sigmay,sigmaxy,sigmax2,sigmay2,tmpx,tmpxd,tmpy;
    vecteur xmoment(2*d+1),xymoment(d+1);
    for (;it!=itend;++it){
      vecteur & w=*it->_VECTptr;
      if (u1.type==_FUNC)
	tmpx=u1(w.front(),contextptr);
      else
	tmpx=w.front();
      tmpxd=evalf_double(tmpx,1,contextptr);
      if (tmpxd.type==_DOUBLE_){
	double tmpxdd=tmpxd._DOUBLE_val;
	if (tmpxdd<xmin)
	  xmin=tmpxdd;
	if (tmpxdd>xmax)
	  xmax=tmpxdd;
      }
      if (u2.type==_FUNC)
	tmpy=u2(w.back(),contextptr);
      else
	tmpy=w.back();
      xmoment[0] += 1;
      xymoment[0] += tmpy;
      for (int i=1;i<=2*d;++i)
	xmoment[i] += pow(tmpx,i);
      for (int i=1;i<=d;++i)
	xymoment[i] += pow(tmpx,i)*tmpy;      
    }
    // make linear system
    matrice mat;
    for (int i=0;i<=d;++i){
      vecteur tmp;
      for (int j=d;j>=0;--j){
	tmp.push_back(xmoment[i+j]);
      }
      mat.push_back(tmp);
    }
    // return multmatvecteur(minv(mat,contextptr),xymoment); 
    return linsolve(mat,xymoment,contextptr);
  }
  static gen polynomial_regression(const gen & g,double & xmin,double & xmax,GIAC_CONTEXT){
    if (g.type==_VECT && g._VECTptr->size()==3){
      vecteur & v=*g._VECTptr;
      if (v[0].type==_VECT && v[1].type==_VECT && v[0]._VECTptr->size()==v[1]._VECTptr->size())
	return polynomial_regression(makevecteur(mtran(makevecteur(v[0],v[1])),v[2]),xmin,xmax,contextptr);
    }
    if (g.type!=_VECT || g._VECTptr->size()!=2)
      return gensizeerr(contextptr);
    gen last=_floor(g._VECTptr->back(),contextptr);
    if (last.type!=_INT_)
      return gensizeerr(contextptr);
    return polynomial_regression(g._VECTptr->front(),absint(last.val),zero,zero,xmin,xmax,contextptr);
  }
  gen _polynomial_regression(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    double xmin,xmax;
    return polynomial_regression(g,xmin,xmax,contextptr);
  }
  static const char _polynomial_regression_s []="polynomial_regression";
  static define_unary_function_eval (__polynomial_regression,&_polynomial_regression,_polynomial_regression_s);
  define_unary_function_ptr5( at_polynomial_regression ,alias_at_polynomial_regression,&__polynomial_regression,0,true);
  gen _polynomial_regression_plot(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    double xmin,xmax;
    vecteur attributs;
    bool eq,r;
    gen G=regression_plot_attributs(g,attributs,eq,r,contextptr);
    gen res=polynomial_regression(G,xmin,xmax,contextptr);
    if (is_undef(res)) return res;
    xmax += (xmax-xmin); 
    res=horner(res,vx_var());
    return put_attributs(_plotfunc(makesequence(res,symb_equal(vx_var(),symb_interval(xmin,xmax))),contextptr),attributs,contextptr);
  }
  static const char _polynomial_regression_plot_s []="polynomial_regression_plot";
  static define_unary_function_eval (__polynomial_regression_plot,&_polynomial_regression_plot,_polynomial_regression_plot_s);
  define_unary_function_ptr5( at_polynomial_regression_plot ,alias_at_polynomial_regression_plot,&__polynomial_regression_plot,0,true);

  static gen read_camembert_args(const gen & g,vecteur & vals,vecteur & names,vecteur & attributs,GIAC_CONTEXT){
    if (g.type!=_VECT)
      return gensizeerr(contextptr);
    attributs=vecteur(1,FL_BLACK | _FILL_POLYGON);
    int s=read_attributs(*g._VECTptr,attributs,contextptr);
    gen args=(s==1)?g._VECTptr->front():gen(vecteur(g._VECTptr->begin(),g._VECTptr->begin()+s),g.subtype);
    if (ckmatrix(args)){
      matrice tmp(*args._VECTptr);
      if (tmp.empty())
	return gendimerr(contextptr);
      if (tmp.size()!=2)
	tmp=mtran(tmp);
      int ts=int(tmp.size());
      if (ts<2)
	return gendimerr(contextptr);
      if (ts>2){
	// draw a camembert for each line
	// [ list_of_class_names camembert1_values camembert2_values etc. ]
	// camembertj_values may begin with a title string
	names=*tmp.front()._VECTptr;
	if (names.size()<2)
	  return gendimerr(contextptr);
	if (names[1].type!=_STRNG)
	  return gensizeerr(contextptr);
	vals=vecteur(tmp.begin()+1,tmp.end());
	return 0;
      }
      vals=*tmp[1]._VECTptr;
      names=*tmp[0]._VECTptr;
      if (vals.front().type==_STRNG)
	std::swap(vals,names);
      vals=vecteur(1,vals);
      return 0;
    }
    if (args.type!=_VECT)
      return gensizeerr(contextptr);
    vals=*args._VECTptr;
    names=vecteur(vals.size(),string2gen("",false));
    vals=vecteur(1,vals);
    return 0;
  }

  int color_list[8]={0,6000,7000,14000,32768,40960,49152,57344};

  // list of values or matrix with col1=list of legends, col2=list of values
  gen _diagramme_batons(const gen & g_,GIAC_CONTEXT){
    gen g(g_);
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    vecteur vals,names,attributs,res;
    double largeur=.8;
    if (g.type==_VECT && g.subtype==_SEQ__VECT){
      vecteur v=*g._VECTptr;
      if (v.size()>1 && v.front().type==_VECT && v.back().type!=_VECT){
	gen l=evalf_double(v.back(),1,contextptr);
	if (l.type==_DOUBLE_){
	  largeur=v.back()._DOUBLE_val;
	  v.pop_back();
	  if (v.size()==1)
	    v=*v.front()._VECTptr;
	}
      }
      for (unsigned i=0;i<v.size();++i){
	if (v[i].is_equal() && v[i]._SYMBptr->feuille.type==_VECT){
	  gen f=v[i]._SYMBptr->feuille._VECTptr->front();
	  if (f==at_size || (f.type==_IDNT && strcmp(f._IDNTptr->id_name,"width")==0)){
	    gen tmp=v[i]._SYMBptr->feuille._VECTptr->back();
	    tmp=evalf_double(tmp,1,contextptr);
	    if (tmp.type!=_DOUBLE_ || tmp._DOUBLE_val<=0 || tmp._DOUBLE_val>1)
	      return gensizeerr(contextptr);
	    largeur=tmp._DOUBLE_val;
	    v.erase(v.begin()+i);
	    --i;
	  }
	}
      }
      if (v.size()==1)
	g=v.front();
      else
	g=gen(v,_SEQ__VECT);
    }
    largeur /=2;
    gen errcode=read_camembert_args(g,vals,names,attributs,contextptr);
    if (is_undef(errcode)) return errcode;
    vecteur attr(gen2vecteur(attributs[0]));
    int ncamemberts=int(vals.size()),s=int(vals.front()._VECTptr->size()),t=int(attr.size());
    int c=FL_BLACK & 0xffff;
    if (t==1){
      t=0;
      c=attr[0].val;
    }
    for (int j=0;j<ncamemberts;j++){
      vecteur & Vals = *vals[j]._VECTptr;
      int i=0;
      gen xy=s*j;
      if (Vals[0].type==_STRNG){
	// add title
	res.push_back(symb_pnt_name(xy+.5-2*cst_i,_POINT_INVISIBLE,Vals[0],contextptr));
	++i;
      }
      for (;i<s;++i){
	gen tmp,xpos,ivals(cst_i*Vals[i]);
	if (names[i].type!=_STRNG && has_evalf(names[i],xpos,1,contextptr)){
	  gen xposl(xpos+largeur),xposml(xpos-largeur);
	  tmp=xposl+ivals;
	  tmp=gen(makevecteur(tmp,xposl,xposml,xposml+ivals,tmp),_LINE__VECT);
	  res.push_back(symb_pnt(tmp,i<t?attr[i]:( (1+i%7) | _FILL_POLYGON | _QUADRANT2),contextptr));
	}
	else {
	  xpos=xy+i;
	  gen xposl(xpos+largeur),xposml(xpos-largeur);
	  tmp=xposl+ivals;
	  tmp=gen(makevecteur(tmp,xposl,xposml,xposml+ivals,tmp),_LINE__VECT);
	  res.push_back(symb_pnt_name(tmp,i<t?attr[i]:( (1+i%7) | _FILL_POLYGON | _QUADRANT2),names[i],contextptr));
	}
      }
    }
    return res;
  }
  static const char _diagramme_batons_s []="bar_plot";
  static define_unary_function_eval (__diagramme_batons,&_diagramme_batons,_diagramme_batons_s);
  define_unary_function_ptr5( at_diagramme_batons ,alias_at_diagramme_batons,&__diagramme_batons,0,true);

  static const char _diagrammebatons_s []="barplot";
  static define_unary_function_eval (__diagrammebatons,&_diagramme_batons,_diagrammebatons_s);
  define_unary_function_ptr5( at_diagrammebatons ,alias_at_diagrammebatons,&__diagrammebatons,0,true);


  gen _camembert(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    vecteur vals,names,attributs,res;
    gen errcode=read_camembert_args(g,vals,names,attributs,contextptr);
    if (is_undef(errcode)) return errcode;
    vecteur attr(gen2vecteur(attributs[0]));
    int ncamemberts=int(vals.size()),s=int(vals.front()._VECTptr->size()),t=int(attr.size());
    t=0;
    for (int j=0;j<ncamemberts;j++){
      gen xy=5*(j%4)-5*(j/4)*cst_i;
      gen diametre=makevecteur(-1+xy,1+xy);
      gen a(0),da;
      double da100;
      char ss[256];
      vecteur & Vals = *vals[j]._VECTptr;
      gen somme;
      int i=0,pos=0;;
      if (Vals[0].type==_STRNG){
	// add title
	res.push_back(symb_pnt_name(xy-1+2*cst_i,_POINT_INVISIBLE,Vals[0],contextptr));
	++i;
	somme=_plus(vecteur(Vals.begin()+1,Vals.end()),contextptr);
      }
      else
	somme=_plus(Vals,contextptr);
      for (;i<s;++i){
	if (ck_is_strictly_positive(-Vals[i],contextptr))
	  return gensizeerr(gettext("Negative value encoutered"));
	da=2*cst_pi*Vals[i]/somme;
	da100=evalf_double(100*Vals[i]/somme,1,contextptr)._DOUBLE_val;
	if (da100>0){
	  da100=int(da100*10+.5)/10.0;
	  sprintfdouble(ss,"%.4g",da100);
	  if (is_positive(a-cst_pi/2,contextptr))
	    pos=_QUADRANT2;
	  if (is_positive(a-cst_pi,contextptr))
	    pos=_QUADRANT3;
	  if (is_positive(a-3*cst_pi/2,contextptr))
	    pos=_QUADRANT4;
	  gen tmp=symbolic(at_cercle,gen(makevecteur(diametre,a,a+da),_PNT__VECT));
	  res.push_back(symb_pnt_name(tmp,i<t?attr[i]:( color_list[i%8] | _FILL_POLYGON | pos),string2gen(gen2string(names[i])+":"+string(ss)+"%",false),contextptr));
	  a=a+da;
	}
      }
    }
    return res;
  }
  static const char _camembert_s []="camembert";
static define_unary_function_eval (__camembert,&_camembert,_camembert_s);
  define_unary_function_ptr5( at_camembert ,alias_at_camembert,&__camembert,0,true);

  gen _is_matrix(const gen & a,GIAC_CONTEXT){
    if (a.type==_STRNG && a.subtype==-1) return  a;
    return change_subtype(ckmatrix(a),_INT_BOOLEAN);
  }
  static const char _is_matrix_s []="is_matrix";
  static define_unary_function_eval (__is_matrix,&_is_matrix,_is_matrix_s);
  define_unary_function_ptr5( at_is_matrix ,alias_at_is_matrix,&__is_matrix,0,true);
#endif

  // Python compat convert to list
  gen _python_list(const gen & a,GIAC_CONTEXT){
    if (a.type==_STRNG && a.subtype==-1) return  a;
    if (a.type==_VECT) return a;
    if (a.type==_STRNG){
      const string & as=*a._STRNGptr;
      unsigned ass=as.size();
      vecteur res(ass);
      for (unsigned i=0;i<ass;++i)
	res[i]=string2gen(string(1,as[i]),false);
      return res;
    }
    return _convert(makesequence(a,change_subtype(_MAPLE_LIST,_INT_MAPLECONVERSION)),contextptr);
  }
  static const char _python_list_s []="python_list";
  static define_unary_function_eval (__python_list,&_python_list,_python_list_s);
  define_unary_function_ptr5( at_python_list ,alias_at_python_list,&__python_list,0,true);
  
  gen _efface(const gen &,GIAC_CONTEXT){
    Bdisp_AllClr_VRAM();
    return 1;
  }
  static const char _clearscreen_s []="clearscreen";
  static define_unary_function_eval2 (__clearscreen,&_efface,_clearscreen_s,&printastifunction);
  define_unary_function_ptr5( at_clearscreen ,alias_at_clearscreen,&__clearscreen,0,T_LOGO);

  //int clip_ymin=0;
  void set_pixel(int x0, int y0,unsigned short color) {
    if (!freeze){
      Bdisp_AllClr_VRAM();
      freeze=true;
    }
    if (x0<0 || x0>=LCD_WIDTH_PX || y0<clip_ymin || y0>=LCD_HEIGHT_PX)
      return;
#if 1
#if defined FX || defined FXCG
    Bdisp_SetPoint_VRAM(x0,y0,color?1:0);
#else
    os_set_pixel(x0,y0,color);
#endif
#else
    unsigned short* VRAM = (unsigned short*)GetVRAMAddress();
    VRAM += (y0*LCD_WIDTH_PX + x0);
    *VRAM=color;
#endif
  }
  
  unsigned short get_pixel(int x0,int y0){
#if 1
#if defined FX || defined FXCG    
    return Bdisp_GetPoint_VRAM(x0,y0);
#else
    return os_get_pixel(x0,y0);
#endif
#else
    unsigned short* VRAM = (unsigned short*)GetVRAMAddress();
    VRAM += (y0*LCD_WIDTH_PX + x0);
    return *VRAM;
#endif
  }

#if 0 // in console.cc/.h
  void draw_circle(int xc,int yc,int r,int color,bool q1,bool q2,bool q3,bool q4){
    if (!freeze){
      Bdisp_AllClr_VRAM();
      freeze=true;
    }
    //dbg_printf("circle color=%i\n",color);
    int x=0,y=r,delta=0;
    while (x<=y){
      if (q4){
	os_set_pixel(xc+x,yc+y,color);
	os_set_pixel(xc+y,yc+x,color);
      }
      if (q3){
	os_set_pixel(xc-x,yc+y,color);
	os_set_pixel(xc-y,yc+x,color);
      }
      if (q1){
	os_set_pixel(xc+x,yc-y,color);
	os_set_pixel(xc+y,yc-x,color);
      }
      if (q2){
	os_set_pixel(xc-x,yc-y,color);
	os_set_pixel(xc-y,yc-x,color);
      }
      ++x;
      if (delta<0){
	delta += 2*y+1;
	--y;
      }
      delta += 1-2*x;
    }
  }

  // arc of ellipse, for y/x in [t1,t2] and in quadrant 1, 2, 3, 4
  // y must be replaced by -y 
  void draw_arc(int xc,int yc,int rx,int ry,int color,double t1, double t2,bool q1,bool q2,bool q3,bool q4,GIAC_CONTEXT){
    int x=0,y=rx,delta=0;
    double ryx=double(ry)/rx;
    // *logptr(contextptr) << "t1,t2:" << t1 << "," << t2 << ",q1234" << q1 << "," << q2 << "," << q3 << "," << q4 << "\n";
    while (x<=y){
      int xeff=x*ryx,yeff=y*ryx;
      if (q4){
	if (y>=-x*t2 && y<=-x*t1) set_pixel(xc+x,yc+yeff,color);
	if (x>=-y*t2 && x<=-y*t1) set_pixel(xc+y,yc+xeff,color);
      }
      if (q3){
	if (y>=x*t1 && y<=x*t2) set_pixel(xc-x,yc+yeff,color);
	if (x>=y*t1 && x<=y*t2) set_pixel(xc-y,yc+xeff,color);
      }
      if (q1){
	if (y>=x*t1 && y<=x*t2) set_pixel(xc+x,yc-yeff,color);
	if (x>=y*t1 && x<=y*t2) set_pixel(xc+y,yc-xeff,color);
      }
      if (q2){
	if (y>=-x*t2 && y<=-x*t1) set_pixel(xc-x,yc-yeff,color);
	if (x>=-y*t2 && x<=-y*t1) set_pixel(xc-y,yc-xeff,color);
      }
      ++x;
      if (delta<0){
	delta += 2*y+1;
	--y;
      }
      delta += 1-2*x;
    }
  }
  
  void draw_arc(int xc,int yc,int rx,int ry,int color,double theta1, double theta2,GIAC_CONTEXT){
    if (theta2-theta1>=2*M_PI){
      draw_arc(xc,yc,rx,ry,color,-1e38,1e38,true,true,true,true,contextptr);
      return;
    }
    // at most one vertical in [theta1,theta2]
    double t1=std::tan(theta1);
    double t2=std::tan(theta2);
    int n=int(std::floor(theta1/M_PI+.5));
    // n%2==0 -pi/2<theta1<pi/2, n%2==1 pi/2<theta1<3*pi/2
    double theta=(n+.5)*M_PI;
    // if theta1 is almost pi/2 mod pi, t1 might be wrong because of rounding
    if (std::fabs(theta1-(theta-M_PI))<1e-6 && t1>0) 
	t1=-1e38;
    //*logptr(contextptr) << "thetas:" << theta1 << "," << theta << "," << theta2 << ", n " << n << ", t:" << t1 << "," << t2 << "\n";
    if (theta2>theta){
      if (theta2>=theta+M_PI){
	if (n%2==0){ // -pi/2<theta1<pi/2<3*pi/2<theta2
	  draw_arc(xc,yc,rx,ry,color,t1,1e38,true,false,false,false,contextptr);
	  draw_arc(xc,yc,rx,ry,color,-1e38,1e38,false,true,true,false,contextptr);	  
	  draw_arc(xc,yc,rx,ry,color,-1e38,t2,false,false,false,true,contextptr);
	}
	else { // -3*pi/2<theta1<-pi/2<pi/2<theta2
	  draw_arc(xc,yc,rx,ry,color,t1,1e38,false,false,true,false,contextptr);
	  draw_arc(xc,yc,rx,ry,color,-1e38,1e38,true,false,false,true,contextptr);
	  draw_arc(xc,yc,rx,ry,color,-1e38,t2,false,true,false,false,contextptr);
	}
	return;
      }
      if (n%2==0){ // -pi/2<theta1<pi/2<theta2<3*pi/2
	draw_arc(xc,yc,rx,ry,color,t1,1e38,true,false,false,false,contextptr);
	draw_arc(xc,yc,rx,ry,color,-1e38,t2,false,true,false,false,contextptr);
      }
      else { // -3*pi/2<theta1<-pi/2<theta2<pi/2
	draw_arc(xc,yc,rx,ry,color,t1,1e38,false,false,true,false,contextptr);
	draw_arc(xc,yc,rx,ry,color,-1e38,t2,false,false,false,true,contextptr);
      }
      return;
    }
    if (n%2==0) { // -pi/2<theta1<theta2<pi/2
      draw_arc(xc,yc,rx,ry,color,t1,t2,true,false,false,true,contextptr);	
    }
    else { // pi/2<theta1<theta2<3*pi/2
      draw_arc(xc,yc,rx,ry,color,t1,t2,false,true,true,false,contextptr);	
    }
  }
  
  void draw_filled_circle(int xc,int yc,int r,int color,bool left,bool right){
    int x=0,y=r,delta=0;
    while (x<=y){
      for (int Y=-y;Y<=y;Y++){
	if (right)
	  set_pixel(xc+x,yc+Y,color);
	if (left)
	  set_pixel(xc-x,yc+Y,color);
      }
      for (int Y=-x;Y<=x;Y++){
	if (right)
	  set_pixel(xc+y,yc+Y,color);
	if (left)
	  set_pixel(xc-y,yc+Y,color);
      }
      ++x;
      if (delta<0){
	delta += 2*y+1;
	--y;
      }
      delta += 1-2*x;
    }
  }

  void drawRectangle(int x, int y, int width, int height, unsigned short color){
    if (x<0){ width+=x; x=0;}
    if (y<clip_ymin){ height+=y-clip_ymin; y=clip_ymin;}
    if (width<=0 || height<=0) return;
    if (x+width>LCD_WIDTH_PX) width=LCD_WIDTH_PX-x;
    if (y+height>LCD_HEIGHT_PX) height=LCD_HEIGHT_PX-y;
    if (width<=0 || height<=0) return;
#if !defined FX && !defined FXCG
    os_fill_rect(x,y,width,height,color);
#else
#if 1
    DISPBOX d={x,y,x+width-1,y+height-1};
    Bdisp_AreaClr_VRAM(&d);
    if (color) Bdisp_AreaReverseVRAM(x,y,x+width-1,y+height-1);
#else
    unsigned short* VRAM = (unsigned short*)GetVRAMAddress();
    VRAM+=(y*128)+x;
    while(height--){
      int i=width;
      while(i--){
        *VRAM++ = color;
      }
      VRAM+=128-width;
    }
#endif
#endif
  }
  
  void draw_rectangle(int x, int y, int width, int height, unsigned short color){
    if (!freeze){
      Bdisp_AllClr_VRAM();
      freeze=true;
    }
    drawRectangle(x,y,width,height,color);
  }

  //Uses the Bresenham line algorithm 
  void draw_line(int x1, int y1, int x2, int y2, int color,unsigned short motif) {
    if (!freeze){
      Bdisp_AllClr_VRAM();
      freeze=true;
    }
    if ( (absint(x1) & 0xfffff000) ||
	 (absint(x2) & 0xfffff000) ||
	 (absint(y1) & 0xfffff000) ||
	 (absint(y2) & 0xfffff000) 
	 )
      return;
    int w =(color & 0x00070000) >> 16;
    ++w;
    color &= 0xffff;
    if ( (x1<0 && x2<0) || (x1>=LCD_WIDTH_PX && x2>=LCD_WIDTH_PX) || (y1<clip_ymin && y2<clip_ymin) || (y1>=LCD_HEIGHT_PX && y2>=LCD_HEIGHT_PX))
      return;
    signed char ix; 
    signed char iy; 
    
    // if x1 == x2 or y1 == y2, then it does not matter what we set here 
    int delta_x = (x2 > x1?(ix = 1, x2 - x1):(ix = -1, x1 - x2)) << 1; 
    int delta_y = (y2 > y1?(iy = 1, y2 - y1):(iy = -1, y1 - y2)) << 1;
    int doit=motif;
    if (doit&1)
      os_set_pixel(x1, y1, color);
    doit >>=1;
    if (!doit) doit=motif;
    if (delta_x >= delta_y) { 
      int error = delta_y - (delta_x >> 1);        // error may go below zero 
      while (x1 != x2) { 
	if (error >= 0) { 
	  if (error || (ix > 0)) { 
	    y1 += iy; 
	    error -= delta_x; 
	  }                           // else do nothing 
	}                              // else do nothing 
	x1 += ix; 
	error += delta_y;
#if 1
	int y__=y1+(w+1)/2;
	for (int y_=y1-w/2;y_<y__;++y_){
	  if (doit&1)
	    os_set_pixel(x1, y_, color);
	  doit >>=1;
	  if (!doit) doit=motif;
	}
#else
	os_set_pixel(x1, y1, color);
#endif
      } 
    } else { 
      int error = delta_x - (delta_y >> 1);      // error may go below zero 
      while (y1 != y2) { 
	if (error >= 0) { 
	  if (error || (iy > 0)) { 
	    x1 += ix; 
	    error -= delta_y; 
	  }                           // else do nothing 
	}                              // else do nothing 
	y1 += iy; 
	error += delta_x;
#if 1
	int x__=x1+(w+1)/2;
	for (int x_=x1-w/2;x_<x__;++x_){
	  if (doit&1)
	    os_set_pixel(x_, y1, color);
	  doit >>=1;
	  if (!doit) doit=motif;
	}
#else
	os_set_pixel(x1, y1, color);
#endif
      } 
    }
  }
#endif // duplicate code in console.cc/.h

  bool isalphanum(char c){
    return isalpha(c) || (c>='0' && c<='9');
  }
  
#ifndef TICE
  gen select_var(GIAC_CONTEXT){
    gen g(_VARS(0,contextptr));
    if (g.type!=_VECT)// || g._VECTptr->empty())
      return undef;
    vecteur & v=*g._VECTptr;
    MenuItem smallmenuitems[v.size()+3];
    vector<string> vs(v.size()+1);
    int i,total=0;
    const char typ[]="idzDcpiveSfEsFRmuMwgPF";
    for (i=0;i<v.size();++i){
      vs[i]=v[i].print(contextptr);
      if (v[i].type==giac::_IDNT){
	giac::gen w;
	v[i]._IDNTptr->in_eval(0,v[i],w,contextptr,true);
#if 1
	vector<int> vi(9);
	tailles(w,vi);
	total += vi[8];
	if (vi[8]<400)
	  vs[i]+=":="+w.print(contextptr);
	else {
	  vs[i] += " ~";
	  vs[i] += giac::print_INT_(vi[8]);
	  vs[i] += ',';
	  vs[i] += typ[w.type];
	}
#else
	if (taille(w,50)<50)
	  vs[i]+=": "+w.print(contextptr);
#endif
      }
      smallmenuitems[i].text=(char *) vs[i].c_str();
    }
    total += sizeof(giac::sym_string_tab) + giac::turtle_stack().capacity()*sizeof(giac::logo_turtle)+sizeof(giac::context)+bytesize(giac::history_in(contextptr))+bytesize(giac::history_out(contextptr));
    vs[i]="purge(~"+giac::print_INT_(total)+')';
    smallmenuitems[i].text=(char *)vs[i].c_str();
    smallmenuitems[i+1].text=(char *)"assume(";
    smallmenuitems[i+2].text=(char *)"restart";
    Menu smallmenu;
    smallmenu.numitems=v.size()+3; 
    smallmenu.items=smallmenuitems;
    smallmenu.height=8;
    smallmenu.scrollbar=1;
    smallmenu.scrollout=1;
    string st=string("VARS ")+print_INT_(get_free_memory());
    smallmenu.title = (char *) st.c_str();
    //MsgBoxPush(5);
    int sres = doMenu(&smallmenu);
    //MsgBoxPop();
    if (sres==KEY_CTRL_DEL && smallmenu.selection<=v.size())
      return symbolic(at_purge,v[smallmenu.selection-1]);
    if (sres!=MENU_RETURN_SELECTION)
      return undef;
    if (smallmenu.selection==1+v.size())
      return string2gen("purge(",false);
    if (smallmenu.selection==2+v.size())
      return string2gen("assume(",false);
    if (smallmenu.selection==3+v.size())
      return string2gen("restart",false);
    return v[smallmenu.selection-1];
  }

  int chartab(){
    static int row=0,col=0;
    for (;;){
      int cur=32+16*row+col;
      col &= 0xf;
      if (row<0) row=5; else if (row>5) row=0;
      // display table
      drawRectangle(0,0,LCD_WIDTH_PX,LCD_HEIGHT_PX,COLOR_WHITE);
      // os_draw_string(0,0,COLOR_BLACK,COLOR_WHITE,lang==1?"Selectionner caractere":"Select char");
      PrintXY(0,0,(const Char *) (lang==1?"shift INS: table de caracteres":"shift INS: char table"),TEXT_MODE_NORMAL);
      int dy=8;
      for (int r=0;r<6;++r){
        for (int c=0;c<16;++c){
          int currc=32+16*r+c;
          Char buf[2]={currc==127?(Char)'X':(Char)currc,0};
#ifdef TICE
          Printmini(9*c,dy+16*r,buf,cur==currc?MINI_REV:0);
#else
          PrintMini(6*c,dy+8*r,buf,cur==currc?MINI_REV:0);
#endif
        }
      }
      string s("Current ");
      s += char(cur);
      s += " ";
      s += giac::print_INT_(cur);
      s += " ";
      s += giac::hexa_print_INT_(cur);
      PrintXY(0,56,(const Char *)s.c_str(),TEXT_MODE_NORMAL);
      // interaction
      int key; ck_getkey(&key);
      if (key==KEY_CTRL_EXIT){
	drawRectangle(0,0,LCD_WIDTH_PX,LCD_HEIGHT_PX,COLOR_WHITE);	
	return -1;
      }
      if (key==KEY_CTRL_EXE){
	drawRectangle(0,0,LCD_WIDTH_PX,LCD_HEIGHT_PX,COLOR_WHITE);
	return cur;
      }
      if (key==KEY_CTRL_LEFT)
	--col;
      if (key==KEY_CTRL_RIGHT)
	++col;
      if (key==KEY_CTRL_UP)
	--row;
      if (key==KEY_CTRL_DOWN)
	++row;
    }
  }

  const char * keytostring(int key,int keyflag,bool py,GIAC_CONTEXT){
    const int textsize=512;
    bool alph=keyflag==4||keyflag==0x84||keyflag==8||keyflag==0x88;
    static char text[textsize];
    switch (key){
    case KEY_CHAR_PLUS:
      return "+";
    case KEY_CHAR_MINUS:
      return "-";
    case KEY_CHAR_PMINUS:
      return "_";
    case KEY_CHAR_MULT:
      return "*";
    case KEY_CHAR_FRAC:
      return py?"\\":"solve(";
    case KEY_CHAR_DIV: 
      return "/";
    case KEY_CHAR_POW:
      return "^";
    case KEY_CHAR_ROOT:
      return "sqrt(";
    case KEY_CHAR_SQUARE:
      return py?"**2":"^2";
    case KEY_CHAR_CUBEROOT:
      return py?"**(1/3)":"^(1/3)";
    case KEY_CHAR_POWROOT:
      return py?"**(1/":"^(1/";
    case KEY_CHAR_RECIP:
      return py?"**-1":"^-1";
    case KEY_CHAR_THETA:
      return "arg(";
    case KEY_CHAR_VALR:
      return "abs(";
    case KEY_CHAR_ANGLE:
      return "polar_complex(";
    case KEY_CTRL_XTT:
      return xthetat?"t":"x";
      //return "x"; 
    case KEY_CHAR_LN:
      return "ln(";
    case KEY_CHAR_LOG:
      return "log10(";
    case KEY_CHAR_EXPN10:
      return "10^";
    case KEY_CHAR_EXPN:
      return "exp(";
    case KEY_CHAR_SIN:
      return "sin(";
    case KEY_CHAR_COS:
      return "cos(";
    case KEY_CHAR_TAN:
      return "tan(";
    case KEY_CHAR_ASIN:
      return "asin(";
    case KEY_CHAR_ACOS:
      return "acos(";
    case KEY_CHAR_ATAN:
      return "atan(";
    case KEY_CTRL_MIXEDFRAC:
      return "limit(";
    case KEY_CTRL_FRACCNVRT:
      return "approx(";
      //case KEY_CTRL_FORMAT: return "purge(";
    case KEY_CTRL_FD:
      return "exact(";
    case KEY_CHAR_STORE:
      // if (keyflag==1) return "inf";
      return "=>";
    case KEY_CHAR_IMGNRY:
      return "i";
    case KEY_CHAR_PI:
      return "pi";
    case KEY_CTRL_VARS: {
      gen var=select_var(contextptr);
      if (!is_undef(var)){
	strcpy(text,(var.type==_STRNG?*var._STRNGptr:var.print()).c_str());
	return text;
      }
      return "";
    }
    case KEY_CHAR_EXP:
      return "e";
    case KEY_CHAR_ANS:
      return "ans()";
    case KEY_CTRL_INS: {
      static string * sptr=0;
      if (!sptr)
	sptr=new string(" ");
      int c=chartab();
      (*sptr)[0]=c<32 || c==127?' ':char(c);
      return sptr->c_str(); // ":=";
    }
    case KEY_CHAR_COMMA:
      // if (keyflag==1) return "solve(";
      return ",";
    case KEY_CHAR_LPAR:
      return "(";
    case KEY_CHAR_RPAR:
      return ")";
    case KEY_CHAR_LBRCKT:
      return "[";
    case KEY_CHAR_RBRCKT:
      return "]";
    case KEY_CHAR_LBRACE:
      return "{";
    case KEY_CHAR_RBRACE:
      return "}";
    case KEY_CHAR_EQUAL:
      return "=";
    case KEY_CTRL_F3:
      if (alph) return ";";
      if (keyflag==1) return "!";
      return ":";
    case KEY_CTRL_F2:
      return (alph)?"#":(keyflag==1?"<":">");
    case KEY_CTRL_F1:
      return (alph)?"\\":(keyflag==1?"%":"'");
    case KEY_CTRL_PASTE:
      return paste_clipboard();
#if 1
    case KEY_CHAR_MAT:{
      // const char * ptr=input_matrix(false); if (ptr) return ptr;
      if (showCatalog(text,17)) return text;
      return "";
    }
    case KEY_CHAR_LIST: {
      //const char * ptr=input_matrix(true); if (ptr) return ptr;
      if (showCatalog(text,16)) return text;
      return "";
    }
    case KEY_CTRL_PRGM:
      // open functions catalog, prgm submenu
      if(showCatalog(text,18))
	return text;
      return "";
    case KEY_CTRL_CATALOG:
      if(showCatalog(text,1)) 
	return text;
      return "";
    case KEY_CTRL_F4:
      if(showCatalog(text,0)) 
	return text;
      return "";
#if 0
    case KEY_SHIFT_OPTN:
      if(showCatalog(text,10))
	return text;
      return "";
#endif
    case KEY_CTRL_OPTN:
      if(showCatalog(text,15))
	return text;
      return "";
    case KEY_CTRL_QUIT: 
      if(showCatalog(text,20))
	return text;
      return "";
    case KEY_CTRL_SETUP:
      if(showCatalog(text,7))
	return text;
      return "";
#endif
    }
    return 0;
  }
#endif
  
  int xthetat=0;

#if defined FX || defined FXCG
  extern "C" {
    char Setup_GetEntry(unsigned int index);
    char *Setup_SetEntry(unsigned int index, char setting);
  }
#define SetSetupSetting Setup_SetEntry
#endif

#if defined FX || defined FXCG
  int handle_f5(){
    char keyflag = Setup_GetEntry(0x14);
    if (keyflag == 0x04 || keyflag == 0x08 || keyflag == (char)0x84 || keyflag == (char)0x88) {
      // ^only applies if some sort of alpha (not locked) is already on
      if (keyflag == 0x08 || keyflag == (char)0x88) { //if lowercase
        SetSetupSetting( (unsigned int)0x14, keyflag-0x04);
        return 1; //do not process the key, because otherwise we will leave alpha status
      } else {
        SetSetupSetting( (unsigned int)0x14, keyflag+0x04);
        return 1; //do not process the key, because otherwise we will leave alpha status
      }
    }
    if (keyflag==0) {
      SetSetupSetting( (unsigned int)0x14, (char)0x88);	
    }
    return 0;
  }
#endif
  
  void delete_clipboard(){}

  bool clip_pasted=true;
  
  string * clipboard(){
    static string * ptr=0;
    if (!ptr)
      ptr=new string;
    return ptr;
  }
  
  void copy_clipboard(const string & s,bool status){
    if (clip_pasted)
      *clipboard()=s;
    else
      *clipboard()+=s;
    clip_pasted=false;
    if (status){ // display status line
    }
  }
  
  const char * paste_clipboard(){
    clip_pasted=true;
    return clipboard()->c_str();
  }

#ifdef TICE
#else  
  int print_msg12(const char * msg1,const char * msg2,int textY){
    drawRectangle(0, textY, LCD_WIDTH_PX, 28, COLOR_WHITE);
    drawRectangle(2,textY,120,2, COLOR_BLACK);
    drawRectangle(2,textY,2,28, COLOR_BLACK);
    drawRectangle(120,textY,2,28, COLOR_BLACK);
    drawRectangle(2,textY+28,120,2, COLOR_BLACK);
    int textX=5;
    textY+=6;
    if (msg1)
      PrintMini(textX,textY,(const Char *)msg1,1);
    textY+=8;
    if (msg2)
      PrintMini(textX,textY,(const Char *)msg2,1);
    Bdisp_PutDisp_DD();
    return textX;
  }
  
  void insert(string & s,int pos,const char * add){
    if (pos>s.size())
      pos=s.size();
    if (pos<0)
      pos=0;
    s=s.substr(0,pos)+add+s.substr(pos,s.size()-pos);
  }
#define C85 (LCD_WIDTH_PX*2/3)
  void print_alpha_shift(int keyflag){
    if (keyflag==0)
      Printmini(C85,C58,(const Char *)"| A<>a",MINI_REV);
    if (keyflag==1)
      Printmini(C85,C58,(const Char *)"SHIFT",0);
    if (keyflag==4)
      Printmini(C85,C58,(const Char *)"ALPHA",0);
    if (keyflag==8)
      Printmini(C85,C58,(const Char *)"alpha",0);
    if (keyflag==0x84)
      Printmini(C85,C58,(const Char *)"ALOCK",0);
    if (keyflag==0x88)
      Printmini(C85,C58,(const Char *)"alock",0);
  }    
  
  int inputline(const char * msg1,const char * msg2,string & s,bool numeric,int ypos){
    // s="";
    int pos=s.size(),beg=0;
    for (;;){
      int X1=print_msg12(msg1,msg2,ypos-19);
      int textX=X1,textY=ypos;
      drawRectangle(textX,textY,LCD_WIDTH_PX-textX-8,8,COLOR_WHITE);
      if (pos-beg>36)
	beg=pos-12;
      if (int(s.size())-beg<36)
	beg=giacmax(0,int(s.size())-36);
      if (beg>pos)
	beg=pos;
      textX=X1;
      PrintXY(textX,textY,(const Char *)s.substr(beg,pos-beg).c_str(),0);
      textX+=(pos-beg)*6;
      int cursorpos=textX;
      PrintXY(textX+2,textY,(const Char *)s.substr(pos,s.size()-pos).c_str(),0);
      //Cursor_SetPosition(cursorpos,textY+1);
      drawRectangle(cursorpos,textY+1,1,6,COLOR_BLACK); // cursor
      int keyflag = (Char)Setup_GetEntry(0x14);
      Printmini(0,C58,(const Char *)"     |     |     |     |     |     ",MINI_REV);
      print_alpha_shift(keyflag);
#if !defined FX && !defined FXCG
      int key;
#else
      unsigned int key;
#endif
      GetKey(&key);
      if (key==KEY_CTRL_F5){
	handle_f5();
	continue;
      }
      if (key==KEY_CTRL_EXE)
	return key;
      if (key>=32 && key<128){
	if (!numeric || key=='-' || key=='.' || key=='e' || key=='E' || (key>='0' && key<='9')){
	  s.insert(s.begin()+pos,char(key));
	  ++pos;
	}
	continue;
      }
      if (key==KEY_CTRL_DEL){
	if (pos){
	  s.erase(s.begin()+pos-1);
	  --pos;
	}
	continue;
      }
      if (key==KEY_CTRL_AC){
	if (s=="")
	  return KEY_CTRL_EXIT;
	s="";
	pos=0;
	continue;
      }
      if (key==KEY_CTRL_EXIT)
	return key;
      if (key==KEY_CTRL_RIGHT){
	if (pos<s.size())
	  ++pos;
	continue;
      }
      // if (key==KEY_SHIFT_RIGHT){ pos=s.size(); continue; }
      if (key==KEY_CTRL_LEFT){
	if (pos)
	  --pos;
	continue;
      }
      //if (key==KEY_SHIFT_LEFT){ pos=0;continue;}
      if (const char * ans=keytostring(key,keyflag,false,context0)){
	insert(s,pos,ans);
	pos+=strlen(ans);
	continue;
      }
    }
  }

  int confirm(const char * msg1,const char * msg2,bool acexit){
    print_msg12(msg1,msg2);
    Printmini(0,C58,(const Char *)" F1 |     |     |     |     | F6  ",MINI_REV);
#if !defined FX && !defined FXCG
    int key=0;
#else
    unsigned int key=0;
#endif
    while (key!=KEY_CTRL_F1 && key!=KEY_CTRL_F6){
      GetKey(&key);
      if (key==KEY_CTRL_EXE)
	key=KEY_CTRL_F1;
      if (key==KEY_CTRL_AC || key==KEY_CTRL_EXIT){
	if (acexit) return -1;
	key=KEY_CTRL_F6;
      }
      // set_xcas_status();
    }
    return key;
  }  

  bool do_confirm(const char * s){
    return confirm(s,(lang?"F1: oui,    F6:annuler":"F1: yes,     F6: cancel"))==KEY_CTRL_F1;
  }

  bool confirm_overwrite(){
    return do_confirm(lang?"F1: oui,    F6:annuler":"F1: yes,     F6: cancel")==KEY_CTRL_F1;
  }

  void invalid_varname(){
    confirm(lang?"Nom de variable incorrect":"Invalid variable name", lang?"F1 ou F6: ok":"F1 or F6: ok");
  }
#endif

  // required arg postprocessing because black is 1 and white is 0
#ifdef FX
  gen invert_bw(const gen & g){
    if (g.type!=_INT_)
      return g;
    int res= ((g.val & 0xffff)?0:1) | (g.val & 0xffff0000);
    return res;
  }
#else
#define invert_bw(g) (g)
#endif

  gen remove_at_display(const gen &g){
    if (g.is_equal()){
      const gen & f=g._SYMBptr->feuille;
      if (f.type==_VECT && f._VECTptr->size()==2 && f._VECTptr->front()==at_display)
        return invert_bw(f._VECTptr->back());
    }
    return invert_bw(g);
  }

  gen _set_pixel(const gen & a_,GIAC_CONTEXT){
    gen a(a_);
    if (a.type==_STRNG && a.subtype==-1) return  a;
#if 1 // Bdisp_PutDisp_DD blocks I don't know why
    if (a.type==_VECT && a._VECTptr->empty()){
      clear_abort();
      Bdisp_PutDisp_DD();
      set_abort();
      return plus_one;
    }
#endif
    if (a.type!=_VECT || a._VECTptr->size()<2)
      return gentypeerr(contextptr);
    const vecteur & v=*a._VECTptr;
    size_t vs=v.size();
    if (vs>=2){
      gen x=v.front();
      gen y=v[1];
      if (x.type==_DOUBLE_)
	x=int(x._DOUBLE_val+.5);
      if (y.type==_DOUBLE_)
	y=int(y._DOUBLE_val+.5);
      if (x.type==_INT_ &&  y.type==_INT_ ){
	set_pixel(x.val,y.val,vs==2?FL_BLACK:remove_at_display(v[2]).val);
	return 1;
      }
    }
    return gensizeerr(contextptr);
    //static gen PIXEL(identificateur("PIXON_P"));
    //return _of(makesequence(PIXEL,a_),contextptr);
  }
  static const char _set_pixel_s []="set_pixel";
  static define_unary_function_eval (__set_pixel,&_set_pixel,_set_pixel_s);
  define_unary_function_ptr5( at_set_pixel ,alias_at_set_pixel,&__set_pixel,0,true);
  
inline void check_freeze(){
  if (!freeze){
    clear_screen();
    freeze=true;
  }
}  

  gen draw_line_or_rectangle(const gen & a_,GIAC_CONTEXT,int rect){
    check_freeze();
    gen a(a_);
    if (a.type==_STRNG && a.subtype==-1) return  a;
    if (a.type!=_VECT || a._VECTptr->size()<2)
      return gentypeerr(contextptr);
    const vecteur & v=*a._VECTptr;
    size_t vs=v.size();
    if (vs>=4){
      gen x0=v.front();
      gen y0=v[1];
      gen x1=v[2];
      gen y1=v[3];
      if (x0.type==_DOUBLE_)
	x0=int(x0._DOUBLE_val+.5);
      if (y0.type==_DOUBLE_)
	y0=int(y0._DOUBLE_val+.5);
      if (x1.type==_DOUBLE_)
	x1=int(x1._DOUBLE_val+.5);
      if (y1.type==_DOUBLE_)
	y1=int(y1._DOUBLE_val+.5);
      if (x0.type==_INT_ &&  y0.type==_INT_ && x1.type==_INT_ && y1.type==_INT_){
	if (rect){
	  int attr=vs==4?FL_BLACK:remove_at_display(v[4]).val;
	  if (rect==2 || (attr & 0x40000000) )
	    draw_rectangle(x0.val,y0.val,x1.val,y1.val,attr & 0xffff);
	  else {
	    draw_line(x0.val,y0.val,x0.val+x1.val,y0.val,attr & 0xffff);
	    draw_line(x0.val+x1.val,y0.val,x0.val+x1.val,y0.val+y1.val,attr & 0xffff);
	    draw_line(x0.val+x1.val,y0.val+y1.val,x0.val,y0.val+y1.val,attr & 0xffff);
	    draw_line(x0.val,y0.val,x0.val,y0.val+y1.val,attr & 0xffff);
	  }	    
	}
	else
	  draw_line(x0.val,y0.val,x1.val,y1.val,vs==4?FL_BLACK:remove_at_display(v[4]).val);
	return 1;
      }
    }
    return gensizeerr(contextptr);
    //static gen PIXEL(identificateur("PIXON_P"));
    //return _of(makesequence(PIXEL,a_),contextptr);
  }
  gen _draw_line(const gen & a_,GIAC_CONTEXT){
    return draw_line_or_rectangle(a_,contextptr,0);
  }
  static const char _draw_line_s []="draw_line";
  static define_unary_function_eval (__draw_line,&_draw_line,_draw_line_s);
  define_unary_function_ptr5( at_draw_line ,alias_at_draw_line,&__draw_line,0,true);

  gen _draw_rectangle(const gen & a_,GIAC_CONTEXT){
    return draw_line_or_rectangle(a_,contextptr,1);
  }
  static const char _draw_rectangle_s []="draw_rectangle";
  static define_unary_function_eval (__draw_rectangle,&_draw_rectangle,_draw_rectangle_s);
  define_unary_function_ptr5( at_draw_rectangle ,alias_at_draw_rectangle,&__draw_rectangle,0,true);

  gen _fill_rect(const gen & a_,GIAC_CONTEXT){
#if 0 // def XLIGHT
    return undef;
#endif
    return draw_line_or_rectangle(a_,contextptr,2);
  }
  static const char _fill_rect_s []="fill_rect";
  static define_unary_function_eval (__fill_rect,&_fill_rect,_fill_rect_s);
  define_unary_function_ptr5( at_fill_rect ,alias_at_fill_rect,&__fill_rect,0,true);

  void draw_filled_arc(int x,int y,int rx,int ry,int color,int theta1_deg,int theta2_deg,int xmin,int xmax,int ymin,int ymax,bool segment){
    // approximation by a filled polygon
    // points: (x,y), (x+rx*cos(theta)/2,y+ry*sin(theta)/2) theta=theta1..theta2
    while (theta2_deg<theta1_deg)
      theta2_deg+=360;
    if (theta2_deg-theta1_deg>=360){
      theta1_deg=0;
      theta2_deg=360;
    }
    int N0=theta2_deg-theta1_deg+1;
    // reduce N if rx or ry is small
    double red=double(rx)/1024*double(ry)/768;
    if (red>1) red=1;
    if (red<0.1) red=0.1;
    int N=red*N0;
    if (N<5)
      N=N0>5?5:N0;
    if (N<2)
      N=2;
    vector< vector<int> > v(segment?N+1:N+2,vector<int>(2));
    int i=0;
    if (!segment){
      v[0][0]=x;
      v[0][1]=y;
      ++i;
    }
    double theta=theta1_deg*M_PI/180;
    double thetastep=(theta2_deg-theta1_deg)*M_PI/(180*(N-1));
    for (;i<v.size()-1;++i){
      v[i][0]=int(x+rx*std::cos(theta)+.5);
      v[i][1]=int(y-ry*std::sin(theta)+.5); // y is inverted
      theta += thetastep;
    }
    v.back()=v.front();
    draw_filled_polygon(v,xmin,xmax,ymin,ymax,color);
  }    

  gen _draw_arc(const gen & a_,bool arc,GIAC_CONTEXT){
#if 0 // def XLIGHT
    return undef;
#endif
    check_freeze();
    gen a(a_);
    if (a.type==_STRNG && a.subtype==-1) return  a;
    if (a.type!=_VECT || a._VECTptr->size()<2)
      return gentypeerr(contextptr);
    const vecteur & v=*a._VECTptr;
    size_t vs=v.size();
    if (arc && vs<6)
      return gendimerr(contextptr);
    if (vs>=3){
      gen x0=v.front();
      gen y0=v[1];
      gen r=v[2];
      if (x0.type==_DOUBLE_)
	x0=int(x0._DOUBLE_val+.5);
      if (y0.type==_DOUBLE_)
	y0=int(y0._DOUBLE_val+.5);
      if (r.type==_DOUBLE_)
	r=int(r._DOUBLE_val+.5);
      int attr=vs==(arc?6:3)?FL_BLACK:remove_at_display(v.back()).val;
      if (x0.type==_INT_ &&  y0.type==_INT_ && r.type==_INT_){
#ifndef XLIGHT
	if (arc){
	  gen ry=v[3];
	  if (ry.type==_DOUBLE_)
	    ry=int(ry._DOUBLE_val+.5);
	  gen theta1=evalf_double(v[4],1,contextptr);
	  gen theta2=evalf_double(v[5],1,contextptr);
	  if (attr & 0x40000000)
	    draw_filled_arc(x0.val,y0.val,r.val,ry.val,attr & 0xffff,int(theta1._DOUBLE_val*180/M_PI+.5),int(theta2._DOUBLE_val*180/M_PI+.5));
	  draw_arc(x0.val,y0.val,r.val,ry.val,attr & 0xffff,theta1._DOUBLE_val,theta2._DOUBLE_val,contextptr);
	}
	else
#endif
          {
            if (attr & 0x40000000)
              draw_filled_circle(x0.val,y0.val,r.val,attr &0xffff);
            else
              draw_circle(x0.val,y0.val,r.val,attr & 0xffff);
          }
	return 1;
      }
    }
    return gensizeerr(contextptr);
    //static gen PIXEL(identificateur("PIXON_P"));
    //return _of(makesequence(PIXEL,a_),contextptr);
  }
  gen _draw_circle(const gen & a_,GIAC_CONTEXT){
#if 0 // def XLIGHT
    return undef;
#endif
    return _draw_arc(a_,false,contextptr);
  }
  static const char _draw_circle_s []="draw_circle";
  static define_unary_function_eval (__draw_circle,&_draw_circle,_draw_circle_s);
  define_unary_function_ptr5( at_draw_circle ,alias_at_draw_circle,&__draw_circle,0,true);

  gen _draw_arc(const gen & a_,GIAC_CONTEXT){
#ifdef XLIGHT
    return undef;
#endif
    return _draw_arc(a_,true,contextptr);
  }
  static const char _draw_arc_s []="draw_arc";
  static define_unary_function_eval (__draw_arc,&_draw_arc,_draw_arc_s);
  define_unary_function_ptr5( at_draw_arc ,alias_at_draw_arc,&__draw_arc,0,true);

  int asc_sort_int(const void * vptr,const void *wptr){
    const vector<int> * v=(const vector<int> * )vptr;
    const vector<int> * w=(const vector<int> * )wptr;
    for (size_t i=0;i<v->size();++i){
      int vi=(*v)[i];
      int wi=(*w)[i];
      if (vi!=wi)
	return vi<wi?-1:1;
    }
    return 0;
  }

  int asc_sort_double(const void * vptr,const void *wptr){
    const vector<double> * v=(const vector<double> * )vptr;
    const vector<double> * w=(const vector<double> * )wptr;
    for (size_t i=0;i<v->size();++i){
      double vi=(*v)[i];
      double wi=(*w)[i];
      if (abs(vi-wi)>1e-6*abs(wi))
	return vi<wi?-1:1;
    }
    return 0;
  }

  // L might be modified by closing the polygon
  void draw_filled_polygon(vector< vector<int> > &L,int xmin,int xmax,int ymin,int ymax,int color){
    int n=L.size();
    // close polygon if it is open
    if (L[n-1]!=L[0])
      L.push_back(L[0]);
    else
      n--;
    // ordered list of ymin,x,index (ordered by ascending ymin)
    vector< vector<int> > om(n,vector<int>(4)); // size==12K for n==384 
    for (int j=0;j<n;j++){
      int y0=L[j][1],y1=L[j+1][1];
      om[j][0]=y0<y1?y0:y1;
      om[j][1]=y0<y1?L[j][0]:L[j+1][0];
      om[j][2]=j;
      om[j][3]=y0<y1?j:(j==n-1?0:j+1);
    }
    qsort(&om.front(),om.size(),sizeof(vector<int>),asc_sort_int);
    // vreverse(om.begin(),om.end());
    vector<double> p(n); // inverses of slopes
    for (int j=0;j<n;j++){
      double dx=L[j+1][0]-L[j][0];
      double dy=L[j+1][1]-L[j][1];
      p[j]=dy==0?(dx>0?1e300:-1e300):dx/dy;
    }
    // initialization, lowest horizontal that is crossing the polygon
    // y at ymin-1, that way lxj is initialized in the loop
    int y=om[0][0]-1,j,ompos=0;
    vector< vector<double> > lxj; // size about 12K for n==384
    // main loop
    for (;y<ymax;){
      if (y>=ymin){ // draw pixels for this horizontal frame
	size_t lxjs=lxj.size();
	qsort(&lxj.front(),lxjs,sizeof(vector<double>),asc_sort_double);
	bool odd=false;
	vector<char> impair(lxjs);
	for (size_t k=0;k<lxjs;++k){
	  int arete=lxj[k][1]; // edge L[arete]->L[arete+1]
	  int y1=L[arete][1],y2=L[arete+1][1];
	  if (y!=y1 && y!=y2)
	    odd=!odd;
	  else {
	    int ym=giacmin(y1,y2);
	    if ( y1!=y2 && (ym==y || ym==y)){
	      odd=!odd;
	    }
	  }
	  impair[k]=odd;
	}
	for (size_t k=0;k<lxjs;++k){
	  if (impair[k]){
	    int x1=giacmax(xmin,int(lxj[k][0]+.5));
	    int x2=k==lxjs-1?xmax:giacmin(xmax,int(lxj[k+1][0]+.5));
#if 0
	    unsigned short* VRAM = (unsigned short*)GetVRAMAddress();
	    unsigned short * VRAMend = VRAM;
	    VRAM += (y*LCD_WIDTH_PX + x1);
	    VRAMend += (y*LCD_WIDTH_PX + x2);
	    for (;VRAM<=VRAMend;++VRAM){
	      *VRAM=color;
	    }
#else
	    for (;x1<=x2;++x1)
	      set_pixel(x1,y,color);
#endif
	  }
	}
      } // end if y>=ymin
      y++;
      if (y>=ymax) break;
      // update lxj
      for (j=0;j<lxj.size();++j){
	int k=lxj[j][1];
	if (y<=giacmax(L[k][1],L[k+1][1]))
	  lxj[j][0] += p[k];
	else {
	  lxj.erase(lxj.begin()+j);
	  --j;
	}
      }
      // new edges
      for (j=ompos;j<n;++j){
	ompos=j;
	if (om[j][0]>y)
	  break;
	if (om[j][0]<y)
	  continue;
	vector<double> add(2,om[j][1]);
	add[1]=om[j][2];
	lxj.push_back(add);
      }
    } // end for (;y<ymax;)
  }
#ifndef TICE
  void draw_polygon(vector< vector<int> > & v1,int color){
    if (v1.back()!=v1.front())
      v1.push_back(v1.front());
    int n=v1.size()-1;
    for (int i=0;i<n;++i){
      int x1=v1[i][0],y1=v1[i][1],x2=v1[i+1][0],y2=v1[i+1][1];
      draw_line(x1,y1,x2,y2,color);
    }
  }
#endif
  gen _draw_polygon(const gen & a,GIAC_CONTEXT){
#if 0 // def XLIGHT
    return undef;
#endif
    check_freeze();
    if (a.type==_STRNG && a.subtype==-1) return  a;
    if (a.type!=_VECT || a._VECTptr->size()<2)
      return gentypeerr(contextptr);
    const vecteur & v=*a._VECTptr;
    vector< vector<int> > v1;
    if (ckmatrix(v) && v.front()._VECTptr->size()==2){
      if (!vecteur2vectvector_int(v,0,v1))
        return gensizeerr(contextptr);
      draw_polygon(v1,(int)FL_BLACK);
      return 1;
    }
    gen g(v[0]);
    if (!ckmatrix(g) || g._VECTptr->front()._VECTptr->size()!=2 || !vecteur2vectvector_int(*g._VECTptr,0,v1))
      return gensizeerr(contextptr);
    int attr=remove_at_display(v.back()).val;
    if (0 && attr & 0x40000000) // crash
      draw_filled_polygon(v1,0,LCD_WIDTH_PX,0,LCD_HEIGHT_PX,attr & 0xffff);
    else
      draw_polygon(v1,attr & 0xffff);
    return 1;
  }
  static const char _draw_polygon_s []="draw_polygon";
  static define_unary_function_eval (__draw_polygon,&_draw_polygon,_draw_polygon_s);
  define_unary_function_ptr5( at_draw_polygon ,alias_at_draw_polygon,&__draw_polygon,0,true);

  void printCentered(const char* text, int y) {
    int len = strlen(text);
    int x = LCD_WIDTH_PX/2-(len*6)/2;
    PrintXY(x,y,(const Char *)text,0);
  }
  
  
  gen _draw_string(const gen & a,GIAC_CONTEXT){
#if 0 // def XLIGHT
    return undef;
#endif
    check_freeze();
    if (a.type==_STRNG && a.subtype==-1) return  a;
    if (a.type!=_VECT)
      return gensizeerr(contextptr);
    vecteur v(*a._VECTptr);
    if (v.size()!=3 && v.size()!=4)
      return gendimerr(contextptr);
    if (v[0].type!=_STRNG || !is_integral(v[1]) || !is_integral(v[2]))
      return gensizeerr(contextptr);
    if (!freeze){
      clear_screen(); // Bdisp_AllClr_VRAM();
      freeze=true;
    }
    int x=v[1].val,y=v[2].val,c=TEXT_COLOR_BLACK;
    if (v.size()==4){
      if (!is_integral(v[3]))
	return gensizeerr(contextptr);
      c=v[3].val;
    }
    Printmini(x, y, (const Char*)v[0]._STRNGptr->c_str(), c);
    return 1;
  }
  static const char _draw_string_s []="draw_string";
  static define_unary_function_eval (__draw_string,&_draw_string,_draw_string_s);
  define_unary_function_ptr5( at_draw_string ,alias_at_draw_string,&__draw_string,0,true);

  gen _get_pixel(const gen & a,GIAC_CONTEXT){
#if 0 // def XLIGHT
    return undef;
#endif
    if (a.type==_STRNG && a.subtype==-1) return  a;
    if (a.type!=_VECT || a._VECTptr->size()!=2)
      return gentypeerr(contextptr);
    const vecteur & v=*a._VECTptr;
    gen x=v.front();
    gen y=v[1];
    if (x.type==_DOUBLE_)
      x=int(x._DOUBLE_val+.5);
    if (y.type==_DOUBLE_)
      y=int(y._DOUBLE_val+.5);
    if (x.type==_INT_ &&  y.type==_INT_ )
      return get_pixel(x.val,y.val);
    return undef;
  }
  static const char _get_pixel_s []="get_pixel";
  static define_unary_function_eval (__get_pixel,&_get_pixel,_get_pixel_s);
  define_unary_function_ptr5( at_get_pixel ,alias_at_get_pixel,&__get_pixel,0,true);

  gen _dtype(const gen & args,GIAC_CONTEXT){
    gen g(args);
    while (g.type==_VECT && !g._VECTptr->empty())
      g=g._VECTptr->front();
    return change_subtype(g.type,_INT_TYPE);
  }
  static const char _dtype_s []="dtype";
  static define_unary_function_eval (__dtype,&_dtype,_dtype_s);
  define_unary_function_ptr5( at_dtype ,alias_at_dtype,&__dtype,0,true);

  gen _rgb(const gen & args,GIAC_CONTEXT){
#ifdef XLIGHT
    return undef;
#endif
    if (args.type==_VECT && args._VECTptr->size()==3){
      const vecteur & v=*args._VECTptr;
      gen a(v[0]);
      gen b(v[1]);
      gen c(v[2]);
      if (a.type==_DOUBLE_ || b.type==_DOUBLE_ || c.type==_DOUBLE_){
	a=_floor(255*a+.5,contextptr);
	b=_floor(255*b+.5,contextptr);
	c=_floor(255*c+.5,contextptr);
      }
      if (a.type==_INT_ && b.type==_INT_ && c.type==_INT_ && a.val>=0 && a.val<256 && b.val>=0 && b.val<256 && c.val>=0 && c.val<256 ){
	int d=0;
	d=(((giacmin(a.val,255)*32)/256)<<11) | (((giacmin(b.val,255)*64)/256)<<5) | ((giacmin(c.val,255)*32)/256);
	if (d>0 && d<512)
	  d += (1<<11);
	return d;
      }
    }
    return gensizeerr(contextptr);
  }
  static const char _rgb_s []="rgb";
  static define_unary_function_eval (__rgb,&_rgb,_rgb_s);
  define_unary_function_ptr5( at_rgb ,alias_at_rgb,&__rgb,0,true);

  gen binop(const gen & g,int type,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type==_VECT){
      const vecteur & v=*g._VECTptr;
      if (v.size()==2 && v.front().type==_INT_ && v.back().type==_INT_){
	switch (type){
	case 0:
	  return v.front().val & v.back().val;
	case 1:
	  return v.front().val | v.back().val;
	case 2:
	  return v.front().val ^ v.back().val;
	}
      }
    }
    return gensizeerr(contextptr);
  }
  gen _bitand(const gen & g,GIAC_CONTEXT){
    return binop(g,0,contextptr);
  }
  static const char _bitand_s []="bitand";
  static define_unary_function_eval (__bitand,&_bitand,_bitand_s);
  define_unary_function_ptr5( at_bitand ,alias_at_bitand,&__bitand,0,true);

  gen _bitor(const gen & g,GIAC_CONTEXT){
    return binop(g,1,contextptr);
  }
  static const char _bitor_s []="bitor";
  static define_unary_function_eval (__bitor,&_bitor,_bitor_s);
  define_unary_function_ptr5( at_bitor ,alias_at_bitor,&__bitor,0,true);

  gen _bitxor(const gen & g,GIAC_CONTEXT){
    return binop(g,2,contextptr);
  }
  static const char _bitxor_s []="bitxor";
  static define_unary_function_eval (__bitxor,&_bitxor,_bitxor_s);
  define_unary_function_ptr5( at_bitxor ,alias_at_bitxor,&__bitxor,0,true);

  gen _bitnot(const gen & g,GIAC_CONTEXT){
    if (g.type!=_INT_)
      return gensizeerr(contextptr);
    return ~g.val;
  }
  static const char _bitnot_s []="bitnot";
  static define_unary_function_eval (__bitnot,&_bitnot,_bitnot_s);
  define_unary_function_ptr5( at_bitnot ,alias_at_bitnot,&__bitnot,0,true);

#ifndef XLIGHT
  // ploarea(polygone), plotarea(f(x),x=a..b), plotarea(f(x),x=a..b,n,method)
  // method=trapeze,point_milieu,rectangle_gauche,rectangle_droit
  gen _plotarea(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    vecteur v(gen2vecteur(g));
    vecteur attributs(1,COLOR_BLACK);
    int s=read_attributs(v,attributs,contextptr);
    if (!s)
      return gensizeerr(contextptr);
    if (attributs.size()<2)
      attributs.push_back(0);
    if (attributs[0].type==_INT_)
      attributs[0].val= attributs[0].val | _FILL_POLYGON;
    v[0]=remove_at_pnt(v[0]);
    if (s>=2 && v[0].type!=_VECT){
      gen tmp(v[1]),a,b,x(vx_var());
      if (is_equal(tmp) && tmp._SYMBptr->feuille.type==_VECT && tmp._SYMBptr->feuille._VECTptr->size()==2){
	x=tmp._SYMBptr->feuille[0];
	tmp=tmp._SYMBptr->feuille[1];
      }
      if (tmp.is_interval() && tmp._SYMBptr->feuille.type==_VECT && tmp._SYMBptr->feuille._VECTptr->size()==2){
	a=tmp._SYMBptr->feuille[0];
	b=tmp._SYMBptr->feuille[1];
      }
      else
	return gensizeerr(gettext("plotarea(f(x),x=a..b[,n,method])"));
      if (s>=2){
	int s1=s-1;
	for (;s1>0;--s1){
	  if (v[s1].type!=_INT_)
	    break;
	}
	gen graph=funcplotfunc(gen(vecteur(v.begin(),v.begin()+s1+1),_SEQ__VECT),false,contextptr); // must be a graph of fcn
	if (is_undef(graph))
	  return graph;
	// extract polygon
	gen graphe=remove_at_pnt(graph);
	if (graphe.type==_VECT && graphe._VECTptr->size()==2)
	  graphe=symbolic(at_curve,makesequence(v.front(),graphe));
	if (graphe.is_symb_of_sommet(at_curve) && graphe._SYMBptr->feuille.type==_VECT){
	  vecteur & graphev=*graphe._SYMBptr->feuille._VECTptr;
	  if (graphev.size()>1){
	    gen polyg=graphev[1];
	    if (polyg.type==_VECT){
	      if (s==2){
		// add verticals and horizontal
		vecteur res(*polyg._VECTptr);
		res.insert(res.begin(),a);
		res.insert(res.begin(),b);
		res.push_back(b);
		int nd=decimal_digits(contextptr);
		decimal_digits(3,contextptr);
		attributs[1]=string2gen(_gaussquad(gen(makevecteur(v[0],v[1]),_SEQ__VECT),contextptr).print(contextptr),false);
		decimal_digits(nd,contextptr);
		return pnt_attrib(gen(res,_GROUP__VECT),attributs,contextptr);
	      } // end s==2
	      if (s>=3)
		v[2]=_floor(v[2],contextptr);
	      if (s>=3 && v[2].type==_INT_){
		int n=v[2].val;
		if (n<1)
		  return gensizeerr(contextptr);
		vecteur res;
		res.push_back(b);
		res.push_back(a);
		gen dx=(b-a)/n,x0=a,xf=x0,fxf,f=v[0],A;
		int method=_TRAPEZE;
		if (s>=4 && v[3].type==_INT_)
		  method = v[3].val;
		if (method==_RECTANGLE_DROIT || method==_RECTANGLE_GAUCHE || method==_POINT_MILIEU){
		  if (method==_RECTANGLE_DROIT)
		    xf=x0+dx;
		  if (method==_POINT_MILIEU)
		    xf=x0+dx/2;
		  for (int i=0;i<n;++i){
		    fxf=evalf(quotesubst(f,x,xf,contextptr),1,contextptr);
		    A=A+dx*fxf;
		    res.push_back(x0+fxf*cst_i);
		    x0=x0+dx;
		    xf=xf+dx;
		    res.push_back(x0+fxf*cst_i);
		  }
		}
		if (method==_TRAPEZE){
		  fxf=evalf(quotesubst(f,x,xf,contextptr),1,contextptr);
		  A=dx*fxf/2;
		  res.push_back(xf+fxf*cst_i);
		  xf=x0+dx;
		  for (int i=0;i<n-1;++i){
		    fxf=evalf(quotesubst(f,x,xf,contextptr),1,contextptr);
		    A=A+dx*fxf;
		    res.push_back(xf+fxf*cst_i);
		    x0=x0+dx;
		    xf=xf+dx;
		  }
		  fxf=evalf(quotesubst(f,x,b,contextptr),1,contextptr);
		  A=A+dx*fxf/2;
		  res.push_back(b+fxf*cst_i);
		}
		res.push_back(b);
		*logptr(contextptr) << "Approx area " << A << "\n";
		return makesequence(A,pnt_attrib(res,attributs,contextptr),_couleur(makesequence(graph,_RED+_DASH_LINE+_LINE_WIDTH_3),contextptr));
	      } // end if (s>=3)
	    } // end polyg.type==_VECT
	  }
	}
      } // end s>=2
    }
    return gensizeerr(gettext(""));
  }
  static const char _plotarea_s []="plotarea";
  static define_unary_function_eval (__plotarea,&_plotarea,_plotarea_s);
  define_unary_function_ptr5( at_plotarea ,alias_at_plotarea,&__plotarea,0,true);
#endif
  
#ifdef EMCC_FETCH
  // with emscripten 1.37.28, it does not work
#include <emscripten/fetch.h>

  string fetch(const string & url){
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_SYNCHRONOUS;
    emscripten_fetch_t *fetch = emscripten_fetch(&attr, url.c_str()); // Blocks here until the operation is complete.
    COUT << "status, bytes: " << fetch->status << "," << fetch->numBytes << "\n";
    if (fetch->status == 200) {
      string fetch_string="";
      for (int i=0;i< fetch->numBytes;++i)
	fetch_string += char(fetch->data[i]);
      return fetch_string;
    }
    return "Failed";
  }
    
#else
#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/curlbuild.h>
  size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream) {
    string data((const char*) ptr, (size_t) size * nmemb);
    *((stringstream*) stream) << data << "\n";
    return size * nmemb;
  }
  string fetch(const string & url){
    void * curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    /* example.com is redirected, so we tell libcurl to follow redirection */
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1); //Prevent "longjmp causes uninitialized stack frame" bug
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "deflate");
    std::stringstream out;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
    /* Perform the request, res will get the return code */
    CURLcode res = curl_easy_perform(curl);
    /* Check for errors */
    if (res != CURLE_OK) {
      string s=string("Failure: ")+curl_easy_strerror(res);
      curl_easy_cleanup(curl);
      return s;
    }
    curl_easy_cleanup(curl);
    return out.str();
  }
#else
  string fetch(const string & url){
    return "Failed";
  }
#endif // HAVE_LIBCURL
#endif // EMCC_FETCH

#ifndef NO_NAMESPACE_GIAC
} // namespace giac
#endif // ndef NO_NAMESPACE_GIAC
