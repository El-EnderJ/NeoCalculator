/*
 * Modified for NumOS / NeoCalculator in 2026 for ESP32-S3 and host integration.
 * See ../NUMOS_CHANGES.md for provenance and the file-by-file history.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* -*- mode:C++ ; compile-command: "g++ -I. -I.. -I../include -g -c plot.cc -Wall -DIN_GIAC -DHAVE_CONFIG_H -DGIAC_GENERIC_CONSTANTS " -*- */
// NB: Using gnuplot optimally requires patching and recompiling gnuplot
// If you use the -DGNUPLOT_IO compile flag, you
// MUST compile gnuplot with interactive mode enabled, file src/plot.c
// line 448
/*
  diff plot.c plot.c~
  448c448
  <     interactive = TRUE; // isatty(fileno(stdin));
  ---
  >     interactive = isatty(fileno(stdin));
*/

/*
 *  Copyright (C) 2000/14 B. Parisse, Institut Fourier, 38402 St Martin d'Heres
 *  implicitplot3d code adapted from 
 *  http://astronomy.swin.edu.au/~pbourke/modelling/polygonise 
 *  by Paul Bourke and  Cory Gene Bloyd
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

#include "giacPCH.h"

using namespace std;
#if !defined NSPIRE && !defined FXCG && !defined TICE
#ifdef VISUALC13
#undef clock
#undef clock_t
#endif
#include <iomanip>
#endif
#include <fstream>
#include "vector.h"
#include <algorithm>
#include <cmath>

// C headers
#include <stdio.h>
#ifndef __VISUALC__ 
#if !defined RTOS_THREADX && !defined BESTA_OS && !defined FREERTOS && !defined FXCG && !defined TICE
#include <fcntl.h>
#endif
#endif // __VISUALC__
#ifdef FXCG
extern "C" {
#include <fx/libfx.h>
int dGetLine (char * s,int max, int isRecording=0, int ml=0);
}
#endif
#ifdef TICE
#include "k_csdk.h"
#endif
#if !defined FXCG && !defined TICE
#include "k_csdk.h"
#endif

// Giac headers
#include "gen.h"
#include "usual.h"
#include "prog.h"
#include "rpn.h"
#include "identificateur.h"
#include "subst.h"
#include "symbolic.h"
#include "derive.h"
#include "solve.h"
#include "intg.h"
#include "path.h"
#include "sym2poly.h"
#include "input_parser.h"
#include "input_lexer.h"
#include "ti89.h"
#include "isom.h"
#include "unary.h"
#include "plot.h"
#include "plot3d.h"
#include "ifactor.h"
#include "gauss.h"
#include "misc.h"
#include "lin.h"
#include "quater.h"
#include "giacintl.h"
#ifdef USE_GMP_REPLACEMENTS
#undef HAVE_GMPXX_H
#undef HAVE_LIBMPFR
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#ifndef BESTA_OS
#define clock_t int
//#define CLOCK() 0
#endif
#endif
#ifndef HAVE_NO_SYS_RESOURCE_WAIT_H
#include <sys/resource.h>
#include <sys/wait.h>
#endif

extern const int BUFFER_SIZE;

#ifndef NO_NAMESPACE_GIAC
namespace giac {
#endif // ndef NO_NAMESPACE_GIAC

  string gen2tex(const gen & args,GIAC_CONTEXT){
    return "undef";
  }

  string translate_underscore(const string & s){
    return s;
  }

#ifdef IPAQ
  int LEGENDE_SIZE=40;
  int COORD_SIZE=26;
  int PARAM_STEP=18;
#else // IPAQ
  int LEGENDE_SIZE=100;
  int COORD_SIZE=30;
  int PARAM_STEP=20;
#endif // IPAQ

#ifdef GNUWINCE
  ofstream * outptr=0;
#endif
  // int plot_instructionsh=300,plot_instructionsw=300;


  const int _GROUP__VECT_subtype[]={_GROUP__VECT,_LINE__VECT,_HALFLINE__VECT,_VECTOR__VECT,_POLYEDRE__VECT,_POINT__VECT,0};
#ifdef IPAQ
  double gnuplot_xmin=-4,gnuplot_xmax=4,gnuplot_ymin=-4,gnuplot_ymax=4,gnuplot_zmin=-5,gnuplot_zmax=5,gnuplot_tmin=-6,gnuplot_tmax=6,gnuplot_tstep=0.3;
#else
  double gnuplot_xmin=-5,gnuplot_xmax=5,gnuplot_ymin=-5,gnuplot_ymax=5,gnuplot_zmin=-5,gnuplot_zmax=5,gnuplot_tmin=-6,gnuplot_tmax=6,gnuplot_tstep=0.1;
#endif
  double global_window_xmin(gnuplot_xmin),global_window_xmax(gnuplot_xmax),global_window_ymin(gnuplot_ymin),global_window_ymax(gnuplot_ymax);
  double x_tick(1.0),y_tick(1.0);
  double class_minimum(0.0),class_size(1.0);
  int gnuplot_pixels_per_eval=LCD_WIDTH_PX/2-1; // divide by 2 on a ti83 (slow)
  bool autoscale=true;

  bool has_gnuplot=true;

  bool fastcurveprint=false;

  gen vect2c(const gen & g){
    if (g.type==_VECT && g._VECTptr->size()==2)
      return gen(g._VECTptr->front(),g._VECTptr->back());
    return g;
  }

  // return t such that tb+(1-t)a is the projection of c on [a,b] 
  gen projection(const gen & a,const gen & b,const gen & c,GIAC_CONTEXT){
    gen ax,ay,bx,by,cx,cy,abx,aby;
    reim(a,ax,ay,contextptr);
    reim(b,bx,by,contextptr);
    reim(c,cx,cy,contextptr);
    abx=ax-bx;aby=ay-by;
    gen num=(ax-cx)*abx+(ay-cy)*aby;
    gen den=abx*abx+aby*aby;
    // gen num=scalar_product(a-c,a-b,contextptr),den=scalar_product(b-a,b-a,contextptr);
    return rdiv(num,den,contextptr);
  }

  void rewrite_with_t_real(gen & eq,const gen & t,GIAC_CONTEXT){
    gen tx,ty; reim(t,tx,ty,contextptr);
    if (!is_zero(ty,contextptr)){
      eq=subst(eq,ty,zero,false,contextptr);
      eq=subst(eq,tx,t,false,contextptr);
    }
  }
  static gen rand_complex(){
    int i=std_rand(),j=std_rand();
    i=i/(RAND_MAX/10)-5;
    j=j/(RAND_MAX/10)-5;
    return gen(i,j);
  }

  gen abs_norm(const gen & g,GIAC_CONTEXT){
    if (g.type==_VECT)
      return sqrt(dotvecteur(*g._VECTptr,*g._VECTptr),contextptr);
    else
      return abs(g,contextptr);
  }

  gen abs_norm(const gen & a,const gen & b,GIAC_CONTEXT){
    if (a.type==_VECT)
      return abs_norm(b-a,contextptr);
    gen ax,ay,bx,by;
    reim(a,ax,ay,contextptr);
    reim(b,bx,by,contextptr);
    bx -= ax;
    by -= ay;
    return sqrt(bx*bx+by*by,contextptr);
  }

  gen abs_norm2(const gen & g,GIAC_CONTEXT){
    if (g.type==_VECT)
      return dotvecteur(*g._VECTptr,*g._VECTptr);
    else
      return ratnormal(_lin(g*conj(g,contextptr),contextptr),contextptr);
  }

  gen dotvecteur(const gen & a,const gen & b,GIAC_CONTEXT){
    return scalar_product(a,b,contextptr);
    /*
      if (a.type==_VECT)
      return dotvecteur(*a._VECTptr,*b._VECTptr);
      else
      return -im(a*conj(cst_i*b,contextptr),contextptr);    
    */
  }

  vecteur seq2vecteur(const gen & g){
    if (g.type==_VECT && g.subtype==_SEQ__VECT)
      return *g._VECTptr;
    else
      return vecteur(1,g);
  }
  vecteur gen2vecteur(const gen & args){
    if (args.type==_VECT)
      return *args._VECTptr;
    else
      return vecteur(1,args);
  }
  gen parameter2point(const vecteur & v,GIAC_CONTEXT){
    return v.front();
  }
  
  gen scalar_product(const gen & a0,const gen & b0,GIAC_CONTEXT){
    gen a(a0);
    gen b(b0);
    if (a.type==_VECT && b.type==_VECT)
      return scalarproduct(*a._VECTptr,*b._VECTptr,contextptr);
    gen ax,ay; reim(a,ax,ay,contextptr);
    gen bx,by; reim(b,bx,by,contextptr);
    return ax*bx+ay*by;
    // gen res=re(a,contextptr)*re(b,contextptr)+im(a,contextptr)*im(b,contextptr);
    // return res;
  }

  static void plotpreprocess(gen & g,vecteur & quoted,GIAC_CONTEXT){
    gen tmp=eval(g,contextptr);
    if (tmp.type==_IDNT){
      g=tmp;
      quoted=vecteur(1,tmp);
      return;
    }
    if (tmp.type==_VECT){
      bool done=true;
      const_iterateur it=tmp._VECTptr->begin(),itend=tmp._VECTptr->end();
      if (it!=itend){
	for (;it!=itend;++it){
	  if (it->type!=_IDNT && !it->is_symb_of_sommet(at_at))
	    break;
	}
	if (it==itend){
	  g=tmp;
	  quoted=*tmp._VECTptr;
	}
        else
	  done=false;
      }
      else
	done=false;
      if (!done){
	if (g.type==_VECT)
	  quoted=*g._VECTptr;
	else
	  quoted=vecteur(1,g);
      }
    }
    else {
      quoted=vecteur(1,g);
    }
  }

  vecteur quote_eval(const vecteur & v,const vecteur & quoted,GIAC_CONTEXT){
    /*
      vecteur l(quoted);
      lidnt(v,l);
      int qs=quoted.size();
      l=vecteur(l.begin()+qs,l.end());
      vecteur lnew=*eval(l,1,contextptr)._VECTptr;
      vecteur w=subst(v,l,lnew,true,contextptr);
      return w;
    */
    const_iterateur it=quoted.begin(),itend=quoted.end();
    vector<int> save;
    for (;it!=itend;++it){
      gen tmp=*it;
      if (is_equal(tmp))
	tmp=tmp._SYMBptr->feuille._VECTptr->front();
      if (tmp.type!=_IDNT)
	save.push_back(-1);
      else {
	if (contextptr && contextptr->quoted_global_vars){
	  contextptr->quoted_global_vars->push_back(tmp);
	  save.push_back(0);
	}
	else {
	  if (tmp._IDNTptr->quoted){
	    save.push_back(*tmp._IDNTptr->quoted);
	    *tmp._IDNTptr->quoted=1;
	  }
	  else
	    save.push_back(0);
	}
      }
    }
    vecteur res(v);
    int s=int(v.size());
    for (int i=0;i<s;++i){
#ifndef NO_STDEXCEPT
      try {
#endif
	bool done=false;
	if (v[i].is_prod() && v[i]._SYMBptr->feuille.type==_VECT){ // hack for polarplot using re(rho)
	  vecteur vi = *v[i]._SYMBptr->feuille._VECTptr;
	  if (!vi.empty() && vi.front().is_symb_of_sommet(at_re)){
	    vi.front()=vi.front()._SYMBptr->feuille;
	    gen tmp=eval(vi,contextptr);
	    if (tmp.type==_VECT){
	      vi=*tmp._VECTptr;
	      vi.front()=symbolic_re(vi.front());
	      res[i]=_prod(vi,contextptr);
	      done=true;
	    }
	  }
	}
	if (!done){
#if 0
	  vecteur lv=lidnt(v[i]);
	  vecteur lw(lv);
	  for (int j=0;j<int(lw.size());++j){
	    gen g=eval(lv[j],1,contextptr);
	    g=ifte2when(g,contextptr); 
	    lw[j]=g;
	  }
	  res[i]=quotesubst(v[i],lv,lw,contextptr); // otherwise plots with if else fails (error not catched with emscripten)
#endif
	  res[i]=eval(v[i],contextptr); 
	}
#ifndef NO_STDEXCEPT
      } catch (std::runtime_error & ){
	last_evaled_argptr(contextptr)=NULL;
	//    *logptr(contextptr) << e.what() << "\n";
      }
#endif
    }
    it=quoted.begin();
    for (int i=0;it!=itend;++it,++i){
      if (save[i]>=0){
	if (contextptr && contextptr->quoted_global_vars)
	  contextptr->quoted_global_vars->pop_back();
	else {
	  gen tmp=*it;
	  if (is_equal(tmp))
	    tmp=tmp._SYMBptr->feuille._VECTptr->front();
	  if (tmp.type==_IDNT && tmp._IDNTptr->quoted)
	    *tmp._IDNTptr->quoted=save[i]>0?save[i]:0;
	}
      }
    }
    return res;
  }

  // args -> vector
  // add vx_var() if args is not a seq
  // evaluate v[1], if it's not an idnt or a vector of idnt keep v[1]
  // evaluate v with v[1] quoted
  vecteur plotpreprocess(const gen & args,GIAC_CONTEXT){
    vecteur v;
    if (args.type==_FUNC)
      return makevecteur(args(vx_var(),contextptr),vx_var());
    gen var,res;
    if (args.type!=_VECT && is_algebraic_program(args,var,res))
      return makevecteur(args,symb_interval(gnuplot_xmin,gnuplot_xmax));
    int nd;
#ifndef XLIGHT
    if ( (nd=is_distribution(args)) ){
      gen a,b;
      if (distrib_support(nd,a,b,true))
	return makevecteur(args,symb_interval(a,b));
    }
#endif
    if ((args.type!=_VECT) || (args.subtype!=_SEQ__VECT) )
      v=makevecteur(args,vx_var());
    else {
      v=*args._VECTptr;
      if (v.empty())
	return vecteur(1,gensizeerr(contextptr));
      if (v.size()==1)
	v.push_back(vx_var());
    }
    // find quoted variables from v[1]
    vecteur quoted;
    if ( v[1].type==_SYMB && (v[1]._SYMBptr->sommet==at_equal || v[1]._SYMBptr->sommet==at_equal2 ||v[1]._SYMBptr->sommet==at_same ))
      plotpreprocess(v[1]._SYMBptr->feuille._VECTptr->front(),quoted,contextptr);
    else
      plotpreprocess(v[1],quoted,contextptr);
    return quote_eval(v,quoted,contextptr);
  }


  // should be print_string?
  std::string gen2string(const gen & g){
    if (g.type==_STRNG)
      return *g._STRNGptr;
    else
      return g.print(context0);
  }

  string print_DOUBLE_(double d,unsigned ndigits){
    char s[256];
    ndigits=ndigits<2?2:ndigits;
    ndigits=ndigits>15?15:ndigits;
    sprintfdouble(s,("%."+print_INT_(ndigits)+"g").c_str(),d);
    // search for a decimal point in s
    int l=strlen(s),i;
    for (i=0;i<l;++i){
      if (s[i]=='.')
	break;
    }
    i += ndigits+1;
    if (i<l){
      int j;
      for (j=i;j<l;++j){
	// exponent?
	if (s[j]=='e' || s[j]=='E'){
	  // move from j to l -> i
	  for (;j<l;++j,++i){
	    s[i]=s[j];
	  }
	  break;
	}
      }
      s[i]=0;
    }
    return s;
  }

  gen symb_pnt(const gen & x,const gen & c,GIAC_CONTEXT){
    if (!x.is_symb_of_sommet(at_curve) && is_undef(x)) return x;
    gen ee = (symbolic(at_pnt,gen(makenewvecteur(x,c),_PNT__VECT)));
    ee.subtype=-1;
    return ee;
  }
  gen symb_pnt(const gen & x,GIAC_CONTEXT){
    return symb_pnt(x,gen(FL_BLACK),contextptr); // 0 instead of FL_BLACK
  }
  gen symb_pnt_name(const gen & x,const gen & c,const gen & nom,GIAC_CONTEXT){
    symbolic e=symbolic(at_pnt,gen(makevecteur(x,c,nom),_PNT__VECT));
    gen ee(e);
    ee.subtype=-1;
    return ee;
  }
  gen pnt_attrib(const gen & point,const vecteur & attributs,GIAC_CONTEXT){
    if (is_undef(point))
      return point;
    if (attributs.empty())
      return symb_pnt(point,COLOR_BLACK,contextptr);
    int s=int(attributs.size());
    if (s==1)
      return symb_pnt(point,attributs[0],contextptr);
    if (s>=3)
      return symb_pnt_name(point,symbolic(at_couleur,attributs),attributs[1],contextptr);
    return symb_pnt_name(point,attributs[0],attributs[1],contextptr);
  }

  bool centre_rayon(const gen & cercle,gen & centre,gen & rayon,bool absrayon,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return false;
#endif
    gen c=remove_at_pnt(cercle);
    if ( (c.type!=_SYMB) || (c._SYMBptr->sommet!=at_cercle))
      return false;
    gen diam=remove_at_pnt(c._SYMBptr->feuille._VECTptr->front());
    if (diam.type!=_VECT)
      return false;
    gen a=remove_at_pnt(diam._VECTptr->front());
    gen b=remove_at_pnt(diam._VECTptr->back());
    centre=recursive_normal(ratnormal(rdiv(a+b,plus_two,contextptr),contextptr),contextptr);
    rayon=rdiv(b-a,plus_two,contextptr);
    if (absrayon)
      rayon=abs(recursive_normal(ratnormal(rayon,contextptr),contextptr),contextptr);
    return true;
  }

  // for a point nothing, segment/line/vect->1st point
  // circle/sphere->diam
  gen get_point(const gen & g,int n,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return g;
#endif
    gen tmp=remove_at_pnt(g);
    bool sphere=false;
    if (tmp.is_symb_of_sommet(at_cercle) || sphere){
      gen c=remove_at_pnt(tmp);
      gen f=c._SYMBptr->feuille;
      if (f.type==_VECT && f._VECTptr->size()>=3)
	f=f._VECTptr->front();
      if (f.type!=_VECT || f._VECTptr->size()!=2)
	return undef;
      gen c1=f._VECTptr->front(),c2=f._VECTptr->back();
      if (n==0 && !sphere)
	return (c1+c2)/2;
      return c1;
    }
    if (tmp.is_symb_of_sommet(at_curve))
      return gensizeerr(contextptr);
    if (tmp.type!=_VECT)
      return tmp;
    vecteur & v =*tmp._VECTptr;
    int s=int(v.size());
    if (tmp.subtype==_POINT__VECT || (tmp.subtype==0 && (s==2 || s==3)) ){
      if (s==2)
	return gen(v[0],v[1]);
      return tmp;
    }
    if (n>=s)
      n=s-1;
    if (!s)
      return undef;
    return v[n];
  }

  // arg=real/complex or (real/complex,label) or same+color
  static gen pointonoff(const gen & args,const vecteur & attributs,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return undef;
#endif
    gen e,a(args.evalf(eval_level(contextptr),contextptr)); 
    if ( (a.is_real(contextptr)) || (a.type==_CPLX) ){
      if (attributs.size()<=1)
	e=symb_pnt(args.eval(eval_level(contextptr),contextptr),attributs,contextptr);
      else
	e=symb_pnt_name(args.eval(eval_level(contextptr),contextptr),attributs[0],attributs[1],contextptr);
    }
    else { 
      if (args.type!=_VECT)
	return symb_pnt(args,attributs,contextptr);
      int s=int(args._VECTptr->size());
      if ( (s!=2) && (s!=3) )
	return gensizeerr(gettext("pointon"));
      gen x=args._VECTptr->front(),y=(*args._VECTptr)[1],c;
      if ( (s==3) || (y.type==_STRNG))
	e=symbolic(at_pnt,args);
      else 
	e=symb_pnt_name(x,attributs[0].val,y,contextptr);
    }
    return e;
  }

  gen _point(const gen & args,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return undef;
#endif
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if ( (args.type==_SYMB) && (args._SYMBptr->sommet==at_pnt))
      return args;
    vecteur attributs(1,COLOR_BLACK | _QUADRANT3);
    if (args.type==_VECT) {
      int s=read_attributs(*args._VECTptr,attributs,contextptr);
      vecteur v(args._VECTptr->begin(),args._VECTptr->begin()+s);
      if (s<1)
	return gendimerr(contextptr);
      bool ismat=ckmatrix(v);
      if (has_i(v) || ismat){
	if ( (v.size()==2 || v.size()==3) && ismat && v.front()._VECTptr->size()>3){
	  v=mtran(v);
	  for (int i=0;i<int(v.size());++i)
	    v[i]=put_attributs(_point(v[i],contextptr),attributs,contextptr);
	  return gen(v,_SEQ__VECT);
	}
	for (int i=0;i<s;++i){
	  if (v[i].type==_VECT)
	    v[i]=put_attributs(_point(v[i],contextptr),attributs,contextptr);
	  else
	    v[i]=pnt_attrib(v[i],attributs,contextptr);
	}
	if (s==1)
	  return v[0];
	return gen(v,_SEQ__VECT);
      }
      if (s==1){
	gen arg1=args._VECTptr->front();
	if (arg1.type==_VECT){
	  if (arg1._VECTptr->size()==2)
	    arg1=gen(arg1._VECTptr->front(),arg1._VECTptr->back());
	  else {
	    arg1=gen(*arg1._VECTptr,_POINT__VECT);
	  }
	}
	return pnt_attrib(arg1,attributs,contextptr);
      }
      if (s==2){
	if (args._VECTptr->front().type==_VECT || args._VECTptr->back().type==_VECT)
	  return gensizeerr(contextptr);
	return pointonoff(vect2c(args),attributs,contextptr);
      }
      return pnt_attrib(gen(v,_POINT__VECT),attributs,contextptr);
    }
    return pnt_attrib(args,attributs,contextptr);
  }
  static const char _point_s []="point";
  static define_unary_function_eval_index (26,__point,&_point,_point_s);
  define_unary_function_ptr5( at_point ,alias_at_point,&__point,0,true);

  static gen _droite_segment(const gen & args,int subtype,const vecteur & attributs,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return undef;
#endif
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    gen e;
    vecteur res(*args._VECTptr);
    iterateur it=res.begin(),itend=res.end();
    for (;it!=itend;++it){
      if (it->is_equal())
	*it=_droite(*it,contextptr);
      bool ispnt=it->is_symb_of_sommet(at_pnt);
      *it=remove_at_pnt(*it);
      if (it->type==_VECT){
	if (it->_VECTptr->size()==2){
	  if (ispnt){
	    if (it->subtype==_LINE__VECT || it->subtype==_HALFLINE__VECT || it->subtype==_VECTOR__VECT)
	      *it=res[0]+it->_VECTptr->back()-it->_VECTptr->front();
	    else
	      *it=(it->_VECTptr->front()+it->_VECTptr->back())/2;
	  }
	  else {
	    *it=vect2c(*it);
	    if (it!=res.begin())
	      *it=*it+res[0];
	  }
	}
	else {
	  if (!ispnt && it!=res.begin() && it->subtype!=_POINT__VECT)
	    *it=*it+res[0];
	  it->subtype=_POINT__VECT;
	}
      }
    }
    if (res.size()==2 && is_zero(res.front()-res.back(),contextptr) && subtype!=_GROUP__VECT && subtype!=_VECTOR__VECT)
      return undef;
    e=pnt_attrib(gen(res,subtype),attributs,contextptr);
    return e;
  }

  static gen bit_and(const gen & a,unsigned mask){
    if (a.type==_INT_)
      return int(unsigned(a.val) & mask);
    if (a.type==_VECT){
      vecteur res;
      const_iterateur it=a._VECTptr->begin(),itend=a._VECTptr->end();
      for (;it!=itend;++it)
	res.push_back(bit_and(*it,mask));
      return res;
    }
    return a;
  }

  static gen bit_ori(const gen & a,unsigned mask){
    if (a.type==_INT_)
      return int(unsigned(a.val) | mask);
    if (a.type==_VECT){
      vecteur res;
      const_iterateur it=a._VECTptr->begin(),itend=a._VECTptr->end();
      for (;it!=itend;++it)
	res.push_back(bit_ori(*it,mask));
      return res;
    }
    return a;
  }

  static gen bit_orv(const gen & a,const vecteur & v){
    if (a.type==_INT_)
      return bit_ori(v,a.val);
    if (a.type==_VECT){
      vecteur res;
      const_iterateur it=a._VECTptr->begin(),itend=a._VECTptr->end(),jt=v.begin(),jtend=v.end();
      for (;it!=itend && jt!=jtend;++it,++jt){
	if (jt->type==_INT_)
	  res.push_back(bit_ori(*it,jt->val));
	else
	  res.push_back(*it);
      }
      for (;it!=itend;++it){
	res.push_back(*it);
      }
      return res;
    }
    return a;
  }

  bool ck_parameter(const gen & g,GIAC_CONTEXT){
    if ( (g.type==_IDNT) && (g.evalf(1,contextptr)!=g) ){
      *logptr(contextptr) << gettext("Variable ")+g.print(contextptr)+gettext(" should be purged") << "\n";
      return false;
    }
    return true;
  }

  bool ck_parameter_x(GIAC_CONTEXT){
    return ck_parameter(x__IDNT_e,contextptr);
  }

  bool ck_parameter_y(GIAC_CONTEXT){
    return ck_parameter(y__IDNT_e,contextptr);
  }

  bool ck_parameter_t(GIAC_CONTEXT){
    return ck_parameter(t__IDNT_e,contextptr);
  }

  static void read_option(const vecteur & v,double xmin,double xmax,double ymin,double ymax,double zmin,double zmax,vecteur & attributs, int & nstep,int & jstep,int & kstep,bool unfactored,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return ;
#endif
    read_attributs(v,attributs,contextptr);
    const_iterateur it=v.begin(),itend=v.end();
    for (;it!=itend;++it){
      if (it->type==_VECT){
	read_option(*it->_VECTptr,xmin,xmax,ymin,ymax,zmin,zmax,attributs,nstep,jstep,kstep,contextptr);
	continue;
      }
      if (it->subtype==_INT_SOLVER && *it==_UNFACTORED)
	unfactored=true;
      if (!is_equal(*it))
	continue;
      gen & opt=it->_SYMBptr->feuille;
      if (opt.type!=_VECT || opt._VECTptr->size()!=2)
	continue;
      gen opt1=opt._VECTptr->front(),opt2=opt._VECTptr->back();
      if ( opt1.type!=_INT_)
	continue;
      switch (opt1.val){
      case _NSTEP:
	if (opt2.type==_INT_)
	  nstep=abs(opt2,context0).val; // ok
	break;
      case _XSTEP:
	opt2=evalf_double(abs(opt2,context0),2,context0); // ok
	if (opt2.type==_DOUBLE_){
	  nstep=int((xmax-xmin)/opt2._DOUBLE_val+.5);
	  if (!jstep)
	    jstep=nstep;
	}
	break;
      case _YSTEP:
	opt2=evalf_double(abs(opt2,context0),2,context0); // ok
	if (opt2.type==_DOUBLE_)
	  jstep=int((ymax-ymin)/opt2._DOUBLE_val+.5);
	break;
      }
    }
  }

  void read_option(const vecteur & v,double xmin,double xmax,double ymin,double ymax,double zmin,double zmax,vecteur & attributs, int & nstep,int & jstep,int & kstep,GIAC_CONTEXT){
    bool unfactored=false;
    read_option(v,xmin,xmax,ymin,ymax,zmin,zmax,attributs,nstep,jstep,kstep,unfactored,contextptr);
  }

  static void read_option(const vecteur & v,double xmin,double xmax,double ymin,double ymax,vecteur & attributs, int & nstep,int & jstep,GIAC_CONTEXT){
    double zmin=gnuplot_zmin,zmax=gnuplot_zmax; int kstep=0;
    bool unfactored=false;
    read_option(v,xmin,xmax,ymin,ymax,zmin,zmax,attributs,nstep,jstep,kstep,unfactored,contextptr);
  }

  void evalfdouble2reim(const gen & a,gen & e,gen & f0,gen & f1,GIAC_CONTEXT){
    if (a.type==_CPLX){
      if (a.subtype==3){
	e=a;
	f0=*a._CPLXptr;
	f1=*(a._CPLXptr+1);
	return;
      }
      f0=a._CPLXptr->evalf2double(1,contextptr);
      f1=(a._CPLXptr+1)->evalf2double(1,contextptr);
      if (a._CPLXptr->type==_DOUBLE_ && (a._CPLXptr+1)->type==_DOUBLE_)
	e=a;
      else
	e=gen(f0._DOUBLE_val,f1._DOUBLE_val);
      return ;
    }
#ifndef NO_STDEXCEPT
    try {
#endif
      e=a.evalf_double(1,contextptr); // FIXME? level 1 does not work for non 0 context
#ifndef NO_STDEXCEPT
    } catch (std::runtime_error & error ){
      last_evaled_argptr(contextptr)=NULL;
      CERR << error.what() << "\n";
    }
#endif
    if (e.type==_CPLX){
      f0=*e._CPLXptr;
      f1=*(e._CPLXptr+1);
    }
    else {
      f0=e;
      f1=0.0;
    }
  }

  static bool in_autoscale(const gen & g,vector<double> & vx,vector<double> & vy,vector<double> & vz,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return false;
#endif
    if (g.type==_CPLX && g.subtype==3){
      vx.push_back(g._CPLXptr->_DOUBLE_val);
      vy.push_back((g._CPLXptr+1)->_DOUBLE_val);
      return false;
    }
#ifndef XLIGHT //3d
    if (g.type==_VECT && g.subtype==_POINT__VECT && g._VECTptr->size()==3){
      vecteur v=*evalf_double(g,1,contextptr)._VECTptr;
      if (v[2].type==_CPLX)
	v[2]=abs(v[2],contextptr);
      if (v[0].type==_DOUBLE_ && v[1].type==_DOUBLE_ && v[2].type==_DOUBLE_ ){
	vx.push_back(v[0]._DOUBLE_val);
	vy.push_back(v[1]._DOUBLE_val);
	vz.push_back(v[2]._DOUBLE_val);
	return false;
      }
    }
#endif
    if (g.type==_VECT ){
      bool ortho=false;
      const_iterateur it=g._VECTptr->begin(),itend=g._VECTptr->end();
      for (;it!=itend;++it){
	ortho = ortho | in_autoscale(*it,vx,vy,vz,contextptr);
      }
      return ortho;
    }
    if (g.is_symb_of_sommet(at_curve)){
      gen & gf=g._SYMBptr->feuille;
      if (gf.type==_VECT && gf._VECTptr->size()>1){
	gen tmp=(*gf._VECTptr)[1];
	if (is_undef(tmp)){
	  tmp=(*gf._VECTptr)[0];
	  if (tmp.type==_VECT && tmp._VECTptr->size()>4)
	    tmp=(*tmp._VECTptr)[4];
	}
	in_autoscale(tmp,vx,vy,vz,contextptr);
      }
      return false;
    }
    if (g.is_symb_of_sommet(at_cercle)){
      gen c,r;
      centre_rayon(g,c,r,false,contextptr);
      if (is_zero(r)) r=1;
      vecteur v;
      if (g._SYMBptr->feuille.type==_VECT && g._SYMBptr->feuille._VECTptr->size()>=3){
	v=*g._SYMBptr->feuille._VECTptr;
	gen v2=evalf_double(v[2],1,contextptr);
	gen v1=evalf_double(v[1],1,contextptr);
	gen delta=v2-v1;
	v=makevecteur(c,c+r*expi((v1-0.1*delta),contextptr),
		      c+r*expi(v1,contextptr),
		      c+r*expi((3*v1+v2)/gen(4),contextptr),
		      c+r*expi((v1+v2)/gen(2),contextptr),
		      c+r*expi((v1+3*v2)/gen(4),contextptr),
		      c+r*expi(v2,contextptr),
		      c+r*expi((v2+0.1*delta),contextptr)); 
      }
      else
	v=makevecteur(c-r,c+r,gen(c,-r),gen(c,r)); 
      in_autoscale(v,vx,vy,vz,contextptr);
      return true;
    }
    // FIXME sphere etc.
    if (g.type!=_VECT){
      gen e,f0,f1;
      evalfdouble2reim(g,e,f0,f1,contextptr);
      if (f0.type==_DOUBLE_ && f1.type==_DOUBLE_){
	vx.push_back(f0._DOUBLE_val);
	vy.push_back(f1._DOUBLE_val);
      }
    }
    return false;
  }

  bool autoscaleg(const gen & g,vector<double> & vx,vector<double> & vy,vector<double> & vz,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return false;
#endif
    if (g.type==_VECT){
      bool ortho=false;
      const_iterateur it=g._VECTptr->begin(),itend=g._VECTptr->end();
      vx.reserve(vx.size()+itend-it);
      vy.reserve(vy.size()+itend-it);
      for (;it!=itend;++it)
	ortho = ortho | autoscaleg(*it,vx,vy,vz,contextptr);
      return ortho;
    }
    if (g.is_symb_of_sommet(at_pnt)){
      gen & gf=g._SYMBptr->feuille;
      if (gf.type==_VECT && !gf._VECTptr->empty()){
	gen & f=gf._VECTptr->front();
	return in_autoscale(f,vx,vy,vz,contextptr);
      }
    }
    if (g.is_equal() && g._SYMBptr->feuille.type==_VECT && g._SYMBptr->feuille._VECTptr->size()==2 && g._SYMBptr->feuille._VECTptr->front()==_GL_ORTHO && !is_zero(g._SYMBptr->feuille._VECTptr->back()))
      return true;
    return false;
  }

  static void zoom(double &m,double & M,double d){
    double x_center=(M+m)/2;
    double dx=(M-m);
    double s=absdouble(m)+absdouble(M);
    if (dx<=1e-5*s){
      dx=s;
      if (dx<=1e-5)
	dx=1;
    }
    dx *= d/2;
    m = x_center - dx;
    M = x_center + dx;
  }

  void autoscaleminmax(vector<double> & v,double & m,double & M,bool fullview){
#ifndef WITH_PLOT
    return ;
#endif
    int s=int(v.size());
    if (s==0){
      v.push_back(0);
      ++s;
    }
    if (s==1){
      v.push_back(v.front());
      ++s;
    }
    if (s>1){    
      sort(v.begin(),v.end());
      m=v[s/10];
      M=v[9*s/10];
      bool b=(M-m)<0.01*(v[s-1]-v[0]);
      if (fullview || 1.75*(M-m)>(v[s-1]-v[0]) || b){
	if (b)
	  zoom(m,M,3);
	else {
	  M=v[s-1];
	  m=v[0];
	  zoom(m,M,1.1);
	}
      }
      else
	zoom(m,M,1/0.8);
      // include origin if not too far
      if ( (m>-M/10 && m<M/2) || (m<-M/10 && m>M/2) )
	m=-M/10;
    }
  }

  vecteur get_style(const vecteur & v,string & legende){
    int s=int(v.size());
    vecteur style(1,int(FL_BLACK));
#ifndef WITH_PLOT
    return style;
#endif
    if (s>=3)
      legende=gen2string(v[2]);
    if (s>1){
      gen f1(v[1]);
      if ( f1.type==_VECT && !f1._VECTptr->empty() )
	f1=f1._VECTptr->front();
      int typ=f1.type;
      if (typ==_INT_ || typ==_ZINT)
	style.front()=f1;
      if ( typ==_SYMB ){
	gen & f2 =f1._SYMBptr->feuille;
	if (f2.type==_VECT)
	  style=*f2._VECTptr;
	else
	  style.front()=f2;
      }
    }
    return style;
  }

  // read color like attributs and returns the first attribut index
  int read_attributs(const vecteur & v,vecteur & attributs,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return 0;
#endif
    if (attributs.empty())
      attributs.push_back(COLOR_BLACK);
    const_iterateur it=v.begin(),itend=v.end();
    int s=int(itend-it),smax(s);
    for (;it!=itend;++it){
      if (*it==at_normalize){
	s=int(it-v.begin());
	attributs.push_back(*it);
	continue;
      }
      if (it->type==_VECT){
	if (read_attributs(*it->_VECTptr,attributs,contextptr)!=int(it->_VECTptr->size()))
	  s=int(it-v.begin());
	continue;
      }
      gen opt;
      if (it->is_symb_of_sommet(at_label)){
	opt=it->_SYMBptr->feuille;
	if (opt.is_symb_of_sommet(at_nop))
	  opt=makevecteur(at_legende,opt._SYMBptr->feuille);
      }
      else {
	if (!is_equal(*it))
	  continue;
	opt=it->_SYMBptr->feuille;
      }
      opt=eval(opt,1,contextptr);
      if (opt.type!=_VECT || opt._VECTptr->size()!=2)
	continue;
      gen opt1=opt._VECTptr->front(),opt2=opt._VECTptr->back().eval(1,0);
      unsigned colormask=0xffff0000;
      if (opt1.type==_IDNT){
	const char * s1 =opt1._IDNTptr->id_name;
	if (strlen(s1)==1){
	  if (s1[0]=='c')
	    opt1=at_couleur;
	  if (s1[0]=='s')
	    opt1=at_size;
	}
	if (strcmp(s1,"marker")==0){
	  if (opt2.type==_STRNG){
	    const string s2 =*opt2._STRNGptr;
	    opt2=0;
	    opt1=_STYLE; opt1.subtype=_INT_COLOR;
	    if (s2.size()==1){
	      switch (s2[0]){
	      case 's': case 'o':
          opt2=(int)_POINT_CARRE;
		break;
	      case 'x':
		opt2=0;
		break;
	      case '*':
		opt2=(int)_POINT_ETOILE;
		break;
	      case '+':
		opt2=(int)_POINT_PLUS;
		break;
	      case 'D':
		opt2=(int)_POINT_LOSANGE;
		break;
	      case 'v': case '^':
		opt2=(int)_POINT_TRIANGLE;
		break;
	      case '.':
		opt2=(int)_POINT_POINT;
		break;
	      }
	    }
	  }
	}
	if (strcmp(s1,"linewidth")==0){
	  opt2=_round(opt2,contextptr);
	  if (opt2.type==_INT_ && opt2.val>0 && opt2.val<8){
	    opt1=_THICKNESS;
	    opt1.subtype=_INT_COLOR;
	  }
	}
      }
      if (opt2.type==_STRNG && opt2._STRNGptr->size()>1){
	const string & s=*opt2._STRNGptr;
	if (s.size()==2){
	  if (s=="--")
	    opt2=(int)_DASHDOT_LINE;
	  if (s=="-.")
	    opt2=(int)_DASHDOTDOT_LINE;
	}
	else {
	  if (opt1!=at_label && opt1!=at_legende)
	    opt2=gen(s,contextptr);
	}
      }
      if (opt1==at_size && opt2.type==_INT_){
	int s=opt2.val;
	if (s<4)
	  opt2=(int)_POINT_WIDTH_1;
	if (s>=4 && s<9)
	  opt2=(int)_POINT_WIDTH_2;
	if (s>=9 && s<16)
	  opt2=(int)_POINT_WIDTH_3;
	if (s>=16 && s<25)
	  opt2=(int)_POINT_WIDTH_4;
	if (s>=25 && s<36)
	  opt2=(int)_POINT_WIDTH_5;
	if (s>=36 && s<49)
	  opt2=(int)_POINT_WIDTH_6;
	if (s>=49 && s<64)
	  opt2=(int)_POINT_WIDTH_7;
	if (s>=64)
	  opt2=(int)_POINT_WIDTH_8;
	opt1=_STYLE; opt1.subtype=_INT_COLOR;
	colormask=0xffffffff;
      }
      if (opt1==at_couleur || opt1==at_display){
	if (opt2.type==_STRNG && opt2._STRNGptr->size()==1){
	  switch ((*opt2._STRNGptr)[0]){
	  case 'r':
	    opt2=(int)_RED;
	    break;
	  case 'b':
	    opt2=(int)_BLUE;
	    break;
	  case 'g':
	    opt2=(int)_GREEN;
	    break;
	  case 'c':
	    opt2=(int)_CYAN;
	    break;
	  case 'm':
	    opt2=(int)_MAGENTA;
	    break;
	  case 'y':
	    opt2=(int)_YELLOW;
	    break;
	  case 'k':
	    opt2=(int)_BLACK;
	    break;
	  case 'w':
	    opt2=(int)_WHITE;
	    break;
	  }
	}
	opt1=_COLOR; opt1.subtype=_INT_COLOR;
      }
      if (opt1==at_legende){
	opt1=_LEGEND;
	opt1.subtype=_INT_COLOR;
      }
      if (opt1.type==_DOUBLE_)
	opt1=gen(int(opt1._DOUBLE_val));
      if (opt2.type==_DOUBLE_ && opt1.val!=_LEGEND)
	opt2=gen(int(opt2._DOUBLE_val));
      if ( opt1.type!=_INT_ || opt1.subtype==0 
	   // || (opt1.subtype==_INT_PLOT && opt1.val>=_GL_X && opt1.val<=_GL_Z)
	   ) 
	continue;
      if (s==smax)
	s=int(it-v.begin());
      switch (opt1.val){
      case _COLOR:
	if (opt2.type==_INT_)
	  attributs[0]=bit_ori(bit_and(attributs[0],0xcfff0000),opt2.val);
	if (opt2.type==_VECT)
	  attributs[0]=bit_orv(bit_and(attributs[0],0xcfff0000),*opt2._VECTptr);
	break;
      case _STYLE:
	if (opt2.type==_INT_)
	  attributs[0]=bit_ori(attributs[0],opt2.val);
	break;
      case _LINESTYLE:
	if (opt2==at_point)
	  opt2=(int)_DOT_LINE;
	if (opt2==at_neg)
	  opt2=0;
	if (opt2.type==_INT_)
	  attributs[0]=bit_ori(bit_and(attributs[0],0xfe3fffff),opt2.val);
	break;
      case _THICKNESS:
	attributs[0]=bit_and(attributs[0], 0xfff8ffff);
	attributs[0]=bit_ori(attributs[0],_LINE_WIDTH_2*(bit_and(opt2,0x7).val-1));
	break;
      case _LEGEND:
	if (attributs.size()>1)
	  attributs[1]=opt2;
	else
	  attributs.push_back(opt2);
	break;
      }
    }
    return s;
  }

  gen put_attributs(const gen & lieu_g,const vecteur & attributs,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return undef;
#endif
    if (is_undef(lieu_g))
      return lieu_g;
    if (lieu_g.is_program())
      return lieu_g;
    gen lieu_geo=remove_at_pnt(lieu_g);
    if (!lieu_g.is_symb_of_sommet(at_pnt) && lieu_geo.type==_VECT && (lieu_geo.subtype<_LINE__VECT || lieu_geo.subtype>_HALFLINE__VECT) ){
      const_iterateur it=lieu_geo._VECTptr->begin(),itend=lieu_geo._VECTptr->end();
      vecteur res;
      res.reserve(itend-it);
      for (;it!=itend;++it){
	res.push_back(put_attributs(*it,attributs,contextptr));
      }
      return gen(res,lieu_geo.subtype);
    }
    return pnt_attrib(lieu_geo,attributs,contextptr);
  }

  gen symb_curve(const gen & source,const gen & plot){
    return symbolic(at_curve,gen(makevecteur(source,plot),_GROUP__VECT));
  }
  gen _curve(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    return symbolic(at_curve,args);
  }
  static const char _curve_s []="curve";
  static define_unary_function_eval (__curve,&_curve,_curve_s);
  define_unary_function_ptr5( at_curve ,alias_at_curve,&__curve,0,true);

  gen _segment(const gen & args,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return undef;
#endif
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return gensizeerr(contextptr);
    vecteur attributs(1,COLOR_BLACK);
    int s=read_attributs(*args._VECTptr,attributs,contextptr);
    if (s<2)
      return gendimerr(contextptr);
    vecteur v = *args._VECTptr;
    gen seg=makesequence(v[0],v[1]);
    if (s==4){
      v[0]=remove_at_pnt(v[0]);
      v[1]=remove_at_pnt(v[1]);
      gen name;
#ifndef NO_STDEXCEPT
      try {
#endif
	name=gen(v[2].print(contextptr)+v[3].print(contextptr),contextptr);
#ifndef NO_STDEXCEPT
      }
      catch (std::runtime_error & ){
	last_evaled_argptr(contextptr)=NULL;
	name=undef;
      }
#endif
      vecteur w;
      if (v[2].type>=_IDNT)
	w.push_back(eval(symb_sto(_point(v[0],contextptr),v[2]),contextptr));
      if (v[3].type>=_IDNT)
	w.push_back(eval(symb_sto(_point(v[1],contextptr),v[3]),contextptr));
      if (name.type!=_IDNT)
	w.push_back(_droite_segment(seg,_GROUP__VECT,attributs,contextptr));
      else
	w.push_back(eval(symb_sto(_droite_segment(seg,_GROUP__VECT,attributs,contextptr),name),contextptr));
      return gen(w,_GROUP__VECT);
      // return _droite_segment(gen(makevecteur(v[0]+cst_i*v[1],v[2]+cst_i*v[3]),_SEQ__VECT),_GROUP__VECT);
    }
    return _droite_segment(seg,_GROUP__VECT,attributs,contextptr);
  }
  static const char _segment_s []="segment";
  static define_unary_function_eval_index (28,__segment,&_segment,_segment_s);
  define_unary_function_ptr5( at_segment ,alias_at_segment,&__segment,0,true);
  gen symb_segment(const gen & x,const gen & y,const vecteur & c,int type,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return undef;
#endif
    gen e;
    if (c.empty())
      e=symbolic(at_pnt,gen(makevecteur(gen(makevecteur(x,y),type),COLOR_BLACK),_PNT__VECT));
    if (c.size()==1 || is_zero(c[1],contextptr))
      e=symbolic(at_pnt,gen(makevecteur(gen(makevecteur(x,y),type),c[0]),_PNT__VECT));
    else
      e=symbolic(at_pnt,gen(makevecteur(gen(makevecteur(x,y),type),c[0],c[1]),_PNT__VECT));
    gen ee(e);
    ee.subtype=-1;
    return ee;
  }

  gen remove_at_pnt(const gen & e){
    int t=e.type;
    if (t==_VECT && e.subtype==_GGB__VECT){
      if (e._VECTptr->size()==2)
	return vect2c(e);
      if (e._VECTptr->size()==3)
	return change_subtype(e,_POINT__VECT);
    }
    if (t==_SYMB && e._SYMBptr->sommet==at_pnt){
      const gen & f = e._SYMBptr->feuille;
      if (f.type==_VECT){
	const vecteur & v = *f._VECTptr;
	if (!v.empty())
	  return v.front();
      }
      return gensizeerr("Bad pnt argument");
    }
    return e;
  }

  gen _affixe(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type==_VECT && args._VECTptr->size()==2 && !args._VECTptr->front().is_symb_of_sommet(at_pnt))
      return vect2c(args);
    if (args.type==_VECT)
      return apply(args,_affixe,contextptr);
    gen g=remove_at_pnt(args);
    if (g.type==_VECT && g.subtype==_VECTOR__VECT && g._VECTptr->size()==2)
      return g._VECTptr->back()-g._VECTptr->front();
    return g;
  }
  static const char _affixe_s []="affix";
  static define_unary_function_eval (__affixe,&_affixe,_affixe_s);
  define_unary_function_ptr5( at_affixe ,alias_at_affixe,&__affixe,0,true);

  gen _slope(const gen & args,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return undef;
#endif
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    gen g=remove_at_pnt(args);
    if (g.type!=_VECT || g._VECTptr->size()!=2)
      return gensizeerr(contextptr);
    g=g._VECTptr->front()-g._VECTptr->back();
    if (g.type==_VECT)
      return gentypeerr(gettext("2-d instruction"));
    return normal(im(g,contextptr)/re(g,contextptr),contextptr);
  }
  static const char _slope_s []="slope";
  static define_unary_function_eval (__slope,&_slope,_slope_s);
  define_unary_function_ptr5( at_slope ,alias_at_slope,&__slope,0,true);

  vecteur eval_with_xy_quoted(const gen & args0,GIAC_CONTEXT){
    vecteur v(lidnt(args0));
    int xy=0, XY=0;
    for (unsigned i=0;i<v.size();++i){
      gen & g=v[i];
      if (g.type!=_IDNT || strlen(g._IDNTptr->id_name)!=1)
	continue;
      char ch=g._IDNTptr->id_name[0];
      if (ch=='x' || ch=='y')
	++xy;
      if (ch=='X' || ch=='Y')
	++XY;
    }
    if (xy || !XY){ // priority to x/y
      gen idx(identificateur("x")),idy(identificateur("y"));
      vecteur v(makevecteur(idx,idy));
      vecteur argv(makevecteur(args0,idx,idy));
      argv=quote_eval(argv,v,contextptr);
      return argv;
    }
    if (XY){
      gen idx(identificateur("X")),idy(identificateur("Y"));
      vecteur v(makevecteur(idx,idy));
      vecteur argv(makevecteur(args0,idx,idy));
      argv=quote_eval(argv,v,contextptr);
      return argv;
    }
    return vecteur(1,eval(args0,eval_level(contextptr),contextptr));
  }

  gen droite_by_equation(const vecteur & v,bool est_plan,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return undef;
#endif
    vecteur attributs(1,COLOR_BLACK);
    int s=read_attributs(v,attributs,contextptr);
    if (!s)
      return gendimerr(contextptr);
    gen eq(remove_equal(v[0])),x(x__IDNT_e),y(y__IDNT_e),z;
    if (s<2) ck_parameter_x(contextptr); else x=v[1];
    if (s<3) ck_parameter_y(contextptr); else y=v[2];
    if ( x.type!=_IDNT || y.type!=_IDNT)
      return gensizeerr(contextptr);
    gen eqx=normal(derive(eq,*x._IDNTptr,contextptr),contextptr);
    gen eqy=normal(derive(eq,*y._IDNTptr,contextptr),contextptr);
    if (is_undef(eqx)||is_undef(eqy))
      return eqx+eqy;
    vecteur eqxyz=makevecteur(eqx,eqy);
    // FIXME The test should be done with all derivatives
    if (!lvarx(eqxyz,x).empty() || !lvarx(eqxyz,y).empty())
      return gensizeerr(contextptr);
    if (is_zero(eqx,contextptr) && is_zero(eqy,contextptr))
      return gensizeerr(contextptr);
    // equation: eqx*x+eqy*y+eqz*z+cte=0
    gen cte,A,B;
    cte=subst(eq,makevecteur(x,y),vecteur(2,zero),false,contextptr);
    // 2-d line
    if (is_zero(eqy,contextptr)){
      eqx=-eqx;
      A=rdiv(cte,eqx,contextptr);
    }
    else
      A=cst_i*rdiv(-cte,eqy,contextptr);
    A=ratnormal(A,contextptr);
    B=A + eqy - eqx*cst_i;
    B=ratnormal(B,contextptr);
    gen e=pnt_attrib(makeline(A,B),//gen(makevecteur(A,B),_LINE__VECT),
		     attributs,contextptr);
    return e;
  }

  gen _droite(const gen & args0,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return undef;
#endif
    if (is_undef(args0)) return args0;
    if (args0.type==_SYMB || args0.type==_IDNT){
      // eval args with x/y or X/Y quoted
      vecteur argv=eval_with_xy_quoted(args0,contextptr);
      return droite_by_equation(argv,false,contextptr);
    }
    gen args=eval(args0,eval_level(contextptr),contextptr);
    if (args.type!=_VECT)
      return gentypeerr(contextptr);
    vecteur attributs(1,COLOR_BLACK);
    int s=read_attributs(*args._VECTptr,attributs,contextptr);
    if (s<1)
      return gendimerr(contextptr);
    gen & v0=args._VECTptr->front();
    if ( v0.type==_IDNT || is_equal(v0))
      return droite_by_equation(*args._VECTptr,false,contextptr);
    if (s<2)
      return gendimerr(contextptr);
    gen v1=(*args._VECTptr)[1];
    if (is_equal(v1) && v1._SYMBptr->feuille.type==_VECT){
      vecteur & v1v=*v1._SYMBptr->feuille._VECTptr;
      if (v1v.size()==2 && v1v[0]==at_slope){
	v0=_affixe(v0,contextptr);
	v1=v0+(1+cst_i*v1v[1]);
      }
    }
    return _droite_segment(makesequence(v0,v1),_LINE__VECT,attributs,contextptr);
  }
  static const char _droite_s []="line";
  static define_unary_function_eval_quoted (__droite,&_droite,_droite_s);
  define_unary_function_ptr5( at_droite ,alias_at_droite,&__droite,_QUOTE_ARGUMENTS,true);

  gen _plot_style(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    return symbolic(at_plot_style,args);
  }
  static const char _plot_style_s []="plot_style";
  static define_unary_function_eval_index (114,__plot_style,&_plot_style,_plot_style_s);
  define_unary_function_ptr5( at_plot_style ,alias_at_plot_style,&__plot_style,0,true);

  gen _parameter(const gen & args,GIAC_CONTEXT){
    if ( (args.type!=_VECT) || (args._VECTptr->size()<4))
      return gensizeerr(contextptr);
    return symbolic(at_parameter,args);
  }
  static const char _parameter_s []="parameter";
  static define_unary_function_eval_quoted (__parameter,&_parameter,_parameter_s);
  define_unary_function_ptr5( at_parameter ,alias_at_parameter,&__parameter,_QUOTE_ARGUMENTS,true);


  bool is3d(const gen & g){
    return false;
  }
  
  gen _coordonnees(const gen & args,GIAC_CONTEXT){
    if (args.type==_VECT) return args;
    gen P=remove_at_pnt(args);
    if (P.type==_VECT)
      return gensizeerr(contextptr);
    return makevecteur(re(P,contextptr),im(P,contextptr));
  }
  static const char _coordonnees_s []="coordonnees";
  static define_unary_function_eval (__coordonnees,&_coordonnees,_coordonnees_s);
  define_unary_function_ptr5( at_coordonnees ,alias_at_coordonnees,&__coordonnees,0,true);

  gen _sphere(const gen & args,GIAC_CONTEXT){
    return args;
  }
  static const char _sphere_s []="sphere";
  static define_unary_function_eval (__sphere,&_sphere,_sphere_s);
  define_unary_function_ptr5( at_sphere ,alias_at_sphere,&__sphere,0,true);

  gen _cylindre(const gen & args,GIAC_CONTEXT){
    return args;
  }
  static const char _cylindre_s []="cylindre";
  static define_unary_function_eval (__cylindre,&_cylindre,_cylindre_s);
  define_unary_function_ptr5( at_cylindre ,alias_at_cylindre,&__cylindre,0,true);

  gen _legende(const gen & args,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return undef;
#endif
    int s;
    if (args.type!=_VECT || (s=args._VECTptr->size())<2)
      return gensizeerr(contextptr);
    gen f=args._VECTptr->front();
#if 0
    if (f.type==_VECT){
      f = s==2?makevecteur(f,args._VECTptr->back(),FL_BLACK):args;
      return symbolic(at_legende,f);
    }
#endif
    f=remove_at_pnt(f);
    // *logptr(contextptr) << "legende " << f << "\n";
    return symbolic(at_pnt,makesequence(f,makevecteur(s==3?(*args._VECTptr)[2]:FL_BLACK),(*args._VECTptr)[1]));
  }
  static const char _legende_s []="legende";
  static define_unary_function_eval (__legende,&_legende,_legende_s);
  define_unary_function_ptr5( at_legende ,alias_at_legende,&__legende,0,true);

  gen _vector(const gen & args,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return undef;
#endif
    if ( is_undef(args)) return args;
    if (args.type!=_VECT || args.subtype!=_SEQ__VECT)
      return _vector(gen(vecteur(1,args),_SEQ__VECT),contextptr);
    vecteur attributs(1,COLOR_BLACK);
    int s=read_attributs(*args._VECTptr,attributs,contextptr);
    vecteur v = *args._VECTptr;
    if (!s)
      return gendimerr(contextptr);
    if (s==1){
      v=makevecteur(0*v[0],v[0]);
      ++s;
    }
    if (s==4){
      v[0]=makevecteur(v[0],v[1]);
      v[1]=makevecteur(v[2],v[3]);
      s=2;
    }
    if (s>=2 && s<=3 && v[0].type!=_VECT && v[1].type!=_VECT && !v[0].is_symb_of_sommet(at_pnt) && !v[1].is_symb_of_sommet(at_pnt) && is_zero(im(v[0],contextptr),contextptr) && is_zero(im(v[1],contextptr),contextptr)){
      if (s==2){
	v[1]=v[0]+cst_i*v[1];
	v[0]=0;
      }
      else {
	if (v[2].type!=_VECT && !v[2].is_symb_of_sommet(at_pnt) && is_zero(im(v[2],contextptr),contextptr)){
	  v[1]=v;
	  v[0]=makevecteur(0,0,0);
	  v.pop_back();
	}
      }
    }
    v[0]=remove_at_pnt(v[0]);
    if (v[1].type!=_VECT) {
      v[1]=remove_at_pnt(v[1]);
      if (v[0].type==_VECT && v[0]._VECTptr->size()==2)
	v[0]=v[0]._VECTptr->front()+cst_i*v[0]._VECTptr->back();
      if (v[1].type==_VECT && v[1].subtype==_VECTOR__VECT && v[1]._VECTptr->size()==2){
	vecteur & w=*v[1]._VECTptr;
	v[1]=v[0]+w[1]-w[0];
      }
      if (v[1].type==_VECT && v[1]._VECTptr->size()==2)
	v[1]=v[1]._VECTptr->front()+cst_i*v[1]._VECTptr->back();
    }
    gen seg=gen(makevecteur(v[0],v[1]),_SEQ__VECT);
    return _droite_segment(seg,_VECTOR__VECT,attributs,contextptr);
  }
  static const char _vector_s []="vector";
  static define_unary_function_eval (_pb_vector,(const gen_op_context &)&_vector,_vector_s);
  define_unary_function_ptr5( at_vector ,alias_at_vector,&_pb_vector,0,true);

  void read_tmintmaxtstep(vecteur & vargs,gen & t,int vstart,double &tmin,double & tmax,double &tstep,bool & tminmax_defined,bool & tstep_defined,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return ;
#endif
    tstep=gnuplot_tstep;
    tminmax_defined=false;
    tstep_defined=false;
    gen tmp;
    if (t.is_equal()){
      readrange(t,gnuplot_tmin,gnuplot_tmax,tmp,tmin,tmax,contextptr);
      tminmax_defined=true;
      t=t._SYMBptr->feuille._VECTptr->front();
    }
    int vs=int(vargs.size());
    for (int i=vstart;i<vs;++i){
      if (readvar(vargs[i])==t){
	readrange(vargs[i],gnuplot_tmin,gnuplot_tmax,tmp,tmin,tmax,contextptr);
	tminmax_defined=true;
	vargs.erase(vargs.begin()+i);
	--vs;
	--i;
      }
      if (vargs[i].is_equal() && vargs[i]._SYMBptr->feuille.type==_VECT && vargs[i]._SYMBptr->feuille._VECTptr->front().type==_INT_){
	gen n=vargs[i]._SYMBptr->feuille._VECTptr->back();
	if (vargs[i]._SYMBptr->feuille._VECTptr->front().val==_TSTEP){
	  n=evalf_double(n,1,contextptr);
	  tstep=absdouble(n._DOUBLE_val);
	  tstep_defined=true;
	  vargs.erase(vargs.begin()+i);
	  --vs;
	  --i;
	}
      }
    }
  }


  gen readvar(const gen & g){
    if (g.type==_IDNT)
      return g;
    if (!is_equal(g))
      return undef;
    gen & f=g._SYMBptr->feuille;
    if (f.type!=_VECT || f._VECTptr->size()!=2)
      return undef;
    return f._VECTptr->front();
  }

  bool chk_double_interval(const gen & g,double & inf,double & sup,GIAC_CONTEXT){
    gen h(g);
    if (!h.is_interval())
      return false;
    h=h._SYMBptr->feuille;
    if (h.type!=_VECT || h._VECTptr->size()!=2)
      return false;
    gen h1=evalf_double(h._VECTptr->front(),1,contextptr);
    gen h2=evalf_double(h._VECTptr->back(),1,contextptr);
    if (h1.type!=_DOUBLE_  || h2.type!=_DOUBLE_ )
      return false;
    inf=h1._DOUBLE_val;
    sup=h2._DOUBLE_val;
    return true;
  }

  bool readrange(const gen & g,double defaultxmin,double defaultxmax,gen & x, double & xmin, double & xmax,GIAC_CONTEXT){
    xmin=defaultxmin;
    xmax=defaultxmax;
    if (g.type==_IDNT){
      x=g;
      return true;
    }
    if (is_equal(g)){
      gen & f=g._SYMBptr->feuille;
      if (f.type!=_VECT)
	return false;
      vecteur & v=*f._VECTptr;
      if (v.size()!=2 || v[0].type!=_IDNT)
	return false;
      bool res= chk_double_interval(v[1],xmin,xmax,contextptr);
      x=v[0];
      return res;
    }
    return false;
  }
  gen _couleur(const gen & a,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return undef;
#endif
#ifdef RELEASE
    if (is_undef(a)) return a;
    if (a.type==_STRNG){
      *logptr(contextptr) << gettext("Use pencolor for the turtle") << "\n";
      return _couleur(gen(*a._STRNGptr,contextptr),contextptr);
    }
    if (a.type==_INT_){
      int i=COLOR_BLACK;
      return i;
    }
    if ( (a.type!=_VECT) || (a._VECTptr->size()<2))
      return COLOR_BLACK;
    gen c=a._VECTptr->back(),b;
    if (a._VECTptr->size()==3 && c.type==_INT_ && (b=a._VECTptr->front()).type==_INT_ && (*a._VECTptr)[1].type==_INT_){
      // 565 color
      int d=(((a.val*32)/256)<<11) | (((b.val*64)/256)<<5) | ((c.val*32)/256);
      if (d>0 && d<512){
	d += (1<<11);
      }
      return d;
    }
    if (a._VECTptr->size()>2)
      b=vecteur(a._VECTptr->begin(),a._VECTptr->end()-1);
    else
      b=a._VECTptr->front();
    if (b.type==_VECT){
      const_iterateur it=b._VECTptr->begin(),itend=b._VECTptr->end();
      vecteur res;
      res.reserve(itend-it);
      for (;it!=itend;++it)
	res.push_back(_couleur(makevecteur(*it,c),contextptr));
      return gen(res,b.subtype);
    }
    if ( (b.type!=_SYMB) || (b._SYMBptr->sommet!=at_pnt))
      return symbolic(at_couleur,a);      
    vecteur v(*b._SYMBptr->feuille._VECTptr);
    v[1]=c;
    gen e=symbolic(at_pnt,gen(v,_PNT__VECT));
    return e;
#else    
    return a;
#endif
  }
  static const char _display_s []="display";
  static define_unary_function_eval_index (160,__display,&_couleur,_display_s);
  define_unary_function_ptr5( at_display ,alias_at_display,&__display,0,true);

  static const char _couleur_s []="color";
  static define_unary_function_eval (__couleur,&_couleur,_couleur_s);
  define_unary_function_ptr5( at_couleur ,alias_at_couleur,&__couleur,0,true);

  int pixon_size=1; // global size, used in all sessions
  gen _pnt(const gen & args,GIAC_CONTEXT){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.is_symb_of_sommet(at_pnt))
      return args;
    if ( args.type==_VECT && args._VECTptr->size() ){
      vecteur v(*args._VECTptr);
      if (v.front().is_symb_of_sommet(at_pnt))
	return v.front();
      if (v.size()==3)
	v.pop_back();
      return symbolic(at_pnt,gen(v,_PNT__VECT));
    }
    return symbolic(at_pnt,args);
  }
  static const char _pnt_s []="pnt";
  static define_unary_function_eval_index (24,__pnt,&_pnt,_pnt_s);
  define_unary_function_ptr5( at_pnt ,alias_at_pnt,&__pnt,0,true);

  void local_sto_double(double value,const identificateur & i,GIAC_CONTEXT){
    control_c();
    if (contextptr)
      (*contextptr->tabptr)[i.id_name]=value;
    else
      i.localvalue->back()=value;
  }

  void local_sto_double_increment(double value,const identificateur & i,GIAC_CONTEXT){
    control_c();
    if (contextptr)
      (*contextptr->tabptr)[i.id_name] += value;
    else
      i.localvalue->back() += value;
  }

  const double max_nstep=1024;
  gen plotfunc(const gen & f_,const gen & vars,const vecteur & attributs,bool densityplot,double function_xmin,double function_xmax,double function_ymin,double function_ymax,double function_zmin, double function_zmax,int nstep,int jstep,bool showeq,const context * contextptr){
#ifndef WITH_PLOT
    return undef;
#endif
    if (f_.is_equal() || is_inequation(f_)){
      return string2gen("Try plot(["+f_._SYMBptr->feuille.print(contextptr)+"],"+vars.print(contextptr)+"). (In)equations can not be plotted.",false);
    }
    gen f=when2piecewise(f_,contextptr);
    f=Heavisidetopiecewise(f,contextptr); 
    double step=(function_xmax-function_xmin)/nstep;
    if (step<=0 || (function_xmax-function_xmin)/step>max_nstep)
      return gensizeerr(gettext("Plotfunc: unable to discretize: xmin, xmax, step=")+print_DOUBLE_(function_xmin,12)+","+print_DOUBLE_(function_xmax,12)+","+print_DOUBLE_(step,12)+gettext("\nTry a larger value for xstep"));
    vecteur res;
    int color=COLOR_BLACK;
    gen attribut=attributs.empty()?color:attributs[0];
    if (attribut.type==_INT_)
      color=attribut.val;
    if (f.type==_VECT){ // multi-plot
      vecteur & vf=*f._VECTptr;
      unsigned s=unsigned(vf.size());
      vecteur vattribut;
      if (attribut.type==_VECT)
	vattribut=gen2vecteur(attribut);
      for (unsigned j=0;j<s;++color,++j)
	vattribut.push_back(color*4096);
      vecteur res;
      for (unsigned i=0;i<s;++i){
	vecteur cur_attributs(1,vattribut[i]);
	if (attributs.size()>1 && attributs[1].type==_VECT && attributs[1]._VECTptr->size()>i)
	  cur_attributs.push_back((*attributs[1]._VECTptr)[i]);
	gen tmp=plotfunc(vf[i],vars,cur_attributs,false,function_xmin,function_xmax,function_ymin,function_ymax,function_zmin,function_zmax,nstep,jstep,showeq,contextptr);
	if (tmp.type==_VECT) 
	  res=mergevecteur(res,*tmp._VECTptr);
	else
	  res.push_back(tmp);
      }
      return res; // gen(res,_SEQ__VECT);
    }
    if (vars.type==_IDNT){ // function plot
      gen a,b;
      if (taille(f,100)<=100 && is_linear_wrt(f,vars,a,b,contextptr))	
	return put_attributs(_segment(makesequence(function_xmin+cst_i*(a*gen(function_xmin)+b),function_xmax+cst_i*(a*gen(function_xmax)+b)),contextptr),attributs,contextptr);
      vecteur lpiece(lop(f,at_piecewise));
      if (!lpiece.empty()) lpiece=lvarx(lpiece,vars);
      if (!lpiece.empty()){
	gen piece=lpiece.front();
	gen & piecef=piece._SYMBptr->feuille;
	if (piecef.type==_VECT){
	  vecteur piecev=*piecef._VECTptr,res;
	  // check conditions: they must be linear wrt x
	  double function_xmin_save=function_xmin,function_xmax_save=function_xmax;
	  int vs=int(piecev.size()),i;
	  for (i=0;i<vs/2;++i){
	    gen cond=piecev[2*i];
	    if (is_equal(cond) || cond.is_symb_of_sommet(at_same)){
	      *logptr(contextptr) << gettext("Assuming false condition ") << cond << "\n";
	      continue;
	    }
	    if (cond.is_symb_of_sommet(at_different)){
	      *logptr(contextptr) << gettext("Assuming true condition ") << cond << "\n";
	      f=quotesubst(f,piece,piecev[2*i+1],contextptr);
	      return plotfunc(f,vars,attributs,densityplot,function_xmin,function_xmax,function_ymin,function_ymax,function_zmin,function_zmax,nstep,jstep,showeq,contextptr);
	    }
	    bool unable=true;
	    if (cond.is_symb_of_sommet(at_superieur_strict) || cond.is_symb_of_sommet(at_superieur_egal)){
	      cond=cond._SYMBptr->feuille[0]-cond._SYMBptr->feuille[1];
	      unable=false;
	    }
	    if (cond.is_symb_of_sommet(at_inferieur_strict) || cond.is_symb_of_sommet(at_inferieur_egal)){
	      cond=cond._SYMBptr->feuille[1]-cond._SYMBptr->feuille[0];
	      unable=false;
	    }
	    gen a,b,l;
	    if (unable || !is_linear_wrt(cond,vars,a,b,contextptr))
	      break;
	    // check if a*x+b>0 on [borne_inf,borne_sup]
	    l=-b/a;
	    l=evalf_double(l,1,contextptr);
	    if (l.type!=_DOUBLE_)
	      break;
	    bool positif=ck_is_greater(a,0,contextptr);
	    gen tmp=quotesubst(f,piece,piecev[2*i+1],contextptr);
	    if (ck_is_greater(l,function_xmax,contextptr)){
	      // borne_inf < borne_sup <= l
	      if (positif) // test is false, continue
		continue;
	      // test is true make the plot
	      gen curres=plotfunc(tmp,vars,attributs,densityplot,function_xmin,function_xmax,function_ymin,function_ymax,function_zmin,function_zmax,nstep,jstep,showeq,contextptr);
	      if (curres.type==_VECT)
		res = mergevecteur(res,*curres._VECTptr);
	      else
		res.push_back(curres);
	      return res;
	    }
	    if (ck_is_greater(function_xmin,l,contextptr)){
	      // l <= borne_inf < borne_sup
	      if (!positif) // test is false, continue
		continue;
	      // test is true we can compute the integral
	      gen curres=plotfunc(tmp,vars,attributs,densityplot,function_xmin,function_xmax,function_ymin,function_ymax,function_zmin,function_zmax,nstep,jstep,showeq,contextptr);
	      if (curres.type==_VECT)
		res = mergevecteur(res,*curres._VECTptr);
	      else
		res.push_back(curres);
	      return res;
	    }
	    // borne_inf<l<borne_sup
	    if (positif){
	      // make plot between l and borne_sup
	      gen curres = plotfunc(tmp,vars,attributs,densityplot,l._DOUBLE_val,function_xmax,function_ymin,function_ymax,function_zmin,function_zmax,nstep,jstep,showeq,contextptr);
	      if (curres.type==_VECT)
		res = mergevecteur(res,*curres._VECTptr);
	      else
		res.push_back(curres);
	      function_xmax=l._DOUBLE_val; // continue with plot from borne_inf to l
	      continue;
	    }
	    // make plot between borne_inf and l
	    gen curres=plotfunc(tmp,vars,attributs,densityplot,function_xmin,l._DOUBLE_val,function_ymin,function_ymax,function_zmin,function_zmax,nstep,jstep,showeq,contextptr);
	    if (curres.type==_VECT)
	      res = mergevecteur(res,*curres._VECTptr);
	    else
	      res.push_back(curres);
	    function_xmin=l._DOUBLE_val; // continue with plot from l to borne_sup
	  } // end loop on i
	  if (i==vs/2){
	    if (vs%2){
	      f=quotesubst(f,piece,piecev[vs-1],contextptr);
	      gen curres = plotfunc(f,vars,attributs,densityplot,function_xmin,function_xmax,function_ymin,function_ymax,function_zmin,function_zmax,nstep,jstep,showeq,contextptr);
	      if (curres.type==_VECT)
		res = mergevecteur(res,*curres._VECTptr);
	      else
		res.push_back(curres);
	    }
	    return res;
	  } // end i==vs/2
	  // restore xmin/xmax
	  function_xmin=function_xmin_save;
	  function_xmax=function_xmax_save;
	} // end piecef.type==_VECT
      } // end piecewise
      gen locvar(vars);
      locvar.subtype=0;
      gen y=quotesubst(f,vars,locvar,contextptr),yy;
      // gen y=f.evalf2double(),yy;
      double j,entrej,oldj=0,xmin=function_xmin,xmax=function_xmax+step/2;
      bool joindre;
      vecteur localvar(1,vars);
      context * newcontextptr= (context *) contextptr;
      int protect=giac_bind(vecteur(1,xmin),localvar,newcontextptr);
      vecteur chemin; chemin.reserve(nstep+1);
      double i=xmin;
      for (;!ctrl_c && !interrupted && i<xmax;i+= step){
	// yy=evalf_double(subst(f,vars,i,false,contextptr),1,contextptr);
	local_sto_double(i,*vars._IDNTptr,newcontextptr);
	// vars._IDNTptr->localvalue->back()._DOUBLE_val =i;
	yy=y.evalf2double(eval_level(contextptr),newcontextptr);
	if (yy.type!=_DOUBLE_){
	  if (!chemin.empty())
	    res.push_back(pnt_attrib(symb_curve(gen(makevecteur(gen(vars,f),vars,xmin,i,showeq),_PNT__VECT),gen(chemin,_GROUP__VECT)),attributs.empty()?color:attributs,contextptr));
	  xmin=i;
	  chemin.clear();
	  continue;
	}
	j=yy._DOUBLE_val;
	if (j>function_ymax)
	  function_ymax=j;
	if (j<function_ymin)
	  function_ymin=j;
	if (i!=xmin){
          double dij=fabs(oldj-j);
          dij = ldexp(dij,2); // dij*=4
	  if (dij>(function_ymax-function_ymin)){ // try middle-pnt
	    local_sto_double_increment(-step/2,*vars._IDNTptr,newcontextptr);
	    // vars._IDNTptr->localvalue->back()._DOUBLE_val -= step/2;
	    yy=y.evalf2double(eval_level(contextptr),newcontextptr);
	    if (yy.type!=_DOUBLE_)
	      joindre=false;
	    else {
	      entrej=yy._DOUBLE_val;
	      if (j>oldj)
		joindre=(j>=entrej) && (entrej>=oldj);
	      else
		joindre=(j<=entrej) && (entrej<=oldj);
	    }
	    local_sto_double_increment(step/2,*vars._IDNTptr,newcontextptr);
	    // vars._IDNTptr->localvalue->back()._DOUBLE_val += step/2;
	  }
	  else
	    joindre=true;
	}
	else
	  joindre=false;
	if (joindre)
	  chemin.push_back(gen(i,j));
	else {
	  if (!chemin.empty()){
	    res.push_back(pnt_attrib(symb_curve(gen(makevecteur(gen(vars,f),vars,xmin,i-step,showeq),_PNT__VECT),gen(chemin,_GROUP__VECT)),attributs.empty()?color:attributs,contextptr));
	  }
	  xmin=i;
	  chemin.clear(); chemin.push_back(gen(i,j));//=vecteur(1,gen(i,j));
	}
	oldj=j;
      }
      if (!chemin.empty()){
	res.push_back(pnt_attrib(symb_curve(gen(makevecteur(gen(vars,f),vars,xmin,i-step,showeq),_PNT__VECT),gen(chemin,_GROUP__VECT)),attributs.empty()?color:attributs,contextptr));
      }
      leave(protect,localvar,newcontextptr);
      if (res.size()==1)
	return res.front();
      return res; // e;
    } // end 1-var function plot
    return gensizeerr(contextptr);
  }

  gen plotseq(const gen& f,const gen&x,double x0,double xmin,double xmax,int niter,const vecteur & attributs,const context * contextptr){
#ifndef WITH_PLOT
    return undef;
#endif
    if (xmin>xmax)
      swapdouble(xmin,xmax);
    vecteur res(2*niter+1);
    res[0]=gen(x0,xmin);
    int j=1;
    gen newx0;
    double x1;
    gprintf("======== %gen=%gen), %gen=%gen",makevecteur(symb_at(u__IDNT_e,n__IDNT_e+1,contextptr),subst(f,x,symb_at(u__IDNT_e,n__IDNT_e,contextptr),false,contextptr),symb_at(u__IDNT_e,0,contextptr),x0),1,contextptr);
    for (int i=0;i<niter;++i){
      newx0=subst(f,x,x0,false,contextptr).evalf2double(eval_level(contextptr),contextptr);
      if (newx0.type!=_DOUBLE_)
	return gensizeerr(gettext("Bad iteration"));
      gprintf("n=%gen u_n=%gen",makevecteur(i+1,newx0),1,contextptr);
      x1=newx0._DOUBLE_val;
      res[j]=gen(x0,x1);
      ++j;
      x0=x1;
      res[j]=gen(x0,x0);
      ++j;
    }
    vecteur g(gen2vecteur(_plotfunc(makesequence(f,symb_equal(x,symb_interval(xmin,xmax))),contextptr)));
    g.push_back(pnt_attrib(gen(makevecteur(gen(xmin,xmin),gen(xmax,xmax)),_LINE__VECT),attributs,contextptr));
    int color=FL_MAGENTA;
    if (!attributs.empty())
      color = color | (attributs[0].val & 0xffff0000);
    g.push_back(symb_pnt(gen(res,_LINE__VECT),color,contextptr));
    g.push_back(symb_pnt(gen(makevecteur(gen(x0,x0),gen(x0,xmin)),_VECTOR__VECT), color | _DASH_LINE,contextptr));
    return g; // gen(g,_SEQ__VECT);
  }
  int find_plotseq_args(const gen & args,gen & expr,gen & x,double & x0d,double & xmin,double & xmax,int & niter,vecteur & attributs,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return 0;
#endif
    vecteur v=gen2vecteur(args);
    attributs=vecteur(1,COLOR_BLACK);
    int l=read_attributs(v,attributs,contextptr);
    if (l<2)
      v.push_back(0);
    expr=v[0];
    niter=10;
    gen x0;
    if (l>3){ // expr,var,x0,niter
      x=v[1];
      x0=v[2];
      if (v[3].type==_INT_)
	niter=absint(v[3].val);
      else
	return -2; // bad iteration
    }
    else {
      if (l>2){
	if (v[2].type==_INT_)
	  niter=absint(v[2].val);
	else
	  return -2;
      }
      if ( (v[1].type==_SYMB) && (v[1]._SYMBptr->sommet==at_equal) ){
	vecteur & w=*v[1]._SYMBptr->feuille._VECTptr;
	x=w[0];
	x0=w[1];
      }
      else {
	x=vx_var();
	x0=v[1];
      }
    }
    x0=evalf_double(x0,eval_level(contextptr),contextptr);
    xmin=gnuplot_xmin;xmax=gnuplot_xmax;
    if (x0.type==_VECT && x0._VECTptr->size()==3){
      vecteur & x0v=*x0._VECTptr;
      if (x0v[1].type!=_DOUBLE_ || x0v[2].type!=_DOUBLE_)
	return -3; // gensizeerr(gettext("Non numeric range value"));
      xmin=x0v[1]._DOUBLE_val;
      xmax=x0v[2]._DOUBLE_val;
      x0=remove_at_pnt(x0v[0]);
      x0=re(x0,contextptr);
    }
    else {
      double xcur=x0._DOUBLE_val;
      xmin=xmax=xcur;
      for (int i=0;i<niter;++i){
        gen tmp=subst(expr,x,xcur,false,contextptr);
        if (tmp.type!=_DOUBLE_)
          break;
        xcur=tmp._DOUBLE_val;
        if (xcur<xmin)
          xmin=xcur;
        if (xcur>xmax)
          xmax=xcur;
      }
      xcur=(xmax-xmin)/10;
      if (xcur<1)
        xcur=1;
      xmax += xcur;
      xmin -= xcur;
    }
    if (x0.type!=_DOUBLE_)
      return -4; // 
    x0d=x0._DOUBLE_val;
    return 0;
  }

  // args=[expr,[var=]x0|[x0,xmin,xmax][,niter]]
  gen _plotseq(const gen & args,const context * contextptr){
#ifndef WITH_PLOT
    return undef;
#endif
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    gen expr,var;
    double x0d,xmin,xmax;
    int niter;
    vecteur attributs;
    if (find_plotseq_args(args,expr,var,x0d,xmin,xmax,niter,attributs,contextptr)<0)
      return gentypeerr(contextptr);
    return plotseq(expr,var,x0d,xmin,xmax,niter,attributs,contextptr);
  }
  static const char _plotseq_s []="plotseq";
  static define_unary_function_eval (__plotseq,&_plotseq,_plotseq_s);
  define_unary_function_ptr5( at_plotseq ,alias_at_plotseq,&__plotseq,0,true);

  static gen divide_by_2(const gen & ra,GIAC_CONTEXT){
    if (ra.type==_DOUBLE_ || (ra.type==_CPLX && (ra._CPLXptr->type==_DOUBLE_ || (ra._CPLXptr+1)->type==_DOUBLE_)) )
      return ra/gen(2.0);
    else
      return normal(rdiv(ra,plus_two,contextptr),contextptr);
  }

  gen _centre(const gen & args,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return undef;
#endif
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    gen a=args;
    if (a.is_equal()){
      a=_cercle(a,contextptr);
      if (a.type==_VECT && !a._VECTptr->empty())
	a=a._VECTptr->front();
    }
    if (a.type==_VECT && a.subtype==_SEQ__VECT && a._VECTptr->size()==1)
      a=a._VECTptr->front();
    a=remove_at_pnt(a);
    gen centre,rayon;
    if (!centre_rayon(a,centre,rayon,false,contextptr))
      return gensizeerr(contextptr);
    vecteur attributs(1,COLOR_BLACK);
    read_attributs(gen2vecteur(args),attributs,contextptr);
    return pnt_attrib(centre,attributs,contextptr);
  }
  static const char _centre_s []="center";
  static define_unary_function_eval (__centre,&_centre,_centre_s);
  define_unary_function_ptr5( at_centre ,alias_at_centre,&__centre,0,true);

  gen _rayon(const gen & args,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return undef;
#endif
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    gen a(args);
    if (a.is_equal()){
      a=_cercle(a,contextptr);
      if (a.type==_VECT && !a._VECTptr->empty())
	a=a._VECTptr->front();
    }
    a=remove_at_pnt(a);
    gen centre,rayon;
    if (!centre_rayon(a,centre,rayon,true,contextptr))
      return false;
    return rayon;
  }
  static const char _rayon_s []="radius";
  static define_unary_function_eval (__rayon,&_rayon,_rayon_s);
  define_unary_function_ptr5( at_rayon ,alias_at_rayon,&__rayon,0,true);

  // point + rayon or line
  gen _cercle(const gen & args,GIAC_CONTEXT){
#ifndef WITH_PLOT
    if (args.type==_STRNG && args.subtype==-1) return args;
    // Keep symbolic geometry payloads available even when plotting backend is disabled.
    return symbolic(at_cercle,args);
#endif
    if (is_undef(args)) return args;
    // inert form (since cercle return itself with a pnt__vect arg)
    if (args.type==_VECT && args.subtype==_PNT__VECT) return symbolic(at_cercle,args); 
    vecteur v(gen2vecteur(args));
    if (v.empty())
      return gensizeerr(gettext("circle"));
    vecteur attributs(1,COLOR_BLACK);
    int s=read_attributs(v,attributs,contextptr);
    // find diametre
#if 0
    if (s==1){
      vecteur w=eval_with_xy_quoted(v.front(),contextptr);
      for (unsigned i=1;i<v.size();++i)
	w.push_back(v[i]);
      gen tmp=_plotimplicit(gen(w,_SEQ__VECT),contextptr);
      if (tmp.type==_VECT && tmp._VECTptr->size()==1)
	return tmp._VECTptr->front();
      else
	return tmp;
    }
#endif
    gen e=eval(v.front(),contextptr),diametre;
    bool diam=true;
    if (e.is_equal() && e._SYMBptr->feuille.type==_VECT && e._SYMBptr->feuille._VECTptr->size()==2){
      if (e._SYMBptr->feuille._VECTptr->front()==at_centre){
	diam=false;
	e=e._SYMBptr->feuille._VECTptr->back();
      }
    }
    int narg=0;
    if ( (e.type==_SYMB) && (e._SYMBptr->sommet==at_pnt)){
      gen f=e._SYMBptr->feuille._VECTptr->front();
      if (f.type==_VECT && f._VECTptr->size()==2){
	diametre=f;
	narg=1;
      }
      else e=remove_at_pnt(e);
    }
    else {
      if ((e.type==_VECT) && (e._VECTptr->size()==2)){
	diametre=e;
	narg=1;
      }
    }
    if (!narg){
      if (s<2)
	return gensizeerr(gettext("circle"));
      gen f=eval(v[1],contextptr);
      if ((f.type==_SYMB) && (f._SYMBptr->sommet==at_pnt)){
	gen g=remove_at_pnt(f);
	if (g.type==_VECT && g._VECTptr->size()==2){
	  // e=center, g=line, project e on g
	  gen g1=g._VECTptr->front();
	  gen g2=g._VECTptr->back();
	  gen t=projection(g1,g2,e,contextptr);
	  if (is_undef(t)) return t;
	  g=g1+t*(g2-g1); // this is the projection
	  diametre=gen(makevecteur(e+(e-g),g),_GROUP__VECT);
	}
	else {
	  g=get_point(g,0,contextptr);
	  if (is_undef(g)) return g;
	  if (diam)
	    diametre=gen(makevecteur(e,g),_GROUP__VECT);
	  else
	    diametre=gen(makevecteur(e+(e-g),g),_GROUP__VECT);
	}
      }
      else {
	while (f.type==_VECT){
	  if (f._VECTptr->empty())
	    return gensizeerr(contextptr);
	  f=f._VECTptr->front();
	}
	if (e.type==_VECT)
	  return gensizeerr(contextptr);
	gen ee=e-f;
	gen ff=e+f;
	diametre=gen(makevecteur(ee,ff),_GROUP__VECT);
      }
      narg=2;
    }
    if (diametre.type!=_VECT || diametre._VECTptr->size()!=2 )
      return gensizeerr(contextptr);
    gen d0=get_point(diametre._VECTptr->front(),0,contextptr);
    gen d1=get_point((*diametre._VECTptr)[1],1,contextptr);
    if (is_undef(d0)) return d0;
    if (is_undef(d1)) return d1;
    gen ce,ra;
    // find angles
    gen a1(zero),a2(cst_two_pi);
#if 0
    if (s==1+narg){
      *logptr(contextptr) << "Assuming circumcircle call" << "\n";
      return _circonscrit(args,contextptr);
    }
#endif
    if (s>1+narg){
      a1=eval(v[narg],contextptr);
      a2=eval(v[narg+1],contextptr);
    }
    gen res=pnt_attrib((symbolic(at_cercle,gen(makenewvecteur(diametre,a1,a2),_PNT__VECT))),attributs,contextptr);
    if (s<3+narg)
      return res;
    vecteur w(1,res);
    ce=divide_by_2(d0+d1,contextptr);
    ra=divide_by_2(d1-d0,contextptr);
    gen ga1=ce+ra*expi(a1,contextptr);
    gen ga2=ce+ra*expi(a2,contextptr);
    if (v[narg+2].type==_IDNT)
      w.push_back(eval(symb_sto(_point(ga1,contextptr),v[narg+2]),contextptr));
    if (s>3+narg)
      w.push_back(eval(symb_sto(_point(ga2,contextptr),v[narg+3]),contextptr));
    return  gen(w,_GROUP__VECT);
  }
  static const char _cercle_s []="circle";
  static define_unary_function_eval_quoted (__cercle,&_cercle,_cercle_s);
  define_unary_function_ptr5( at_cercle ,alias_at_cercle,&__cercle,_QUOTE_ARGUMENTS,true);

  static gen equation(const gen & arg,const gen & x,const gen & y, const gen & z,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return undef;
#endif
    if (arg.type==_VECT){
      vecteur res;
      const_iterateur it=arg._VECTptr->begin(),itend=arg._VECTptr->end();
      gen prev;
      for (;it!=itend;++it){
	gen tmp=equation(*it,x,y,z,contextptr);
	if (tmp==prev)
	  continue;
	prev=tmp;
	//if (calc_mode(contextptr)==1) tmp=remove_equal(tmp);
	res.push_back(tmp);
      }
      // if (calc_mode(contextptr)==1) return symbolic(at_equal,makesequence(_prod(res,contextptr),0));
      if (res.size()==1)
	return res.front();
      return res;
    }
    gen e=remove_at_pnt(arg);
    vecteur vxyz(makevecteur(x,y,z));
    if ((e.type==_SYMB) && (e._SYMBptr->sommet==at_cercle)){
      gen centre,rayon,r,i;
      if (!centre_rayon(e,centre,rayon,false,contextptr))
	return gensizeerr(contextptr);
      rayon=recursive_normal(rayon,contextptr);
      reim(rayon,r,i,contextptr);
      r=recursive_normal(r*r+i*i,contextptr);
      return symb_equal(pow(x-re(centre,contextptr),2)+pow(y-im(centre,contextptr),2),r);//symbolic(at_equal,makesequence(pow(x-re(centre,contextptr),2)+pow(y-im(centre,contextptr),2),r));
    }
    if ( (e.type==_VECT) && (e._VECTptr->size()==2) ){
      gen A=e._VECTptr->front();
      gen B=e._VECTptr->back();
      gen v=B-A;
      gen a=im(v,contextptr);
      gen b=-re(v,contextptr);
      gen d=gcd(a,b);
      a=normal(a/d,contextptr);
      b=normal(b/d,contextptr);
      gen c=a*re(A,contextptr)+b*im(A,contextptr);
      if (!is_zero(b,contextptr))
	return symb_equal(y,normal(-a/b,contextptr)*x+normal(c/b,contextptr));//symbolic(at_equal,makesequence(y,normal(-a/b,contextptr)*x+normal(c/b,contextptr)));
      return symb_equal(x,normal(c/a,contextptr));//symbolic(at_equal,makesequence(x,normal(c/a,contextptr)));//symbolic(at_equal,makesequence(a*x+b*y,c));
    }
    if ( (e.type==_SYMB) && (e._SYMBptr->sommet==at_curve)){
      vecteur v=*e._SYMBptr->feuille._VECTptr->front()._VECTptr; 
      if (v.size()>=6 && !is_undef(v[5]))
	return symb_equal(subst(v[5],makevecteur(x__IDNT_e,y__IDNT_e,z__IDNT_e),makevecteur(x,y,z),false,contextptr),0);//symbolic(at_equal,makesequence(subst(v[5],makevecteur(x__IDNT_e,y__IDNT_e,z__IDNT_e),makevecteur(x,y,z),false,contextptr),0));
      return gensizeerr(contextptr);
    }
    return e;
  }

  gen _equation(const gen & args,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return undef;
#endif
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT || args._VECTptr->size()!=2 || (*args._VECTptr)[1].type!=_IDNT){
      ck_parameter_x(contextptr);
      ck_parameter_y(contextptr);
      return equation(args,x__IDNT_e,y__IDNT_e,z__IDNT_e,contextptr);
    }
    vecteur v(*args._VECTptr);
    // e= symb_curve or line or cercle
    gen xy=v.back();
    if ( (xy.type!=_VECT) || (xy._VECTptr->size()<2))
      return gensizeerr(contextptr);
    vecteur & vxyz=*xy._VECTptr;
    gen x(vxyz[0]),y(vxyz[1]),z;
    if (vxyz.size()==3)
      z=vxyz[2];
    return equation(v.front(),x,y,z,contextptr);
  }
  static const char _equation_s []="equation";
  static define_unary_function_eval (__equation,&_equation,_equation_s);
  define_unary_function_ptr5( at_equation ,alias_at_equation,&__equation,0,true);

  static void polygonify(vecteur & v,GIAC_CONTEXT){
    int vs=int(v.size());
    for (int i=0;i<vs;++i){
      v[i]=vect2c(get_point(v[i],0,contextptr));
    }
  }

  gen _polygone(const gen & args,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return undef;
#endif
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    if (args.type!=_VECT)
      return symbolic(at_polygone,args);
    vecteur v(*apply(args,remove_at_pnt)._VECTptr);
    vecteur attributs(1,COLOR_BLACK);
    int s=read_attributs(v,attributs,contextptr);
    if (s<2)
      return gendimerr(contextptr);
    v=vecteur(v.begin(),v.begin()+s);
    gen vf=v.front();
    v.push_back(vf);
    polygonify(v,contextptr);
    return pnt_attrib(gen(v,_GROUP__VECT),attributs,contextptr);
  }
  static const char _polygone_s []="polygon";
  static define_unary_function_eval (__polygone,&_polygone,_polygone_s);
  define_unary_function_ptr5( at_polygone ,alias_at_polygone,&__polygone,0,true);

