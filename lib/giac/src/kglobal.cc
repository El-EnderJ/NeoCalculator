/*
 * Modified for NumOS / NeoCalculator in 2026 for ESP32-S3 and host integration.
 * See ../NUMOS_CHANGES.md for provenance and the file-by-file history.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* -*- compile-command: "g++-3.4 -I.. -g -c global.cc  -DHAVE_CONFIG_H -DIN_GIAC" -*- */

#include "giacPCH.h"
#if defined(TICE)
#include <ti/getcsc.h>
#include "k_csdk.h"
int Console_Disp(int redraw_mode); // bit0=0 means minimal redraw (current line only)
#endif

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
#ifdef HAVE_SSTREAM
#include <sstream>
#else
#include <strstream>
#endif
#include "global.h"
// #include <time.h>
#if !defined BESTA_OS && !defined FXCG && !defined TICE
#include <signal.h>
#endif
#include <math.h>
#ifndef WINDOWS
#include <stdint.h>   // for uintptr_t
#endif
#ifdef HAVE_UNISTD_H
//#include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#include <string.h>
#include <stdexcept>
#include <algorithm>
#if !defined BESTA_OS && !defined FXCG && !defined TICE
#include <cerrno>
#endif
#include "gen.h"
#include "identificateur.h"
#include "symbolic.h"
#include "sym2poly.h"
#include "plot.h"
#include "rpn.h"
#include "prog.h"
#include "usual.h"
#include "tex.h"
#include "path.h"
#include "input_lexer.h"
#include "giacintl.h"
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef _HAS_LIMITS
#include <limits>
#endif
#ifndef BESTA_OS
#ifdef WIN32
#ifndef VISUALC
#if !defined(GNUWINCE) && !defined(__MINGW_H)
#include <sys/cygwin.h>
#include <windows.h>
#endif // ndef gnuwince
#endif // ndef visualc
#endif // win32
#endif // ndef bestaos

#if defined VISUALC && !defined BESTA_OS && !defined RTOS_THREADX && !defined FREERTOS
#include <Windows.h>
#endif 

#ifdef BESTA_OS
#include <stdlib.h>
#endif // besta_os

#include <stdio.h>
#include <stdarg.h>
#if defined(TICE)
#include <keypadc.h>
#endif

#if defined(FIR)
extern "C" int firvsprintf(char*,const char*, va_list);
#endif

#ifdef FXCG
extern "C" int KeyPressed( void );
extern int esc_flag;
extern "C" {
#include "fx/fxlib.h"
#include "syscalls.h"
}
#endif

#ifdef NSPIRE_NEWLIB
#include <os.h>
#endif

int my_sprintf(char * s, const char * format, ...){
    int z;
    va_list ap;
    va_start(ap,format);
#if defined(FIR) && !defined(FIR_LINUX)
    z = firvsprintf(s, format, ap);
#else
    z = vsprintf(s, format, ap);
#endif
    va_end(ap);
    return z;
}

#ifndef NO_NAMESPACE_GIAC
namespace giac {
#endif // ndef NO_NAMESPACE_GIAC
  void opaque_double_copy(void * source,void * target){
    *((double *) target) = * ((double *) source);
  }

  double opaque_double_val(const void * source){
    longlong r = * (longlong *)(source) ;
    (* (gen *) (&r)).type = 0;
    return * (double *)(&r); 
  }

  double min_proba_time=10; // in seconds


#ifdef TICE
  void set_abort(){
    //kb_ClearOnLatch();
    //kb_EnableOnLatch();
  }
  void clear_abort(){
    //kb_ClearOnLatch();
    //kb_DisableOnLatch();
  }
  void control_c(){
    //static int count=0;
    //if (ctrl_c) return;
    //++count;
    //if (kb_Data[1]) dbg_printf("control_c count=%i kbd=%i\n",count,kb_Data[1]);
    if ( //(count & 0x1f==0) &&
         kb_Data[6]==64){
      // dbg_printf("control_c interrupt\n",count);
      // kb_ClearOnLatch();
      ctrl_c=true; interrupted=true;
    }
  }
#endif


#ifdef NSPIRE_NEWLIB
  void usleep(int t){
  }
  
#ifdef TIMEOUT
#if !defined(EMCC) && !defined(EMCC2)
  double time(int ){
    return double(CLOCK())/1000000; // CLOCKS_PER_SEC;
  }
#endif
  time_t caseval_begin,caseval_current;
  double caseval_maxtime=15; // max 15 seconds
  int caseval_n=0,caseval_mod=0,caseval_unitialized=-123454321;
#if !defined POCKETCAS
  void control_c(){
#if defined NSPIRE || defined KHICAS
    if (
#if defined NSPIRE || defined NSPIRE_NEWLIB
	on_key_enabled && on_key_pressed()
#else
	back_key_pressed()
#endif
	){ 
      kbd_interrupted=true;
      ctrl_c=interrupted=true; 
    }
#else
    if (caseval_unitialized!=-123454321){
      caseval_unitialized=-123454321;
      caseval_mod=0;
      caseval_n=0;
      caseval_maxtime=15;
    }
    if (caseval_mod>0){ 
      ++caseval_n; 
      if (caseval_n >=caseval_mod){
	caseval_n=0; 
	caseval_current=time(0); 
#if defined(EMCC) || defined(EMCC2)
	if (difftime(caseval_current,caseval_begin)>caseval_maxtime)
#else
	if (caseval_current>caseval_maxtime+caseval_begin)
#endif
	  { 
	    CERR << "Timeout" << '\n'; ctrl_c=true; interrupted=true; 
	    caseval_begin=caseval_current;
	  } 
      } 
    }
#endif // NSPIRE
  }
#endif // POCKETCAS
#endif // TIMEOUT
#endif

#if defined VISUALC || defined BESTA_OS
#if !defined FREERTOS && !defined HAVE_LIBMPIR 
  int R_OK=4;
#endif
  int access(const char *path, int mode ){
    // return _access(path, mode );
    return 0;
  }
#ifdef RTOS_THREADX
extern "C" void Sleep(unsigned int miliSecond);
#endif

  void usleep(int t){
#ifdef RTOS_THREADX
    Sleep(t/1000);
#else
    Sleep(int(t/1000.+.5));
#endif
  }
#endif

#ifdef __APPLE__
  int PARENTHESIS_NWAIT=10;
#else
  int PARENTHESIS_NWAIT=100;
#endif

  // FIXME: threads allowed curently disabled
  // otherwise fermat_gcd_mod_2var crashes at puccini
  bool threads_allowed=true,mpzclass_allowed=true;
#ifdef HAVE_LIBPTHREAD
  pthread_mutex_t interactive_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

#ifdef NSPIRE_NEWLIB
  const context * context0=new context;
#else
  const context * context0=0;
#endif
  // Global variable when context is 0
  void (*fl_widget_delete_function)(void *) =0;
#ifndef NSPIRE
  ostream & (*fl_widget_archive_function)(ostream &,void *)=0;
  gen (*fl_widget_unarchive_function)(istream &)=0;
#endif
  gen (*fl_widget_updatepict_function)(const gen & g)=0;
  std::string (*fl_widget_texprint_function)(void * ptr)=0;

  const char * _last_evaled_function_name_=0;
  const char * & last_evaled_function_name(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_last_evaled_function_name_;
    else
      return _last_evaled_function_name_;
  }

  const char * _currently_scanned=0;

  const char * & currently_scanned(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_currently_scanned_;
    else
      return _currently_scanned;
  }

  const gen * _last_evaled_argptr_=0;
  const gen * & last_evaled_argptr(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_last_evaled_argptr_;
    else
      return _last_evaled_argptr_;
  }

  static int _language_=0; 
  int & language(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_language_;
    else
      return _language_;
  }
  void language(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_language_=b;
#ifndef EMCC
    else
#endif
      _language_=b;
  }

  static int _max_sum_sqrt_=3; 
  int & max_sum_sqrt(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_max_sum_sqrt_;
    else
      return _max_sum_sqrt_;
  }
  void max_sum_sqrt(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_max_sum_sqrt_=b;
    else
      _max_sum_sqrt_=b;
  }

#ifdef GIAC_HAS_STO_38 // Prime sum(x^2,x,0,100000) crash on hardware
  static int _max_sum_add_=10000; 
#else
  static int _max_sum_add_=100000; 
#endif
  int & max_sum_add(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_max_sum_add_;
    else
      return _max_sum_add_;
  }

  static void * _evaled_table_=0;
  void * & evaled_table(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_evaled_table_;
    else
      return _evaled_table_;
  }

  static void * _extra_ptr_=0;
  void * & extra_ptr(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_extra_ptr_;
    else
      return _extra_ptr_;
  }

  static int _spread_Row_=0;
  int & spread_Row(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_spread_Row_;
    else
      return _spread_Row_;
  }
  void spread_Row(int c,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr)
      contextptr->globalptr->_spread_Row_=c;
    else
      _spread_Row_=c;
  }

  static int _spread_Col_=0;
  int & spread_Col(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_spread_Col_;
    else
      return _spread_Col_;
  }
  void spread_Col(int c,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr)
      contextptr->globalptr->_spread_Col_=c;
    else
      _spread_Col_=c;
  }

  static int _printcell_current_row_=0;
  int & printcell_current_row(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_printcell_current_row_;
    else
      return _printcell_current_row_;
  }
  void printcell_current_row(int c,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr)
      contextptr->globalptr->_printcell_current_row_=c;
    else
      _printcell_current_row_=c;
  }

  static int _printcell_current_col_=0;
  int & printcell_current_col(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_printcell_current_col_;
    else
      return _printcell_current_col_;
  }
  void printcell_current_col(int c,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr)
      contextptr->globalptr->_printcell_current_col_=c;
    else
      _printcell_current_col_=c;
  }

  static double _total_time_=0.0;
  double & total_time(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_total_time_;
    else
      return _total_time_;
  }

#if 1
#ifdef TICE
  const double init_epsilon=1e-5;
#else  
  const double init_epsilon=1e-12;
#endif
  static double _epsilon_=init_epsilon;
#else
#ifdef __SGI_CPP_LIMITS
  static double _epsilon_=100*numeric_limits<double>::epsilon();
#else
  static double _epsilon_=1e-12;
#endif
#endif
  double & epsilon(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_epsilon_;
    else
      return _epsilon_;
  }
  void epsilon(double c,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr)
      contextptr->globalptr->_epsilon_=c;
    else
      _epsilon_=c;
  }

  static double _proba_epsilon_=1e-15;
  double & proba_epsilon(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_proba_epsilon_;
    else
      return _proba_epsilon_;
  }

  static bool _expand_re_im_=true; 
  bool & expand_re_im(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_expand_re_im_;
    else
      return _expand_re_im_;
  }
  void expand_re_im(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_expand_re_im_=b;
    else
      _expand_re_im_=b;
  }

  static int _scientific_format_=0; 
  int & scientific_format(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_scientific_format_;
    else
      return _scientific_format_;
  }
  void scientific_format(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_scientific_format_=b;
    else
      _scientific_format_=b;
  }

  static int _decimal_digits_=12; 

  int & decimal_digits(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_decimal_digits_;
    else
      return _decimal_digits_;
  }
  void decimal_digits(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_decimal_digits_=b;
    else
      _decimal_digits_=b;
  }

  static int _minchar_for_quote_as_string_=1; 

  int & minchar_for_quote_as_string(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_minchar_for_quote_as_string_;
    else
      return _minchar_for_quote_as_string_;
  }
  void minchar_for_quote_as_string(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_minchar_for_quote_as_string_=b;
    else
      _minchar_for_quote_as_string_=b;
  }

  static int _xcas_mode_=0; 
  int & xcas_mode(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_xcas_mode_;
    else
      return _xcas_mode_;
  }
  void xcas_mode(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_xcas_mode_=b;
    else
      _xcas_mode_=b;
  }


  static int _integer_format_=0; 
  int & integer_format(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_integer_format_;
    else
      return _integer_format_;
  }
  void integer_format(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_integer_format_=b;
    else
      _integer_format_=b;
  }
  static int _latex_format_=0; 
  int & latex_format(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_latex_format_;
    else
      return _latex_format_;
  }
#ifdef BCD
  static u32 _bcd_decpoint_='.'|('E'<<16)|(' '<<24); 
  u32 & bcd_decpoint(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_bcd_decpoint_;
    else
      return _bcd_decpoint_;
  }

  static u32 _bcd_mantissa_=12+(15<<8); 
  u32 & bcd_mantissa(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_bcd_mantissa_;
    else
      return _bcd_mantissa_;
  }

  static u32 _bcd_flags_=0; 
  u32 & bcd_flags(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_bcd_flags_;
    else
      return _bcd_flags_;
  }

  static bool _bcd_printdouble_=false;
  bool & bcd_printdouble(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_bcd_printdouble_;
    else
      return _bcd_printdouble_;
  }

