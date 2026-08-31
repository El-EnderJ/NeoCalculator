/*
 * Modified for NumOS / NeoCalculator in 2026 for ESP32-S3 and host integration.
 * See ../NUMOS_CHANGES.md for provenance and the file-by-file history.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

 /* -*- mode: C++; compile-command: "flex input_lexer.ll && g++ -g -I.. -c input_lexer.cc" -*- */
/** @file input_lexer.h
 *
 *  Lexical analyzer definition for reading expressions.
 *  input_lexer.ll must be processed with flex. */

/*
 *  Original version by GiNaC 
 *  Copyright (C) 1999-2000 Johannes Gutenberg University Mainz, Germany
 *  Modified for Giac (c) 2001,2014 Bernard Parisse, Institut Fourier
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

#ifndef __GIAC_INPUT_LEXER_H__
#define __GIAC_INPUT_LEXER_H__
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "first.h"

extern "C" {
#include <stdio.h>
}

#include "global.h"
#include <string>
#include <map>
#include "help.h"
// yacc stack type
#ifndef YYSTYPE
#define YYSTYPE gen
#endif
#define YY_EXTRA_TYPE  context const *
typedef struct yy_buffer_state *YY_BUFFER_STATE;

// lex functions/variables
extern int giac_yyerror(void *scanner,const char *s);
extern int giac_yylex(giac::YYSTYPE * yylval_param ,void * yyscanner);

#ifndef NO_NAMESPACE_GIAC
namespace giac {
#endif // ndef NO_NAMESPACE_GIAC
  

    // gen alias for static initialization on 32 bits processor
  struct charptr_gen_unary {
    const char * s;
    size_t _FUNC_; // unary_function_ptr *
    signed char subtype;
    unsigned char type;  // see dispatch.h
  };
  extern const charptr_gen_unary builtin_lexer_functions[] ;
  extern const unsigned builtin_lexer_functions_number;
  //std::vector<size_t> * builtin_lexer_functions_();

  /* integer values */
  struct lexer_tab_int_type {
    const char * keyword;
    unsigned char status;
    int value;
    signed char subtype;
    short int return_value;
  };
  extern const lexer_tab_int_type lexer_tab_int_values[];
  extern const lexer_tab_int_type * const lexer_tab_int_values_begin;
  extern const lexer_tab_int_type * const lexer_tab_int_values_end;
  std::string translate_at(const char * ch);

  inline bool tri(const std::pair<const char *,gen> & a ,const std::pair<const char *,gen> & b){
    return strcmp(a.first, b.first) < 0;
  }
  bool tri1(const lexer_tab_int_type & a,const lexer_tab_int_type & b);
 
  typedef std::pair<const char *,gen> charptr_gen;
  charptr_gen * builtin_lexer_functions_begin();
  charptr_gen * builtin_lexer_functions_end();

  // return true/false to tell if s is recognized. return the appropriate gen if true
  // bool CasIsBuildInFunction(char const *s, gen &g);

  int lock_syms_mutex();
  void unlock_syms_mutex();
  sym_string_tab & syms();
  // The lexer recognize first static declared symbols as declared in
  // input_lexer.ll, then it does 2 steps:
  // step 1: look in lexer_translator if the name is a recognized
  // function name, if so translate to the real giac function name
  // step 2: look in syms for an identifier, if no exists with this
  // name make one
  // lexer_functions is the table of all used symbols with real giac names. 
  // The subtype of the gen is used
  // to keep the parser token returned by the lexer

  
  struct unary_function_ptr;
  // Return true if s is associated to a function with non prefix syntax
  bool has_special_syntax(const char * s);
  bool lexer_functions_register(const unary_function_ptr & u,const char * s,int parser_token);
  inline bool lexer_functions_register(const unary_function_ptr * u,const char * s,int parser_token){ return lexer_functions_register(*u,s,parser_token); }
  bool lexer_function_remove(const std::vector<user_function> & v);

  // return the token associated to the string, T_SYMBOL if not found
  int find_or_make_symbol(const std::string & s,gen & res,void * scanner,bool check38,GIAC_CONTEXT);
  
  /** Add to the list of predefined symbols for the lexer. */
  void set_lexer_symbols(const vecteur & l,GIAC_CONTEXT);
  
  /** Set the input string to be parsed by giac_yyparse() (used internally). */
  YY_BUFFER_STATE set_lexer_string(const std::string &s,void * & scanner,const context * contextptr,int maxsize=0);
  int delete_lexer_string(YY_BUFFER_STATE &state,void * & scanner);
  
  /** Get error message from the parser. */
  std::string get_parser_error(void);

#ifndef NO_NAMESPACE_GIAC
} // namespace giac
#endif // ndef NO_NAMESPACE_GIAC

#endif // ndef __GIAC_INPUT_LEXER_H__