#ifndef XLIGHT // plotcontour memory size 10K
  // v is a list of polygon vertices, add [A,B] to it
  static bool is_approx0(const gen & a,double dx,double dy){
    if (a.type==_CPLX) 
      return (fabs(a._CPLXptr->_DOUBLE_val) < 1e-6*dx) && (fabs((a._CPLXptr+1)->_DOUBLE_val) < 1e-6*dy);
    return is_zero(a);
  }

  static void add_segment(vecteur & v,const gen & A,const gen & B,double dx,double dy){
    if (is_approx0(A-B,dx,dy))
      return;
    iterateur it=v.begin(),itend=v.end();
    for (;it!=itend;++it){
      gen & tmp = *it;
      if (tmp.type==_VECT && !tmp._VECTptr->empty()){
	gen & b =tmp._VECTptr->back();
	if (is_approx0(b-A,dx,dy)){
	  tmp._VECTptr->push_back(B);
	  break;
	}
	if (is_approx0(b-B,dx,dy)){
	  tmp._VECTptr->push_back(A);
	  break;
	}
	gen & a=tmp._VECTptr->front();
	if (is_approx0(a-A,dx,dy)){
	  tmp._VECTptr->insert(tmp._VECTptr->begin(),B);
	  break;
	}
	if (is_approx0(a-B,dx,dy)){
	  tmp._VECTptr->insert(tmp._VECTptr->begin(),A);
	  break;
	}
      }
    }
    if (it==itend)
      v.push_back(makevecteur(A,B));
  }

  static void polygonify(vecteur & v,const vecteur & attributs,GIAC_CONTEXT){
    iterateur it=v.begin(),itend=v.end();
    for (;it!=itend;++it){
      gen & tmp=*it;
      if (tmp.type==_VECT && tmp._VECTptr->size()>1){
	tmp.subtype=_GROUP__VECT;
	*it=pnt_attrib(tmp,attributs,contextptr);
      }
    }
  }
  static void glue_components(vecteur & v,double dx,double dy){
    int s = int(v.size());
    for (int i=0;i<s-1;++i){
      gen & cur = v[i];
      for (int j=i+1;j<s;++j){
	gen & next=v[j]; 
	if (cur.type==_VECT && next.type==_VECT && !cur._VECTptr->empty() && !next._VECTptr->empty()){
	  if (is_approx0(cur._VECTptr->front()-next._VECTptr->back(),dx,dy) || is_approx0(cur._VECTptr->front()-next._VECTptr->front(),dx,dy))
	    vreverse(cur._VECTptr->begin(),cur._VECTptr->end());
	  if (is_approx0(cur._VECTptr->back()-next._VECTptr->back(),dx,dy))
	    vreverse(next._VECTptr->begin(),next._VECTptr->end());
	  if (is_approx0(cur._VECTptr->back()-next._VECTptr->front(),dx,dy)){
	    // FIXME mergevecteur will repeat cur.back and next.front
	    v[i]=mergevecteur(*cur._VECTptr,*next._VECTptr);
	    v.erase(v.begin()+j);
	    j=i; // restart j loop
	    --s;
	  }
	} // endif
      } // end for j
    } // end for i
  }
  gen plot_array(const vector< vector< double> > & fij,int imax,int jmax,double xmin,double xmax,double dx,double ymin,double ymax,double dy,const vecteur & lz,const vecteur & attributs,bool contour,GIAC_CONTEXT){
    // do linear interpolation between points for levels
    // with a marching rectangle
    // if all 4 vertices values are > or < nothing added
    // else 3/1 -> one segment between 2 interpolated zeros
    // 2/2 
    // ++ __   +- + or \\   +- |
    // --      -+           +- |
    int nz=int(lz.size());
    vector<double> Z;
    for (int k=0;k<nz;k++){
      gen zg=evalf_double(lz[k],eval_level(contextptr),contextptr);
      if (zg.type==_DOUBLE_)
	Z.push_back(zg._DOUBLE_val);
    }
    nz=int(Z.size());
    vector<vecteur> res(nz);
    for (int i=0;i<imax-1;++i){
      double x=xmin+i*dx;
      for (int j=0;j<jmax-1;++j){
	double y=ymin+j*dy;
	double a=fij[i][j+1];
	double c=fij[i][j];
	double b=fij[i+1][j+1];
	double d=fij[i+1][j]; 
	double eps=1e-12;
	// a b (y+dy)
	// c d (y)
	for (int k=0;k<nz;k++){
	  double z=Z[k],zeps=z?z*eps:eps;;
	  if (a==z)
	    a += zeps;
	  if (b==z)
	    b += zeps;
	  if (c==z)
	    c += zeps;
	  if (d==z)
	    d += zeps;
	  bool ab=(a-z)*(b-z)<=0,ca=(a-z)*(c-z)<=0,db=(d-z)*(b-z)<=0,cd=(d-z)*(c-z)<=0;
	  gen AB,CA,DB,CD;
	  // intercepts
	  if (ab)
	    AB=gen(x+(a-z)/(a-b)*dx,y+dy);
	  if (cd)
	    CD=gen(x+(c-z)/(c-d)*dx,y);
	  if (ca)
	    CA=gen(x,y+(c-z)/(c-a)*dy);
	  if (db)
	    DB=gen(x+dx,y+(d-z)/(d-b)*dy);
	  // diagonal
	  if (ab && ca){
	    add_segment(res[k],AB,CA,dx,dy);
	    ab=false;
	    ca=false;
	  }
	  if (ab && db){
	    add_segment(res[k],AB,DB,dx,dy);
	    ab=false;
	    db=false;
	  }
	  if (db && cd){
	    add_segment(res[k],DB,CD,dx,dy);
	    db=false;
	    cd=false;
	  }
	  if (ca && cd){
	    add_segment(res[k],CA,CD,dx,dy);
	    ca=false;
	    cd=false;
	  }
	  // horizontal
	  if (ab && cd)
	    add_segment(res[k],AB,CD,dx,dy);
	  // vertical
	  if (ca && db)
	    add_segment(res[k],CA,DB,dx,dy);
	}
      }
    }
    vecteur res0,attr(attributs);
    vecteur legendes,colors;
    if (attr.empty())
      attr.push_back(0);
    if (attributs.size()<2){
      attr.push_back(contour?lz:0);
    }
    if (attr[0].type==_VECT)
      colors=*attr[0]._VECTptr;
    else
      colors=vecteur(1,attr[0]);
    if (attr[1].type==_VECT)
      legendes=*attr[1]._VECTptr;
    int legs=int(legendes.size());
    int cols=int(colors.size());
    double dx11=1.1*dx,dx1em6=1e-6*dx,dy11=1.1*dy,dy1em6=1e-6*dy;
    for (int k=0;k<nz;++k){
      attr[0]=colors[k<cols?k:0];
      if (!contour && attr[0].type==_INT_)
	attr[0].val=attr[0].val | _FILL_POLYGON;
      attr[1]=k<legs?legendes[k]:string2gen("",false);
      glue_components(res[k],dx,dy);
      if (attr[0].type==_INT_ && (attr[0].val & _FILL_POLYGON)){
	// now finsih gluing with the xmin/xmax/ymin/ymax border
	int ncomp=int(res[k].size());
	for (int n=0;n<ncomp;n++){
	  gen composante=res[k][n];
	  // look if begin and end of composante is at border
	  if (composante.type!=_VECT || composante._VECTptr->size()<2)
	    continue;
	  gen begin=composante._VECTptr->front(),end=composante._VECTptr->back();
	  if (is_approx0(begin-end,dx,dy)){
	    // look if + inside ok else we must break the composante
	    // to display outside instead of inside
	    // find the nearest point to xmin,ymin
	    vecteur cv=*composante._VECTptr;
	    gen xymin(xmin,ymin);
	    int cs=int(cv.size()),pos=0;
	    gen dmin=abs(begin-xymin,contextptr);
	    for (int i=1;i<cs;++i){
	      gen dcur=abs(cv[i]-xymin,contextptr);
	      if (is_strictly_positive(dmin-dcur,contextptr)){
		pos=i;
		dmin=dcur;
	      }
	    }
	    // make a little step from cv[pos] in direction of xymin
	    // and check sign of f
	    gen recv,imcv;
	    reim(cv[pos],recv,imcv,contextptr);
	    int itmp=int((recv._DOUBLE_val-xmin)/dx-0.5);
	    int jtmp=int((imcv._DOUBLE_val-ymin)/dy-0.5);
	    if (fij[itmp][jtmp]>Z[k]){
	      // no luck, build the exterior
	      res[k][n]=mergevecteur(mergevecteur(vecteur(cv.begin(),cv.begin()+pos+1),makevecteur(xymin,gen(xmax,ymin),gen(xmax,ymax),gen(xmin,ymax),xymin)),vecteur(cv.begin()+pos,cv.end()));
	    }
	    continue;
	  }
	  double bx,by=0,ex,ey=0;
	  if (begin.type==_CPLX){
	    bx=begin._CPLXptr->_DOUBLE_val;
	    by=(begin._CPLXptr+1)->_DOUBLE_val;
	  } else {
	    if (begin.type==_DOUBLE_)
	      bx=begin._DOUBLE_val;
	    else
	      continue;
	  }
	  if (end.type==_CPLX){
	    ex=end._CPLXptr->_DOUBLE_val;
	    ey=(end._CPLXptr+1)->_DOUBLE_val;
	  } else {
	    if (end.type==_DOUBLE_)
	      ex=end._DOUBLE_val;
	    else
	      continue;
	  }
	  bool bxmin=fabs(bx-xmin)<dx1em6;
	  bool bxmax=fabs(bx-xmax)<dx1em6;
	  bool bymin=fabs(by-ymin)<dy1em6;
	  bool bymax=fabs(by-ymax)<dy1em6;
	  bool exmin=fabs(ex-xmin)<dx1em6;
	  bool exmax=fabs(ex-xmax)<dx1em6;
	  bool eymin=fabs(ey-ymin)<dy1em6;
	  bool eymax=fabs(ey-ymax)<dy1em6;
	  if ( (bxmin || bxmax || bymin || bymax) &&
	       (exmin || exmax || eymin || eymax) ){
	    bxmin=fabs(bx-xmin)<dx11;
	    bxmax=fabs(bx-xmax)<dx11;
	    bymin=fabs(by-ymin)<dy11;
	    bymax=fabs(by-ymax)<dy11;
	    exmin=fabs(ex-xmin)<dx11;
	    exmax=fabs(ex-xmax)<dx11;
	    eymin=fabs(ey-ymin)<dy11;
	    eymax=fabs(ey-ymax)<dy11;
	    int i,j,di,dj,ij,ijmax=2*(imax+jmax); // perimeter
	    vecteur coins;
	    // begin and end are on border, try to connect end
	    if (exmin || exmax){
	      // move y to the right, is it + ?
	      i=exmin?0:imax-1;
	      if (eymax){ // coin
		j=jmax-2;
		if (fij[i][j]>Z[k]){
		  dj=-1;
		  di=0;
		}
		else {
		  j=jmax-1;
		  i=exmin?1:imax-2;
		  di=exmin?1:-1;
		  dj=0;
		}
	      }
	      else {
		if (eymin){
		  j=1;
		  if (fij[i][j]>Z[k]){
		    dj=1;
		    di=0;
		  }
		  else {
		    j=0;
		    i=exmin?1:imax-2;
		    di=exmin?1:-1;
		    dj=0;
		  }
		}
		else { // not a coin
		  j=int((ey-ymin)/dy+.5);
		  // yes, increase j, no decrease j
		  dj=(fij[i][j+1]>Z[k])?1:-1;
		  j+=dj;
		  di=0;
		}
	      }
	    }
	    else {
	      i=int((ex-xmin)/dx+.5);
	      j=(eymin)?0:jmax-1;
	      di=(fij[i+1][j]>Z[k])?1:-1;
	      i+=di;
	      dj=0;
	    } // end if bx==xmin or bx==xmax
	    for (ij=0; ij<ijmax;j+=dj,i+=di,++ij){
	      if (fij[i][j]<Z[k]){
		break;
	      }
	      gen tmpz(xmin+i*dx,ymin+j*dy);
	      if (di){ 
		if (i==0 || i==imax-1){
		  coins.push_back(tmpz);
		  dj=j?-1:1;
		  di=0;
		}
	      }
	      else {
		if (j==0 || j==jmax-1){
		  coins.push_back(tmpz);
		  di=i?-1:1;
		  dj=0;
		}
	      }
	    } // end for
	    if (ij==ijmax)
	      continue; // everywhere > 0
	    // find component with begin or end near i,j
	    double e1x=xmin+i*dx,e1y=ymin+j*dy;
	    int m=n;
	    for (;m<ncomp;m++){
	      gen composante2=res[k][m];
	      // look if begin and end of composante are on border
	      if (composante2.type!=_VECT || composante2._VECTptr->size()<2)
		continue;
	      gen begin2=composante2._VECTptr->front(),end2=composante2._VECTptr->back();
	      double b2x,b2y,e2x,e2y;
	      if (begin2.type==_DOUBLE_){
		b2x=begin2._DOUBLE_val; b2y=0;
	      }
	      else {
		if (begin2.type!=_CPLX)
		  continue;
		b2x=begin2._CPLXptr->_DOUBLE_val;b2y=(begin2._CPLXptr+1)->_DOUBLE_val;
	      }
	      if (end2.type==_DOUBLE_){
		e2x=end2._DOUBLE_val; e2y=0;
	      }
	      else {
		if (end2.type!=_CPLX)
		  continue;
		e2x=end2._CPLXptr->_DOUBLE_val;
		e2y=(end2._CPLXptr+1)->_DOUBLE_val;
	      }
	      if (fabs(e1x-e2x)<=dx11 && fabs(e1y-e2y)<=dy11){
		vreverse(composante2._VECTptr->begin(),composante2._VECTptr->end());
		swapdouble(b2x,e2x); 
		swapdouble(b2y,e2y);
	      }
	      if (fabs(e1x-b2x)<=dx11 && fabs(e1y-b2y)<=dy11){
		// found! glue res[k][n] with coins and res[k][m]
		vecteur tmp=mergevecteur(*composante._VECTptr,coins);
		if (n==m){
		  tmp.push_back(begin2);
		  res[k][n]=tmp;
		}
		else {
		  res[k][n]=mergevecteur(tmp,*composante2._VECTptr);
		  res[k].erase(res[k].begin()+m);
		  --ncomp;
		  --n;
		}
		break;
	      }
	    }
	  }
	} // end for n<=ncomp
      } // end if (contour || )
      polygonify(res[k],attr,contextptr);
      res0=mergevecteur(res0,res[k]);
    }
    return res0; // gen(res0,_SEQ__VECT);
  }
  gen plotcontour(const gen & f0,bool contour,GIAC_CONTEXT){
    vecteur v(gen2vecteur(f0));
    gen xvar=x__IDNT_e,yvar=y__IDNT_e;
    v=quote_eval(v,makevecteur(xvar,yvar),contextptr);
    gen attribut=COLOR_BLACK;
    vecteur attributs(1,attribut);
    int s=read_attributs(v,attributs,contextptr);
    if (!s)
      return gensizeerr(contextptr);
    gen f=v[0];
    double xmin=gnuplot_xmin,xmax=gnuplot_xmax*(1+1e-6); // small shift 1e-6 for plotinequation(x^2+y^2<=4)
    double ymin=gnuplot_ymin,ymax=gnuplot_ymax*(1+1e-6);
    double zmin=gnuplot_zmin,zmax=gnuplot_zmax;
    if (s>1){
      gen tmp(v[1]);
      if (tmp.type==_VECT && tmp._VECTptr->size()==2){
	readrange(tmp._VECTptr->front(),xmin,xmax,xvar,xmin,xmax,contextptr);
	readrange(tmp._VECTptr->back(),ymin,ymax,yvar,ymin,ymax,contextptr);
      }
    }
    vecteur lz;
    if (s>2){
      gen tmp(v[2]);
      if (tmp.type==_VECT && !tmp._VECTptr->empty())
	lz=*tmp._VECTptr;
    }
    else {
      if (contour){
	lz=vecteur(11);
	for (int i=0;i<11;++i)
	  lz[i]=i;
      }
      else
	lz=vecteur(1);
    }
    int imax=50;
    int jmax=imax,kmax=0;
    vecteur vtmp;
    read_option(v,xmin,xmax,ymin,ymax,zmin,zmax,vtmp,imax,jmax,kmax,contextptr);
    double dx=(xmax-xmin)/imax,dy=(ymax-ymin)/jmax;
    ++imax; ++jmax;
    vector< vector<double> > fij;
    vecteur xy(makevecteur(xvar,yvar)),xyval(xy);
    // eval f from xmin to xmax, in jstep and ymin to ymax in kstep
    for (int i=0;i<imax;++i){
      vector<double> fi;
      xyval[0]=xmin+i*dx;
      for (int j=0;!ctrl_c && !interrupted && j<jmax;++j){
	xyval[1]=ymin+j*dy;
	gen f1=evalf_double(evalf(quotesubst(f,xy,xyval,contextptr),eval_level(contextptr),contextptr),eval_level(contextptr),contextptr);
	double zero=0.0;
	fi.push_back(f1.type==_DOUBLE_?f1._DOUBLE_val:0.0/zero);
      }
      fij.push_back(fi);
    }
    return plot_array(fij,imax,jmax,xmin,xmax,dx,ymin,ymax,dy,lz,attributs,contour,contextptr);
  }
  gen _plotcontour(const gen & f0,GIAC_CONTEXT){
    if ( f0.type==_STRNG && f0.subtype==-1) return  f0;
    return plotcontour(f0,true,contextptr);
  }
  static const char _plotcontour_s []="plotcontour";
  static define_unary_function_eval_quoted (__plotcontour,&_plotcontour,_plotcontour_s);
  define_unary_function_ptr5( at_plotcontour ,alias_at_plotcontour,&__plotcontour,_QUOTE_ARGUMENTS,true);