#endif

  static bool _integer_mode_=true;
  bool & integer_mode(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_integer_mode_;
    else
      return _integer_mode_;
  }

  void integer_mode(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_integer_mode_=b;
    else
      _integer_mode_=b;
  }

  bool python_color=false;
  static int _python_compat_=false;
  int & python_compat(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_python_compat_;
    else
      return _python_compat_;
  }

  void python_compat(int b,GIAC_CONTEXT){
    python_color=b; //cout << "python_color " << b << "\n";
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_python_compat_=b;
    else
      _python_compat_=b;
  }

  static bool _complex_mode_=false; 
  bool & complex_mode(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_complex_mode_;
    else
      return _complex_mode_;
  }

  void complex_mode(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_complex_mode_=b;
    else
      _complex_mode_=b;
  }

  static bool _escape_real_=true; 
  bool & escape_real(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_escape_real_;
    else
      return _escape_real_;
  }

  void escape_real(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_escape_real_=b;
    else
      _escape_real_=b;
  }

  static bool _do_lnabs_=true;
  bool & do_lnabs(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_do_lnabs_;
    else
      return _do_lnabs_;
  }

  void do_lnabs(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_do_lnabs_=b;
    else
      _do_lnabs_=b;
  }

  static bool _eval_abs_=true;
  bool & eval_abs(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_eval_abs_;
    else
      return _eval_abs_;
  }

  void eval_abs(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_eval_abs_=b;
    else
      _eval_abs_=b;
  }

  static bool _eval_equaltosto_=true;
  bool & eval_equaltosto(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_eval_equaltosto_;
    else
      return _eval_equaltosto_;
  }

  void eval_equaltosto(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_eval_equaltosto_=b;
    else
      _eval_equaltosto_=b;
  }

  static bool _all_trig_sol_=false; 
  bool & all_trig_sol(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_all_trig_sol_;
    else
      return _all_trig_sol_;
  }

  void all_trig_sol(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_all_trig_sol_=b;
    else
      _all_trig_sol_=b;
  }

  static bool _try_parse_i_=true; 
  bool & try_parse_i(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_try_parse_i_;
    else
      return _try_parse_i_;
  }

  void try_parse_i(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_try_parse_i_=b;
    else
      _try_parse_i_=b;
  }

  static bool _specialtexprint_double_=false; 
  bool & specialtexprint_double(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_specialtexprint_double_;
    else
      return _specialtexprint_double_;
  }

  void specialtexprint_double(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_specialtexprint_double_=b;
    else
      _specialtexprint_double_=b;
  }

  static bool _atan_tan_no_floor_=false; 
  bool & atan_tan_no_floor(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_atan_tan_no_floor_;
    else
      return _atan_tan_no_floor_;
  }

  void atan_tan_no_floor(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_atan_tan_no_floor_=b;
    else
      _atan_tan_no_floor_=b;
  }

  static bool _keep_acosh_asinh_=false; 
  bool & keep_acosh_asinh(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_keep_acosh_asinh_;
    else
      return _keep_acosh_asinh_;
  }

  void keep_acosh_asinh(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_keep_acosh_asinh_=b;
    else
      _keep_acosh_asinh_=b;
  }

  static bool _keep_algext_=false; 
  bool & keep_algext(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_keep_algext_;
    else
      return _keep_algext_;
  }

  void keep_algext(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_keep_algext_=b;
    else
      _keep_algext_=b;
  }

  static bool _lexer_close_parenthesis_=true; 
  bool & lexer_close_parenthesis(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_lexer_close_parenthesis_;
    else
      return _lexer_close_parenthesis_;
  }

  void lexer_close_parenthesis(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_lexer_close_parenthesis_=b;
    else
      _lexer_close_parenthesis_=b;
  }

  static bool _rpn_mode_=false; 
  bool & rpn_mode(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_rpn_mode_;
    else
      return _rpn_mode_;
  }

  void rpn_mode(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_rpn_mode_=b;
    else
      _rpn_mode_=b;
  }

  static bool _ntl_on_=true; 
  bool & ntl_on(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_ntl_on_;
    else
      return _ntl_on_;
  }

  void ntl_on(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_ntl_on_=b;
    else
      _ntl_on_=b;
  }

  static bool _complex_variables_=false; 
  bool & complex_variables(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_complex_variables_;
    else
      return _complex_variables_;
  }

  void complex_variables(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_complex_variables_=b;
    else
      _complex_variables_=b;
  }

  static bool _increasing_power_=false;
  bool & increasing_power(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_increasing_power_;
    else
      return _increasing_power_;
  }

  void increasing_power(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_increasing_power_=b;
    else
      _increasing_power_=b;
  }

  static vecteur & _history_in_(){
    static vecteur * ans = 0;
    if (ans)
      ans=new vecteur;
    return *ans;
  }
  vecteur & history_in(GIAC_CONTEXT){
    if (contextptr)
      return *contextptr->history_in_ptr;
    else
      return _history_in_();
  }

  static vecteur & _history_out_(){
    static vecteur * ans = 0;
    if (!ans)
      ans=new vecteur;
    return *ans;
  }
  vecteur & history_out(GIAC_CONTEXT){
    if (contextptr)
      return *contextptr->history_out_ptr;
    else
      return _history_out_();
  }

  static vecteur & _history_plot_(){
    static vecteur * ans = 0;
    if (!ans)
      ans=new vecteur;
    return *ans;
  }
  vecteur & history_plot(GIAC_CONTEXT){
    if (contextptr)
      return *contextptr->history_plot_ptr;
    else
      return _history_plot_();
  }

  static bool _approx_mode_=false;
  bool & approx_mode(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_approx_mode_;
    else
      return _approx_mode_;
  }

  void approx_mode(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_approx_mode_=b;
    else
      _approx_mode_=b;
  }

  static char _series_variable_name_='h';
  char & series_variable_name(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_series_variable_name_;
    else
      return _series_variable_name_;
  }

  void series_variable_name(char b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_series_variable_name_=b;
    else
      _series_variable_name_=b;
  }

  static unsigned short _series_default_order_=5;
  unsigned short & series_default_order(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_series_default_order_;
    else
      return _series_default_order_;
  }

  void series_default_order(unsigned short b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_series_default_order_=b;
    else
      _series_default_order_=b;
  }

  static int _angle_mode_=0;
  bool angle_radian(GIAC_CONTEXT)
  {
    if(contextptr && contextptr->globalptr)
      return contextptr->globalptr->_angle_mode_ == 0;
    else
      return _angle_mode_ == 0;
  }

  void angle_radian(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_angle_mode_=(b?0:1);
    else
      _angle_mode_=(b?0:1);
  }

  bool angle_degree(GIAC_CONTEXT)
  {
    if(contextptr && contextptr->globalptr)
      return contextptr->globalptr->_angle_mode_ == 1;
    else
      return _angle_mode_ == 1;
  }

  int get_mode_set_radian(GIAC_CONTEXT)
  {
    int mode;
    if(contextptr && contextptr->globalptr)
    {
      mode = contextptr->globalptr->_angle_mode_;
      contextptr->globalptr->_angle_mode_ = 0;
    }
    else
    {
      mode = _angle_mode_;
      _angle_mode_ = 0;
    }
    return mode;
  }

  void angle_mode(int b, GIAC_CONTEXT)
  {
    if(contextptr && contextptr->globalptr)
      contextptr->globalptr->_angle_mode_ = b;
    else
      _angle_mode_ = b;
  }

  int & angle_mode(GIAC_CONTEXT)
  {
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_angle_mode_;
    else
      return _angle_mode_;
  }

  static bool _variables_are_files_=false;
  bool & variables_are_files(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_variables_are_files_;
    else
      return _variables_are_files_;
  }

  void variables_are_files(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_variables_are_files_=b;
    else
      _variables_are_files_=b;
  }

  static int _bounded_function_no_=0;
  int & bounded_function_no(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_bounded_function_no_;
    else
      return _bounded_function_no_;
  }

  void bounded_function_no(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_bounded_function_no_=b;
    else
      _bounded_function_no_=b;
  }

  static int _series_flags_=0x3;
  int & series_flags(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_series_flags_;
    else
      return _series_flags_;
  }

  void series_flags(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_series_flags_=b;
    else
      _series_flags_=b;
  }

  static int _step_infolevel_=0;
  int & step_infolevel(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_step_infolevel_;
    else
      return _step_infolevel_;
  }

  void step_infolevel(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_step_infolevel_=b;
    else
      _step_infolevel_=b;
  }

  static bool _local_eval_=true;
  bool & local_eval(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_local_eval_;
    else
      return _local_eval_;
  }

  void local_eval(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_local_eval_=b;
    else
      _local_eval_=b;
  }

  static bool _withsqrt_=true;
  bool & withsqrt(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_withsqrt_;
    else
      return _withsqrt_;
  }

  void withsqrt(bool b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_withsqrt_=b;
    else
      _withsqrt_=b;
  }

#ifdef NSPIRE_NEWLIB
  stdostream Cout;

  static stdostream * _logptr_=&Cout;
#else
  static stdostream * _logptr_=&cout;
#endif
  
  stdostream * logptr(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_logptr_;
    else
      return _logptr_;
  }

  void logptr(stdostream * b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_logptr_=b;
    else
      _logptr_=b;
  }

  thread_param::thread_param(): _kill_thread(false), thread_eval_status(-1), v(6)
#ifdef HAVE_LIBPTHREAD
#ifdef __MINGW_H
			      ,eval_thread(),stackaddr(0)
#else
			      ,eval_thread(0),stackaddr(0)
#endif
#endif
  { 
  }

  thread_param * & context0_thread_param_ptr(){
    static thread_param * ans=0;
    if (!ans)
      ans=new thread_param();
    return ans;
  }

#if 0
  static thread_param & context0_thread_param(){
    return *context0_thread_param_ptr();
  }
#endif

  thread_param * thread_param_ptr(const context * contextptr){
    return (contextptr && contextptr->globalptr)?contextptr->globalptr->_thread_param_ptr:context0_thread_param_ptr();
  }

  bool kill_thread(GIAC_CONTEXT){
    thread_param * ptr= (contextptr && contextptr->globalptr )?contextptr->globalptr->_thread_param_ptr:0;
    return ptr?ptr->_kill_thread:context0_thread_param_ptr()->_kill_thread;
  }

  void kill_thread(bool b,GIAC_CONTEXT){
    thread_param * ptr= (contextptr && contextptr->globalptr )?contextptr->globalptr->_thread_param_ptr:0;
    if (!ptr)
      ptr=context0_thread_param_ptr();
    ptr->_kill_thread=b;
  }


#ifdef HAVE_LIBPTHREAD
  pthread_mutex_t _mutexptr = PTHREAD_MUTEX_INITIALIZER,_mutex_eval_status= PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t * mutexptr(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr)
      return contextptr->globalptr->_mutexptr;
    return &_mutexptr;
  }

  bool is_context_busy(GIAC_CONTEXT){
    int concurrent=pthread_mutex_trylock(mutexptr(contextptr));
    bool res=concurrent==EBUSY;
    if (!res)
      pthread_mutex_unlock(mutexptr(contextptr));
    return res;
  }

  int thread_eval_status(GIAC_CONTEXT){
    int res;
    if (contextptr && contextptr->globalptr){
      pthread_mutex_lock(contextptr->globalptr->_mutex_eval_status_ptr);
      res=contextptr->globalptr->_thread_param_ptr->thread_eval_status;
      pthread_mutex_unlock(contextptr->globalptr->_mutex_eval_status_ptr);
    }
    else {
      pthread_mutex_lock(&_mutex_eval_status);
      res=context0_thread_param_ptr()->thread_eval_status;
      pthread_mutex_unlock(&_mutex_eval_status);
    }
    return res;
  }

  void thread_eval_status(int val,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr){
      pthread_mutex_lock(contextptr->globalptr->_mutex_eval_status_ptr);
      contextptr->globalptr->_thread_param_ptr->thread_eval_status=val;
      pthread_mutex_unlock(contextptr->globalptr->_mutex_eval_status_ptr);
    }
    else {
      pthread_mutex_lock(&_mutex_eval_status);
      context0_thread_param_ptr()->thread_eval_status=val;
      pthread_mutex_unlock(&_mutex_eval_status);
    }
  }

#else
  bool is_context_busy(GIAC_CONTEXT){
    return false;
  }

  int thread_eval_status(GIAC_CONTEXT){
    return -1;
  }
  
  void thread_eval_status(int val,GIAC_CONTEXT){
  }

#endif

  static int _eval_level=DEFAULT_EVAL_LEVEL;
  int & eval_level(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_eval_level;
    else
      return _eval_level;
  }

  void eval_level(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_eval_level=b;
    else
      _eval_level=b;
  }

#if defined FXCG || defined TICE// defined(GIAC_HAS_STO_38) || defined(ConnectivityKit)
  static unsigned int _rand_seed=123457;
#else
  static tinymt32_t _rand_seed;
#endif

#if defined FXCG || defined TICE // defined(GIAC_HAS_STO_38) || defined(ConnectivityKit)
  unsigned int & rand_seed(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_rand_seed;
    else
      return _rand_seed;
  }
#else
  tinymt32_t * rand_seed(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return &contextptr->globalptr->_rand_seed;
    else
      return &_rand_seed;
  }
#endif

  void rand_seed(unsigned int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_rand_seed=b;
    else
      _rand_seed=b;
  }

  int std_rand(){
#if 0 // def NSPIRE
    static unsigned long r = 0;
    r = (1664525ULL*r+1013904223UL)%(1UL<<31);
    dbg_printf("rand %f\n",double(r));
    return r>>8;
#else
    unsigned r=std::rand();
    //dbg_printf("rand %i\n",r);
    return r;
#endif
  }

#ifdef TICE
  int giac_rand(GIAC_CONTEXT){
    unsigned r=std::rand();
    //dbg_printf("rand %i\n",r);
    return r;
  }
#else
  int giac_rand(GIAC_CONTEXT){
#if defined FXCG // defined(GIAC_HAS_STO_38) || defined(ConnectivityKit)
    unsigned int & r = rand_seed(contextptr);
    // r = (2147483629*ulonglong(r)+ 2147483587)% 2147483647;
    r = unsigned ((1664525*ulonglong(r)+1013904223)%(ulonglong(1)<<31));
    return r;
#else
    for (;;){
      unsigned r=tinymt32_generate_uint32(rand_seed(contextptr)) >> 1;
      if (!(r>>INT_MAXSHIFT))
	return r;
    }
#endif // tinymt32
  }
#endif

  static int _prog_eval_level_val=1;
  int & prog_eval_level_val(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_prog_eval_level_val;
    else
      return _prog_eval_level_val;
  }

  void prog_eval_level_val(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_prog_eval_level_val=b;
    else
      _prog_eval_level_val=b;
  }

  void cleanup_context(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr ){
      contextptr->globalptr->_eval_level=DEFAULT_EVAL_LEVEL;
    }
    eval_level(contextptr)=DEFAULT_EVAL_LEVEL;
    if (!contextptr)
      protection_level=0;
    local_eval(true,contextptr);
  }


  static parser_lexer & _pl(){
    static parser_lexer * ans = 0;
    if (!ans)
      ans=new parser_lexer();
    ans->_i_sqrt_minus1_=1;
    return * ans;
  }
  int & lexer_column_number(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_pl._lexer_column_number_;
    else
      return _pl()._lexer_column_number_;
  }
  int & lexer_line_number(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_pl._lexer_line_number_;
    else
      return _pl()._lexer_line_number_;
  }
  void lexer_line_number(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_pl._lexer_line_number_=b;
    else
      _pl()._lexer_line_number_=b;
  }
  void increment_lexer_line_number(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      ++contextptr->globalptr->_pl._lexer_line_number_;
    else
      ++_pl()._lexer_line_number_;
  }

  int & index_status(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_pl._index_status_;
    else
      return _pl()._index_status_;
  }
  void index_status(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_pl._index_status_=b;
    else
      _pl()._index_status_=b;
  }

  int & i_sqrt_minus1(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_pl._i_sqrt_minus1_;
    else
      return _pl()._i_sqrt_minus1_;
  }
  void i_sqrt_minus1(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_pl._i_sqrt_minus1_=b;
    else
      _pl()._i_sqrt_minus1_=b;
  }

  int & opened_quote(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_pl._opened_quote_;
    else
      return _pl()._opened_quote_;
  }
  void opened_quote(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_pl._opened_quote_=b;
    else
      _pl()._opened_quote_=b;
  }

  int & in_rpn(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_pl._in_rpn_;
    else
      return _pl()._in_rpn_;
  }
  void in_rpn(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_pl._in_rpn_=b;
    else
      _pl()._in_rpn_=b;
  }

  int & spread_formula(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_pl._spread_formula_;
    else
      return _pl()._spread_formula_;
  }
  void spread_formula(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_pl._spread_formula_=b;
    else
      _pl()._spread_formula_=b;
  }

  int & initialisation_done(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_pl._initialisation_done_;
    else
      return _pl()._initialisation_done_;
  }
  void initialisation_done(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_pl._initialisation_done_=b;
    else
      _pl()._initialisation_done_=b;
  }

  static int _calc_mode_=0; 
  int & calc_mode(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_calc_mode_;
    else
      return _calc_mode_;
  }
  int abs_calc_mode(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return absint(contextptr->globalptr->_calc_mode_);
    else
      return absint(_calc_mode_);
  }
  void calc_mode(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_calc_mode_=b;
    else
      _calc_mode_=b;
  }

  int array_start(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr){
      bool hp38=absint(contextptr->globalptr->_calc_mode_)==38;
      return (!contextptr->globalptr->_python_compat_ && (contextptr->globalptr->_xcas_mode_ || hp38))?1:0;
    }
    return (!_python_compat_ && (_xcas_mode_ || absint(_calc_mode_)==38))?1:0;
  }


  static std::string & _autosimplify_(){
    static string * ans = 0;
    if (!ans)
      ans=new string("regroup");
    return *ans;
  }
  std::string autosimplify(GIAC_CONTEXT){
    std::string res;
    if (contextptr && contextptr->globalptr )
      res=contextptr->globalptr->_autosimplify_;
    else
      res=_autosimplify_();
    return res;
  }
  std::string autosimplify(const std::string & s,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_autosimplify_=s;
    else
      _autosimplify_()=s;
    return s;
  }

  static std::string & _lastprog_name_(){
    static string * ans = 0;
    if (!ans)
      ans=new string("lastprog");
    return *ans;
  }
  std::string lastprog_name(GIAC_CONTEXT){
    std::string res;
    if (contextptr && contextptr->globalptr )
      res=contextptr->globalptr->_lastprog_name_;
    else
      res=_lastprog_name_();
    return res;
  }
  std::string lastprog_name(const std::string & s,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_lastprog_name_=s;
    else
      _lastprog_name_()=s;
    return s;
  }

  static std::string & _format_double_(){
    static string * ans = 0;
    if (!ans)
      ans=new string("");
    return * ans;
  }
  std::string & format_double(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_format_double_;
    else
      return _format_double_();
  }

  std::string comment_s(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_pl._comment_s_;
    else
      return _pl()._comment_s_;
  }
  void comment_s(const std::string & b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_pl._comment_s_=b;
    else
      _pl()._comment_s_=b;
  }

  void increment_comment_s(const std::string & b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_pl._comment_s_ += b;
    else
      _pl()._comment_s_ += b;
  }

  void increment_comment_s(char b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_pl._comment_s_ += b;
    else
      _pl()._comment_s_ += b;
  }

  std::string parser_filename(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_pl._parser_filename_;
    else
      return _pl()._parser_filename_;
  }
  void parser_filename(const std::string & b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_pl._parser_filename_=b;
    else
      _pl()._parser_filename_=b;
  }

  std::string parser_error(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_pl._parser_error_;
    else
      return _pl()._parser_error_;
  }
  void parser_error(const std::string & b,GIAC_CONTEXT){
#ifndef GIAC_HAS_STO_38
    if (!first_error_line(contextptr))
      alert(b,contextptr);
    else
      *logptr(contextptr) << b << "\n";
#endif
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_pl._parser_error_=b;
    else
      _pl()._parser_error_=b;
  }

  std::string error_token_name(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_pl._error_token_name_;
    else
      return _pl()._error_token_name_;
  }
  void error_token_name(const std::string & b0,GIAC_CONTEXT){
    string b(b0);
    if (b0.size()==2 && b0[0]==-61 && b0[1]==-65)
      b="end of input";
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_pl._error_token_name_=b;
    else
      _pl()._error_token_name_=b;
  }

  int & first_error_line(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return contextptr->globalptr->_pl._first_error_line_;
    else
      return _pl()._first_error_line_;
  }
  void first_error_line(int b,GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      contextptr->globalptr->_pl._first_error_line_=b;
    else
      _pl()._first_error_line_=b;
  }

  static gen & _parsed_gen_(){
    static gen * ans = 0;
    if (!ans)
      ans=new gen;
    return * ans;
  }
  gen parsed_gen(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr )
      return *contextptr->globalptr->_parsed_genptr_;
    else
      return _parsed_gen_();
  }
  void parsed_gen(const gen & b,GIAC_CONTEXT){
    // dbg_printf("parsed_gen\n");
    //dbg_printf("%s\n",b.print(contextptr).c_str());
    if (contextptr && contextptr->globalptr )
      *contextptr->globalptr->_parsed_genptr_=b;
    else
      _parsed_gen_()=b;
  }


  // Other global variables
#ifdef NSPIRE
  bool secure_run=false;
#else
  bool secure_run=true;
#endif
  bool center_history=false;
  bool in_texmacs=false;
  bool block_signal=false;
  bool CAN_USE_LAPACK = true;
  bool simplify_sincosexp_pi=true;
  int history_begin_level=0; 
  // variable used to avoid copying the whole history between processes 
#ifdef TICE // Temporary
  const int debug_infolevel=0;
#else
  int debug_infolevel=0;
#endif
  int printprog=0;
#if defined __APPLE__ || defined VISUALC || defined __MINGW_H || defined BESTA_OS || defined NSPIRE || defined FXCG || defined NSPIRE_NEWLIB || defined TICE 
  int threads=1;
#else
  int threads=sysconf (_SC_NPROCESSORS_ONLN);
#endif
  unsigned short int GIAC_PADIC=50;
  const char cas_suffixe[]=".cas";
#if defined RTOS_THREADX || defined BESTA_OS
#else
  int CALL_LAPACK=1111;
  int LIST_SIZE_LIMIT = 800 ;
#ifdef USE_GMP_REPLACEMENTS
  int FACTORIAL_SIZE_LIMIT = 1000 ;
#else
  int FACTORIAL_SIZE_LIMIT = 10000000 ;
#endif
  int GAMMA_LIMIT = 100 ;
  int NEWTON_DEFAULT_ITERATION=60;
  int TEST_PROBAB_PRIME=25;
  int GCDHEU_MAXTRY=5;
  int GCDHEU_DEGREE=100;
  int DEFAULT_EVAL_LEVEL=25;
  int MODFACTOR_PRIMES =5;
  int NTL_MODGCD=50;
  int HENSEL_QUADRATIC_POWER=25;
  int KARAMUL_SIZE=13;
  int INT_KARAMUL_SIZE=300;
  int FFTMUL_SIZE=100; 
  int FFTMUL_INT_MAXBITS=1024;
#ifdef GIAC_GGB
  int MAX_ALG_EXT_ORDER_SIZE = 3;
#else
  int MAX_ALG_EXT_ORDER_SIZE = 6;
#endif
#if defined EMCC || defined NO_TEMPLATE_MULTGCD
  int MAX_COMMON_ALG_EXT_ORDER_SIZE = 16;
#else
  int MAX_COMMON_ALG_EXT_ORDER_SIZE = 64;
#endif
  int TRY_FU_UPRIME=5;
  int SOLVER_MAX_ITERATE=25;
  int MAX_PRINTABLE_ZINT=1000000;
  int MAX_RECURSION_LEVEL=100;
  int GBASIS_DETERMINISTIC=50;
  int GBASISF4_MAX_TOTALDEG=16384;
  int GBASISF4_MAXITER=1024;
  // int GBASISF4_BUCHBERGER=5;
  const int BUFFER_SIZE=16384;
#endif
  volatile bool ctrl_c=false,interrupted=false,kbd_interrupted=false;
#if defined GIAC_HAS_STO_38 || defined TICE
  double powlog2float=1e4;
  int MPZ_MAXLOG2=8600; // max 2^8600 about 1K
#else
  double powlog2float=1e8;
  int MPZ_MAXLOG2=80000000; // 100 millions bits
#endif
#ifdef HAVE_LIBNTL
  int PROOT_FACTOR_MAXDEG=300;
#else
  int PROOT_FACTOR_MAXDEG=30;
#endif
  int ABS_NBITS_EVALF=1000;


#if defined HAVE_SIGNAL_H && !defined HAVE_NO_SIGNAL_H
  pid_t parent_id=getpid();
#else
  pid_t parent_id=0;
#endif
  pid_t child_id=0; // child process (to replace by a vector of childs?)

  void ctrl_c_signal_handler(int signum){
    ctrl_c=true;
#if !defined NSPIRE_NEWLIB && !defined WIN32 && !defined(_WIN32) && !defined BESTA_OS && !defined NSPIRE && !defined FXCG && !defined TICE
    if (child_id)
      kill(child_id,SIGINT);
#endif
#if defined HAVE_SIGNAL_H && !defined HAVE_NO_SIGNAL_H
    cerr << "Ctrl-C pressed (pid " << getpid() << ")" << "\n";
#endif
  }
#if !defined NSPIRE && !defined FXCG && !defined TICE
  gen catch_err(const std::runtime_error & error){
    cerr << error.what() << "\n";
    debug_ptr(0)->sst_at_stack.clear();
    debug_ptr(0)->current_instruction_stack.clear();
    debug_ptr(0)->args_stack.clear();
    protection_level=0;
    debug_ptr(0)->debug_mode=false;
    return string2gen(string(error.what()),false);
  }