#endif
  
  static gen setplotfuncerr(){
    return gensizeerr(gettext("Plotfunc: bad variable name"));
  }
  gen funcplotfunc(const gen & args,bool densityplot,const context * contextptr){
#ifndef WITH_PLOT
    return undef;
#endif
    double xmin=gnuplot_xmin,xmax=gnuplot_xmax,ymin=gnuplot_ymin,ymax=gnuplot_ymax,zmin=gnuplot_zmin,zmax=gnuplot_zmax;
    bool showeq=false;
    if (densityplot)
      zmin=zmax; // if z-range is not given, then fmin/fmax will be used 
    int nstep=gnuplot_pixels_per_eval,jstep=0;
    gen attribut=COLOR_BLACK;
    vecteur vargs(plotpreprocess(args,contextptr));
    if (is_undef(vargs))
      return vargs;
    int s=int(vargs.size());
    for (int i=0;i<s;++i){
      if (vargs[i]==at_equation){
	showeq=true;
	vargs.erase(vargs.begin()+i);
	--s;
	break;
      }
    }
    if (s<1)
      return gensizeerr(contextptr);
    gen e1=vargs[1];
    bool newsyntax;
    if (e1.type!=_VECT){
      newsyntax=readrange(e1,gnuplot_xmin,gnuplot_xmax,e1,xmin,xmax,contextptr) && (is_equal(vargs[1]) || s<4);
    }
    else {
      if (e1._VECTptr->size()!=2)
	return setplotfuncerr();
      vecteur v(*e1._VECTptr);
      newsyntax=readrange(v[0],gnuplot_xmin,gnuplot_xmax,v[0],xmin,xmax,contextptr) && readrange(v[1],gnuplot_ymin,gnuplot_ymax,v[1],ymin,ymax,contextptr);
      if (newsyntax)
	e1=v;
    }
    if (!newsyntax){ // plotfunc(fonction,var,min,max[,zminmax,attribut])
      if (s<=3)
	return setplotfuncerr();
      gen e2=vargs[2];
      gen e3=vargs[3];
      if (e1.type==_VECT){
	if ((e2.type!=_VECT) || (e3.type!=_VECT) || (e2._VECTptr->size()!=2) || (e3._VECTptr->size()!=2))
	  return gentypeerr(gettext("Plotfunc: Range must be [xmin,ymin] or [xmax,ymax]"));
	gen e21=evalf_double(e2._VECTptr->front(),eval_level(contextptr),contextptr);
	gen e22=evalf_double(e2._VECTptr->back(),eval_level(contextptr),contextptr);
	gen e31=evalf_double(e3._VECTptr->front(),eval_level(contextptr),contextptr);
	gen e32=evalf_double(e3._VECTptr->back(),eval_level(contextptr),contextptr);
	if ((e21.type!=_DOUBLE_) || (e22.type!=_DOUBLE_) || (e31.type!=_DOUBLE_) || (e32.type!=_DOUBLE_))
	  return gentypeerr(gettext("Plotfunc: bad range value!"));
	xmin=e21._DOUBLE_val;
	ymin=e22._DOUBLE_val;
	xmax=e31._DOUBLE_val;
	ymax=e32._DOUBLE_val; 
	if (s>4)
	    attribut=vargs[4];
	if (s>5 && vargs[5].type==_INT_)
	  nstep=vargs[5].val;
	if (s>6 && vargs[6].type==_INT_)
	  jstep=vargs[6].val;
      }
      else {
	e2=e2.evalf_double(eval_level(contextptr),contextptr);
	e3=e3.evalf_double(eval_level(contextptr),contextptr);
	if ((e2.type!=_DOUBLE_) || (e3.type!=_DOUBLE_))
	  return gentypeerr(gettext("Plotfunc: bad range value!"));
	xmin=e2._DOUBLE_val;
	xmax=e3._DOUBLE_val;
	if (s>4)
	  attribut=vargs[4];
	if (s>5 && vargs[5].type==_INT_)
	  nstep=vargs[5].val;
	if (s>6 && vargs[6].type==_INT_)
	  jstep=vargs[6].val;
      }
      return plotfunc(vargs.front(),e1,vecteur(1,attribut),densityplot,xmin,xmax,ymin,ymax,zmin,zmax,nstep,jstep,showeq,contextptr);
    }
    vecteur attributs(1,attribut);
    read_option(vargs,xmin,xmax,ymin,ymax,attributs,nstep,jstep,contextptr);
    return plotfunc(vargs[0],e1,attributs,densityplot,xmin,xmax,ymin,ymax,zmin,zmax,nstep,jstep,showeq,contextptr);
  }
  gen _plotfunc(const gen & args,const context * contextptr){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    return funcplotfunc(args,false,contextptr);
  }
  static const char _plotfunc_s []="plotfunc";
  static define_unary_function_eval_quoted (__plotfunc,&_plotfunc,_plotfunc_s);
  define_unary_function_ptr5( at_plotfunc ,alias_at_plotfunc,&__plotfunc,_QUOTE_ARGUMENTS,true);

  static gen plotpoints(const vecteur & v,const vecteur & attributs,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return undef;
#endif
    gen attribut=attributs.empty()?COLOR_BLACK:attributs[0];
    vecteur w(v);
    iterateur it=w.begin(),itend=w.end();
    for (;it!=itend;++it){
      if (it->type!=_VECT || it->_VECTptr->size()!=2)
	return gensizeerr(contextptr);
      *it=vect2c(*it);
    }
    return symb_pnt(gen(w,_GROUP__VECT),attribut.val,contextptr); 
    // should change symb_pnt so that attribut is more generic than color
  }
  gen _plot(const gen & g,const context * contextptr){
#ifndef WITH_PLOT
    return undef;
#endif
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    gen var,res;
    if (g.type!=_VECT && !is_distribution(g) && !is_algebraic_program(g,var,res) )
      return _plotfunc(g,contextptr);
    vecteur v;
    gen g_(g);
    if (g.type==_VECT)
      v=*g._VECTptr;
    v=plotpreprocess(g_,contextptr);
    if (is_undef(v))
      return v;
    int s=int(v.size());
    gen attribut=COLOR_BLACK;
    vecteur attributs(1,attribut);
    gen v0=eval(v[0],1,contextptr);
    gen v1=eval(v[1],1,contextptr);
    if (s>1 && v0.type<=_REAL && v1.type<=_REAL){
      *logptr(contextptr) << gettext("To get a point, run point(")<<v0<<","<<v1<<")" << "\n";
    }
    if (s>1 && v0.type==_VECT && v1.type==_VECT && v0._VECTptr->size()==v1._VECTptr->size()){
      *logptr(contextptr) << gettext("Assuming you want to run polygonplot") << "\n";
      vecteur w0=*v0._VECTptr,w1=*v1._VECTptr;
      int ss=w0.size(),i;
      for (i=0;i<ss;++i){
	if (w0[i].type>_REAL || w1[i].type>_REAL)
	  break;
      }
      if (i==ss){
	// polygonplot
	s=read_attributs(v,attributs,contextptr);
	return put_attributs(_polygonplot(makesequence(v0,v1),contextptr),attributs,contextptr);
      }
    }
#if 0
    if (g.subtype!=_SEQ__VECT && s==3 ){
      gen v0v1(v[0],v[1]);
      if (v[2].type==_IDNT)
	return plotparam(v0v1,v[2],attributs,true,gnuplot_xmin,gnuplot_xmax,gnuplot_ymin,gnuplot_ymax,gnuplot_tmin,gnuplot_tmax,gnuplot_tstep,undef,undef,contextptr); // FIX equation?
      if (v[2].is_equal()){ // parametric plot
	gen & gw=v[2]._SYMBptr->feuille;
	if (gw.type==_VECT && gw._VECTptr->size()==2){
	  gen gx=gw._VECTptr->front();
	  double inf,sup;
	  if (gx.type==_IDNT && chk_double_interval(gw._VECTptr->back(),inf,sup,contextptr))
	    return plotparam(v0v1,gx,attributs,true,gnuplot_xmin,gnuplot_xmax,gnuplot_ymin,gnuplot_ymax,inf,sup,gnuplot_tstep,undef,undef,contextptr); // FIX EQUATION?
	}
      }
    }
#endif
    if (s<1)
      return _plotfunc(g,contextptr);
    if (g.type==_VECT && g.subtype!=_SEQ__VECT)
      return plotpoints(v,attributs,contextptr);
    double xmin=gnuplot_xmin,xmax=gnuplot_xmax,ymin=gnuplot_ymin,ymax=gnuplot_ymax,zmin=gnuplot_zmin,zmax=gnuplot_zmax;
    gen xvar(vx_var()),yvar(y__IDNT_e);
    int nstep=gnuplot_pixels_per_eval;
    bool showeq=false;
    // bool clrplot=true;
    // parse options
    for (int i=1;i<s;++i){
      const gen &vi=v[i];
      if (vi==at_equation){
	showeq=true;
	continue;
      }
      if (vi.type==_IDNT){
	if (i==1)
	  xvar=v[1];
	if (i==2)
	  yvar=v[2];
      }
      if (i==1 && vi.is_interval()){
	identificateur tmp(" x");
	vecteur w(v);
	w[0]=v[0](tmp,contextptr);
	w[1]=symb_equal(tmp,v[1]);//symbolic(at_equal,makesequence(tmp,v[1]));
	return _plot(gen(w,_SEQ__VECT),contextptr);
      }
      if (i==2 && (vi.type<_CPLX ))
	xmin=evalf_double(vi,1,contextptr)._DOUBLE_val;
      if (i==3 && (vi.type<_CPLX ))
	xmax=evalf_double(vi,1,contextptr)._DOUBLE_val;
      if (!vi.is_equal())
	continue;
      gen & opt=vi._SYMBptr->feuille;
      if (opt.type!=_VECT || opt._VECTptr->size()!=2)
	continue;
      gen opt1=opt._VECTptr->front(),opt2=opt._VECTptr->back();
      double inf,sup;
      if ( opt1.type==_IDNT &&chk_double_interval(opt2,inf,sup,contextptr)){	
	if (i==1){
	  xvar=opt1;
	  xmin=inf;
	  xmax=sup;
	}
	if (i==2){
	  yvar=opt1;
	  ymin=inf;
	  ymax=sup;
	}
      }
    }
    int jstep,kstep;
    read_option(v,xmin,xmax,ymin,ymax,zmin,zmax,attributs,nstep,jstep,kstep,contextptr);
    bool v0cst=lidnt(evalf(v0,1,contextptr)).empty(),v1cst=lidnt(evalf(v1,1,contextptr)).empty();
    if (v0cst && v1cst){
      if (s==2 || v[2].is_equal())
	return _point(eval(g,1,contextptr),contextptr);
      gen v2=eval(v[2],1,contextptr);
      if (lidnt(evalf(v2,1,contextptr)).empty())
	return _point(eval(g,1,contextptr),contextptr);
    }
    if (v0cst && v0.type==_VECT && !v0._VECTptr->empty() && v0._VECTptr->front().type==_VECT)
      return plotpoints(*v0._VECTptr,attributs,contextptr);
    return plotfunc(v[0],xvar,attributs,false,xmin,xmax,ymin,ymax,zmin,zmax,nstep,0,showeq,contextptr);
  }
  static const char _plot_s []="plot"; // FIXME use maple arguments
  static define_unary_function_eval_quoted (__plot,&_plot,_plot_s);
  define_unary_function_ptr5( at_plot ,alias_at_plot,&__plot,_QUOTE_ARGUMENTS,true);

  gen plotparam(const gen & f,const gen & vars,const vecteur & attributs,bool densityplot,double function_xmin,double function_xmax,double function_ymin,double function_ymax,double function_tmin, double function_tmax,double function_tstep,const gen & equation,const gen & parameq,const gen & vparam,const context * contextptr){
#ifndef WITH_PLOT
    return undef;
#endif
    if (function_tstep<=0 || (function_tmax-function_tmin)/function_tstep>max_nstep || function_tmin==function_tmax)
      return gensizeerr(gettext("Plotparam: unable to discretize: tmin, tmax, tstep=")+print_DOUBLE_(function_tmin,12)+","+print_DOUBLE_(function_tmax,12)+","+print_DOUBLE_(function_tstep,12)+gettext("\nTry a larger value for tstep"));
    gen fC(f);
    if (f.type==_VECT && f._VECTptr->size()==2)
      fC=vect2c(f);
    gen attribut=attributs.empty()?COLOR_BLACK:attributs[0];
    // bool save_approx_mode=approx_mode(contextptr);
    // approx_mode(true,contextptr);
    gen locvar(vars);
    locvar.subtype=0;
    gen xy=quotesubst(f,vars,locvar,contextptr),xy_,x_,y_;
    bool joindre;
    vecteur localvar(1,vars),res;
    context * newcontextptr=(context *) contextptr;
    int protect=giac_bind(vecteur(1,function_tmin),localvar,newcontextptr);
    vecteur chemin;
    double i,j,oldi=0,oldj=0,entrei,entrej;
    double t=function_tmin;
    int nstep=int((function_tmax-function_tmin)/function_tstep+.5);
    int elevel=eval_level(contextptr);
    // bool old_io_graph=io_graph(contextptr);
    // io_graph(false,contextptr);
    for (int count=0;!ctrl_c && !interrupted && count<=nstep;++count,t+= function_tstep){
      local_sto_double(t,*vars._IDNTptr,newcontextptr);
      // vars._IDNTptr->localvalue->back()._DOUBLE_val =t;
      if (xy.type==_VECT && xy._VECTptr->size()==2){
	x_=xy._VECTptr->front().evalf2double(elevel,newcontextptr);
	y_=xy._VECTptr->back().evalf2double(elevel,newcontextptr);
      }
      else {
	xy_=xy.evalf2double(elevel,newcontextptr);
	if (xy_.type==_VECT && xy_._VECTptr->size()==2){
	  x_=xy_._VECTptr->front();
	  y_=xy_._VECTptr->back();
	}
	else {
	  reim(xy_,x_,y_,contextptr); // x_=re(xy_,newcontextptr); y_=im(xy_,newcontextptr).evalf_double(elevel,newcontextptr);
	  y_=y_.evalf_double(elevel,newcontextptr);
	}
      }
      if ( (x_.type!=_DOUBLE_) || (y_.type!=_DOUBLE_) )
	continue;
      //dbg_printf("count=%i nstep=%i t=%f x_=%s y_=%s\n",count,nstep,t,x_.print(contextptr).c_str(),y_.print(contextptr).c_str());
      i=x_._DOUBLE_val;
      j=y_._DOUBLE_val;
      if (t!=function_tmin){
	if ( (fabs(oldj-j)>(function_ymax-function_ymin)/5) ||
	     (fabs(oldi-i)>(function_xmax-function_xmin)/5) ){
	  local_sto_double_increment(-function_tstep/2,*vars._IDNTptr,newcontextptr);
	  // vars._IDNTptr->localvalue->back()._DOUBLE_val -= function_tstep/2;
	  xy_=xy.evalf2double(elevel,newcontextptr);
	  if (xy_.type==_VECT && xy_._VECTptr->size()==2){
	    x_=xy_._VECTptr->front();
	    y_=xy_._VECTptr->back();
	  }
	  else {
	    reim(xy_,x_,y_,newcontextptr);
	  }
	  if ( (x_.type!=_DOUBLE_) || (y_.type!=_DOUBLE_) )
	    joindre=false;
	  else {
	    entrei=x_._DOUBLE_val;
	    entrej=y_._DOUBLE_val;
	    if (j>oldj)
	      joindre=(j>=entrej) && (entrej>=oldj);
	    else
	      joindre=(j<=entrej) && (entrej<=oldj);
	    if (i>oldi)
	      joindre=joindre && (i>=entrei) && (entrei>=oldi);
	    else
	      joindre=joindre && (i<=entrei) && (entrei<=oldi);
	  }
	  local_sto_double_increment(-function_tstep/2,*vars._IDNTptr,newcontextptr);
	  // vars._IDNTptr->localvalue->back()._DOUBLE_val -= function_tstep/2;
	}
	else
	  joindre=true;
      } // if t!=function_tmin
      else
	joindre=true;
      if (joindre){
	chemin.push_back(gen(i,j));
        //dbg_printf("chemin add %s\n",chemin.back().print().c_str());
      }
      else {
	if (!chemin.empty()){
          //dbg_printf("res push, chemin size %i\n",chemin.size());
	  res.push_back(symb_pnt(symb_curve(gen(makevecteur(fC,vars,function_tmin,t-function_tstep,0,equation,parameq,vparam),_PNT__VECT),gen(chemin,_GROUP__VECT)),attribut,contextptr));
        }
	function_tmin=t;
	chemin.clear();
        chemin.push_back(gen(i,j));
      }
      oldi=i;
      oldj=j;
    }
    if (!chemin.empty()){
      res.push_back(symb_pnt(symb_curve(gen(makevecteur(fC,vars,function_tmin,function_tmax,0,equation,parameq,vparam),_PNT__VECT),gen(chemin,_GROUP__VECT)),attribut,contextptr));
      //dbg_printf("res push chemin size=%i\n",chemin.size());
    }
    leave(protect,localvar,newcontextptr);
    if (res.size()==1)
      return res.front();
    //dbg_printf("res size=%i\n",res.size());
    return res; // e;
  }

  gen plotparam(const gen & f,const gen & vars,const vecteur & attributs,bool densityplot,double function_xmin,double function_xmax,double function_ymin,double function_ymax,double function_tmin, double function_tmax,double function_tstep,const gen & equation,const gen & parameq,const context * contextptr){
    return plotparam(f,vars,attributs,densityplot,function_xmin,function_xmax,function_ymin,function_ymax,function_tmin,function_tmax,function_tstep,equation,parameq,0,contextptr);
  }

  gen paramplotparam(const gen & args,bool densityplot,const context * contextptr){
#ifndef WITH_PLOT
    return undef;
#endif
    // args= [x(t)+i*y(t),t] should add a t interval
    bool f_autoscale=autoscale;
    if (args.type!=_VECT || args.subtype!=_SEQ__VECT)
      return paramplotparam(makesequence(args,ggb_var(args)),false,contextptr);
    vecteur vargs(plotpreprocess(args,contextptr));
    if (is_undef(vargs))
      return vargs;
    if (vargs.size()<2)
      return symbolic(at_plotparam,args);
    gen f=vargs.front();
    gen vars=vargs[1];
    /*
      if (f.type==_VECT && f._VECTptr->size()==2)
      f=f._VECTptr->front()+cst_i*f._VECTptr->back();
    */
    bool param2d=f.type!=_VECT || f._VECTptr->size()==2;
    if (param2d){
      if (vars.type==_VECT){
	if (vars._VECTptr->size()!=1)
	  return gensizeerr(contextptr);
	vars=vars._VECTptr->front();
      }
    }
    int s=int(vargs.size());
    gen eq=0,parameq=0,vparam=0;
    if (s>5)
      eq=vargs[5];
    if (s>6)
      parameq=vargs[6];
    if (s>7)
      vparam=vargs[7];
    double tmin=gnuplot_tmin,tmax=gnuplot_tmax,xmin=gnuplot_xmin,xmax=gnuplot_xmax,ymin=gnuplot_ymin,ymax=gnuplot_ymax,tstep=-1;
    int elevel=eval_level(contextptr);
    if (param2d && s>=3){
      gen trange=vargs[2].evalf_double(elevel,contextptr);
      if ( (trange.type==_SYMB) && (trange._SYMBptr->sommet==at_interval)){
	tmin=evalf_double(trange._SYMBptr->feuille._VECTptr->front(),elevel,contextptr)._DOUBLE_val;
	tmax=evalf_double(trange._SYMBptr->feuille._VECTptr->back(),elevel,contextptr)._DOUBLE_val;
	if (s>3)
	  tstep=vargs[3].evalf_double(elevel,contextptr)._DOUBLE_val;
      }
      else {
	if (s>=4){
	  tmin=vargs[2].evalf_double(elevel,contextptr)._DOUBLE_val;
	  tmax=vargs[3].evalf_double(elevel,contextptr)._DOUBLE_val;
	  if (s>4 && !vargs[4].is_equal())
	    tstep=vargs[4].evalf_double(elevel,contextptr)._DOUBLE_val;
	}
      }
      if (tmin>tmax )
	swapdouble(tmin,tmax);
    }
    gen attribut=COLOR_BLACK;
    for (int i=1;i<s;++i){
      gen & vargsi= vargs[i];
      if (readvar(vargsi)==x__IDNT_e){
	f_autoscale=false;
	readrange(vargsi,gnuplot_xmin,gnuplot_xmax,vargsi,xmin,xmax,contextptr);
      }
      if (readvar(vargsi)==y__IDNT_e){
	f_autoscale=false;
	readrange(vargsi,gnuplot_ymin,gnuplot_ymax,vargsi,ymin,ymax,contextptr);
      }
      if (readvar(vargsi)==t__IDNT_e){
	f_autoscale=false;
	readrange(vargsi,tmin,tmax,vargsi,tmin,tmax,contextptr); 
      }
      if (vargsi.is_equal() && vargsi._SYMBptr->feuille.type==_VECT && vargsi._SYMBptr->feuille._VECTptr->front().type==_INT_){
	gen n=vargsi._SYMBptr->feuille._VECTptr->back();
	switch (vargsi._SYMBptr->feuille._VECTptr->front().val){
	case _NSTEP:
	  if (n.type!=_INT_) return gensizeerr(contextptr);
	  tstep=absdouble((tmax-tmin)/n.val);
	  break;
	case _TSTEP:
	  n=evalf_double(n,1,contextptr);
	  tstep=absdouble(n._DOUBLE_val);
	  break;
	}
      }
    }
    vecteur attributs(1,attribut);
    s=read_attributs(vargs,attributs,contextptr);
    if (tstep<0)
      tstep=(tmax-tmin)/gnuplot_pixels_per_eval;
    if (!readrange(vars,tmin,tmax,vars,tmin,tmax,contextptr))
      return gensizeerr(gettext("2nd arg must be a free variable"));
    return plotparam(f,vars,attributs,densityplot,xmin,xmax,ymin,ymax,tmin,tmax,tstep,eq,parameq,vparam,contextptr);
  }
  gen _plotparam(const gen & args,const context * contextptr){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    return paramplotparam(args,true,contextptr);
  }
  static const char _plotparam_s []="plotparam";
  static define_unary_function_eval_quoted (__plotparam,&_plotparam,_plotparam_s);
  define_unary_function_ptr5( at_plotparam ,alias_at_plotparam,&__plotparam,_QUOTE_ARGUMENTS,true);

  gen _plotpolar(const gen & args,const context * contextptr){
#ifndef WITH_PLOT
    return undef;
#endif
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    // args= [rho(theta),theta] should add a theta interval
    vecteur vargs(plotpreprocess(args,contextptr));
    if (is_undef(vargs))
      return vargs;
    gen rho=vargs.front();
    gen theta=vargs[1];
    if (theta.is_equal())
      theta=theta._SYMBptr->feuille._VECTptr->front();
    if (theta.type!=_IDNT)
      return gensizeerr(gettext("2nd arg must be a free variable"));    
    // vargs.front()=symbolic_re(rho)*expi(degtorad(theta,contextptr),contextptr);
    vargs.front()=rho*exp(cst_i*angletorad(theta,contextptr),contextptr); // makevecteur(rho*cos(angletorad(theta,contextptr),contextptr),rho*sin(angletorad(theta,contextptr),contextptr));
    return _plotparam(gen(vargs,_SEQ__VECT),contextptr);
  }
  static const char _plotpolar_s []="plotpolar";
  static define_unary_function_eval_quoted (__plotpolar,&_plotpolar,_plotpolar_s);
  define_unary_function_ptr5( at_plotpolar ,alias_at_plotpolar,&__plotpolar,_QUOTE_ARGUMENTS,true);

  gen _LineTan(const gen & g,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return undef;
#endif
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    vecteur attributs(1,COLOR_BLACK);
    vecteur v(seq2vecteur(g));
    int s=read_attributs(v,attributs,contextptr);
    if (s<2 || s>3)
      return gensizeerr(contextptr);
    gen f(v[0]),x(vx_var()),x0(0);
    if (s==3){
      x=v[1];
      x0=v[2];
    }
    if (s==2){
      x0=v[1];
      if (is_equal(x0)){
	gen & x0f=x0._SYMBptr->feuille;
	if (x0f.type==_VECT && x0f._VECTptr->size()==2){
	  x=x0f._VECTptr->front();
	  x0=x0f._VECTptr->back();
	}
      }
    }
    bool param;
    if (f.type==_VECT){
      if (f._VECTptr->size()!=2)
	return undef;
      f=vect2c(f); // f=gen(f._VECTptr->front(),f._VECTptr->back());
      param=true;
    }
    else
      param=!is_zero(im(f,contextptr),contextptr);
    gen fprime(derive(f,x,contextptr)),M0,direction;
    if (is_undef(fprime)) return fprime;
    M0=subst(f,x,x0,false,contextptr);
    direction=subst(fprime,x,x0,false,contextptr);
    if (!param){
      M0=gen(x0,M0);
      direction=gen(1,direction);
    }
    return put_attributs(_droite(makesequence(M0,M0+direction),contextptr),attributs,contextptr);
  }
  static const char _LineTan_s[]="linetan";
  static define_unary_function_eval (__LineTan,&_LineTan,_LineTan_s);
  define_unary_function_ptr5( at_LineTan ,alias_at_LineTan,&__LineTan,0,T_RETURN);

  // plot solution of y'=f(x,y) [x0,y0] in current plot range
  // OR [x,y]'=f(t,x,y) t0 x0 y0 in current plot range
  // args = dy/dx, [x, y], [x0, y0]
  // OR [dx/dt, dy/dt], [t, x, y], [t0, x0, y0]
  static gen plotode(const vecteur & w,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return undef;
#endif
    vecteur v(w);
    bool curve=true;
    gen fp=v[0];
    if (fp.is_equal() && fp._SYMBptr->feuille[0].type==_INT_){
      fp=fp._SYMBptr->feuille[1];
      *logptr(contextptr) << "Warning, replacing plotode(" << v[0] << " by plotode(" << fp << "\n";
    }
    if (fp.type!=_VECT) // y'=f(x,y)
      fp=makevecteur(plus_one,fp);
    gen vars=v[1];
    bool dim3=vars.type==_VECT && vars._VECTptr->size()==3;
    gen init=remove_at_pnt(v[2]);
    if (init.type!=_VECT)
      init=makevecteur(re(init,contextptr),im(init,contextptr));
    gen t;
    int s;
    if (vars.type != _VECT || (s = int(vars._VECTptr->size()))<2)
      return gensizeerr(contextptr);
    if (s==3){
      t=vars._VECTptr->front();
      vars=makevecteur(vars[1],vars[2]);
    }
    else {
      identificateur tt(" t");
      t=tt;
      if (s==2 && vars[0].is_equal()){
	v.push_back(vars[0]);
	t=vars[0]._SYMBptr->feuille[0];
	vars=makevecteur(t,vars[1]);
      }
    }
    if (vars.type==_VECT && vars._VECTptr->size()==2 && vars._VECTptr->back().is_equal())
      vars=makevecteur(vars._VECTptr->front(),vars._VECTptr->back()._SYMBptr->feuille[0]);
    v[1]=vars;
    vecteur f=makevecteur(fp,(t.is_equal()?t._SYMBptr->feuille._VECTptr->front():t),vars);
    if (init.type != _VECT || (s = int(init._VECTptr->size()))<2)
      return gensizeerr(contextptr);
    vecteur & initv=*init._VECTptr;
    gen t0=initv.front();
    gen x0=t0;
    if (s==3)
      x0=initv[1];
    gen y0=makevecteur(x0,initv.back());
    double ym[2]={gnuplot_xmin,gnuplot_ymin},yM[2]={gnuplot_xmin,gnuplot_ymin};
    double * ymin = 0;
    double * ymax = 0;
    double tstep=0,tmin=-5,tmax=5;
    bool tminmax_defined,tstep_defined;
    read_tmintmaxtstep(v,t,3,tmin,tmax,tstep,tminmax_defined,tstep_defined,contextptr);
    if (tmin>tmax || tstep<=0)
      return gensizeerr(gettext("Time"));
    int maxstep=100;
    if (tminmax_defined && tstep_defined)
      maxstep=giacmax(maxstep,2*int((tmax-tmin)/tstep));
    int vs=int(v.size());
    for (int i=3;i<vs;++i){
      gen & vi=v[i];
      if (readvar(vi)==vars[0]){
	if (readrange(vi,gnuplot_xmin,gnuplot_xmax,vi,ym[0],yM[0],contextptr)){
	  ymin=ym;
	  ymax=yM;
	  v.erase(v.begin()+i);
	  --vs;
	}
      }
      if (readvar(vi)==vars[1]){
	if (readrange(vi,gnuplot_xmin,gnuplot_xmax,vi,ym[1],yM[1],contextptr)){
	  ymin=ym;
	  ymax=yM;
	  v.erase(v.begin()+i);
	  --vs;
	}
      }
    }
    vecteur res1v;
    gen t0f=evalf_double(t0,1,contextptr);
    double d0=t0f.type==_DOUBLE_?t0f._DOUBLE_val:0;
    if (tmin<d0){
      gen res1=odesolve(t0,tmin,f,y0,tstep,curve,ymin,ymax,maxstep,contextptr);
      if (is_undef(res1)) return res1;
      res1v.swap(*res1._VECTptr);
      std::reverse(res1v.begin(),res1v.end());
    }
    res1v.push_back(makevecteur(t0,y0));
    if (tmax>d0){
      gen res2=odesolve(t0,tmax,f,y0,tstep,curve,ymin,ymax,maxstep,contextptr);
      if (is_undef(res2)) return res2;
      const vecteur & v=*res2._VECTptr;
      res1v.reserve(res1v.size()+v.size());
      for (int i=0;i<v.size();++i)
        res1v.push_back(v[i]);
    }
    // make the curve
    const_iterateur it=res1v.begin(),itend=res1v.end();
    vecteur res;
    res.reserve(itend-it);
    for (;it!=itend;++it){
      if (it->type!=_VECT || it->_VECTptr->empty() || it->_VECTptr->back().type!=_VECT) 
	continue;
      const vecteur & tmp=*it->_VECTptr->back()._VECTptr;
      if (tmp.size()!=2 || is_undef(tmp.front()) || is_undef(tmp.back()))
	continue;
      if (dim3)
	res.push_back(gen(makevecteur(it->_VECTptr->front(),tmp.front(),tmp.back()),_POINT__VECT));
      else
	res.push_back(vect2c(tmp));
    }
    return symb_pnt(gen(res,_GROUP__VECT),contextptr);
  }
  gen _plotode(const gen & args,GIAC_CONTEXT){
#ifndef WITH_PLOT
    return undef;
#endif
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    vecteur attributs(1,COLOR_BLACK);
    vecteur v(seq2vecteur(args));
    int s=read_attributs(v,attributs,contextptr);
    if (s<3)
      return gendimerr(contextptr);
    // v.erase(v.begin()+s,v.end());
    return put_attributs(plotode(v,contextptr),attributs,contextptr);
  }
  static const char _plotode_s []="plotode";
  static define_unary_function_eval (__plotode,&_plotode,_plotode_s);
  define_unary_function_ptr5( at_plotode ,alias_at_plotode,&__plotode,0,true);