#endif


  string home_directory(){
    string s("/");
    return s;
  }

  
  void read_config(const string & name,GIAC_CONTEXT,bool verbose){
    return;
  }

  // Unix: configuration is read from xcas.rc in the giac_aide_location dir
  // then from the user ~/.xcasrc
  // Win: configuration from $XCAS_ROOT/xcas.rc then from home_dir()+xcasrc
  // or if not available from current dir xcasrc
  void protected_read_config(GIAC_CONTEXT,bool verbose){
    return;
  }

  string giac_aide_dir(){
    return "";
  }

  std::string absolute_path(const std::string & orig_file){
    return orig_file;
  }

  bool is_file_available(const char * ch){
    if (!ch)
      return false;
    return true;
  }

  bool file_not_available(const char * ch){
    return !is_file_available(ch);
  }

  static void add_slash(string & path){
    if (!path.empty() && path[path.size()-1]!='/')
      path += '/';
  }

  const char * getenv(const char *){
    return "";
  }


  bool check_file_path(const string & s){
    int ss=int(s.size()),i;
    for (i=0;i<ss;++i){
      if (s[i]==' ')
	break;
    }
    string name=s.substr(0,i);
#if defined FXCG || defined TICE
    const char ch[]="/";
#else
    const char * ch=getenv("PATH");
#endif
    if (!ch || name[0]=='/')
      return is_file_available(name.c_str());
    string path;
    int l=int(strlen(ch));
    for (i=0;i<l;++i){
      if (ch[i]==':'){
	if (!path.empty()){
	  add_slash(path);
	  if (is_file_available((path+name).c_str()))
	    return true;
	}
	path="";
      }
      else
	path += ch[i];
    }
    add_slash(path);
    return path.empty()?false:is_file_available((path+name).c_str());
  }

  string browser_command(const string & orig_file){
    return "";
  }

  bool system_browser_command(const string & file){
    return false;
  }

  int equalposcomp(const vector<int> v,int i){
    vector<int>::const_iterator it=v.begin(),itend=v.end();
    for (;it!=itend;++it)
      if (*it==i)
	return int(it-v.begin())+1;
    return 0;
  }

  int equalposcomp(const vector<short int> v,int i){
    vector<short int>::const_iterator it=v.begin(),itend=v.end();
    for (;it!=itend;++it)
      if (*it==i)
	return int(it-v.begin())+1;
    return 0;
  }

  int equalposcomp(int tab[],int f){
    for (int i=1;*tab!=0;++tab,++i){
      if (*tab==f)
	return i;
    }
    return 0;
  }

  std::string find_lang_prefix(int i){
    return "";
  }

  std::string find_doc_prefix(int i){
    return "";
  }

  void update_completions(){
  }

  void add_language(int i,GIAC_CONTEXT){
    return;
  }

  void remove_language(int i,GIAC_CONTEXT){
    return;
  }

  int string2lang(const string & s){
    return 2;
  }

  std::string set_language(int i,GIAC_CONTEXT){
    return find_doc_prefix(i);
  }

  std::string read_env(GIAC_CONTEXT,bool verbose){
    return find_doc_prefix(0);
  }

  string cas_setup_string(GIAC_CONTEXT){
    string s("cas_setup(");
    s += print_VECT(cas_setup(contextptr),_SEQ__VECT,contextptr);
    s += "),";
    s += "xcas_mode(";
    s += print_INT_(xcas_mode(contextptr)+(python_compat(contextptr)?256:0));
    s += ")";
    return s;
  }

  string geo_setup_string(){
    return "";
  }

  string add_extension(const string & s,const string & ext,const string & def){
    if (s.empty())
      return def+"."+ext;
    int i=int(s.size());
    for (--i;i>0;--i){
      if (s[i]=='.')
	break;
    }
    if (i<=0)
      return s+"."+ext;
    return s.substr(0,i)+"."+ext;
  }

  vector<context *> & context_list(){
    static vector<context *> * ans=0;
    if (!ans) ans=new vector<context *>(1,(context *) 0);
    return *ans;
  }
  context::context() { 
    // CERR << "new context " << this << "\n";
    parent=0;
    tabptr=new sym_tab; 
    globalcontextptr=this; previous=0; globalptr=new global; 
    quoted_global_vars=new vecteur;
    rootofs=new vecteur;
    history_in_ptr=new vecteur;
    history_out_ptr=new vecteur;
    history_plot_ptr=new vecteur;
    context_list().push_back(this);
  }


  context::context(const context & c) { 
    *this = c;
  }

  context * context::clone() const{
    context * ptr = new context;
    *ptr->globalptr = *globalptr;
    return ptr;
  }

  void init_context(context * ptr){
    if (!ptr){
      CERR << "init_context on null context" << "\n";
      return;
    }
     ptr->globalptr->_xcas_mode_=_xcas_mode_;
     ptr->globalptr->_calc_mode_=_calc_mode_;
     ptr->globalptr->_decimal_digits_=_decimal_digits_;
     ptr->globalptr->_minchar_for_quote_as_string_=_minchar_for_quote_as_string_;
     ptr->globalptr->_scientific_format_=_scientific_format_;
     ptr->globalptr->_integer_format_=_integer_format_;
     ptr->globalptr->_integer_mode_=_integer_mode_;
     ptr->globalptr->_latex_format_=_latex_format_;
     ptr->globalptr->_expand_re_im_=_expand_re_im_;
     ptr->globalptr->_do_lnabs_=_do_lnabs_;
     ptr->globalptr->_eval_abs_=_eval_abs_;
     ptr->globalptr->_eval_equaltosto_=_eval_equaltosto_;
     ptr->globalptr->_complex_mode_=_complex_mode_;
     ptr->globalptr->_escape_real_=_escape_real_;
     ptr->globalptr->_try_parse_i_=_try_parse_i_;
     ptr->globalptr->_specialtexprint_double_=_specialtexprint_double_;
     ptr->globalptr->_atan_tan_no_floor_=_atan_tan_no_floor_;
     ptr->globalptr->_keep_acosh_asinh_=_keep_acosh_asinh_;
     ptr->globalptr->_keep_algext_=_keep_algext_;
     ptr->globalptr->_python_compat_=_python_compat_;
     ptr->globalptr->_complex_variables_=_complex_variables_;
     ptr->globalptr->_increasing_power_=_increasing_power_;
     ptr->globalptr->_approx_mode_=_approx_mode_;
     ptr->globalptr->_series_variable_name_=_series_variable_name_;
     ptr->globalptr->_series_default_order_=_series_default_order_;
     ptr->globalptr->_autosimplify_=_autosimplify_();
     ptr->globalptr->_lastprog_name_=_lastprog_name_();
     ptr->globalptr->_angle_mode_=_angle_mode_;
     ptr->globalptr->_variables_are_files_=_variables_are_files_;
     ptr->globalptr->_bounded_function_no_=_bounded_function_no_;
     ptr->globalptr->_series_flags_=_series_flags_; // bit1= full simplify, bit2=1 for truncation
     ptr->globalptr->_step_infolevel_=_step_infolevel_; // bit1= full simplify, bit2=1 for truncation
     ptr->globalptr->_local_eval_=_local_eval_;
     ptr->globalptr->_epsilon_=_epsilon_<=0?init_epsilon:_epsilon_;
     ptr->globalptr->_proba_epsilon_=_proba_epsilon_;
     ptr->globalptr->_withsqrt_=_withsqrt_;
     ptr->globalptr->_spread_Row_=_spread_Row_;
     ptr->globalptr->_spread_Col_=_spread_Col_;
     ptr->globalptr->_printcell_current_row_=_printcell_current_row_;
     ptr->globalptr->_printcell_current_col_=_printcell_current_col_;
     ptr->globalptr->_all_trig_sol_=_all_trig_sol_;
     ptr->globalptr->_lexer_close_parenthesis_=_lexer_close_parenthesis_;
     ptr->globalptr->_rpn_mode_=_rpn_mode_;
     ptr->globalptr->_ntl_on_=_ntl_on_;
     ptr->globalptr->_prog_eval_level_val =_prog_eval_level_val ;
     ptr->globalptr->_eval_level=_eval_level;
     ptr->globalptr->_rand_seed=_rand_seed;
     ptr->globalptr->_language_=_language_;
     ptr->globalptr->_last_evaled_argptr_=_last_evaled_argptr_;
     ptr->globalptr->_last_evaled_function_name_=_last_evaled_function_name_;
     ptr->globalptr->_currently_scanned_="";
     ptr->globalptr->_max_sum_sqrt_=_max_sum_sqrt_;      
     ptr->globalptr->_max_sum_add_=_max_sum_add_;   
     
  }

  context * clone_context(const context * contextptr) {
    context * ptr = new context;
    if (contextptr){
      *ptr->globalptr = *contextptr->globalptr;
      *ptr->tabptr = *contextptr->tabptr;
    }
    else {
      init_context(ptr);
    }
    return ptr;
  }

  context::~context(){
    // CERR << "delete context " << this << "\n";
    if (!previous){
      if (history_in_ptr)
	delete history_in_ptr;
      if (history_out_ptr)
	delete history_out_ptr;
      if (history_plot_ptr)
	delete history_plot_ptr;
      if (quoted_global_vars)
	delete quoted_global_vars;
      if (rootofs)
	delete rootofs;
      if (globalptr)
	delete globalptr;
      if (tabptr)
	delete tabptr;
#ifdef HAVE_LIBPTHREAD
      pthread_mutex_lock(&context_list_mutex);
#endif
      int s=int(context_list().size());
      for (int i=s-1;i>0;--i){
	if (context_list()[i]==this){
	  context_list().erase(context_list().begin()+i);
	  break;
	}
      }
#ifndef RTOS_THREADX
#if !defined BESTA_OS && !defined NSPIRE && !defined FXCG && !defined TICE && !defined NSPIRE_NEWLIB
      if (context_names){
	map<string,context *>::iterator it=context_names->begin(),itend=context_names->end();
	for (;it!=itend;++it){
	  if (it->second==this){
	    context_names->erase(it);
	    break;
	  }
	}
      }
#endif
#endif
#ifdef HAVE_LIBPTHREAD
      pthread_mutex_unlock(&context_list_mutex);
#endif
    }
  }

#ifndef CLK_TCK
#define CLK_TCK 1
#endif


  string remove_filename(const string & s){
    int l=int(s.size());
    for (;l;--l){
      if (s[l-1]=='/')
	break;
    }
    return s.substr(0,l);
  }


  bool make_thread(const gen & g,int level,const giac_callback & f,void * f_param,context * contextptr){
    return false;
  }

  int check_thread(context * contextptr){
    return -1;
  }

  int check_threads(int i){
    return -1;
  }

  gen thread_eval(const gen & g,int level,context * contextptr,void (* wait_001)(context * )){
    return protecteval(g,level,contextptr);
  }

  debug_struct::debug_struct():indent_spaces(0),debug_mode(false),sst_mode(false),sst_in_mode(false),debug_allowed(true),current_instruction(-1),debug_refresh(false){
    debug_info_ptr=new gen;
    fast_debug_info_ptr=new gen;
    debug_prog_name=new gen;
    debug_localvars=new gen;
    debug_contextptr=0;
  }
  
  debug_struct::~debug_struct(){
    delete debug_info_ptr;
    delete fast_debug_info_ptr;
    delete debug_prog_name;
    delete debug_localvars;
  }

  debug_struct & debug_struct::operator =(const debug_struct & dbg){
    indent_spaces=dbg.indent_spaces;
    args_stack=dbg.args_stack;
    debug_breakpoint=dbg.debug_breakpoint;
    debug_watch=dbg.debug_watch ;
    debug_mode=dbg.debug_mode;
    sst_mode=dbg.sst_mode ;
    sst_in_mode=dbg.sst_in_mode ;
    debug_allowed=dbg.debug_allowed;
    current_instruction_stack=dbg.current_instruction_stack;
    current_instruction=dbg.current_instruction;
    sst_at_stack=dbg.sst_at_stack;
    sst_at=dbg.sst_at;
    if (debug_info_ptr)
      delete debug_info_ptr;
    debug_info_ptr=new gen(dbg.debug_info_ptr?*dbg.debug_info_ptr:0) ;
    if (fast_debug_info_ptr)
      delete fast_debug_info_ptr;
    fast_debug_info_ptr= new gen(dbg.fast_debug_info_ptr?*dbg.fast_debug_info_ptr:0);
    if (debug_prog_name)
      delete debug_prog_name;
    debug_prog_name=new gen(dbg.debug_prog_name?*dbg.debug_prog_name:0);
    if (debug_localvars)
      delete debug_localvars;
    debug_localvars=new gen(dbg.debug_localvars?*dbg.debug_localvars:0);
    debug_refresh=dbg.debug_refresh;
    debug_contextptr=dbg.debug_contextptr;
    return *this;
  }

  static debug_struct & _debug_data(){
    static debug_struct * ans = 0;
    if (!ans) ans=new debug_struct;
    return *ans;
  }

  debug_struct * debug_ptr(GIAC_CONTEXT){
    if (contextptr && contextptr->globalptr)
      return contextptr->globalptr->_debug_ptr;
    return &_debug_data();
  }

  void clear_prog_status(GIAC_CONTEXT){
    debug_struct * ptr=debug_ptr(contextptr);
    if (ptr){
      ptr->args_stack.clear();
      ptr->debug_mode=false;
      ptr->sst_at_stack.clear();
      if (!contextptr)
	protection_level=0;
    }
  }


  global::global() : _xcas_mode_(0), 
		     _calc_mode_(0),_decimal_digits_(12),_minchar_for_quote_as_string_(1),
		     _scientific_format_(0), _integer_format_(0), _latex_format_(0), 