#ifdef WITH_PLOT
  gen plotfield(const gen & xp,const gen & yp,const gen & x,const gen & y,double xmin,double xmax,double xstep,double ymin,double ymax,double ystep,double scaling,vecteur & attributs,bool normalize,const context * contextptr){
    if (xstep<=0 || ystep<=0)
      return gensizeerr("Invalid xstep or ystep");
    ck_parameter_x(contextptr);
    ck_parameter_y(contextptr);
    // vecteur xy_v(makevecteur(x,y)),xy(xy_v),curxcury(2);
    gen xp_eval,yp_eval,origine;
    vecteur res;
    double echelle,minxstepystep;
    if (xstep<ystep) minxstepystep=xstep; else minxstepystep=ystep;
    echelle=minxstepystep;
    int n1=1+std::ceil((xmax-xmin)/(scaling*xstep));
    int n2=1+std::ceil((ymax-ymin)/(scaling*ystep));
    res.reserve(n1*n2+1);
    for (double curx=xmin;curx<=xmax;curx+=scaling*xstep){
      if (ctrl_c || interrupted) break;
      // curxcury[0]=curx;
      gen xpx(subst(xp,x,curx,false,contextptr));
      gen ypx(subst(yp,x,curx,false,contextptr));
      for (double cury=ymin;cury<=ymax;cury+=scaling*ystep){
	if (ctrl_c || interrupted) break;
	// curxcury[1]=cury;
	xp_eval=subst(xpx,y,cury,false,contextptr).evalf2double(eval_level(contextptr),contextptr);
	yp_eval=subst(ypx,y,cury,false,contextptr).evalf2double(eval_level(contextptr),contextptr);
	if ((xp_eval.type==_DOUBLE_) && (yp_eval.type==_DOUBLE_)){
	  double xpd=xp_eval._DOUBLE_val,ypd=yp_eval._DOUBLE_val;
	  if (normalize){
	    echelle=minxstepystep/std::sqrt(xpd*xpd+ypd*ypd);
	    origine=gen(curx-xpd/2*echelle,cury-ypd/2*echelle);
	  }
	  else {
	    origine=gen(curx,cury);
	  }
	  // always return vectors now, before it was != in dimension 1
	  res.push_back(pnt_attrib(gen(makevecteur(origine,origine+gen(echelle*xpd,echelle*ypd)),
				       //is_one(xp)?_GROUP__VECT:_VECTOR__VECT
				       _VECTOR__VECT
				       ),attributs,contextptr));
	}
      }
    }
    return gen(res,_GROUP__VECT);
  }

  static bool read_plotfield_args(const gen & args,gen & xp,gen & yp,gen & x,gen & y,double & xmin,double & xmax,double & xstep,double & ymin,double & ymax,double & ystep,vecteur & attributs,bool & normalize,vecteur & initcondv,GIAC_CONTEXT){
    if (args.type!=_VECT || args._VECTptr->size()<2)
      return false; // setsizeerr(contextptr);
    normalize=false;
    vecteur v(*args._VECTptr);
    int s = int(v.size());
    if (s && v[0].is_equal()){
      // accept plotfield(y'=f(t,y),...) instead of plotfield(f(t,y),...)
      gen f=v[0]._SYMBptr->feuille;
      if (f.type==_VECT && f._VECTptr->size()==2 && f._VECTptr->front().type==_INT_){
	*logptr(contextptr) << "Warning, replacing plotfield of " << v[0] << " by " << f._VECTptr->back() << "\n";
	v[0]=f._VECTptr->back(); 
      }
    }
    for (int i=0;i<s;++i){
      if (v[i]==at_normalize){
	normalize=true;
	v.erase(v.begin()+i);
	--s;
      }
      if (v[i].is_equal() && v[i]._SYMBptr->feuille.type==_VECT && v[i]._SYMBptr->feuille._VECTptr->size()==2 && v[i]._SYMBptr->feuille._VECTptr->front()==at_plotode){
	gen add=v[i]._SYMBptr->feuille._VECTptr->back();
	if (add.type==_VECT && !add._VECTptr->empty()){
	  if (add._VECTptr->front().type==_VECT){
	    const_iterateur it=add._VECTptr->begin(),itend=add._VECTptr->end();
	    for (;it!=itend;++it)
	      initcondv.push_back(*it);
	  }
	  else
	    initcondv.push_back(add);
	  v.erase(v.begin()+i);
	}
	--s;
      }
    }
    s=read_attributs(v,attributs,contextptr);
    v=vecteur(args._VECTptr->begin(),args._VECTptr->begin()+s);
    if (s>=2){
      initcondv.insert(initcondv.begin(),v[0]);
      initcondv.insert(initcondv.begin()+1,v[1]);
    }
    switch (s){
    case 0: case 1:
      return false; // setsizeerr(contextptr);
    case 2:
      if ( (v.back().type!=_VECT) || (v.back()._VECTptr->size()!=2) )
	return false; // setsizeerr(contextptr);
      x=v.back()._VECTptr->front();
      y=v.back()._VECTptr->back();
      yp=equal2diff(v.front());
      if (yp.type==_VECT){
	xp=yp._VECTptr->front();
	yp=yp._VECTptr->back();
      }
      else
	xp=plus_one;
      break;
    case 3:
      if (v[0].type!=_VECT){
	xp=plus_one;
	yp=equal2diff(v[0]);
      }
      else {
	if (v[0]._VECTptr->size()!=2)
	  return false; // setsizeerr(contextptr);
	xp=v[0]._VECTptr->front();
	yp=v[0]._VECTptr->back();
      }
      x=v[1];
      y=v[2];
      break;
    default:
      xp=v[0];
      yp=v[1];
      x=v[2];
      y=v[3];
    }
    int nstep=int(std::sqrt(double(gnuplot_pixels_per_eval)/2)),jstep=0,kstep=0;
    readrange(x,gnuplot_xmin,gnuplot_xmax,x,xmin,xmax,contextptr);
    readrange(y,gnuplot_xmin,gnuplot_xmax,y,ymin,ymax,contextptr);
    vecteur tmp;
    read_option(*args._VECTptr,xmin,xmax,ymin,ymax,gnuplot_zmin,gnuplot_zmax,tmp,nstep,jstep,kstep,contextptr);
    xstep=(xmax-xmin)/nstep;
    ystep=(ymax-ymin)/(jstep?jstep:nstep);
    return true;
  }
  // args=[dx/dt,dy/dt,x,y] or [dy/dx,x,y]
  // or [ [dx/dt,dy/dt], [x,y] ] or [ dy/dx, [x,y]]
  gen _plotfield(const gen & args,const context * contextptr){
    if ( args.type==_STRNG && args.subtype==-1) return  args;
    vecteur attributs;
    gen xp,yp,x,y;
    double xmin,xmax,ymin,ymax,xstep,ystep;
    bool normalize;
    vecteur initcondv;
    if (!read_plotfield_args(args,xp,yp,x,y,xmin,xmax,xstep,ymin,ymax,ystep,attributs,normalize,initcondv,contextptr))
      return gensizeerr(contextptr);
    int s=initcondv.size();
    double scaling=2;
    if (s>2){
      vecteur res;
      res.push_back(plotfield(xp,yp,x,y,xmin,xmax,xstep/scaling,ymin,ymax,ystep/scaling,scaling,attributs,normalize,contextptr));
      vecteur argu(3);
      argu[0]=initcondv[0]; argu[1]=initcondv[1];
      for (int i=2;i<s;++i){
	argu[2]=initcondv[i];
	res.push_back(plotode(argu,contextptr));
      }
      return res;
    }
    return plotfield(xp,yp,x,y,xmin,xmax,xstep/scaling,ymin,ymax,ystep/scaling,scaling,attributs,normalize,contextptr);
  }
  static const char _plotfield_s []="plotfield";
  static define_unary_function_eval (__plotfield,&_plotfield,_plotfield_s);
  define_unary_function_ptr5( at_plotfield ,alias_at_plotfield,&__plotfield,0,true);


#endif

  
  logo_turtle * turtleptr=0;
  
  logo_turtle & turtle(){
    if (!turtleptr)
      turtleptr=new logo_turtle;
    return * turtleptr;
  }
  
#ifndef NOTURTLE
#ifdef TURTLETAB
  const int MAX_LOGO=128; // 1024
  logo_turtle tablogo[MAX_LOGO];
  int turtle_stack_size=0;
  int turtle_stack_push_back(const logo_turtle & l){
    if (turtle_stack_size>=MAX_LOGO)
      return -1;
    if (turtle_stack_size==0){
      tablogo[0]=logo_turtle();
      ++turtle_stack_size;
    }
    tablogo[turtle_stack_size]=l;
    ++turtle_stack_size;
    return turtle_stack_size;
  }

#else
  const int MAX_LOGO=256;//512;

  std::vector<logo_turtle> & turtle_stack(){
    static std::vector<logo_turtle> * ans = 0;
    if (!ans){
      ans=new std::vector<logo_turtle>(1,(*turtleptr));
    }
    return *ans;
  }
#endif

  logo_turtle vecteur2turtle(const vecteur & v){
    int s=int(v.size());
    if (s>=5 && v[0].type==_DOUBLE_ && v[1].type==_DOUBLE_ && v[2].type==_DOUBLE_ && v[3].type==_INT_ && v[4].type==_INT_ ){
      logo_turtle t;
      t.x=v[0]._DOUBLE_val;
      t.y=v[1]._DOUBLE_val;
      t.theta=v[2]._DOUBLE_val;
      int i=v[3].val;
      t.mark=(i%2)!=0;
      i=i >> 1;
      t.visible=(i%2)!=0;
      i=i >> 1;
      t.direct = (i%2)!=0;
      i=i >> 1;
      t.turtle_length = i & 0xff;
      i=i >> 8;
      t.color = i;
      t.radius = v[4].val;
      if (s>5 && v[5].type==_INT_)
	t.s=v[5].val;
      else
	t.s=-1;
      return t;
    }
#ifndef NO_STDEXCEPT
    setsizeerr(gettext("vecteur2turtle")); // FIXME
#endif
    return logo_turtle();
  }

  static int turtle_status(const logo_turtle & turtle){
    int status= (turtle.color << 11) | ( (turtle.turtle_length & 0xff) << 3) ;
    if (turtle.direct)
      status += 4;
    if (turtle.visible)
      status += 2;
    if (turtle.mark)
      status += 1;
    return status;
  }

  bool set_turtle_state(const vecteur & v,GIAC_CONTEXT){
    if (v.size()>=2 && v[0].type==_DOUBLE_ && v[1].type==_DOUBLE_){
      vecteur w(v);
      int s=int(w.size());
      if (s==2)
	w.push_back(double((*turtleptr).theta));
      if (s<4)
	w.push_back(turtle_status((*turtleptr)));
      if (s<5)
	w.push_back(0);
      if (w[2].type==_DOUBLE_ && w[3].type==_INT_ && w[4].type==_INT_){
	(*turtleptr)=vecteur2turtle(w);
#ifdef TURTLETAB
	turtle_stack_push_back(*turtleptr);
#else
	turtle_stack().push_back((*turtleptr));
#endif
	return true;
      }
    }
    return false;
  }

  gen turtle2gen(const logo_turtle & turtle){
    return gen(makevecteur(turtle.x,turtle.y,double(turtle.theta),turtle_status(turtle),turtle.radius,turtle.s),_LOGO__VECT);
  }

  gen turtle_state(GIAC_CONTEXT){
    return turtle2gen((*turtleptr));
  }

  static gen update_turtle_state(bool clrstring,GIAC_CONTEXT){
#ifdef TURTLETAB
    if (turtle_stack_size>=MAX_LOGO)
      return gensizeerr("Not enough memory");
#else
    if (turtle_stack().size()>=MAX_LOGO){
      ctrl_c=true; interrupted=true;
      return gensizeerr("Not enough memory");
    }
#endif
    if (clrstring)
      (*turtleptr).s=-1;
    (*turtleptr).theta = (*turtleptr).theta - floor((*turtleptr).theta/360)*360;
#ifdef TURTLETAB
    turtle_stack_push_back((*turtleptr));
#else
    turtle_stack().push_back((*turtleptr));
#endif
    gen res=turtle_state(contextptr);
#ifdef EMCC // should directly interact with canvas
    return gen(turtlevect2vecteur(turtle_stack()),_LOGO__VECT);
#endif
    return res;
  }

  gen _avance(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    // logo instruction
    double i;
    if (g.type!=_INT_){
      if (g.type==_VECT)
	i=(*turtleptr).turtle_length;
      else {
	gen g1=evalf_double(g,1,contextptr);
	if (g1.type==_DOUBLE_)
	  i=g1._DOUBLE_val;
	else
	  return gensizeerr(contextptr);
      }
    }
    else
      i=g.val;
    (*turtleptr).x += i * std::cos((*turtleptr).theta*deg2rad_d);
    (*turtleptr).y += i * std::sin((*turtleptr).theta*deg2rad_d) ;
    (*turtleptr).radius = 0;
    return update_turtle_state(true,contextptr);
  }
  static const char _avance_s []="avance";
  static define_unary_function_eval2 (__avance,&_avance,_avance_s,&printastifunction);
  define_unary_function_ptr5( at_avance ,alias_at_avance,&__avance,0,T_LOGO);

  //static const char _forward_s []="forward";
  //static define_unary_function_eval (__forward,&_avance,_forward_s);
  //define_unary_function_ptr5( at_forward ,alias_at_forward,&__forward,0,true);

  gen _recule(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    // logo instruction
    if (g.type==_VECT)
      return _avance(-(*turtleptr).turtle_length,contextptr);
    return _avance(-g,contextptr);
  }
  static const char _recule_s []="recule";
  static define_unary_function_eval2 (__recule,&_recule,_recule_s,&printastifunction);
  define_unary_function_ptr5( at_recule ,alias_at_recule,&__recule,0,T_LOGO);

  //static const char _backward_s []="backward";
  //static define_unary_function_eval (__backward,&_recule,_backward_s);
  //define_unary_function_ptr5( at_backward ,alias_at_backward,&__backward,0,true);

  gen _position(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    // logo instruction
    if (g.type!=_VECT)
      return makevecteur((*turtleptr).x,(*turtleptr).y);
    // return turtle_state();
    vecteur v = *g._VECTptr;
    int s=int(v.size());
    if (!s)
      return makevecteur((*turtleptr).x,(*turtleptr).y);
    v[0]=evalf_double(v[0],1,contextptr);
    if (s>1)
      v[1]=evalf_double(v[1],1,contextptr);
    if (s>2)
      v[2]=evalf_double(v[2],1,contextptr); 
    if (set_turtle_state(v,contextptr))
      return update_turtle_state(true,contextptr);
    return zero;
  }
  static const char _position_s []="position";
  static define_unary_function_eval2 (__position,&_position,_position_s,&printastifunction);
  define_unary_function_ptr5( at_position ,alias_at_position,&__position,0,T_LOGO);

  gen _cap(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    // logo instruction
    gen gg=evalf_double(g,1,contextptr);
    if (gg.type!=_DOUBLE_)
      return double((*turtleptr).theta);
    (*turtleptr).theta=gg._DOUBLE_val;
    (*turtleptr).radius = 0;
    return update_turtle_state(true,contextptr);
  }
  static const char _cap_s []="cap";
  static define_unary_function_eval2 (__cap,&_cap,_cap_s,&printastifunction);
  define_unary_function_ptr5( at_cap ,alias_at_cap,&__cap,0,T_LOGO);

  //static const char _heading_s []="heading";
  //static define_unary_function_eval (__heading,&_cap,_heading_s);
  //define_unary_function_ptr5( at_heading ,alias_at_heading,&__heading,0,true);


  gen _tourne_droite(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    // logo instruction
    if (g.type!=_INT_){
      if (g.type==_VECT)
	(*turtleptr).theta -= 90;
      else {
	gen g1=evalf_double(g,1,contextptr);
	if (g1.type==_DOUBLE_)
	  (*turtleptr).theta -= g1._DOUBLE_val;
	else
	  return gensizeerr(contextptr);
      }
    }
    else
      (*turtleptr).theta -= g.val;
    (*turtleptr).radius = 0;
    return update_turtle_state(true,contextptr);
  }
  static const char _tourne_droite_s []="tourne_droite";
  static define_unary_function_eval2 (__tourne_droite,&_tourne_droite,_tourne_droite_s,&printastifunction);
  define_unary_function_ptr5( at_tourne_droite ,alias_at_tourne_droite,&__tourne_droite,0,T_LOGO);

  gen _tourne_gauche(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    // logo instruction
    if (g.type==_VECT){
      (*turtleptr).theta += 90;
      (*turtleptr).radius = 0;
      return update_turtle_state(true,contextptr);
    }
    return _tourne_droite(-g,contextptr);
  }
  static const char _tourne_gauche_s []="tourne_gauche";
  static define_unary_function_eval2 (__tourne_gauche,&_tourne_gauche,_tourne_gauche_s,&printastifunction);
  define_unary_function_ptr5( at_tourne_gauche ,alias_at_tourne_gauche,&__tourne_gauche,0,T_LOGO);

  gen _leve_crayon(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    // logo instruction
    (*turtleptr).mark = false;
    (*turtleptr).radius = 0;
    return update_turtle_state(true,contextptr);
  }
  static const char _leve_crayon_s []="leve_crayon";
  static define_unary_function_eval2 (__leve_crayon,&_leve_crayon,_leve_crayon_s,&printastifunction);
  define_unary_function_ptr5( at_leve_crayon ,alias_at_leve_crayon,&__leve_crayon,0,T_LOGO);

  //static const char _penup_s []="penup";
  //static define_unary_function_eval (__penup,&_leve_crayon,_penup_s);
  //define_unary_function_ptr5( at_penup ,alias_at_penup,&__penup,0,T_LOGO);

  gen _baisse_crayon(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    // logo instruction
    (*turtleptr).mark = true;
    (*turtleptr).radius = 0;
    return update_turtle_state(true,contextptr);
  }
  static const char _baisse_crayon_s []="baisse_crayon";
  static define_unary_function_eval2 (__baisse_crayon,&_baisse_crayon,_baisse_crayon_s,&printastifunction);
  define_unary_function_ptr5( at_baisse_crayon ,alias_at_baisse_crayon,&__baisse_crayon,0,T_LOGO);

  //static const char _pendown_s []="pendown";
  //static define_unary_function_eval (__pendown,&_baisse_crayon,_pendown_s);
  //define_unary_function_ptr5( at_pendown ,alias_at_pendown,&__pendown,0,T_LOGO);

  vector<string> * ecrisptr=0;
  vector<string> & ecristab(){
    if (!ecrisptr)
      ecrisptr=new vector<string>;
    return * ecrisptr;
  }
  gen _ecris(const gen & g,GIAC_CONTEXT){    
    if ( g.type==_STRNG && g.subtype==-1) return  g;
#if 0 //def TURTLETAB
    return gensizeerr("String support does not work with static turtle table");
#endif
    // logo instruction
    (*turtleptr).radius=14;
    if (g.type==_VECT){ 
      vecteur & v =*g._VECTptr;
      int s=int(v.size());
      if (s==2 && v[1].type==_INT_){
	(*turtleptr).radius=absint(v[1].val);
	(*turtleptr).s=ecristab().size();
	ecristab().push_back(gen2string(v.front()));
	return update_turtle_state(false,contextptr);
      }
      if (s==4 && v[1].type==_INT_ && v[2].type==_INT_ && v[3].type==_INT_){
	logo_turtle t=(*turtleptr);
	_leve_crayon(0,contextptr);
	_position(makevecteur(v[2],v[3]),contextptr);
	(*turtleptr).radius=absint(v[1].val);
	(*turtleptr).s=ecristab().size();
	ecristab().push_back(gen2string(v.front()));
	update_turtle_state(false,contextptr);
	(*turtleptr)=t;
	return update_turtle_state(true,contextptr);
      }
    }
    (*turtleptr).s=ecristab().size();
    ecristab().push_back(gen2string(g));
    return update_turtle_state(false,contextptr);
  }
  static const char _ecris_s []="ecris";
  static define_unary_function_eval2 (__ecris,&_ecris,_ecris_s,&printastifunction);
  define_unary_function_ptr5( at_ecris ,alias_at_ecris,&__ecris,0,T_LOGO);

  gen _signe(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    // logo instruction
    return _ecris(makevecteur(g,20,10,10),contextptr);
  }
  static const char _signe_s []="signe";
  static define_unary_function_eval2 (__signe,&_signe,_signe_s,&printastifunction);
  define_unary_function_ptr5( at_signe ,alias_at_signe,&__signe,0,T_LOGO);

  gen _saute(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    _leve_crayon(0,contextptr);
    _avance(g,contextptr);
    return _baisse_crayon(0,contextptr);
  }
  static const char _saute_s []="saute";
  static define_unary_function_eval2 (__saute,&_saute,_saute_s,&printastifunction);
  define_unary_function_ptr5( at_saute ,alias_at_saute,&__saute,0,T_LOGO);

  //static const char _jump_s []="jump";
  //static define_unary_function_eval2 (__jump,&_saute,_jump_s,&printastifunction);
  //define_unary_function_ptr5( at_jump ,alias_at_jump,&__jump,0,T_LOGO);

  gen _pas_de_cote(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    _leve_crayon(0,contextptr);
    _tourne_droite(-90,contextptr);
    _avance(g,contextptr);
    _tourne_droite(90,contextptr);
    return _baisse_crayon(0,contextptr);
  }
  static const char _pas_de_cote_s []="pas_de_cote";
  static define_unary_function_eval2 (__pas_de_cote,&_pas_de_cote,_pas_de_cote_s,&printastifunction);
  define_unary_function_ptr5( at_pas_de_cote ,alias_at_pas_de_cote,&__pas_de_cote,0,T_LOGO);

  //static const char _skip_s []="skip";
  //static define_unary_function_eval2 (__skip,&_pas_de_cote,_skip_s,&printastifunction);
  //define_unary_function_ptr5( at_skip ,alias_at_skip,&__skip,0,T_LOGO);

  gen _cache_tortue(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    // logo instruction
    (*turtleptr).visible=false;
    (*turtleptr).radius = 0;
    return update_turtle_state(true,contextptr);
  }
  static const char _cache_tortue_s []="cache_tortue";
  static define_unary_function_eval2 (__cache_tortue,&_cache_tortue,_cache_tortue_s,&printastifunction);
  define_unary_function_ptr5( at_cache_tortue ,alias_at_cache_tortue,&__cache_tortue,0,T_LOGO);

  //static const char _hideturtle_s []="hideturtle";
  //static define_unary_function_eval (__hideturtle,&_cache_tortue,_hideturtle_s);
  //define_unary_function_ptr5( at_hideturtle ,alias_at_hideturtle,&__hideturtle,0,true);

  gen _montre_tortue(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    // logo instruction
    (*turtleptr).visible=true;
    (*turtleptr).radius = 0;
    return update_turtle_state(true,contextptr);
  }
  static const char _montre_tortue_s []="montre_tortue";
  static define_unary_function_eval2 (__montre_tortue,&_montre_tortue,_montre_tortue_s,&printastifunction);
  define_unary_function_ptr5( at_montre_tortue ,alias_at_montre_tortue,&__montre_tortue,0,T_LOGO);

  //static const char _showturtle_s []="showturtle";
  //static define_unary_function_eval (__showturtle,&_montre_tortue,_showturtle_s);
  //define_unary_function_ptr5( at_showturtle ,alias_at_showturtle,&__showturtle,0,true);


  gen _repete(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type!=_VECT || g._VECTptr->size()<2)
      return gensizeerr(contextptr);
    // logo instruction
    vecteur v = *g._VECTptr;
    v[0]=eval(v[0],contextptr);
    if (v.front().type!=_INT_)
      return gentypeerr(contextptr);
    gen prog=vecteur(v.begin()+1,v.end());
    int i=absint(v.front().val);
    gen res;
    for (int j=0;j<i;++j){
      res=eval(prog,contextptr);
    }
    return res;
  }
  static const char _repete_s []="repete";
  static define_unary_function_eval_quoted (__repete,&_repete,_repete_s);
  define_unary_function_ptr5( at_repete ,alias_at_repete,&__repete,_QUOTE_ARGUMENTS,T_RETURN);

  gen _crayon(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type==_STRNG) return _crayon(gen(*g._STRNGptr,contextptr),contextptr);
    // logo instruction
    if (g.type==_VECT && g._VECTptr->size()==3)
      return _crayon(_rgb(g,contextptr),contextptr);
    if (g.type!=_INT_){
      gen res=(*turtleptr).color;
      res.subtype=_INT_COLOR;
      return res;
    }
    (*turtleptr).color=g.val;
    (*turtleptr).radius = 0;
    return update_turtle_state(true,contextptr);
  }
  static const char _crayon_s []="crayon";
  static define_unary_function_eval2 (__crayon,&_crayon,_crayon_s,&printastifunction);
  define_unary_function_ptr5( at_crayon ,alias_at_crayon,&__crayon,0,T_LOGO);

  //static const char _pencolor_s []="pencolor";
  //static define_unary_function_eval (__pencolor,&_crayon,_pencolor_s);
  //define_unary_function_ptr5( at_pencolor ,alias_at_pencolor,&__pencolor,0,T_LOGO);

  gen _efface_logo(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type==_INT_){
      _crayon(int(FL_WHITE),contextptr);
      _recule(g,contextptr);
      return _crayon(0,contextptr);
    }
    // logo instruction
    (*turtleptr) = logo_turtle();