#ifdef BCD
		     _bcd_decpoint_('.'|('E'<<16)|(' '<<24)),_bcd_mantissa_(12+(15<<8)), _bcd_flags_(0),_bcd_printdouble_(false),
#endif
		     _expand_re_im_(true), _do_lnabs_(true), _eval_abs_(true),_eval_equaltosto_(true),_integer_mode_(true),_complex_mode_(false), _escape_real_(true),_complex_variables_(false), _increasing_power_(false), _approx_mode_(false), _variables_are_files_(false), _local_eval_(true), 
		     _withsqrt_(true), 
		     _all_trig_sol_(false),
#ifdef WITH_MYOSTREAM
		     _ntl_on_(true),
		     _lexer_close_parenthesis_(true),_rpn_mode_(false),_try_parse_i_(true),_specialtexprint_double_(false),_atan_tan_no_floor_(false),_keep_acosh_asinh_(false),_keep_algext_(false),_python_compat_(false),_angle_mode_(0), _bounded_function_no_(0), _series_flags_(0x3),_step_infolevel_(0), _epsilon_(init_epsilon), _proba_epsilon_(1e-15),  _spread_Row_ (-1), _spread_Col_ (-1),_logptr_(&my_CERR),_prog_eval_level_val(1), _eval_level(DEFAULT_EVAL_LEVEL), _rand_seed(123457),_last_evaled_function_name_(0),_currently_scanned_(""),_last_evaled_argptr_(0),_max_sum_sqrt_(3),
#ifdef GIAC_HAS_STO_38 // Prime sum(x^2,x,0,100000) crash on hardware	
		     _max_sum_add_(10000),
#else
		     _max_sum_add_(100000),
#endif
		     _total_time_(0),_evaled_table_(0),_extra_ptr_(0),_series_variable_name_('h'),_series_default_order_(5),
#else
		     _ntl_on_(true),
		     _lexer_close_parenthesis_(true),_rpn_mode_(false),_try_parse_i_(true),_specialtexprint_double_(false),_atan_tan_no_floor_(false),_keep_acosh_asinh_(false),_keep_algext_(false),_python_compat_(false),_angle_mode_(0), _bounded_function_no_(0), _series_flags_(0x3),_step_infolevel_(0), _epsilon_(init_epsilon), _proba_epsilon_(1e-15),  _spread_Row_ (-1), _spread_Col_ (-1), 
#ifdef EMCC
		     _logptr_(&COUT), 
#else
#if defined FXCG || defined TICE 
		     _logptr_(&cout),
#else
#if defined NSPIRE_NEWLIB
		     _logptr_(&Cout),
#else
		     _logptr_(&CERR),
#endif
#endif
#endif
		     _prog_eval_level_val(1), _eval_level(DEFAULT_EVAL_LEVEL), _rand_seed(123457),_last_evaled_function_name_(0),_currently_scanned_(""),_last_evaled_argptr_(0),_max_sum_sqrt_(3),
#ifdef GIAC_HAS_STO_38 // Prime sum(x^2,x,0,100000) crash on hardware	
		     _max_sum_add_(10000),
#else
		     _max_sum_add_(100000),
#endif
		     _total_time_(0),_evaled_table_(0),_extra_ptr_(0),_series_variable_name_('h'),_series_default_order_(5)
#endif
  { 
    _pl._i_sqrt_minus1_=1;
    _debug_ptr=new debug_struct;
    _thread_param_ptr=new thread_param;
    _parsed_genptr_=new gen;
#ifdef GIAC_HAS_STO_38
    _autoname_="GA";
#else
    _autoname_="A";
#endif
    _autosimplify_="regroup";
    _lastprog_name_="lastprog";
    _format_double_="";
#ifdef HAVE_LIBPTHREAD
    _mutexptr = new pthread_mutex_t;
    pthread_mutex_init(_mutexptr,0);
    _mutex_eval_status_ptr = new pthread_mutex_t;
    pthread_mutex_init(_mutex_eval_status_ptr,0);
#endif
  }

  global & global::operator = (const global & g){
     _xcas_mode_=g._xcas_mode_;
     _calc_mode_=g._calc_mode_;
     _decimal_digits_=g._decimal_digits_;
     _minchar_for_quote_as_string_=g._minchar_for_quote_as_string_;
     _scientific_format_=g._scientific_format_;
     _integer_format_=g._integer_format_;
     _integer_mode_=g._integer_mode_;
     _latex_format_=g._latex_format_;
#ifdef BCD
     _bcd_decpoint_=g._bcd_decpoint_;
     _bcd_mantissa_=g._bcd_mantissa_;
     _bcd_flags_=g._bcd_flags_;
     _bcd_printdouble_=g._bcd_printdouble_;
#endif
     _expand_re_im_=g._expand_re_im_;
     _do_lnabs_=g._do_lnabs_;
     _eval_abs_=g._eval_abs_;
     _eval_equaltosto_=g._eval_equaltosto_;
     _complex_mode_=g._complex_mode_;
     _escape_real_=g._escape_real_;
     _complex_variables_=g._complex_variables_;
     _increasing_power_=g._increasing_power_;
     _approx_mode_=g._approx_mode_;
     _series_variable_name_=g._series_variable_name_;
     _series_default_order_=g._series_default_order_;
     _angle_mode_=g._angle_mode_;
     _atan_tan_no_floor_=g._atan_tan_no_floor_;
     _keep_acosh_asinh_=g._keep_acosh_asinh_;
     _keep_algext_=g._keep_algext_;
     _python_compat_=g._python_compat_;
     _variables_are_files_=g._variables_are_files_;
     _bounded_function_no_=g._bounded_function_no_;
     _series_flags_=g._series_flags_; // bit1= full simplify, bit2=1 for truncation, bit3=?, bit4=1 do not convert back SPOL1 to symbolic expression
     _step_infolevel_=g._step_infolevel_; // bit1= full simplify, bit2=1 for truncation
     _local_eval_=g._local_eval_;
     _epsilon_=g._epsilon_;
     _proba_epsilon_=g._proba_epsilon_;
     _withsqrt_=g._withsqrt_;
     _spread_Row_=g._spread_Row_;
     _spread_Col_=g._spread_Col_;
     _printcell_current_row_=g._printcell_current_row_;
     _printcell_current_col_=g._printcell_current_col_;
     _all_trig_sol_=g._all_trig_sol_;
     _ntl_on_=g._ntl_on_;
     _prog_eval_level_val =g._prog_eval_level_val ;
     _eval_level=g._eval_level;
     _rand_seed=g._rand_seed;
     _language_=g._language_;
     _last_evaled_argptr_=g._last_evaled_argptr_;
     _last_evaled_function_name_=g._last_evaled_function_name_;
     _currently_scanned_=g._currently_scanned_;
     _max_sum_sqrt_=g._max_sum_sqrt_;
     _max_sum_add_=g._max_sum_add_;
     _autoname_=g._autoname_;
     _format_double_=g._format_double_;
     _extra_ptr_=g._extra_ptr_;
     return *this;
  }

  global::~global(){
    delete _parsed_genptr_;
    delete _thread_param_ptr;
    delete _debug_ptr;
#ifdef HAVE_LIBPTHREAD
    pthread_mutex_destroy(_mutexptr);
    delete _mutexptr;
    pthread_mutex_destroy(_mutex_eval_status_ptr);
    delete _mutex_eval_status_ptr;
#endif
  }

#ifndef TICE
  bool my_isinf(double d){
    return 1/d==0.0;
  }
  bool my_isnan(double d){
    return d!=0 && d==d*2 && !my_isinf(d);
  }
#endif

#if 0
  double giac_floor(double d){
    double maxdouble=longlong(1)<<30;
    if (d>=maxdouble || d<=-maxdouble)
      return std::floor(d);
    if (d>0)
      return int(d);
    double k=int(d);
    if (k==d)
      return k;
    else
      return k-1;
  }

  double giac_ceil(double d){
    double maxdouble=longlong(1)<<54;
    if (d>=maxdouble || d<=-maxdouble)
      return d;
    if (d<0)
      return double(longlong(d));
    double k=double(longlong(d));
    if (k==d)
      return k;
    else
      return k+1;
  }
#endif

  const char * const do_not_autosimplify[]={
    "POLYFORM",
    "autosimplify",
    "canonical_form",
    "cfactor",
    "cpartfrac",
    "diff",
    "domain",
    "element",
    "evalc",
    "expand",
    "expexpand",
    "factor",
    "ifactor",
    "lncollect",
    "lnexpand",
    "mult_c_conjugate",
    "mult_conjugate",
    "nodisp",
    "normal",
    "op",
    "partfrac",
    "pow2exp",
    "powexpand",
    "propfrac",
    "quote",
    "regroup",
    "reorder",
    "series",
    "simplify",
    "tabvar",
    "taylor",
    "texpand",
    "trace",
    "trigexpand",
    0
  };

  int dichotomic_search(const char * const * tab,unsigned tab_size,const char * s){
    int beg=0,end=tab_size,cur,test;
    // string index is always >= begin and < end
    for (;;){
      cur=(beg+end)/2;
      test=strcmp(s,tab[cur]);
      if (!test)
	return cur;
      if (cur==beg)
	return -1;
      if (test>0)
	beg=cur;
      else
	end=cur;
    }
    return -1;
  }

  gen add_autosimplify(const gen & g,GIAC_CONTEXT){
    if (g.type==_VECT)
      return apply(g,add_autosimplify,contextptr);
    if (g.type==_SYMB){
      if (g._SYMBptr->sommet==at_program)
	return g;
#ifdef GIAC_HAS_STO_38
      const char * c=g._SYMBptr->sommet.ptr()->s;
#else
      string ss=unlocalize(g._SYMBptr->sommet.ptr()->s);
      const char * c=ss.c_str();
#endif
#if 1
      if (dichotomic_search(do_not_autosimplify,sizeof(do_not_autosimplify)/sizeof(char*)-1,c)!=-1)
	return g;
#else
      const char ** ptr=do_not_autosimplify;
      for (;*ptr;++ptr){
	if (!strcmp(*ptr,c))
	  return g;
      }
#endif
    }
    std::string s=autosimplify(contextptr);
    if (s.size()<1 || s=="'nop'")
      return g;
    gen a(s,contextptr);
    if (a.type==_FUNC)
      return symbolic(*a._FUNCptr,g);
    if (a.type>=_IDNT)
      return symb_of(a,g);
    return g;
  }

  void (*my_gprintf)(unsigned special,const string & format,const vecteur & v,GIAC_CONTEXT)=0;


#ifdef EMCC
  static void newlinestobr(string &s,const string & add){
    int l=int(add.size());
    for (int i=0;i<l;++i){
      if (add[i]=='\n')
	s+="<br>";
      else
	s+=add[i];
    }
  }
#else
  static void newlinestobr(string &s,const string & add){
    s+=add;
  }
#endif

  void gprintf(unsigned special,const string & format,const vecteur & v,GIAC_CONTEXT){
    return gprintf(special,format,v,step_infolevel(contextptr),contextptr);
  }

  void gprintf(unsigned special,const string & format,const vecteur & v,int step_info,GIAC_CONTEXT){
    if (step_info==0)
      return;
    if (my_gprintf){
      my_gprintf(special,format,v,contextptr);
      return;
    }
    string s;
    int pos=0;
#ifdef EMCC
    *logptr(contextptr) << char(2) << "\n"; // start mixed text/mathml
#endif
    for (unsigned i=0;i<v.size();++i){
      int p=int(format.find("%gen",pos));
      if (p<0 || p>=int(format.size()))
	break;
      newlinestobr(s,format.substr(pos,p-pos));
#ifdef EMCC
      gen tmp;
      if (v[i].is_symb_of_sommet(at_pnt))
	tmp=_svg(v[i],contextptr);
      else
	tmp=_mathml(makesequence(v[i],1),contextptr);
      s = s+((tmp.type==_STRNG)?(*tmp._STRNGptr):v[i].print(contextptr));
#else
      s += v[i].print(contextptr);
#endif
      pos=p+4;
    }
    newlinestobr(s,format.substr(pos,format.size()-pos));
    *logptr(contextptr) << s << "\n";
#ifdef TICE
    Console_Disp(1);
#endif
#ifdef EMCC
    *logptr(contextptr) << char(3) << "\n"; // end mixed text/mathml
    *logptr(contextptr) << "\n";
#endif
  }

  void gprintf(const string & format,const vecteur & v,GIAC_CONTEXT){
    gprintf(step_nothing_special,format,v,contextptr);
  }

  void gprintf(const string & format,const vecteur & v,int step_info,GIAC_CONTEXT){
    gprintf(step_nothing_special,format,v,step_info,contextptr);
  }

  // moved from input_lexer.ll for easier debug
  const char invalid_name[]="Invalid name";



#include "input_parser.h" 

    bool lexer_functions_register(const unary_function_ptr & u,const char * s,int parser_token){
      return false;
    }