#ifdef TURTLETAB
    turtle_stack_size=0;
#else
    turtle_stack().clear();
#endif
    ecristab().clear();
    return update_turtle_state(true,contextptr);
  }
  static const char _efface_logo_s []="efface";
  static define_unary_function_eval2 (__efface_logo,&_efface_logo,_efface_logo_s,&printastifunction);
  define_unary_function_ptr5( at_efface_logo ,alias_at_efface_logo,&__efface_logo,0,T_LOGO);

  gen _vers(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    // logo instruction
    if (g.type!=_VECT || g._VECTptr->size()!=2)
      return gensizeerr(contextptr);
    gen x=evalf_double(g._VECTptr->front(),1,contextptr),
      y=evalf_double(g._VECTptr->back(),1,contextptr);
    if (x.type!=_DOUBLE_ || y.type!=_DOUBLE_)
      return gensizeerr(contextptr);
    double xv=x._DOUBLE_val,yv=y._DOUBLE_val,xt=(*turtleptr).x,yt=(*turtleptr).y;
    double theta=atan2(yv-yt,xv-xt);
    return _cap(theta*180/M_PI,contextptr);
  }
  static const char _vers_s []="vers";
  static define_unary_function_eval2 (__vers,&_vers,_vers_s,&printastifunction);
  define_unary_function_ptr5( at_vers ,alias_at_vers,&__vers,0,T_LOGO);

  static int find_radius(const gen & g,int & r,int & theta2,bool & direct){
    int radius;
    direct=true;
    theta2 = 360 ;
    // logo instruction
    if (g.type==_VECT && !g._VECTptr->empty()){
      vecteur v = *g._VECTptr;
      bool seg=false;
      if (v.back()==at_segment){
	v.pop_back();
	seg=true;
      }
      if (v.size()<2)
	return RAND_MAX; // setdimerr(contextptr);
      if (v[0].type==_INT_)
	r=v[0].val;
      else {
	gen v0=evalf_double(v[0],1,context0);
	if (v0.type==_DOUBLE_)
	  r=int(v0._DOUBLE_val+0.5);
	else 
	  return RAND_MAX; // setsizeerr(contextptr);
      }
      if (r<0){
	r=-r;
	direct=false;
      }
      int theta1;
      if (v[1].type==_DOUBLE_)
	theta1=int(v[1]._DOUBLE_val+0.5);
      else { 
	if (v[1].type==_INT_)
	  theta1=v[1].val;
	else return RAND_MAX; // setsizeerr(contextptr);
      }
      while (theta1<0)
	theta1 += 360;
      if (v.size()>=3){
	if (v[2].type==_DOUBLE_)
	  theta2 = int(v[2]._DOUBLE_val+0.5);
	else {
	  if (v[2].type==_INT_)
	    theta2 = v[2].val;
	  else return RAND_MAX; // setsizeerr(contextptr);
	}
	while (theta2<0)
	  theta2 += 360;
	radius = giacmin(r,512) | (giacmin(theta1,360) << 9) | (giacmin(theta2,360) << 18 ) | (seg?(1<<28):0);
      }
      else {// angle 1=0
	theta2 = theta1;
	if (theta2<0)
	  theta2 += 360;
	radius = giacmin(r,512) | (giacmin(theta2,360) << 18 ) | (seg?(1<<28):0);
      }
      return radius;
    }
    radius = 10;
    if (g.type==_INT_)
      radius= (r=g.val);
    if (g.type==_DOUBLE_)
      radius= (r=int(g._DOUBLE_val));
    if (radius<=0){
      radius = -radius;
      direct=false;
    }
    radius = giacmin(radius,512 )+(360 << 18) ; // 2nd angle = 360 degrees
    return radius;
  }

  static void turtle_move(int r,int theta2,GIAC_CONTEXT){
    double theta0;
    if ((*turtleptr).direct)
      theta0=(*turtleptr).theta-90;
    else {
      theta0=(*turtleptr).theta+90;
      theta2=-theta2;
    }
    (*turtleptr).x += r*(std::cos(M_PI/180*(theta2+theta0))-std::cos(M_PI/180*theta0));
    (*turtleptr).y += r*(std::sin(M_PI/180*(theta2+theta0))-std::sin(M_PI/180*theta0));
    (*turtleptr).theta = (*turtleptr).theta+theta2 ;
    if ((*turtleptr).theta<0)
      (*turtleptr).theta += 360;
    if ((*turtleptr).theta>360)
      (*turtleptr).theta -= 360;
  }

  gen _rond(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    int r,theta2,tmpr;
    tmpr=find_radius(g,r,theta2,(*turtleptr).direct);
    if (tmpr==RAND_MAX)
      return gensizeerr(contextptr);
    (*turtleptr).radius=tmpr;
    turtle_move(r,theta2,contextptr);
    return update_turtle_state(true,contextptr);
  }
  static const char _rond_s []="rond";
  static define_unary_function_eval2 (__rond,&_rond,_rond_s,&printastifunction);
  define_unary_function_ptr5( at_rond ,alias_at_rond,&__rond,0,T_LOGO);

  gen _disque(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    int r,theta2,tmpr=find_radius(g,r,theta2,(*turtleptr).direct);
    if (tmpr==RAND_MAX)
      return gensizeerr(contextptr);
    (*turtleptr).radius=tmpr;
    turtle_move(r,theta2,contextptr);
    (*turtleptr).radius += 1 << 27;
    return update_turtle_state(true,contextptr);
  }
  static const char _disque_s []="disque";
  static define_unary_function_eval2 (__disque,&_disque,_disque_s,&printastifunction);
  define_unary_function_ptr5( at_disque ,alias_at_disque,&__disque,0,T_LOGO);

  gen _disque_centre(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    int r,theta2;
    bool direct;
    int radius=find_radius(g,r,theta2,direct);
    if (radius==RAND_MAX)
      return gensizeerr(contextptr);
    r=absint(r);
    _saute(r,contextptr);
    _tourne_gauche(direct?90:-90,contextptr);
    (*turtleptr).radius = radius;
    (*turtleptr).direct=direct;
    turtle_move(r,theta2,contextptr);
    (*turtleptr).radius += 1 << 27;
    update_turtle_state(true,contextptr);
    _tourne_droite(direct?90:-90,contextptr);
    return _saute(-r,contextptr);
  }
  static const char _disque_centre_s []="disque_centre";
  static define_unary_function_eval2 (__disque_centre,&_disque_centre,_disque_centre_s,&printastifunction);
  define_unary_function_ptr5( at_disque_centre ,alias_at_disque_centre,&__disque_centre,0,T_LOGO);

  gen _polygone_rempli(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    if (g.type==_INT_){
      (*turtleptr).radius=-absint(g.val);
      if ((*turtleptr).radius<-1)
	return update_turtle_state(true,contextptr);
    }
    return gensizeerr(gettext("Integer argument >= 2"));
  }
  static const char _polygone_rempli_s []="polygone_rempli";
  static define_unary_function_eval2 (__polygone_rempli,&_polygone_rempli,_polygone_rempli_s,&printastifunction);
  define_unary_function_ptr5( at_polygone_rempli ,alias_at_polygone_rempli,&__polygone_rempli,0,T_LOGO);

  gen _rectangle_plein(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    gen gx=g,gy=g;
    if (g.type==_VECT && g._VECTptr->size()==2){
      gx=g._VECTptr->front();
      gy=g._VECTptr->back();
    }
    for (int i=0;i<2;++i){
      _avance(gx,contextptr);
      _tourne_droite(-90,contextptr);
      _avance(gy,contextptr);
      _tourne_droite(-90,contextptr);
    }
    //for (int i=0;i<turtle_stack().size();++i){ *logptr(contextptr) << turtle2gen(turtle_stack()[i]) <<"\n";}
    return _polygone_rempli(-8,contextptr);
  }
  static const char _rectangle_plein_s []="rectangle_plein";
  static define_unary_function_eval2 (__rectangle_plein,&_rectangle_plein,_rectangle_plein_s,&printastifunction);
  define_unary_function_ptr5( at_rectangle_plein ,alias_at_rectangle_plein,&__rectangle_plein,0,T_LOGO);

  gen _triangle_plein(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    gen gx=g,gy=g,gtheta=60;
    if (g.type==_VECT && g._VECTptr->size()>=2){
      vecteur & v=*g._VECTptr;
      gx=v.front();
      gy=v[1];
      gtheta=90;
      if (v.size()>2)
	gtheta=v[2];
    }
    logo_turtle t=(*turtleptr);
    _avance(gx,contextptr);
    double save_x=(*turtleptr).x,save_y=(*turtleptr).y;
    _recule(gx,contextptr);
    _tourne_gauche(gtheta,contextptr);
    _avance(gy,contextptr);
    (*turtleptr).x=save_x;
    (*turtleptr).y=save_y;
    update_turtle_state(true,contextptr);
    (*turtleptr)=t;
    (*turtleptr).radius=0;
    update_turtle_state(true,contextptr);
    return _polygone_rempli(-3,contextptr);
  }
  static const char _triangle_plein_s []="triangle_plein";
  static define_unary_function_eval2 (__triangle_plein,&_triangle_plein,_triangle_plein_s,&printastifunction);
  define_unary_function_ptr5( at_triangle_plein ,alias_at_triangle_plein,&__triangle_plein,0,T_LOGO);

  gen _dessine_tortue(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    // logo instruction
    /*
      _triangle_plein(makevecteur(17,5));
      _tourne_droite(90);
      _triangle_plein(makevecteur(5,17));
      return _tourne_droite(-90);
    */
    double save_x=(*turtleptr).x,save_y=(*turtleptr).y;
    _tourne_droite(90,contextptr);
    _avance(5,contextptr);
    _tourne_gauche(106,contextptr);
    _avance(18,contextptr);
    _tourne_gauche(148,contextptr);
    _avance(18,contextptr);
    _tourne_gauche(106,contextptr);
    _avance(5,contextptr);
    (*turtleptr).x=save_x; (*turtleptr).y=save_y;
    gen res(_tourne_gauche(90,contextptr));
    if (is_one(g))
      return res;
    return _polygone_rempli(-9,contextptr);
  }
  static const char _dessine_tortue_s []="dessine_tortue";
  static define_unary_function_eval2 (__dessine_tortue,&_dessine_tortue,_dessine_tortue_s,&printastifunction);
  define_unary_function_ptr5( at_dessine_tortue ,alias_at_dessine_tortue,&__dessine_tortue,0,T_LOGO);

  gen _inverser(const gen & g,GIAC_CONTEXT){
    if ( g.type==_STRNG && g.subtype==-1) return  g;
    return sto(inv(eval(g,1,contextptr),contextptr),g,contextptr);
  }
  static const char _inverser_s []="inverser";
  static define_unary_function_eval2 (__inverser,&_inverser,_inverser_s,&printastifunction);
  define_unary_function_ptr5( at_inverser ,alias_at_inverser,&__inverser,_QUOTE_ARGUMENTS,T_LOGO);

#else // TURTLE
  std::vector<logo_turtle> & turtle_stack(){
    static std::vector<logo_turtle> * ans = 0;
    if (!ans){
      ans=new std::vector<logo_turtle>(1,(*turtleptr));
    }
    return *ans;
  }

#endif // TURTLE
  
#if defined(GIAC_GENERIC_CONSTANTS) || (defined(VISUALC) && !defined(RTOS_THREADX)) || defined(x86_64)
unary_function_ptr plot_sommets[]={*at_pnt,0};
  unary_function_ptr not_point_sommets[]={0};
  unary_function_ptr notexprint_plot_sommets[]={0};
  unary_function_ptr implicittex_plot_sommets[]={0};
  unary_function_ptr point_sommet_tab_op[]={0};
  unary_function_ptr nosplit_polygon_function[]={0}; 
  unary_function_ptr measure_functions[]={0};
  unary_function_ptr transformation_functions[]={0};

#else
  const size_t plot_sommets_alias[]={(size_t)&__pnt,0};
  const unary_function_ptr * const plot_sommets = (const unary_function_ptr *) plot_sommets_alias;

  const size_t not_point_sommets_alias[]={0};
  const unary_function_ptr * const not_point_sommets = (const unary_function_ptr *) plot_sommets_alias;

  const size_t notexprint_plot_sommets_alias[]={0};
  const unary_function_ptr * const notexprint_plot_sommets = (const unary_function_ptr *) notexprint_plot_sommets_alias;

  const size_t implicittex_plot_sommets_alias[]={0};
  const unary_function_ptr * const implicittex_plot_sommets = (const unary_function_ptr *) implicittex_plot_sommets_alias;

  const size_t point_sommet_tab_op_alias[]={0};
  const unary_function_ptr * const point_sommet_tab_op = (const unary_function_ptr *) point_sommet_tab_op_alias;

  const size_t nosplit_polygon_function_alias[]={0};
  const unary_function_ptr * const nosplit_polygon_function = (const unary_function_ptr *) nosplit_polygon_function_alias;

  const size_t measure_functions_alias[]={0};
  const unary_function_ptr * const measure_functions = (const unary_function_ptr *) measure_functions_alias;

  const size_t transformation_functions_alias[]={0};
  const unary_function_ptr * const transformation_functions = (const unary_function_ptr *) transformation_functions_alias;
#endif

#ifndef NO_NAMESPACE_GIAC
} // namespace giac
#endif // ndef NO_NAMESPACE_GIAC