#if 0
  bool cas_builtin(const char * s){
    std::pair<charptr_gen *,charptr_gen *> p=std::equal_range(builtin_lexer_functions_begin(),builtin_lexer_functions_end(),std::pair<const char *,gen>(s,0),tri);
    return p.first!=p.second && p.first!=builtin_lexer_functions_end();
  }
#endif

    int find_or_make_symbol(const string & s,gen & res,void * scanner,bool check38,GIAC_CONTEXT){
#if 0
      if (s.size()==1){
	switch (s[0]){
	case '+':
	  res=at_plus;
	  return T_UNARY_OP;
	case '-':
	  res=at_neg;
	  return T_UNARY_OP;
	case '*':
	  res=at_prod;
	  return T_UNARY_OP;
	case '/':
	  res=at_division;
	  return T_UNARY_OP;
	case '^':
	  res=at_pow;
	  return T_UNARY_OP;
	}
      }
#endif
      string ts(s);
      if (ts=="I"){
  res=cst_i;
  index_status(contextptr)=1;
  return T_LITERAL;
      }
      if (ts=="inf" || ts=="+inf"){
  res=plus_inf;
    	index_status(contextptr)=1;
    	return T_LITERAL;
      }
      if (ts=="-inf"){
    	res=minus_inf;
    	index_status(contextptr)=1;
    	return T_LITERAL;
      }
      int lo=0,hi=int(builtin_lexer_functions_number)-1,pos=-1;
      while (lo<=hi){
  int mid=(lo+hi)>>1;
  int cmp=strcmp(ts.c_str(),builtin_lexer_functions[mid].s);
  if (!cmp){
    pos=mid;
    break;
  }
  if (cmp<0)
    hi=mid-1;
  else
    lo=mid+1;
      }
      if (pos>=0){
  int subtype=builtin_lexer_functions[pos].subtype;
  if (subtype==T_TO-256)
    res=plus_one;
  else {
    const unary_function_ptr * tab[]={
#include "static_lexer_.h"
    };
    res=gen(tab[pos]);
    if (builtin_lexer_functions[pos]._FUNC_%2){
#ifdef TICE
      res._FUNC_ |= 0x800000;
#else
      res._FUNC_ +=1;
#endif
    }
  }
  index_status(contextptr)=(subtype==T_UNARY_OP-256);
  int token=subtype;
  token += (token<0)?512:256 ;	
  return token;
      }
      lexer_tab_int_type tst={ts.c_str(),0,0,0,0};
#ifdef USTL
      ustl::pair<const lexer_tab_int_type *,const lexer_tab_int_type *> pp = ustl::equal_range(lexer_tab_int_values,lexer_tab_int_values_end,tst,tri1);
#else
      std::pair<const lexer_tab_int_type *,const lexer_tab_int_type *> pp = equal_range(lexer_tab_int_values,lexer_tab_int_values_end,tst,tri1);
#endif
      if (pp.first!=pp.second && pp.first!=lexer_tab_int_values_end){
	index_status(contextptr)=pp.first->status;
	res=int(pp.first->value);
	res.subtype=pp.first->subtype;
	return pp.first->return_value;
      }
      lock_syms_mutex();
      sym_string_tab::const_iterator i2 = syms().find(s),i2end=syms().end();
      if (i2 == i2end) {
	unlock_syms_mutex();  
	const char * S = s.c_str();
	res = identificateur(s);
	// printf("ident %s %s\n",S,res._IDNTptr->id_name);
	lock_syms_mutex();
	syms()[s] = res;
	unlock_syms_mutex();
	return T_SYMBOL;
      } // end if ==syms.end()
      res = i2->second;
      unlock_syms_mutex();  
      return T_SYMBOL;
    }

  // Add to the list of predefined symbols
  void set_lexer_symbols(const vecteur & l,GIAC_CONTEXT){
    if (initialisation_done(contextptr))
      return;
    initialisation_done(contextptr)=true;
    const_iterateur it=l.begin(),itend=l.end();
    for (; it!=itend; ++it) {
      if (it->type!=_IDNT)
	continue;
      lock_syms_mutex();
      sym_string_tab::const_iterator i = syms().find(it->_IDNTptr->id_name),iend=syms().end();
      if (i==iend)
	syms()[it->_IDNTptr->name()] = *it;
      unlock_syms_mutex();  
    }
  }


  const charptr_gen_unary  builtin_lexer_functions[] ={
#include "static_lexer.h"
  };

  const unsigned builtin_lexer_functions_number=sizeof(builtin_lexer_functions)/sizeof(charptr_gen_unary);

#if 0
  vector<size_t> * builtin_lexer_functions_(){
    static vector<size_t> * res=0;
    if (res) return res;
    res = new vector<size_t>;
    res->reserve(builtin_lexer_functions_number+1);
#include "static_lexer_at.h"
    return res;
  }
#endif
  
  charptr_gen * builtin_lexer_functions_begin(){
    return (charptr_gen *) builtin_lexer_functions;
  }
  
  charptr_gen * builtin_lexer_functions_end(){
    return builtin_lexer_functions_begin()+builtin_lexer_functions_number;
  }
  
  gen make_symbolic(const gen & op,const gen & args){
    return symbolic(*op._FUNCptr,args);
  }

  /* PYTHON */

  string replace(const string & s,char c1,char c2){
    string res;
    int l=s.size();
    res.reserve(l);
    const char * ch=s.c_str();
    for (int i=0;i<l;++i,++ch){
      res+= (*ch==c1? c2: *ch);
    }
    return res;
  }

  static string remove_comment(const string & s,const string &pattern,bool rep){
    string res(s);
    for (;;){
      int pos1=res.find(pattern);
      if (pos1<0 || pos1+3>=int(res.size()))
	break;
      int pos2=res.find(pattern,pos1+3);
      if (pos2<0 || pos2+3>=int(res.size()))
	break;
      if (rep){
	string tmp=res.substr(0,pos1);
	tmp +=  '"';
	tmp += replace(res.substr(pos1+3,pos2-pos1-3),'\n',' ');
	tmp += '"';
	tmp += res.substr(pos2+3,res.size()-pos2-3);
	res = tmp;
      }
      else
	res = res.substr(0,pos1)+res.substr(pos2+3,res.size()-pos2-3);
    }
    return res;
  }

  struct int_string {
    int decal;
    std::string endbloc;
    int_string():decal(0){}
    int_string(int i,string s):decal(i),endbloc(s){}
  };

  static bool instruction_at(const string & s,int pos,int shift){
    if (pos && isalphan(s[pos-1]))
      return false;
    if (pos+shift<int(s.size()) && isalphan(s[pos+shift]))
      return false;
    return true;
  }

  void convert_python(string & cur,GIAC_CONTEXT){
    bool indexshift=array_start(contextptr); //xcas_mode(contextptr)!=0 || abs_calc_mode(contextptr)==38;
    if (cur[0]=='_' && (cur.size()==1 || !isalpha(cur[1])))
      cur[0]='@'; // python shortcut for ans(-1)
    bool instring=cur.size() && cur[0]=='"';
    int openpar=0;
    for (int pos=1;pos<int(cur.size());++pos){
      char prevch=cur[pos-1],curch=cur[pos];
      if (curch=='"' && prevch!='\\')
	instring=!instring;
      if (instring)
	continue;
      if (curch=='(')
	++openpar;
      if (curch==')')
	--openpar;
      if (curch==',' && pos<int(cur.size()-1)){
	char nextch=cur[pos+1];
	if (nextch=='}' || nextch==']' || nextch==')'){
	  cur.erase(cur.begin()+pos);
	  continue;
	}
      }
      if (curch=='}' && prevch=='{'){
	cur=cur.substr(0,pos-1)+"table()"+cur.substr(pos+1,cur.size()-pos-1);
	continue;
      }
      if (curch==':' && (prevch=='[' || prevch==',')){
	cur.insert(cur.begin()+pos,indexshift?'1':'0');
	continue;
      }
      if (curch==':' && pos<int(cur.size())-1 && cur[pos+1]!='=' && cur[pos+1]!=';'){
	int posif=cur.find("if "),curpos,cursize=int(cur.size()),count=0;
	// check is : for slicing?
	for (curpos=pos+1;curpos<cursize;++curpos){
	  if (cur[curpos]=='[')
	    ++count;
	  if (cur[curpos]==']')
	    --count;
	}
	if (count==0 && posif>=0 && posif<pos){
	  cur[pos]=')';
	  cur.insert(cur.begin()+posif+3,'(');
	  continue;
	}
      }
      if ( (curch==']' && (prevch==':' || prevch==',')) ||
	   (curch==',' && prevch==':') ){
	cur[pos-1]='.';
	cur.insert(cur.begin()+pos,'.');
	++pos;
	if (indexshift)
	  cur.insert(cur.begin()+pos,'0');
	else {
	  cur.insert(cur.begin()+pos,'1');
	  cur.insert(cur.begin()+pos,'-');
	}
	continue;
      }
      if (curch==':' && prevch==':'){
	if (pos+1<int(cur.size()) && cur[pos+1]=='-'){
	  cur.insert(cur.begin()+pos,'-');
	  cur.insert(cur.begin()+pos+1,'1');
	}
	else
	  cur.insert(cur.begin()+pos,'0');
	continue;	
      }
      if (curch=='%'){
	cur.insert(cur.begin()+pos+1,'/');
	++pos;
	continue;
      }
      if (curch=='=' && openpar==0 && prevch!='>' && prevch!='<' && prevch!='!' && prevch!=':' && prevch!=';' && prevch!='=' && prevch!='+' && prevch!='-' && prevch!='*' && prevch!='/' && prevch!='%' && (pos==int(cur.size())-1 || (cur[pos+1]!='=' && cur[pos+1]!='<'))){
	cur.insert(cur.begin()+pos,':');
	++pos;
	continue;
      }
      if (prevch=='/' && curch=='/' && pos>1)
	cur[pos]='%';
    }
  }

  string glue_lines_backslash(const string & s){
    int ss=s.size();
    int i=s.find('\\');
    if (i<0 || i>=ss)
      return s;
    string res,line;    
    for (i=0;i<ss;++i){
      if (s[i]!='\n'){
	line += s[i];
	continue;
      }
      int ls=line.size(),j;
      for (j=ls-1;j>=0;--j){
	if (line[j]!=' ')
	  break;
      }
      if (line[j]!='\\' || (j && line[j-1]=='\\')){
	res += line;
	res += '\n';
	line ="";
      }
      else
	line=line.substr(0,j); 
    }
    return res+line;
  }

  static void python_import(string & cur,int cs,int posturtle,int poscmath,int posmath,int posnumpy,int posmatplotlib,GIAC_CONTEXT){
    if (posmatplotlib>=0 && posmatplotlib<cs){
      cur += "np:=numpy:;xlim(a,b):=gl_x=a..b:;ylim(a,b):=gl_y=a..b:;scatter:=scatterplot:;bar:=bar_plot:;text:=legend:;";
      posnumpy=posmatplotlib;
    }
    if (posnumpy>=0 && posnumpy<cs){
      static bool alertnum=true;
      // add python numpy shortcuts
      cur += "mat:=matrix:;arange:=range:;resize:=redim:;shape:=dim:;conjugate:=conj:;full:=matrix:;eye:=identity:;ones(n,c):=matrix(n,c,1):; astype:=convert:;float64:=float:;asarray:=array:;astype:=convert:;reshape(m,n,c):=matrix(n,c,flatten(m));";
      if (alertnum){
	alertnum=false;
	alert("mat:=matrix;arange:=range;resize:=redim;shape:=dim;conjugate:=conj;full:=matrix;eye:=idn;ones(n,c):=matrix(n,c,1);reshape(m,n,c):=matrix(n,c,flatten(m));",contextptr);
      }
      return;
    }
    if (poscmath>=0 && poscmath<cs){
      // add python cmath shortcuts
      static bool alertcmath=true;      
      if (alertcmath){
	alertcmath=false;
	alert(gettext("Assigning phase, j, J and rect."),contextptr);
      }
      cur += "phase:=arg:;j:=i:;J:=i:;rect(r,theta):=r*exp(i*theta):;";
      posmath=poscmath;
    }
    if (posmath>=0 && posmath<cs){
      // add python math shortcuts
      static bool alertmath=true;      
      if (alertmath){
	alertmath=false;
	alert(gettext("Assigning log2, gamma, fabs, modf, radians and degrees."),contextptr);
      }
      cur += "log2(x):=logb(x,2):;gamma:=Gamma:;fabs:=abs:;function modf(x) local y; y:=floor(x); return x-y,y; ffunction:;radians(x):=x/180*pi:;degrees(x):=x/pi*180";
      // todo copysign, isinf, isnan, isfinite, frexp, ldexp
    }
  }


  string replace_deuxpoints_egal(const string & s){
    string res;
    int instring=0;
    for (size_t i=0;i<s.size();++i){
      char ch=s[i];
      if (i==0 || s[i-1]!='\\'){
	if (ch=='\''){
	  if (instring==2)
	    res += ch;
	  else {
	    res +='"';
	    instring=instring?0:1;
	  }
	  continue;
	}
	if (instring){
	  if (ch=='"'){
	    if (instring==1)
	      res +="\"\"";
	    else {
	      res += ch;
	      instring=0;
	    }
	  }
	  else
	    res += ch;
	  continue;
	}
	if (ch=='"'){
	  res +='"';
	  instring=2;
	  continue;
	}
      }
      switch (ch){
      case ':':
	res +="-/-";
	break;
      case '{':
	res += "{/";
	break;
      case '}':
	res += "/}";
	break;
      default:
	res += ch;
      }
    }
    return res;
  }


  // detect Python like syntax: 
  // remove """ """ docstrings and ''' ''' comments
  // cut string in lines, remove comments at the end (search for #)
  // warning don't take care of # inside strings
  // if a line of s ends with a :
  // search for matching def/for/if/else/while
  // stores matching end keyword in a stack as a vector<[int,string]>
  // int is the number of white spaces at the start of the next line
  // def ... : -> function [ffunction]
  // for ... : -> for ... do [od]
  // while ... : -> while ... do [od]
  // if ...: -> if ... then [fi]
  // else: -> else [nothing in stack]
  // elif ...: -> elif ... then [nothing in stack]
  // try: ... except: ...
  std::string python2xcas(const std::string & s_orig,GIAC_CONTEXT){
    if (xcas_mode(contextptr)>0 && abs_calc_mode(contextptr)!=38)
      return s_orig;
    // quick check for python-like syntax: search line ending with :
    int first=0,sss=s_orig.size();
    first=s_orig.find("maple_mode");
    if (first>=0 && first<sss)
      return s_orig;
    first=s_orig.find("xcas_mode");
    if (first>=0 && first<sss)
      return s_orig;
    bool pythoncompat=python_compat(contextptr);
    bool pythonmode=false;
    first=0;
    if (sss>19 && s_orig.substr(first,17)=="add_autosimplify(")
      first+=17;
    if (s_orig[first]=='/' )
      return s_orig;
    //if (sss>first+2 && s_orig[first]=='@' && s_orig[first+1]!='@') return s_orig.substr(first+1,sss-first-1);
    if (sss>first+2 && s_orig.substr(first,2)=="@@"){
      pythonmode=true;
      pythoncompat=true;
    }
    if (s_orig[first]=='#' || (s_orig[first]=='_' && !isalpha(s_orig[first+1])) || s_orig.substr(first,4)=="from" || s_orig.substr(first,7)=="import "){
      pythonmode=true;
      pythoncompat=true;
    }
    if (pythoncompat){
      int pos=s_orig.find("{");
      if (pos>=0 && pos<sss)
	pythonmode=true;
      pos=s_orig.find("}");
      if (pos>=0 && pos<sss)
	pythonmode=true;
      pos=s_orig.find("//");
      if (pos>=0 && pos<sss)
	pythonmode=true;
      if (pos>=0 && pos<sss)
	pythonmode=true;
      pos=s_orig.find(":");
      if (pos>=0 && pos<sss-1 && s_orig[pos+1]!=';')
	pythonmode=true;
    }
    for (first=0;!pythonmode && first<sss;){
      int pos=s_orig.find(":]");
      if (pos>=0 && pos<sss){
	pythonmode=true;
	break;
      }
      pos=s_orig.find("[:");
      if (pos>=0 && pos<sss){
	pythonmode=true;
	break;
      }
      pos=s_orig.find(",:");
      if (pos>=0 && pos<sss){
	pythonmode=true;
	break;
      }
      pos=s_orig.find(":,");
      if (pos>=0 && pos<sss){
	pythonmode=true;
	break;
      }
      first=s_orig.find(':',first);
      if (first<0 || first>=sss)
	return s_orig; // not Python like
      pos=s_orig.find("lambda");
      if (pos>=0 && pos<sss)
	break;
      int endl=s_orig.find('\n',first);
      if (endl<0 || endl>=sss)
	endl=sss;
      ++first;
      if (first<endl && (s_orig[first]==';' || s_orig[first]=='=')) 
	continue; // ignore :;
      // search for line finishing with : (or with # comment)
      for (;first<endl;++first){
	char ch=s_orig[first];
	if (ch!=' '){
	  if (ch=='#')
	    first=endl;
	  break;
	}
      }
      if (first==endl) 
	break;
    }
    // probably Python-like
    string res;
    res.reserve(1.2*s_orig.size());
    res=s_orig;
    if (res.size()>18 && res.substr(0,17)=="add_autosimplify(" 
	&& res[res.size()-1]==')'
	)
      res=res.substr(17,res.size()-18);
    if (res.size()>2 && res.substr(0,2)=="@@")
      res=res.substr(2,res.size()-2);
    res=remove_comment(res,"\"\"\"",true);
    res=remove_comment(res,"'''",true);
    res=glue_lines_backslash(res);
    vector<int_string> stack;
    string s,cur; 
    s.reserve(res.capacity());
    if (pythoncompat) pythonmode=true;
    for (;res.size();){
      int pos=-1;
      bool cherche=true;
      for (;cherche;){
	pos=res.find('\n',pos+1);
	if (pos<0 || pos>=int(res.size()))
	  break;
	cherche=false;
	char ch=0;
	// check if we should skip to next newline, look at previous non space
	for (int pos2=0;pos2<pos;++pos2){
	  ch=res[pos2];
	  if (ch=='#')
	    break;
	}
	if (ch=='#')
	  break;
	for (int pos2=pos-1;pos2>=0;--pos2){
	  ch=res[pos2];
	  if (ch!=' ' && ch!=9){
	    if (ch=='{' || ch=='[' || ch==',' || ch=='-' || ch=='+' ||  ch=='/')
	      cherche=true;
	    break;
	  }
	}
	for (size_t pos2=pos+1;pos2<res.size();++pos2){
	  ch=res[pos2];
	  if (ch!=' ' && ch!=9){
	    if (ch==']' || ch=='}' || ch==')')
	      cherche=true;
	    break;
	  }
	}
      }
      if (pos<0 || pos>=int(res.size())){
	cur=res; res="";
      }
      else {
	cur=res.substr(0,pos); // without \n
	res=res.substr(pos+1,res.size()-pos-1);
      }
      // detect comment (outside of a string) and lambda expr:expr
      bool instring=false,chkfrom=true;
      for (pos=0;pos<int(cur.size());++pos){
	char ch=cur[pos];
	if (ch==' ' || ch==char(9))
	  continue;
	if (!instring && pythoncompat && ch=='{' && (pos==0 || cur[pos-1]!='\\')){
	  // find matching }, counting : and , and ;
	  int c1=0,c2=0,c3=0,cs=int(cur.size()),p;
	  for (p=pos;p<cs;++p){
	    char ch=cur[p];
	    if (ch=='}' && cur[p-1]!='\\')
	      break;
	    if (ch==':')
	      ++c1;
	    if (ch==',')
	      ++c2;
	    if (ch==';')
	      ++c3;
	  }
	  if (p<cs
	      //&& c1
	      && c3==0){
	    // table initialization, replace {} by table( ) , 
	    // cur=cur.substr(0,pos)+"table("+cur.substr(pos+1,p-pos-1)+")"+cur.substr(p+1,cs-pos-1);
	    string tmp=cur.substr(0,pos);
	    tmp += "{/";
	    tmp += replace_deuxpoints_egal(cur.substr(pos+1,p-1-pos));
	    tmp += "/}";
	    tmp += cur.substr(p+1,cs-pos-1);
	    cur = tmp;
	  }
	}
	if (!instring && pythoncompat &&
	    ch=='\'' && pos<cur.size()-2 && cur[pos+1]!='\\' && (pos==0 || (cur[pos-1]!='\\' && cur[pos-1]!='\''))){ // workaround for '' string delimiters
	  static bool alertstring=true;
	  if (alertstring){
	    alert("Python compatibility, please use \"...\" for strings",contextptr);
	    alertstring=false;
	  }
	  int p=pos,q=pos+1,beg; // skip spaces
	  for (p++;p<int(cur.size());++p)
	    if (cur[p]!=' ') 
	      break;
	  if (p!=cur.size()){
	    // find matching ' 
	    beg=q;
	    for (;p<int(cur.size());++p)
	      if (cur[p]=='\'') 
		break;
	    if (p>0 && p<int(cur.size())){
	      --p;
	      // does cur[pos+1..p-1] look like a string?
	      bool str=!isalpha(cur[q]) || !isalphan(cur[p]);
	      if (p && cur[p]=='.' && cur[p-1]>'9')
		str=true;
	      if (p-q>=minchar_for_quote_as_string(contextptr))
		str=true;
	      for (;!str && q<p;++q){
		char ch=cur[q];
		if (ch=='"' || ch==' ')
		  str=true;
	      }
	      if (str){ // replace delimiters with " and " inside by \"
		string rep("\"");
		for (q=beg;q<=p;++q){
		  if (cur[q]!='"')
		    rep+=cur[q];
		  else
		    rep+="\\\"";
		}
		rep += '"';
		string tmp=cur.substr(0,pos);
		tmp += rep;
		tmp += cur.substr(p+2,cur.size()-(p+2));
		cur = tmp;
		ch=cur[pos];
	      }
	    }
	  }
	}
	if (ch=='"' && (pos==0 || cur[pos-1]!='\\')){
	  chkfrom=false;
	  instring=!instring;
	}
	if (instring)
	  continue;
	if (ch=='#'){
	  // workaround to declare local variables
	  if (cur.size()>pos+8 && (cur.substr(pos,8)=="# local " || cur.substr(pos,7)=="#local ")){
	    cur.erase(cur.begin()+pos);
	    if (cur[pos]==' ')
	      cur.erase(cur.begin()+pos);	      
	  }
	  else
	    cur=cur.substr(0,pos);
	  pythonmode=true;
	  break;
	}
	// skip from * import *
	if (chkfrom && ch=='f' && pos+15<int(cur.size()) && cur.substr(pos,5)=="from "){
	  chkfrom=false;
	  int posi=cur.find(" import ");
	  if (posi<0 || posi>=int(cur.size()))
	    posi = cur.find(" import*");
	  if (posi>pos+5 && posi<int(cur.size())){
	    int posturtle=cur.find("turtle");
	    int poscmath=cur.find("cmath");
	    int posmath=cur.find("math");
	    int posnumpy=cur.find("numpy");
	    int posmatplotlib=cur.find("matplotlib");
	    if (posmatplotlib<0 || posmatplotlib>=cur.size())
	      posmatplotlib=cur.find("pylab");
	    int cs=int(cur.size());
	    cur=cur.substr(0,pos);
	    python_import(cur,cs,posturtle,poscmath,posmath,posnumpy,posmatplotlib,contextptr);
	    pythonmode=true;
	    break;
	  }
	}
	chkfrom=false;
	// import * as ** -> **:=*
	if (ch=='i' && pos+7<int(cur.size()) && cur.substr(pos,7)=="import "){
	  int posturtle=cur.find("turtle");
	  int poscmath=cur.find("cmath");
	  int posmath=cur.find("math");
	  int posnumpy=cur.find("numpy");
	  int posmatplotlib=cur.find("matplotlib");
	  if (posmatplotlib<0 || posmatplotlib>=cur.size())
	    posmatplotlib=cur.find("pylab");
	  int cs=int(cur.size());
	  int posi=cur.find(" as ");
	  int posp=cur.find('.');
	  if (posp>=posi || posp<0)
	    posp=posi;
	  if (posi>pos+5 && posi<int(cur.size())){
	    string tmp=cur.substr(posi+4,cur.size()-posi-4);
	    tmp += ":=";
	    tmp += cur.substr(pos+7,posp-(pos+7));
	    tmp += ';';
	    cur = tmp ;
	  }
	  else
	    cur=cur.substr(pos+7,cur.size()-pos-7);
	  for (int i=0;i<pos;++i)
	    cur = ' '+cur;
	  python_import(cur,cs,posturtle,poscmath,posmath,posnumpy,posmatplotlib,contextptr);
	  pythonmode=true;
	  break;	    
	}
	if (ch=='l' && pos+6<int(cur.size()) && cur.substr(pos,6)=="lambda" && instruction_at(cur,pos,6)){
	  int posdot=cur.find(':',pos);
	  if (posdot>pos+7 && posdot<int(cur.size())-1 && cur[posdot+1]!='=' && cur[posdot+1]!=';'){
	    pythonmode=true;
	    string tmp=cur.substr(0,pos);
	    tmp += "(";
	    tmp += cur.substr(pos+6,posdot-pos-6);
	    tmp += ")->";
	    tmp += cur.substr(posdot+1,cur.size()-posdot-1);
	    cur = tmp;
	  }
	}
	if (ch=='e' && pos+4<int(cur.size()) && cur.substr(pos,3)=="end" && instruction_at(cur,pos,3)){
	  // if next char after end is =, replace by endl
	  for (size_t tmp=pos+3;tmp<cur.size();++tmp){
	    if (cur[tmp]!=' '){
	      if (cur[tmp]=='=')
		cur.insert(cur.begin()+pos+3,'l');
	      break;
	    }
	  }
	}
      }
      if (instring){
	*logptr(contextptr) << "Warning: multi-line strings can not be converted from Python like syntax"<<"\n";
	return s_orig;
      }
      // detect : at end of line
      for (pos=int(cur.size())-1;pos>=0;--pos){
	if (cur[pos]!=' ' && cur[pos]!=char(9))
	  break;
      }
      if (pos<0){ 
	s+='\n';  
	continue;
      }
      if (cur[pos]!=':'){ // detect oneliner and function/fonction
	int p;
	for (p=0;p<pos;++p){
	  if (cur[p]!=' ')
	    break;
	}
	if (p<pos-8 && (cur.substr(p,8)=="function" || cur.substr(p,8)=="fonction")){
	  s += cur+'\n';
	  continue;
	}
	bool instr=false;
	for (p=pos;p>0;--p){
	  if (instr){
	    if (cur[p]=='"' && cur[p-1]!='\\')
	      instr=false;
	    continue;
	  }
	  if (cur[p]==':' && cur[p+1]!=';')
	    break;
	  if (cur[p]=='"' && cur[p-1]!='\\')
	    instr=true;	  
	}
	if (p==0){
	  // = or return expr if cond else alt_expr => ifte(cond,expr,alt_expr)
	  int cs=int(cur.size());
	  int elsepos=cur.find("else");
	  if (elsepos>0 && elsepos<cs){
	    int ifpos=cur.find("if");
	    if (ifpos>0 && ifpos<elsepos){
	      int retpos=cur.find("return"),endretpos=retpos+6;
	      if (retpos<0 || retpos>=cs){
		retpos=cur.find("=");
		endretpos=retpos+1;
	      }
	      if (retpos>=0 && retpos<ifpos){
		cur=cur.substr(0,endretpos)+" ifte("+cur.substr(ifpos+2,elsepos-ifpos-2)+","+cur.substr(endretpos,ifpos-endretpos)+","+cur.substr(elsepos+4,cs-elsepos-4)+")";
	      }
	    }
	  }
	}
	if (p>0){
	  int cs=int(cur.size()),q=4;
	  int progpos=cur.find("elif");;
	  if (progpos<0 || progpos>=cs){
	    progpos=cur.find("if");
	    q=2;
	  }
	  if (p && progpos>=0 && progpos<cs && instruction_at(cur,progpos,q)){
	    pythonmode=true;
	    string tmp=cur.substr(0,p);
#if 1
	    tmp += ":\n";
	    tmp += string(progpos+4,' ');
	    tmp += cur.substr(p+1,pos-p);
	    tmp += '\n';
	    res= tmp +res;
	    continue;
#else
	    tmp += " then ";
	    tmp += cur.substr(p+1,pos-p);
	    cur = tmp;
	    convert_python(cur,contextptr);
	    // no fi if there is an else or elif
	    for (p=0;p<int(res.size());++p){
	      if (res[p]!=' ' && res[p]!=char(9))
		break;
	    }
	    if (p<res.size()+5 && (res.substr(p,4)=="else" || res.substr(p,4)=="elif"))
	      cur += " ";
	    else
	      cur += " fi";
	    p=0;
#endif
	  }
	  progpos=cur.find("else");
	  if (p && progpos>=0 && progpos<cs && instruction_at(cur,progpos,4)){
	    pythonmode=true;
	    string tmp=cur.substr(0,p);
#if 1
	    tmp += ":\n";
	    tmp += string(progpos+4,' ');
	    tmp += cur.substr(p+1,pos-p);
	    tmp += '\n';
	    res= tmp +res;
	    continue;
#else
	    tmp += ' ';
	    tmp += cur.substr(p+1,pos-p);
	    tmp += " fi";
	    cur = tmp;
	    convert_python(cur,contextptr);
	    p=0;
#endif
	  }
	  progpos=cur.find("for");
	  if (p && progpos>=0 && progpos<cs && instruction_at(cur,progpos,3)){
	    pythonmode=true;
	    string tmp=cur.substr(0,p);
	    tmp += " do ";
	    tmp += cur.substr(p+1,pos-p);
	    cur= tmp;
	    convert_python(cur,contextptr);
	    cur += " od";
	    p=0;
	  }
	  progpos=cur.find("while");
	  if (p && progpos>=0 && progpos<cs && instruction_at(cur,progpos,5)){
	    pythonmode=true;
	    string tmp=cur.substr(0,p);
	    tmp += " do ";
	    tmp += cur.substr(p+1,pos-p);
	    cur = tmp;
	    convert_python(cur,contextptr);
	    cur += " od";
	    p=0;
	  }
	}
      }
      // count whitespaces, compare to stack
      int ws=0;
      int cs=cur.size();
      for (ws=0;ws<cs;++ws){
	if (cur[ws]!=' ' && cur[ws]!=char(9))
	  break;
      }
      if (cur[pos]==':'){
	// detect else or elif or except
	int progpos=cur.find("else");
	if (progpos>=0 && progpos<cs && instruction_at(cur,progpos,4)){
	  pythonmode=true;
	  if (stack.size()>1){ 
	    int indent=stack[stack.size()-1].decal;
	    if (ws<indent){
	      // remove last \n and add explicit endbloc delimiters from stack
	      int ss=s.size();
	      bool nl= ss && s[ss-1]=='\n';
	      if (nl)
		s=s.substr(0,ss-1);
	      while (stack.size()>1 && stack[stack.size()-1].decal>ws){
		string tmp=" ";
		tmp += stack.back().endbloc;
		tmp += ';';
		s += tmp ;
		stack.pop_back();
	      }
	      if (nl)
		s += '\n';
	    }
	  }
	  s += cur.substr(0,pos);
	  s += "\n";
	  continue;
	}
	progpos=cur.find("except");
	if (progpos>=0 && progpos<cs && instruction_at(cur,progpos,6)){
	  pythonmode=true;
	  if (stack.size()>1){ 
	    int indent=stack[stack.size()-1].decal;
	    if (ws<indent){
	      // remove last \n and add explicit endbloc delimiters from stack
	      int ss=s.size();
	      bool nl= ss && s[ss-1]=='\n';
	      if (nl)
		s=s.substr(0,ss-1);
	      while (stack.size()>1 && stack[stack.size()-1].decal>ws){
		string tmp=" ";
		tmp += stack.back().endbloc;
		tmp += ';';
		s += tmp;
		stack.pop_back();
	      }
	      if (nl)
		s += '\n';
	    }
	  }
	  s += cur.substr(0,progpos)+"then\n";
	  continue;
	}
	progpos=cur.find("elif");
	if (progpos>=0 && progpos<cs && instruction_at(cur,progpos,4)){
	  pythonmode=true;
	  if (stack.size()>1){ 
	    int indent=stack[stack.size()-1].decal;
	    if (ws<indent){
	      // remove last \n and add explicit endbloc delimiters from stack
	      int ss=s.size();
	      bool nl= ss && s[ss-1]=='\n';
	      if (nl)
		s=s.substr(0,ss-1);
	      while (stack.size()>1 && stack[stack.size()-1].decal>ws){
		string tmp=" ";
		tmp += stack.back().endbloc;
		tmp += ';';
		s += tmp;
		stack.pop_back();
	      }
	      if (nl)
		s += '\n';
	    }
	  }
	  cur=cur.substr(0,pos);
	  convert_python(cur,contextptr);
	  s += cur;
	  s += " then\n";
	  continue;
	}
      }
      if (!stack.empty()){ 
	int indent=stack.back().decal;
	if (ws<=indent){
	  // remove last \n and add explicit endbloc delimiters from stack
	  int ss=s.size();
	  bool nl= ss && s[ss-1]=='\n';
	  if (nl)
	    s=s.substr(0,ss-1);
	  while (!stack.empty() && stack.back().decal>=ws){
	    int sb=stack.back().decal;
	    string tmp=" ";
	    tmp += stack.back().endbloc;
	    tmp += ';';
	    s += tmp;
	    stack.pop_back();
	    // indent must match one of the saved indent
	    if (sb!=ws && !stack.empty() && stack.back().decal<ws){
	      return "\"Bad indentation at "+cur+"\"";
	    }
	  }
	  if (nl)
	    s += '\n';
	}
      }
      if (cur[pos]==':'){
	// detect matching programming structure
	int progpos=cur.find("if");
	if (progpos>=0 && progpos<cs && instruction_at(cur,progpos,2)){
	  pythonmode=true;
	  cur=cur.substr(0,pos);
	  convert_python(cur,contextptr);
	  s += cur;
	  s += " then\n";
	  stack.push_back(int_string(ws,"fi"));
	  continue;
	}
	progpos=cur.find("try");
	if (progpos>=0 && progpos<cs && instruction_at(cur,progpos,3)){
	  pythonmode=true;
	  cur=cur.substr(0,progpos);
	  convert_python(cur,contextptr);
	  s += cur;
	  s += "IFERR\n";
	  stack.push_back(int_string(ws,"end"));
	  continue;
	}
	progpos=cur.find("for");
	if (progpos>=0 && progpos<cs && instruction_at(cur,progpos,3)){
	  pythonmode=true;
	  // for _ -> for x_
	  cur=cur.substr(0,pos);
	  if (progpos+5<cs && cur[progpos+3]==' ' && cur[progpos+4]=='_' && cur[progpos+5]==' '){
	    cur.insert(cur.begin()+progpos+4,'x');
	  }
	  convert_python(cur,contextptr);
	  s += cur;
	  s += " do\n";
	  stack.push_back(int_string(ws,"od"));
	  continue;
	}
	progpos=cur.find("while");
	if (progpos>=0 && progpos<cs && instruction_at(cur,progpos,5)){
	  pythonmode=true;
	  cur=cur.substr(0,pos);
	  convert_python(cur,contextptr);
	  s += cur;
	  s += " do\n";
	  stack.push_back(int_string(ws,"od"));
	  continue;
	}
	progpos=cur.find("def");
	if (progpos>=0 && progpos<cs && instruction_at(cur,progpos,3)){
	  pythonmode=true;
	  //python_compat(1,contextptr); 
	  pythoncompat=true;
	  // should remove possible returned type, between -> ... and :
	  string entete=cur.substr(progpos+3,pos-progpos-3);
	  int posfleche=entete.find("->");
	  if (posfleche>0 || posfleche<entete.size())
	    entete=entete.substr(0,posfleche);
	  s += cur.substr(0,progpos);
	  s +="function";
	  s +=entete;
	  s +="\n";
	  stack.push_back(int_string(ws,"ffunction:")); // ; added later
	  continue;
	}
	// no match found, return s
	s += cur;
      }
      else {
	// normal line add ; at end
	char curpos=cur[pos];
	if (pythonmode && !res.empty() && pos>=0 && curpos!=';' && curpos!=',' && curpos!='{' && curpos!='(' && curpos!='[' && curpos!=':' && curpos!='+' && curpos!='-' && curpos!='*' && curpos!='/' && curpos!='%')
	  cur += ';';
	if (pythonmode)
	  convert_python(cur,contextptr);
	cur += '\n';
	s += cur;
      }
    }
    while (!stack.empty()){
      string tmp(" ");
      tmp += stack.back().endbloc;
      tmp += ';';
      s += tmp;
      stack.pop_back();
    }
    if (pythonmode){
      char ch;
      while ((ch=s[s.size()-1])==';'  || (ch=='\n'))
	s=s.substr(0,s.size()-1);
      // replace ;) by )
      for (int i=s.size()-1;i>=2;--i){
	if (s[i]==')' && s[i-1]=='\n' && s[i-2]==';'){
	  s.erase(s.begin()+i-2);
	  break;
	}
      }
      if (s.size()>10 && s.substr(s.size()-9,9)=="ffunction")
	s += ":;";
      else {
	int pos=s.find('\n');
	if (pos>=0 && pos<s.size())
	  s += ";";
      }
      if (debug_infolevel)
	*logptr(contextptr) << "Translated to Xcas as:\n" << s << "\n";
    }
    res.clear(); cur.clear();
    return string(s.begin(),s.end());
  }
  
  /* END PYTHON */

  // optional, call it just before exiting
  int release_globals(){
#ifndef VISUALC
    delete normal_sin_pi_12_ptr_();
    delete normal_cos_pi_12_ptr_();
#endif
#ifndef STATIC_BUILTIN_LEXER_FUNCTIONS
    if (debug_infolevel)
      CERR << "releasing " << builtin_lexer_functions_number << " functions" << "\n";
    for (int i=0;i<builtin_lexer_functions_number;++i){
#ifdef SMARTPTR64
      if (debug_infolevel)
	CERR << builtin_lexer_functions_begin()[i].first << "\n"; 
      builtin_lexer_functions_begin()[i].second=0;
      //delete (ref_unary_function_ptr *) (* ((ulonglong * ) &builtin_lexer_functions_begin()[i].second) >> 16);
#endif
    }
#endif
    delete &syms();
    //delete &usual_units();
    delete &symbolic_rootof_list();
    delete &proot_list();
    delete &galoisconj_list();
    delete &_lastprog_name_();
    return 0;
  }

#ifndef NO_NAMESPACE_GIAC
} // namespace giac
#endif // ndef NO_NAMESPACE_GIAC
