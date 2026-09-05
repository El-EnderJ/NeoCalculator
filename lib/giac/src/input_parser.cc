/*
 * Modified for NumOS / NeoCalculator in 2026 for ESP32-S3 and host integration.
 * See ../NUMOS_CHANGES.md for provenance and the file-by-file history.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.0.4"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1


/* Substitute the variable and function names.  */
#define yyparse         giac_yyparse
#define yylex           giac_yylex
#define yyerror         giac_yyerror
#define yydebug         giac_yydebug
#define yynerrs         giac_yynerrs


/* Copy the first part of user declarations.  */
#line 27 "input_parser.yy" /* yacc.c:339  */

         #define YYPARSE_PARAM scanner
         #define YYLEX_PARAM   scanner
	 
#line 36 "input_parser.yy" /* yacc.c:339  */

#include "giacPCH.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "first.h"
#include <stdexcept>
#include <cstdlib>
#include "giacPCH.h"
#include "index.h"
#include "gen.h"
#define YYSTYPE gen
#define YY_EXTRA_TYPE  context const *
#include "lexer.h"
#include "input_lexer.h"
#include "usual.h"
#include "derive.h"
#include "sym2poly.h"
#include "vecteur.h"
#include "modpoly.h"
#include "alg_ext.h"
#include "prog.h"
#include "rpn.h"
#include "intg.h"
#include "plot.h"
#include "maple.h"
using namespace std;

#ifndef NO_NAMESPACE_GIAC
namespace giac {
#endif // ndef NO_NAMESPACE_GIAC

// It seems there is a bison bug when it reallocates space for the stack
// therefore I redefine YYINITDEPTH to 4000 (max size is YYMAXDEPTH)
// instead of 200
// Feel free to change if you need but then readjust YYMAXDEPTH
#define YYINITDEPTH 100
#define YYMAXDEPTH 101


gen polynome_or_sparse_poly1(const gen & coeff, const gen & index){
  if (index.type==_VECT){
    index_t i;
    const_iterateur it=index._VECTptr->begin(),itend=index._VECTptr->end();
    i.reserve(itend-it);
    for (;it!=itend;++it){
      if (it->type!=_INT_)
         return gentypeerr();
      i.push_back(it->val);
    }
    monomial<gen> m(coeff,i);
    return polynome(m);
  }
  else {
    sparse_poly1 res;
    res.push_back(monome(coeff,index));
    return res;
  }
}

#line 138 "y.tab.c" /* yacc.c:339  */

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* In a future release of Bison, this section will be replaced
   by #include "y.tab.h".  */
#ifndef YY_GIAC_YY_Y_TAB_H_INCLUDED
# define YY_GIAC_YY_Y_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int giac_yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    T_NUMBER = 258,
    T_SYMBOL = 259,
    T_LITERAL = 260,
    T_DIGITS = 261,
    T_STRING = 262,
    T_END_INPUT = 263,
    T_EXPRESSION = 264,
    T_UNARY_OP = 265,
    T_OF = 266,
    T_NOT = 267,
    T_TYPE_ID = 268,
    T_VIRGULE = 269,
    T_AFFECT = 270,
    T_MAPSTO = 271,
    T_BEGIN_PAR = 272,
    T_END_PAR = 273,
    T_PLUS = 274,
    T_MOINS = 275,
    T_DIV = 276,
    T_MOD = 277,
    T_POW = 278,
    T_QUOTED_BINARY = 279,
    T_QUOTE = 280,
    T_PRIME = 281,
    T_TEST_EQUAL = 282,
    T_EQUAL = 283,
    T_INTERVAL = 284,
    T_UNION = 285,
    T_INTERSECT = 286,
    T_MINUS = 287,
    T_AND_OP = 288,
    T_COMPOSE = 289,
    T_DOLLAR = 290,
    T_DOLLAR_MAPLE = 291,
    T_INDEX_BEGIN = 292,
    T_VECT_BEGIN = 293,
    T_VECT_DISPATCH = 294,
    T_VECT_END = 295,
    T_SEMI = 296,
    T_DEUXPOINTS = 297,
    T_DOUBLE_DEUX_POINTS = 298,
    T_IF = 299,
    T_ELIF = 300,
    T_THEN = 301,
    T_ELSE = 302,
    T_IFTE = 303,
    T_SWITCH = 304,
    T_CASE = 305,
    T_DEFAULT = 306,
    T_ENDCASE = 307,
    T_FOR = 308,
    T_FROM = 309,
    T_TO = 310,
    T_DO = 311,
    T_BY = 312,
    T_WHILE = 313,
    T_MUPMAP_WHILE = 314,
    T_REPEAT = 315,
    T_UNTIL = 316,
    T_IN = 317,
    T_START = 318,
    T_BREAK = 319,
    T_CONTINUE = 320,
    T_TRY = 321,
    T_CATCH = 322,
    T_TRY_CATCH = 323,
    T_PROC = 324,
    T_BLOC = 325,
    T_BLOC_BEGIN = 326,
    T_BLOC_END = 327,
    T_RETURN = 328,
    T_LOCAL = 329,
    T_ARGS = 330,
    T_FACTORIAL = 331,
    T_ROOTOF_BEGIN = 332,
    T_ROOTOF_END = 333,
    T_SPOLY1_BEGIN = 334,
    T_SPOLY1_END = 335,
    T_HELP = 336,
    TI_STO = 337,
    T_PIPE = 338,
    TI_HASH = 339,
    T_INTERROGATION = 340,
    T_UNIT = 341,
    T_SQ = 342,
    T_IFERR = 343,
    T_UNARY_OP_38 = 344,
    T_FUNCTION = 345,
    T_LOGO = 346,
    T_BIDON = 347,
    T_IMPMULT = 348
  };
#endif
/* Tokens.  */
#define T_NUMBER 258
#define T_SYMBOL 259
#define T_LITERAL 260
#define T_DIGITS 261
#define T_STRING 262
#define T_END_INPUT 263
#define T_EXPRESSION 264
#define T_UNARY_OP 265
#define T_OF 266
#define T_NOT 267
#define T_TYPE_ID 268
#define T_VIRGULE 269
#define T_AFFECT 270
#define T_MAPSTO 271
#define T_BEGIN_PAR 272
#define T_END_PAR 273
#define T_PLUS 274
#define T_MOINS 275
#define T_DIV 276
#define T_MOD 277
#define T_POW 278
#define T_QUOTED_BINARY 279
#define T_QUOTE 280
#define T_PRIME 281
#define T_TEST_EQUAL 282
#define T_EQUAL 283
#define T_INTERVAL 284
#define T_UNION 285
#define T_INTERSECT 286
#define T_MINUS 287
#define T_AND_OP 288
#define T_COMPOSE 289
#define T_DOLLAR 290
#define T_DOLLAR_MAPLE 291
#define T_INDEX_BEGIN 292
#define T_VECT_BEGIN 293
#define T_VECT_DISPATCH 294
#define T_VECT_END 295
#define T_SEMI 296
#define T_DEUXPOINTS 297
#define T_DOUBLE_DEUX_POINTS 298
#define T_IF 299
#define T_ELIF 300
#define T_THEN 301
#define T_ELSE 302
#define T_IFTE 303
#define T_SWITCH 304
#define T_CASE 305
#define T_DEFAULT 306
#define T_ENDCASE 307
#define T_FOR 308
#define T_FROM 309
#define T_TO 310
#define T_DO 311
#define T_BY 312
#define T_WHILE 313
#define T_MUPMAP_WHILE 314
#define T_REPEAT 315
#define T_UNTIL 316
#define T_IN 317
#define T_START 318
#define T_BREAK 319
#define T_CONTINUE 320
#define T_TRY 321
#define T_CATCH 322
#define T_TRY_CATCH 323
#define T_PROC 324
#define T_BLOC 325
#define T_BLOC_BEGIN 326
#define T_BLOC_END 327
#define T_RETURN 328
#define T_LOCAL 329
#define T_ARGS 330
#define T_FACTORIAL 331
#define T_ROOTOF_BEGIN 332
#define T_ROOTOF_END 333
#define T_SPOLY1_BEGIN 334
#define T_SPOLY1_END 335
#define T_HELP 336
#define TI_STO 337
#define T_PIPE 338
#define TI_HASH 339
#define T_INTERROGATION 340
#define T_UNIT 341
#define T_SQ 342
#define T_IFERR 343
#define T_UNARY_OP_38 344
#define T_FUNCTION 345
#define T_LOGO 346
#define T_BIDON 347
#define T_IMPMULT 348

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif



int giac_yyparse (void * scanner);

#endif /* !YY_GIAC_YY_Y_TAB_H_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 374 "y.tab.c" /* yacc.c:358  */

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef YY_ATTRIBUTE
# if (defined __GNUC__                                               \
      && (2 < __GNUC__ || (__GNUC__ == 2 && 96 <= __GNUC_MINOR__)))  \
     || defined __SUNPRO_C && 0x5110 <= __SUNPRO_C
#  define YY_ATTRIBUTE(Spec) __attribute__(Spec)
# else
#  define YY_ATTRIBUTE(Spec) /* empty */
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# define YY_ATTRIBUTE_PURE   YY_ATTRIBUTE ((__pure__))
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# define YY_ATTRIBUTE_UNUSED YY_ATTRIBUTE ((__unused__))
#endif

#if !defined _Noreturn \
     && (!defined __STDC_VERSION__ || __STDC_VERSION__ < 201112)
# if defined _MSC_VER && 1200 <= _MSC_VER
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn YY_ATTRIBUTE ((__noreturn__))
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif


#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  111
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   7934

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  94
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  25
/* YYNRULES -- Number of rules.  */
#define YYNRULES  203
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  443

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   348

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   171,   171,   179,   180,   181,   184,   185,   186,   187,
     188,   189,   191,   192,   195,   196,   197,   198,   202,   206,
     207,   208,   209,   210,   211,   212,   213,   214,   215,   216,
     217,   218,   219,   220,   221,   222,   223,   224,   225,   231,
     233,   234,   235,   236,   237,   238,   239,   241,   242,   243,
     252,   253,   258,   259,   260,   261,   262,   263,   264,   273,
     278,   282,   289,   292,   293,   294,   295,   296,   299,   300,
     301,   305,   312,   313,   314,   315,   316,   318,   320,   321,
     323,   324,   325,   343,   348,   361,   362,   367,   373,   377,
     381,   385,   386,   387,   388,   389,   390,   391,   393,   394,
     395,   396,   397,   398,   401,   402,   407,   411,   415,   416,
     438,   444,   450,   451,   452,   457,   461,   462,   471,   474,
     479,   484,   489,   494,   499,   504,   509,   514,   515,   516,
     517,   518,   519,   520,   521,   525,   528,   532,   533,   534,
     535,   536,   537,   540,   541,   542,   545,   546,   551,   552,
     553,   554,   555,   556,   564,   569,   572,   573,   576,   577,
     581,   584,   585,   592,   593,   594,   595,   596,   597,   598,
     599,   607,   617,   618,   621,   622,   625,   627,   632,   636,
     637,   640,   641,   642,   643,   646,   647,   650,   651,   652,
     655,   658,   665,   666,   670,   675,   678,   681,   682,   683,
     686,   687,   688,   691
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "T_NUMBER", "T_SYMBOL", "T_LITERAL",
  "T_DIGITS", "T_STRING", "T_END_INPUT", "T_EXPRESSION", "T_UNARY_OP",
  "T_OF", "T_NOT", "T_TYPE_ID", "T_VIRGULE", "T_AFFECT", "T_MAPSTO",
  "T_BEGIN_PAR", "T_END_PAR", "T_PLUS", "T_MOINS", "T_DIV", "T_MOD",
  "T_POW", "T_QUOTED_BINARY", "T_QUOTE", "T_PRIME", "T_TEST_EQUAL",
  "T_EQUAL", "T_INTERVAL", "T_UNION", "T_INTERSECT", "T_MINUS", "T_AND_OP",
  "T_COMPOSE", "T_DOLLAR", "T_DOLLAR_MAPLE", "T_INDEX_BEGIN",
  "T_VECT_BEGIN", "T_VECT_DISPATCH", "T_VECT_END", "T_SEMI",
  "T_DEUXPOINTS", "T_DOUBLE_DEUX_POINTS", "T_IF", "T_ELIF", "T_THEN",
  "T_ELSE", "T_IFTE", "T_SWITCH", "T_CASE", "T_DEFAULT", "T_ENDCASE",
  "T_FOR", "T_FROM", "T_TO", "T_DO", "T_BY", "T_WHILE", "T_MUPMAP_WHILE",
  "T_REPEAT", "T_UNTIL", "T_IN", "T_START", "T_BREAK", "T_CONTINUE",
  "T_TRY", "T_CATCH", "T_TRY_CATCH", "T_PROC", "T_BLOC", "T_BLOC_BEGIN",
  "T_BLOC_END", "T_RETURN", "T_LOCAL", "T_ARGS", "T_FACTORIAL",
  "T_ROOTOF_BEGIN", "T_ROOTOF_END", "T_SPOLY1_BEGIN", "T_SPOLY1_END",
  "T_HELP", "TI_STO", "T_PIPE", "TI_HASH", "T_INTERROGATION", "T_UNIT",
  "T_SQ", "T_IFERR", "T_UNARY_OP_38", "T_FUNCTION", "T_LOGO", "T_BIDON",
  "T_IMPMULT", "$accept", "input", "correct_input", "exp", "symbol_for",
  "symbol", "symbol_or_literal", "entete", "local", "suite_symbol",
  "affectable_symbol", "exp_or_empty", "suite", "prg_suite", "step",
  "from", "loop38_do", "else", "bloc", "elif", "ti_bloc_end", "ti_else",
  "switch", "case", "semi", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
     345,   346,   347,   348
};
# endif

#define YYPACT_NINF -303

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-303)))

#define YYTABLE_NINF -202

#define yytable_value_is_error(Yytable_value) \
  (!!((Yytable_value) == (-202)))

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    4303,  -303,    39,   -10,  -303,    15,  -303,  -303,    47,  -303,
    4303,    72,  4303,  4303,  -303,  4392,  4303,  3146,  -303,    82,
    4303,  3235,    90,  4481,    87,    94,  4570,    70,  4659,  4303,
    -303,  -303,    66,   130,   149,   131,   743,    53,  4303,  4303,
    4303,  4303,  4303,  4303,   132,  5015,   156,  -303,  5097,   146,
    -303,  -303,   151,   165,   -20,    21,  4303,  3324,  3413,  4303,
    5363,  -303,  5399,  5363,    10,  1010,  5437,  7582,  4303,  5178,
    -303,  7764,  5475,   133,  -303,  4303,  5551,  4303,  4303,  4748,
    5134,   127,    26,  -303,    72,  3502,  -303,   110,    -6,  4303,
    5588,  7173,  1455,  3591,   114,  4303,  3680,   168,  4303,  7173,
    4303,  4303,  5664,  7211,  7173,    54,  7840,  1544,  4303,  3769,
    7582,  -303,  -303,   124,  4303,  4303,  3858,  3680,  4303,  4303,
    4303,  4303,  -303,  4303,  4303,   476,  4303,  4303,  4303,  4303,
    4303,  4303,  4303,  4837,  3947,  4303,  4303,  -303,   161,  4303,
    4303,  4303,  -303,  3680,  4303,  -303,   187,  -303,  -303,  -303,
    -303,  4303,  -303,  7325,  -303,  5701,  -303,  5777,  5814,   174,
    -303,   565,  -303,  7473,   203,  -303,  5890,  3858,  5927,  6003,
      29,   191,  4303,   144,  6040,   158,  4303,  4303,  4303,  4303,
     142,  6116,  4303,  -303,  4303,  7173,  -303,  4036,  1633,   184,
    6153,  7173,   192,  3680,  6229,  6266,  6342,  -303,  4303,  4303,
    6379,  -303,  4303,  7473,  7325,  7619,  -303,   193,  6648,  7847,
    5519,   166,  7785,     2,  7764,  5178,   315,   296,  7812,  7681,
      54,  5745,  7764,   -12,  3235,  6455,  -303,  -303,  7582,  7702,
    -303,  -303,  -303,  -303,  -303,  -303,  4303,   105,  7249,  7510,
    7840,   194,  6492,  -303,  6568,  -303,  -303,  -303,  3680,   127,
       9,   162,    72,   203,  -303,     4,  -303,   832,  1099,   167,
    -303,   138,  -303,   159,  1722,  -303,  -303,  3502,  6605,  7173,
    7173,  7173,  4303,   921,  1811,  6681,   203,  -303,  1900,  -303,
    4303,  -303,  -303,   199,  -303,  -303,  -303,  7401,  1188,  -303,
    7702,  -303,  4303,   654,  4303,  5210,  -303,  7840,  3680,  4926,
      -8,   195,  -303,   201,  4303,  4303,  4303,  4303,   204,   203,
    4303,  3680,  6757,   176,  4303,  -303,  -303,  -303,  -303,  4303,
      66,    12,  4303,  7173,   185,  4303,  6794,  6870,  -303,  -303,
    -303,    40,  -303,  6907,  1989,  2078,  -303,  4303,  -303,  7785,
    7702,   188,   207,  3235,  6983,  4125,  -303,   227,  -303,  7173,
    7363,  7287,  7363,  -303,  -303,  5286,  5399,   176,  -303,  3858,
    7021,  2167,  -303,   228,   190,   163,  1277,  4214,  1366,  4303,
     104,  -303,  -303,    66,  4303,  2256,  -303,  4036,  2345,  2434,
    -303,  -303,  5323,  -303,   149,  7325,  -303,  4036,  -303,  -303,
    4303,  -303,  7059,  -303,  4303,  -303,   196,    66,  -303,   191,
    -303,   219,  4303,  -303,  7173,  -303,  -303,  4303,  -303,  2523,
    -303,  4036,  2612,  -303,  -303,   211,  4036,  2701,  7097,  -303,
    1099,    66,  -303,  3858,  2790,  2879,  -303,  2968,  -303,  -303,
    3057,  -303,  -303,  -303,    12,  7135,  -303,  -303,  -303,  -303,
    -303,  -303,  -303
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,   117,     6,   146,    29,    30,    12,    13,    62,    52,
       0,    90,     0,     0,   104,     0,     0,     0,    98,     0,
       0,     0,     0,     0,    69,     0,     0,    85,     0,     0,
      80,    81,     0,   139,     0,    73,     0,    57,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     2,     0,    28,
     156,   157,     0,     0,     7,     0,     0,     0,     0,     0,
      54,   154,     0,    49,    90,     0,     0,    38,     0,    45,
      96,    92,   175,     0,   152,     0,     0,     0,     0,     0,
     200,     0,   146,   144,     0,     0,   145,     0,   181,     0,
       0,   176,     0,     0,     0,     0,     0,     0,     0,    77,
       0,     0,     0,     0,   112,   155,   114,     0,     0,     0,
      74,     1,     3,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    63,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    64,     0,     0,
       0,     0,   116,     0,     0,   153,     0,     9,   149,   148,
     147,     0,   150,    31,    33,     0,    61,     0,     0,   109,
      91,     0,   105,    46,     0,   110,     0,     0,     0,     0,
     146,     0,     0,     0,   173,     0,     0,     0,     0,     0,
       0,     0,     0,   203,     0,   177,   178,     0,     0,     0,
       0,   175,     0,     0,     0,     0,     0,    51,     0,     0,
       0,    76,     0,   111,    53,    71,    70,     0,    39,    40,
      42,    41,    35,    28,    37,    43,    99,   101,   102,    47,
      97,    95,    94,    28,     0,     0,     4,     5,    48,   130,
      34,    20,    23,    21,    22,    24,     0,    19,   103,   113,
     115,     0,     0,     8,     0,    32,    58,    60,     0,   171,
     146,   168,   170,     0,   163,     0,   161,     0,     0,    66,
      68,     0,   141,     0,     0,   142,   129,     0,     0,   182,
     183,   184,     0,     0,     0,    86,     0,   159,     0,   190,
       0,   138,   158,     0,    72,    55,    56,   111,     0,    59,
     131,    27,     0,     0,     0,     0,   106,    25,     0,     0,
      26,    10,   151,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   187,     0,   196,   195,    67,   192,     0,
       0,   197,     0,   173,     0,     0,   179,     0,   134,   136,
      87,     0,   191,     0,     0,     0,    50,     0,    89,    36,
      93,     0,     0,     0,     0,     0,   158,     0,   108,   164,
     165,   166,   169,   167,   162,     0,     0,   187,   118,     0,
       0,     0,    65,     0,     0,     0,     0,     0,     0,     0,
       0,   135,   160,     0,     0,     0,   158,     0,     0,     0,
     107,    16,     0,    17,   158,    15,    14,     0,    11,   132,
       0,   119,     0,   189,     0,   193,     0,     0,   140,    52,
     202,     0,     0,    82,   180,   185,   186,     0,   137,     0,
     120,     0,     0,   122,    88,     0,     0,     0,     0,   188,
       0,     0,   198,     0,     0,     0,   124,     0,   121,    18,
       0,   125,   133,   194,   197,     0,   127,    83,    84,   123,
     126,   199,   128
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -303,  -303,   107,     0,  -303,   -26,  -303,  -189,  -303,   -37,
    -249,  -265,   -90,    62,  -303,  -303,  -303,  -105,   -17,  -166,
    -106,  -302,  -175,  -104,  -303
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,    46,    47,    91,    87,    49,    54,   187,   277,   255,
     256,   175,    73,    92,   370,   180,   407,   358,    94,   317,
     318,   319,   365,   173,   186
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      48,    88,   324,   146,   308,   143,   192,   345,    97,   177,
      60,   359,    62,    63,    61,    66,    67,    69,   309,   143,
      71,    72,   178,    76,   304,   148,    80,   207,    90,   292,
      56,   149,    57,    55,   150,   160,    99,   305,   102,   103,
     104,   105,   106,    50,    51,   110,   151,   262,   179,    52,
     294,   306,    55,   241,   309,   359,   153,   155,   157,   158,
     354,   346,   363,   364,    58,    99,   310,   147,   163,    55,
     100,   117,    55,    81,    82,   166,    61,   168,   169,    62,
      83,   372,    53,    84,    59,   174,    70,    85,  -143,   181,
     101,   133,   185,   334,    74,   190,   191,   213,   194,   206,
     195,   196,   401,   283,    77,   107,   223,   185,   200,    62,
     152,    78,   237,    22,   203,   204,   205,   191,   208,   209,
     210,   211,   298,   212,   214,   215,   216,   217,   218,   219,
     220,   221,   222,   225,    48,   228,   229,    93,   254,   238,
     239,   240,   299,   191,   242,   405,   377,    95,    98,   108,
     259,   244,    81,     3,    41,   188,   111,   387,   303,    86,
     406,    66,    84,   143,    81,     3,    96,   230,   144,   145,
      53,   231,   176,   165,    84,   232,   268,   269,   270,   271,
     233,   189,   234,   117,   275,   193,   202,   411,   185,   121,
     243,   248,    22,   191,   263,   416,   265,   272,   287,   267,
     130,   280,   290,   133,    22,   307,   249,   250,   342,   321,
     282,   291,   300,   251,   320,   322,   252,   335,   347,   348,
     253,   303,   353,   315,   295,   381,   367,   254,   380,   258,
     388,   396,   397,    41,   264,   398,   297,   423,   421,   331,
     313,   227,   137,   235,   274,    41,    22,   236,   191,   278,
     254,   429,   391,   142,   433,   395,   328,   312,   185,   441,
       0,   288,   400,     0,   185,     0,     0,   323,     0,     0,
       0,     0,   326,   327,   185,     0,     0,     0,   185,     0,
     333,     0,     0,   254,     0,     0,     0,    41,   185,     0,
       0,     0,   339,   210,   340,     0,     0,     0,   191,   344,
       0,     0,     0,   362,   349,   350,   351,   352,   113,     0,
     355,   356,     0,   117,   360,   118,     0,   119,   120,   121,
       0,     0,   122,     0,     0,   125,     0,   113,   386,     0,
     130,   131,   117,   133,   118,     0,   119,   293,   121,     0,
       0,   122,   393,   382,   125,   385,   127,   128,     0,   130,
     131,     0,   133,     0,     0,     0,   408,     0,    97,   392,
       0,   185,     0,     0,     0,     0,   185,   323,   185,   404,
       0,     0,   137,     0,     0,   185,     0,     0,   185,   185,
     422,   361,   141,   142,   366,     0,     0,   368,     0,     0,
     418,   137,     0,     0,     0,     0,   375,   378,     0,   379,
       0,   141,   142,     0,   434,     0,   436,     0,     0,   185,
       0,     0,   185,     0,     0,     0,     0,   185,     0,     0,
     185,     0,     0,   435,   185,   185,     0,   185,     0,     0,
     185,     0,     0,     0,     0,     0,   409,     0,     0,   412,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   417,
       0,     0,     0,     0,     0,     0,   420,     0,     0,     0,
       0,     0,     0,     0,   424,     0,     0,     0,     0,   425,
       0,     0,     0,   427,     0,     0,     0,     1,   430,     2,
       3,     4,     5,     6,   -44,     7,     8,     9,    10,    11,
     -44,   -44,   -44,    12,   -44,    13,     0,   -44,   -44,   -44,
      14,    15,   -44,   -44,   -44,   -44,   -44,   -44,   -44,   -44,
      18,    19,   -44,   -44,     0,    21,   -44,   -44,   -44,    22,
      23,   -44,   -44,   -44,    24,    25,    26,   -44,   -44,   -44,
     -44,   -44,   -44,   -44,    28,     0,    29,   -44,   -44,     0,
      30,    31,    32,     0,    33,    34,    35,     0,   -44,   -44,
       0,    37,   -44,    38,   -44,    39,   -44,    40,   -44,   -44,
      41,   -44,    42,   -44,    43,    44,     1,   -44,     2,     3,
       4,     5,     6,   -79,     7,     8,     9,    10,    64,   -79,
     -79,   -79,    12,   -79,    13,     0,   -79,   -79,   -79,    14,
      15,   -79,   -79,    16,    17,   -79,   -79,   -79,   -79,    18,
      19,    20,   -79,     0,    21,   -79,   -79,   -79,    22,    23,
     -79,   -79,   -79,    24,    25,    26,   -79,   -79,    27,   -79,
     -79,   -79,   -79,    28,     0,    29,   -79,   -79,     0,    30,
      31,    32,     0,    33,    34,    35,     0,   -79,    65,     0,
      37,   -79,    38,   -79,    39,   -79,    40,   -79,   -79,    41,
     -79,    42,   -79,    43,    44,     1,    45,  -100,     3,     4,
       5,     6,  -100,     7,     8,     9,    10,    11,  -100,  -100,
    -100,    12,  -100,  -100,     0,  -100,  -100,  -100,    14,    15,
    -100,  -100,  -100,  -100,  -100,  -100,  -100,  -100,    18,    19,
    -100,  -100,     0,    21,  -100,  -100,  -100,    22,    23,  -100,
    -100,  -100,    24,    25,    26,  -100,  -100,  -100,  -100,  -100,
    -100,  -100,    28,     0,    29,  -100,  -100,     0,    30,    31,
      32,     0,    33,    34,    35,     0,  -100,  -100,     0,    37,
    -100,    38,  -100,    39,  -100,    40,  -100,  -100,    41,  -100,
    -100,  -100,    43,    44,     1,  -100,     2,     3,     4,     5,
       6,   -78,     7,     8,     9,    10,    11,   -78,   -78,   -78,
      12,   -78,    13,     0,   -78,   -78,   -78,    14,    15,   -78,
     -78,    16,    17,   -78,   -78,   -78,   -78,    18,    19,    20,
     -78,     0,    21,   -78,   -78,   -78,    22,    23,   -78,   -78,
     -78,    24,    25,    26,   -78,   -78,    27,   -78,   -78,   -78,
     -78,    28,     0,    29,   -78,   -78,     0,    30,    31,    32,
       0,    33,    34,    35,     0,   -78,     0,     0,    37,   -78,
      38,   -78,    39,   -78,    40,   -78,   -78,    41,   -78,    42,
     -78,    43,    44,     1,    45,     2,     3,     4,     5,     6,
       0,     7,     8,     9,    10,    11,  -109,  -109,  -109,   311,
       0,    13,     0,  -109,  -109,  -109,    14,    15,  -109,  -109,
      16,    17,  -109,  -109,  -109,  -109,    18,    19,    20,  -109,
       0,    21,     0,     0,  -109,    22,    23,     0,  -109,     0,
      24,    25,    26,     0,     0,    27,     0,     0,     0,     0,
      28,     0,    29,     0,  -109,     0,    30,    31,    32,     0,
      33,    34,    35,    93,     0,    36,     0,    37,  -109,    38,
       0,    39,     0,    40,  -109,  -109,    41,  -109,    42,  -109,
      43,    44,     1,    45,     2,     3,     4,     5,     6,     0,
       7,     8,     9,    10,    11,  -109,  -109,  -109,   311,     0,
      13,     0,  -109,  -109,  -109,    14,    15,  -109,  -109,    16,
      17,  -109,  -109,  -109,  -109,    18,    19,    20,  -109,     0,
      21,     0,     0,  -109,    22,    23,     0,     0,     0,    24,
      25,    26,     0,     0,    27,     0,     0,  -109,     0,    28,
       0,    29,     0,  -109,     0,    30,    31,    32,     0,    33,
      34,    35,    93,     0,    36,     0,    37,  -109,    38,     0,
      39,     0,    40,  -109,  -109,    41,  -109,    42,  -109,    43,
      44,     1,    45,     2,     3,     4,     5,     6,     0,     7,
       8,     9,    10,    11,   -78,   -78,   -78,    12,     0,    13,
       0,   -78,   -78,   -78,    14,   161,   -78,   -78,    16,    17,
     -78,   -78,   -78,   -78,    18,    19,    20,   -78,     0,    21,
       0,     0,   -78,    22,    23,     0,     0,     0,    24,    25,
      26,     0,     0,    27,     0,     0,     0,     0,    28,     0,
      29,     0,   -78,     0,    30,    31,    32,     0,    33,    34,
      35,     0,     0,    36,     0,    37,   -78,    38,     0,    39,
       0,    40,   -78,   -78,    41,   -78,    42,   -78,    43,    44,
       1,    45,     2,     3,     4,     5,     6,     0,     7,     8,
       9,    10,    11,     0,     0,     0,    12,     0,    13,     0,
       0,     0,     0,    14,    15,     0,     0,    16,    17,     0,
       0,     0,     0,    18,    19,    20,     0,     0,    21,     0,
     183,     0,    22,    23,   314,     0,   315,    24,    25,    26,
       0,     0,    27,     0,     0,     0,     0,    28,     0,    29,
       0,     0,     0,    30,    31,    32,     0,    33,    34,    35,
       0,   316,    36,     0,    37,     0,    38,     0,    39,     0,
      40,     0,     0,    41,     0,    42,     0,    43,    44,     1,
      45,     2,     3,     4,     5,     6,     0,     7,     8,     9,
      10,    11,     0,     0,     0,    12,     0,    13,     0,     0,
       0,     0,    14,    15,     0,     0,    16,    17,     0,     0,
       0,     0,    18,    19,    20,     0,     0,    21,     0,   183,
       0,    22,    23,     0,     0,   337,    24,    25,    26,     0,
       0,    27,     0,     0,     0,     0,    28,     0,    29,     0,
       0,     0,    30,    31,    32,     0,    33,    34,    35,     0,
     338,    36,     0,    37,     0,    38,     0,    39,     0,    40,
       0,     0,    41,     0,    42,     0,    43,    44,     1,    45,
       2,     3,     4,     5,     6,     0,     7,     8,   399,    10,
      11,     0,     0,     0,    12,     0,    13,     0,     0,     0,
       0,    14,    15,     0,     0,    16,    17,     0,     0,     0,
       0,    18,    19,    20,     0,     0,    21,     0,   183,     0,
      22,    23,     0,     0,     0,    24,    25,    26,   172,  -200,
      27,     0,     0,     0,     0,    28,     0,    29,     0,     0,
       0,    30,    31,    32,     0,    33,    34,    35,     0,     0,
      36,     0,    37,     0,    38,     0,    39,     0,    40,     0,
       0,    41,     0,    42,     0,    43,    44,     1,    45,     2,
       3,     4,     5,     6,     0,     7,     8,     9,    10,    11,
       0,     0,     0,    12,     0,    13,     0,     0,     0,     0,
      14,    15,     0,     0,    16,    17,     0,     0,     0,     0,
      18,    19,    20,     0,     0,    21,     0,   183,     0,    22,
      23,     0,     0,   402,    24,    25,    26,     0,     0,    27,
       0,     0,     0,     0,    28,     0,    29,     0,     0,     0,
      30,    31,    32,     0,    33,    34,    35,     0,   403,    36,
       0,    37,     0,    38,     0,    39,     0,    40,     0,     0,
      41,     0,    42,     0,    43,    44,     1,    45,     2,     3,
       4,     5,     6,     0,     7,     8,     9,    10,    11,     0,
       0,     0,    12,     0,    13,     0,     0,     0,     0,    14,
      15,     0,     0,    16,    17,     0,     0,     0,     0,    18,
      19,    20,     0,     0,    21,     0,   183,     0,    22,    23,
       0,     0,     0,    24,    25,    26,     0,     0,    27,     0,
       0,     0,     0,    28,     0,    29,   184,     0,     0,    30,
      31,    32,     0,    33,    34,    35,     0,     0,    36,     0,
      37,     0,    38,     0,    39,     0,    40,     0,     0,    41,
       0,    42,     0,    43,    44,     1,    45,     2,     3,     4,
       5,     6,     0,     7,     8,     9,    10,    11,     0,     0,
       0,    12,     0,    13,     0,     0,     0,     0,    14,    15,
       0,     0,    16,    17,     0,     0,     0,     0,    18,    19,
      20,     0,     0,    21,     0,   183,     0,    22,    23,     0,
     199,     0,    24,    25,    26,     0,     0,    27,     0,     0,
       0,     0,    28,     0,    29,     0,     0,     0,    30,    31,
      32,     0,    33,    34,    35,     0,     0,    36,     0,    37,
       0,    38,     0,    39,     0,    40,     0,     0,    41,     0,
      42,     0,    43,    44,     1,    45,     2,     3,     4,     5,
       6,     0,     7,     8,     9,    10,    11,     0,     0,     0,
      12,     0,    13,     0,     0,     0,     0,    14,    15,     0,
       0,    16,    17,     0,     0,     0,     0,    18,    19,    20,
       0,     0,    21,     0,   183,     0,    22,    23,     0,     0,
       0,    24,    25,    26,     0,     0,    27,     0,     0,     0,
       0,    28,     0,    29,     0,     0,     0,    30,    31,    32,
       0,    33,    34,    35,     0,   279,    36,     0,    37,     0,
      38,     0,    39,     0,    40,     0,     0,    41,     0,    42,
       0,    43,    44,     1,    45,     2,     3,     4,     5,     6,
       0,     7,     8,     9,    10,    11,     0,     0,     0,    12,
       0,    13,     0,     0,     0,     0,    14,    15,     0,     0,
      16,    17,     0,     0,     0,     0,    18,    19,    20,     0,
       0,    21,     0,   183,     0,    22,    23,     0,     0,     0,
      24,    25,    26,     0,  -201,    27,     0,     0,     0,     0,
      28,     0,    29,     0,     0,     0,    30,    31,    32,     0,
      33,    34,    35,     0,     0,    36,     0,    37,     0,    38,
       0,    39,     0,    40,     0,     0,    41,     0,    42,     0,
      43,    44,     1,    45,     2,     3,     4,     5,     6,     0,
       7,     8,     9,    10,    11,     0,     0,     0,    12,     0,
      13,     0,     0,     0,     0,    14,    15,     0,     0,    16,
      17,     0,     0,     0,     0,    18,    19,    20,     0,     0,
      21,     0,   183,     0,    22,    23,     0,     0,     0,    24,
      25,    26,     0,     0,    27,     0,     0,     0,     0,    28,
       0,    29,     0,     0,     0,    30,    31,    32,     0,    33,
      34,    35,     0,   329,    36,     0,    37,     0,    38,     0,
      39,     0,    40,     0,     0,    41,     0,    42,     0,    43,
      44,     1,    45,     2,     3,     4,     5,     6,     0,     7,
       8,     9,    10,    11,     0,     0,     0,    12,     0,    13,
       0,     0,     0,     0,    14,    15,     0,     0,    16,    17,
       0,     0,     0,     0,    18,    19,    20,     0,     0,    21,
       0,   183,     0,    22,    23,     0,     0,     0,    24,    25,
      26,     0,     0,    27,     0,     0,     0,     0,    28,     0,
      29,     0,     0,     0,    30,    31,    32,     0,    33,    34,
      35,     0,   332,    36,     0,    37,     0,    38,     0,    39,
       0,    40,     0,     0,    41,     0,    42,     0,    43,    44,
       1,    45,     2,     3,     4,     5,     6,     0,     7,     8,
       9,    10,    11,     0,     0,     0,    12,     0,    13,     0,
       0,     0,     0,    14,    15,     0,     0,    16,    17,     0,
       0,     0,     0,    18,    19,    20,     0,     0,    21,     0,
       0,     0,    22,    23,     0,     0,     0,    24,    25,    26,
       0,     0,    27,     0,     0,     0,     0,    28,     0,    29,
       0,     0,     0,    30,    31,    32,     0,    33,    34,    35,
     374,     0,    36,   276,    37,     0,    38,     0,    39,     0,
      40,     0,     0,    41,     0,    42,     0,    43,    44,     1,
      45,     2,     3,     4,     5,     6,     0,     7,     8,     9,
      10,    11,     0,     0,     0,    12,     0,    13,     0,     0,
       0,     0,    14,    15,     0,     0,    16,    17,     0,     0,
       0,     0,    18,    19,    20,     0,     0,    21,     0,     0,
       0,    22,    23,     0,     0,     0,    24,    25,    26,     0,
       0,    27,     0,     0,     0,     0,    28,     0,    29,     0,
       0,     0,    30,    31,    32,     0,    33,    34,    35,   376,
       0,    36,  -158,    37,     0,    38,     0,    39,     0,    40,
       0,     0,    41,     0,    42,     0,    43,    44,     1,    45,
       2,     3,     4,     5,     6,     0,     7,     8,     9,    10,
      11,     0,     0,     0,    12,     0,    13,     0,     0,     0,
       0,    14,    15,     0,     0,    16,    17,     0,     0,     0,
       0,    18,    19,    20,     0,     0,    21,     0,   183,     0,
      22,    23,     0,     0,     0,    24,    25,    26,     0,     0,
      27,     0,     0,     0,     0,    28,     0,    29,     0,     0,
       0,    30,    31,    32,     0,    33,    34,    35,     0,   316,
      36,     0,    37,     0,    38,     0,    39,     0,    40,     0,
       0,    41,     0,    42,     0,    43,    44,     1,    45,     2,
       3,     4,     5,     6,     0,     7,     8,     9,    10,    11,
       0,     0,     0,    12,     0,    13,     0,     0,     0,     0,
      14,    15,     0,     0,    16,    17,     0,     0,     0,     0,
      18,    19,    20,     0,     0,    21,     0,   183,     0,    22,
      23,     0,     0,     0,    24,    25,    26,     0,     0,    27,
       0,     0,     0,     0,    28,     0,    29,     0,     0,     0,
      30,    31,    32,     0,    33,    34,    35,     0,   410,    36,
       0,    37,     0,    38,     0,    39,     0,    40,     0,     0,
      41,     0,    42,     0,    43,    44,     1,    45,     2,     3,
       4,     5,     6,     0,     7,     8,     9,    10,    11,     0,
       0,     0,    12,     0,    13,     0,     0,     0,     0,    14,
      15,     0,     0,    16,    17,     0,     0,     0,     0,    18,
      19,    20,     0,     0,    21,     0,   183,     0,    22,    23,
       0,     0,     0,    24,    25,    26,     0,     0,    27,     0,
       0,     0,     0,    28,     0,    29,     0,     0,     0,    30,
      31,    32,     0,    33,    34,    35,     0,   413,    36,     0,
      37,     0,    38,     0,    39,     0,    40,     0,     0,    41,
       0,    42,     0,    43,    44,     1,    45,     2,     3,     4,
       5,     6,     0,     7,     8,     9,    10,    11,     0,     0,
       0,    12,     0,    13,     0,     0,     0,     0,    14,    15,
       0,     0,    16,    17,     0,     0,     0,     0,    18,    19,
      20,     0,     0,    21,     0,   183,     0,    22,    23,     0,
       0,     0,    24,    25,    26,     0,     0,    27,     0,     0,
       0,     0,    28,     0,    29,     0,     0,     0,    30,    31,
      32,     0,    33,    34,    35,     0,   414,    36,     0,    37,
       0,    38,     0,    39,     0,    40,     0,     0,    41,     0,
      42,     0,    43,    44,     1,    45,     2,     3,     4,     5,
       6,     0,     7,     8,     9,    10,    11,     0,     0,     0,
      12,     0,    13,     0,     0,     0,     0,    14,    15,     0,
       0,    16,    17,     0,     0,     0,     0,    18,    19,    20,
       0,     0,    21,     0,   183,     0,    22,    23,     0,     0,
       0,    24,    25,    26,     0,     0,    27,     0,     0,     0,
       0,    28,     0,    29,     0,     0,     0,    30,    31,    32,
       0,    33,    34,    35,     0,   426,    36,     0,    37,     0,
      38,     0,    39,     0,    40,     0,     0,    41,     0,    42,
       0,    43,    44,     1,    45,     2,     3,     4,     5,     6,
       0,     7,     8,     9,    10,    11,     0,     0,     0,    12,
       0,    13,     0,     0,     0,     0,    14,    15,     0,     0,
      16,    17,     0,     0,     0,     0,    18,    19,    20,     0,
       0,    21,     0,   183,     0,    22,    23,     0,     0,     0,
      24,    25,    26,     0,     0,    27,     0,     0,     0,     0,
      28,     0,    29,     0,     0,     0,    30,    31,    32,     0,
      33,    34,    35,     0,   428,    36,     0,    37,     0,    38,
       0,    39,     0,    40,     0,     0,    41,     0,    42,     0,
      43,    44,     1,    45,     2,     3,     4,     5,     6,     0,
       7,     8,     9,    10,    11,     0,     0,     0,    12,     0,
      13,     0,     0,     0,     0,    14,    15,     0,     0,    16,
      17,     0,     0,     0,     0,    18,    19,    20,     0,     0,
      21,     0,   183,     0,    22,    23,     0,     0,     0,    24,
      25,    26,     0,     0,    27,     0,     0,     0,     0,    28,
       0,    29,     0,     0,     0,    30,    31,    32,     0,    33,
      34,    35,     0,   431,    36,     0,    37,     0,    38,     0,
      39,     0,    40,     0,     0,    41,     0,    42,     0,    43,
      44,     1,    45,     2,     3,     4,     5,     6,     0,     7,
       8,     9,    10,    11,     0,     0,     0,    12,     0,    13,
       0,     0,     0,     0,    14,    15,     0,     0,    16,    17,
       0,     0,     0,     0,    18,    19,    20,     0,     0,    21,
       0,   183,     0,    22,    23,     0,     0,     0,    24,    25,
      26,     0,     0,    27,     0,     0,     0,     0,    28,     0,
      29,     0,     0,     0,    30,    31,    32,     0,    33,    34,
      35,     0,   437,    36,     0,    37,     0,    38,     0,    39,
       0,    40,     0,     0,    41,     0,    42,     0,    43,    44,
       1,    45,     2,     3,     4,     5,     6,     0,     7,     8,
       9,    10,    11,     0,     0,     0,    12,     0,    13,     0,
       0,     0,     0,    14,    15,     0,     0,    16,    17,     0,
       0,     0,     0,    18,    19,    20,     0,     0,    21,     0,
     183,     0,    22,    23,     0,     0,     0,    24,    25,    26,
       0,     0,    27,     0,     0,     0,     0,    28,     0,    29,
       0,     0,     0,    30,    31,    32,     0,    33,    34,    35,
       0,   438,    36,     0,    37,     0,    38,     0,    39,     0,
      40,     0,     0,    41,     0,    42,     0,    43,    44,     1,
      45,     2,     3,     4,     5,     6,     0,     7,     8,     9,
      10,    11,     0,     0,     0,    12,     0,    13,     0,     0,
       0,     0,    14,    15,     0,     0,    16,    17,     0,     0,
       0,     0,    18,    19,    20,     0,     0,    21,     0,   183,
       0,    22,    23,     0,     0,     0,    24,    25,    26,     0,
       0,    27,     0,     0,     0,     0,    28,     0,    29,     0,
       0,     0,    30,    31,    32,     0,    33,    34,    35,     0,
     439,    36,     0,    37,     0,    38,     0,    39,     0,    40,
       0,     0,    41,     0,    42,     0,    43,    44,     1,    45,
       2,     3,     4,     5,     6,     0,     7,     8,     9,    10,
      11,     0,     0,     0,    12,     0,    13,     0,     0,     0,
       0,    14,    15,     0,     0,    16,    17,     0,     0,     0,
       0,    18,    19,    20,     0,     0,    21,     0,   183,     0,
      22,    23,     0,     0,     0,    24,    25,    26,     0,     0,
      27,     0,     0,     0,     0,    28,     0,    29,     0,     0,
       0,    30,    31,    32,     0,    33,    34,    35,     0,   440,
      36,     0,    37,     0,    38,     0,    39,     0,    40,     0,
       0,    41,     0,    42,     0,    43,    44,     1,    45,     2,
       3,     4,     5,     6,     0,     7,     8,     9,    10,    11,
      68,     0,     0,    12,     0,    13,     0,     0,     0,     0,
      14,    15,     0,     0,    16,    17,     0,     0,     0,     0,
      18,    19,    20,     0,     0,    21,     0,     0,     0,    22,
      23,     0,     0,     0,    24,    25,    26,     0,     0,    27,
       0,     0,     0,     0,    28,     0,    29,     0,     0,     0,
      30,    31,    32,     0,    33,    34,    35,     0,     0,    36,
       0,    37,     0,    38,     0,    39,     0,    40,     0,     0,
      41,     0,    42,     0,    43,    44,     1,    45,     2,     3,
       4,     5,     6,     0,     7,     8,     9,    10,    11,     0,
       0,     0,    12,     0,    13,     0,     0,     0,     0,    14,
      15,     0,     0,    16,    17,     0,     0,     0,     0,    18,
      19,    20,     0,     0,    21,  -174,     0,     0,    22,    23,
       0,     0,     0,    24,    25,    26,     0,     0,    27,     0,
       0,     0,     0,    28,     0,    29,     0,     0,     0,    30,
      31,    32,     0,    33,    34,    35,     0,     0,    36,     0,
      37,     0,    38,     0,    39,     0,    40,     0,     0,    41,
       0,    42,     0,    43,    44,     1,    45,     2,     3,     4,
       5,     6,     0,     7,     8,     9,    10,    11,     0,     0,
       0,    12,   154,    13,     0,     0,     0,     0,    14,    15,
       0,     0,    16,    17,     0,     0,     0,     0,    18,    19,
      20,     0,     0,    21,     0,     0,     0,    22,    23,     0,
       0,     0,    24,    25,    26,     0,     0,    27,     0,     0,
       0,     0,    28,     0,    29,     0,     0,     0,    30,    31,
      32,     0,    33,    34,    35,     0,     0,    36,     0,    37,
       0,    38,     0,    39,     0,    40,     0,     0,    41,     0,
      42,     0,    43,    44,     1,    45,     2,     3,     4,     5,
       6,     0,     7,     8,     9,    10,    11,     0,     0,     0,
      12,   156,    13,     0,     0,     0,     0,    14,    15,     0,
       0,    16,    17,     0,     0,     0,     0,    18,    19,    20,
       0,     0,    21,     0,     0,     0,    22,    23,     0,     0,
       0,    24,    25,    26,     0,     0,    27,     0,     0,     0,
       0,    28,     0,    29,     0,     0,     0,    30,    31,    32,
       0,    33,    34,    35,     0,     0,    36,     0,    37,     0,
      38,     0,    39,     0,    40,     0,     0,    41,     0,    42,
       0,    43,    44,     1,    45,     2,     3,     4,     5,     6,
       0,     7,     8,     9,    10,    11,     0,     0,     0,    12,
       0,    13,     0,     0,     0,     0,    14,    15,     0,     0,
      16,    17,     0,     0,     0,     0,    18,    19,    20,     0,
       0,    21,     0,  -172,     0,    22,    23,     0,     0,     0,
      24,    25,    26,     0,     0,    27,     0,     0,     0,     0,
      28,     0,    29,     0,     0,     0,    30,    31,    32,     0,
      33,    34,    35,     0,     0,    36,     0,    37,     0,    38,
       0,    39,     0,    40,     0,     0,    41,     0,    42,     0,
      43,    44,     1,    45,     2,     3,     4,     5,     6,     0,
       7,     8,     9,    10,    11,     0,     0,     0,    12,     0,
      13,     0,     0,     0,     0,    14,    15,     0,     0,    16,
      17,     0,     0,     0,     0,    18,    19,    20,     0,     0,
      21,     0,     0,     0,    22,    23,     0,     0,     0,    24,
      25,    26,     0,     0,    27,     0,     0,     0,     0,    28,
       0,    29,     0,     0,     0,    30,    31,    32,     0,    33,
      34,    35,     0,     0,    36,  -158,    37,     0,    38,     0,
      39,     0,    40,     0,     0,    41,     0,    42,     0,    43,
      44,     1,    45,     2,     3,     4,     5,     6,     0,     7,
       8,     9,    10,    11,     0,     0,     0,    12,  -174,    13,
       0,     0,     0,     0,    14,    15,     0,     0,    16,    17,
       0,     0,     0,     0,    18,    19,    20,     0,     0,    21,
       0,     0,     0,    22,    23,     0,     0,     0,    24,    25,
      26,     0,     0,    27,     0,     0,     0,     0,    28,     0,
      29,     0,     0,     0,    30,    31,    32,     0,    33,    34,
      35,     0,     0,    36,     0,    37,     0,    38,     0,    39,
       0,    40,     0,     0,    41,     0,    42,     0,    43,    44,
       1,    45,     2,     3,     4,     5,     6,     0,     7,     8,
       9,    10,    11,     0,     0,     0,    12,   201,    13,     0,
       0,     0,     0,    14,    15,     0,     0,    16,    17,     0,
       0,     0,     0,    18,    19,    20,     0,     0,    21,     0,
       0,     0,    22,    23,     0,     0,     0,    24,    25,    26,
       0,     0,    27,     0,     0,     0,     0,    28,     0,    29,
       0,     0,     0,    30,    31,    32,     0,    33,    34,    35,
       0,     0,    36,     0,    37,     0,    38,     0,    39,     0,
      40,     0,     0,    41,     0,    42,     0,    43,    44,     1,
      45,     2,     3,     4,     5,     6,     0,     7,     8,     9,
      10,    11,     0,     0,     0,    12,     0,    13,     0,     0,
       0,     0,    14,    15,     0,     0,    16,    17,     0,     0,
       0,     0,    18,    19,    20,     0,     0,    21,     0,     0,
       0,    22,    23,     0,     0,     0,    24,    25,    26,     0,
       0,    27,     0,     0,     0,     0,    28,     0,    29,     0,
       0,     0,    30,    31,    32,     0,    33,    34,    35,    93,
       0,    36,     0,    37,     0,    38,     0,    39,     0,    40,
       0,     0,    41,     0,    42,     0,    43,    44,     1,    45,
       2,     3,     4,     5,     6,   226,     7,     8,     9,    10,
      11,     0,     0,     0,    12,     0,    13,     0,     0,     0,
       0,    14,    15,     0,     0,    16,    17,     0,     0,     0,
       0,    18,    19,    20,     0,     0,    21,     0,     0,     0,
      22,    23,     0,     0,     0,    24,    25,    26,     0,     0,
      27,     0,     0,     0,     0,    28,     0,    29,     0,     0,
       0,    30,    31,    32,     0,    33,    34,    35,     0,     0,
      36,     0,    37,     0,    38,     0,    39,     0,    40,     0,
       0,    41,     0,    42,     0,    43,    44,     1,    45,     2,
       3,     4,     5,     6,     0,     7,     8,     9,    10,    11,
       0,     0,     0,    12,     0,    13,     0,     0,     0,     0,
      14,    15,     0,     0,    16,    17,     0,     0,     0,     0,
      18,    19,    20,     0,     0,    21,     0,     0,     0,    22,
      23,     0,     0,     0,    24,    25,    26,     0,     0,    27,
       0,     0,     0,     0,    28,     0,    29,     0,     0,     0,
      30,    31,    32,     0,    33,    34,    35,     0,     0,    36,
     276,    37,     0,    38,     0,    39,     0,    40,     0,     0,
      41,     0,    42,     0,    43,    44,     1,    45,     2,     3,
       4,     5,     6,     0,     7,     8,     9,    10,    11,     0,
       0,     0,    12,     0,    13,     0,     0,     0,     0,    14,
      15,     0,     0,    16,    17,     0,     0,     0,     0,    18,
      19,    20,     0,     0,    21,     0,     0,     0,    22,    23,
       0,     0,     0,    24,    25,    26,     0,     0,    27,     0,
       0,     0,     0,    28,     0,    29,     0,     0,     0,    30,
      31,    32,     0,    33,   384,    35,    93,     0,    36,     0,
      37,     0,    38,     0,    39,     0,    40,     0,     0,    41,
       0,    42,     0,    43,    44,     1,    45,     2,     3,     4,
       5,     6,     0,     7,     8,     9,    10,    11,     0,     0,
       0,    12,  -172,    13,     0,     0,     0,     0,    14,    15,
       0,     0,    16,    17,     0,     0,     0,     0,    18,    19,
      20,     0,     0,    21,     0,     0,     0,    22,    23,     0,
       0,     0,    24,    25,    26,     0,     0,    27,     0,     0,
       0,     0,    28,     0,    29,     0,     0,     0,    30,    31,
      32,     0,    33,    34,    35,     0,     0,    36,     0,    37,
       0,    38,     0,    39,     0,    40,     0,     0,    41,     0,
      42,     0,    43,    44,     1,    45,     2,     3,     4,     5,
       6,     0,     7,     8,     9,    10,    11,     0,     0,     0,
      12,     0,    13,     0,     0,     0,     0,    14,    15,     0,
       0,    16,    17,     0,     0,     0,     0,    18,    19,    20,
       0,     0,    21,     0,     0,     0,    22,    23,     0,     0,
       0,    24,    25,    26,     0,     0,    27,     0,     0,     0,
       0,    28,     0,    29,     0,     0,     0,    30,    31,    32,
       0,    33,    34,    35,     0,     0,    36,     0,    37,     0,
      38,     0,    39,     0,    40,     0,     0,    41,     0,    42,
       0,    43,    44,     1,    45,     2,     3,     4,     5,     6,
       0,     7,     8,     9,    10,    64,     0,     0,     0,    12,
       0,    13,     0,     0,     0,     0,    14,    15,     0,     0,
      16,    17,     0,     0,     0,     0,    18,    19,    20,     0,
       0,    21,     0,     0,     0,    22,    23,     0,     0,     0,
      24,    25,    26,     0,     0,    27,     0,     0,     0,     0,
      28,     0,    29,     0,     0,     0,    30,    31,    32,     0,
      33,    34,    35,     0,     0,    65,     0,    37,     0,    38,
       0,    39,     0,    40,     0,     0,    41,     0,    42,     0,
      43,    44,     1,    45,     2,     3,     4,     5,     6,     0,
       7,     8,     9,    10,    11,     0,     0,     0,    75,     0,
      13,     0,     0,     0,     0,    14,    15,     0,     0,    16,
      17,     0,     0,     0,     0,    18,    19,    20,     0,     0,
      21,     0,     0,     0,    22,    23,     0,     0,     0,    24,
      25,    26,     0,     0,    27,     0,     0,     0,     0,    28,
       0,    29,     0,     0,     0,    30,    31,    32,     0,    33,
      34,    35,     0,     0,    36,     0,    37,     0,    38,     0,
      39,     0,    40,     0,     0,    41,     0,    42,     0,    43,
      44,     1,    45,     2,     3,     4,     5,     6,     0,     7,
       8,     9,    10,    11,     0,     0,     0,    79,     0,    13,
       0,     0,     0,     0,    14,    15,     0,     0,    16,    17,
       0,     0,     0,     0,    18,    19,    20,     0,     0,    21,
       0,     0,     0,    22,    23,     0,     0,     0,    24,    25,
      26,     0,     0,    27,     0,     0,     0,     0,    28,     0,
      29,     0,     0,     0,    30,    31,    32,     0,    33,    34,
      35,     0,     0,    36,     0,    37,     0,    38,     0,    39,
       0,    40,     0,     0,    41,     0,    42,     0,    43,    44,
       1,    45,     2,     3,     4,     5,     6,     0,     7,     8,
       9,    10,    11,     0,     0,     0,    89,     0,    13,     0,
       0,     0,     0,    14,    15,     0,     0,    16,    17,     0,
       0,     0,     0,    18,    19,    20,     0,     0,    21,     0,
       0,     0,    22,    23,     0,     0,     0,    24,    25,    26,
       0,     0,    27,     0,     0,     0,     0,    28,     0,    29,
       0,     0,     0,    30,    31,    32,     0,    33,    34,    35,
       0,     0,    36,     0,    37,     0,    38,     0,    39,     0,
      40,     0,     0,    41,     0,    42,     0,    43,    44,     1,
      45,     2,   170,     4,     5,     6,     0,     7,     8,     9,
      10,    11,     0,     0,     0,    12,     0,    13,     0,     0,
       0,     0,    14,    15,     0,     0,    16,    17,     0,     0,
       0,     0,    18,    19,    20,     0,     0,    21,     0,     0,
       0,    22,    23,     0,     0,     0,    24,    25,    26,     0,
       0,    27,     0,     0,     0,     0,    28,     0,    29,     0,
       0,     0,    30,    31,    32,     0,    33,    34,    35,     0,
       0,    36,     0,    37,     0,    38,     0,    39,     0,    40,
       0,     0,    41,     0,    42,     0,    43,    44,     1,    45,
       2,     3,     4,     5,     6,     0,     7,     8,     9,    10,
      11,     0,     0,     0,    12,     0,    13,     0,     0,     0,
       0,    14,    15,     0,     0,    16,    17,     0,     0,     0,
       0,    18,    19,    20,     0,     0,   224,     0,     0,     0,
      22,    23,     0,     0,     0,    24,    25,    26,     0,     0,
      27,     0,     0,     0,     0,    28,     0,    29,     0,     0,
       0,    30,    31,    32,     0,    33,    34,    35,     0,     0,
      36,     0,    37,     0,    38,     0,    39,     0,    40,     0,
       0,    41,     0,    42,     0,    43,    44,     1,    45,     2,
       3,     4,     5,     6,     0,     7,     8,     9,    10,    11,
       0,     0,     0,    12,     0,    13,     0,     0,     0,     0,
      14,    15,     0,     0,    16,    17,     0,     0,     0,     0,
      18,    19,    20,     0,     0,   343,     0,     0,     0,    22,
      23,     0,     0,     0,    24,    25,    26,     0,     0,    27,
       0,     0,     0,     0,    28,     0,    29,     0,     0,     0,
      30,    31,    32,     0,    33,    34,    35,     0,     0,    36,
       0,    37,     0,    38,     0,    39,     0,    40,     0,     0,
      41,     0,    42,     0,    43,    44,     1,    45,     2,     3,
       4,     5,     6,   -75,     7,     8,     9,    10,    11,   -75,
     -75,   -75,   109,   -75,    13,     0,   -75,   -75,   -75,    14,
      15,   -75,   -75,    16,    17,   -75,   -75,   -75,   -75,    18,
      19,    20,   -75,     0,    21,   -75,   -75,   -75,    22,    23,
     -75,   -75,   -75,    24,    25,    26,   -75,   -75,   -75,   -75,
     -75,   -75,   -75,    28,     0,    29,   -75,   -75,     0,    30,
      31,    32,     0,    33,    34,    35,     0,   -75,   -75,     0,
      37,   -75,    38,   -75,    39,   -75,    40,   -75,   -75,    41,
     -75,    42,   -75,    43,    44,   112,     0,     0,     0,   113,
       0,   114,   115,   116,   117,     0,   118,     0,   119,   120,
     121,     0,     0,   122,   123,   124,   125,   126,   127,   128,
     129,   130,   131,   132,   133,     0,     0,     0,   134,   135,
       0,     0,     0,     0,     0,   171,   113,     0,   114,   115,
     116,   117,     0,   118,     0,   119,   120,   121,     0,   136,
     122,   123,   124,   125,   126,   127,   128,   129,   130,   131,
     132,   133,     0,   137,     0,     0,   135,     0,     0,   138,
     139,     0,   140,   141,   142,   172,     0,     0,     0,     0,
     113,     0,     0,     0,     0,   117,   136,   118,     0,   119,
     120,   121,     0,     0,   122,     0,     0,     0,     0,     0,
     137,     0,   130,   131,     0,   133,   138,   139,     0,   140,
     141,   142,   113,     0,   114,   115,   116,   117,     0,   118,
       0,   119,   120,   121,     0,     0,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   131,   132,   133,     0,     0,
     341,     0,   135,     0,   137,     0,     0,     0,     0,     0,
       0,     0,     0,   164,   141,   142,     0,     0,     0,     0,
       0,     0,   136,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   137,     0,     0,     0,
       0,     0,   138,   139,     0,   140,   141,   142,   113,     0,
     114,   115,   116,   117,     0,   118,     0,   119,   120,   121,
       0,     0,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,     0,     0,   389,     0,   135,     0,
     390,     0,     0,     0,     0,   113,     0,   114,   115,   116,
     117,     0,   118,     0,   119,   120,   121,     0,   136,   122,
     123,   124,   125,   126,   127,   128,   129,   130,   131,   132,
     133,     0,   137,   415,     0,   135,     0,     0,   138,   139,
       0,   140,   141,   142,     0,  -202,   164,     0,     0,     0,
     117,     0,     0,     0,     0,   136,   121,     0,     0,   122,
       0,     0,     0,     0,     0,     0,     0,   130,   131,   137,
     133,     0,     0,     0,     0,   138,   139,     0,   140,   141,
     142,   113,     0,   114,   115,   116,   117,   159,   118,     0,
     119,   120,   121,     0,     0,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,     0,     0,   137,
       0,   135,     0,     0,     0,     0,     0,     0,     0,   113,
     142,   114,   115,   116,   117,     0,   118,     0,   119,   120,
     121,   136,   162,   122,   123,   124,   125,   126,   127,   128,
     129,   130,   131,   132,   133,   137,     0,     0,     0,   135,
       0,   138,   139,     0,   140,   141,   142,   113,     0,   114,
     115,   116,   117,     0,   118,     0,   119,   120,   121,   136,
       0,   122,   123,   124,   125,   126,   127,   128,   129,   130,
     131,   132,   133,   137,     0,     0,     0,   135,     0,   138,
     139,     0,   140,   141,   142,     0,     0,     0,   164,     0,
       0,   113,     0,     0,     0,     0,   117,   136,     0,     0,
       0,  -202,   121,     0,     0,   122,     0,     0,     0,     0,
       0,   137,     0,   130,   131,     0,   133,   138,   139,     0,
     140,   141,   142,   113,     0,   114,   115,   116,   117,     0,
     118,     0,   119,   120,   121,     0,     0,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,     0,
       0,     0,     0,   135,     0,   137,     0,   167,     0,     0,
     113,     0,   114,   115,   116,   117,   142,   118,     0,   119,
     120,   121,     0,   136,   122,   123,   124,   125,   126,   127,
     128,   129,   130,   131,   132,   133,     0,   137,     0,     0,
     135,     0,     0,   138,   139,     0,   140,   141,   142,     0,
       0,     0,     0,     0,   182,     0,     0,     0,     0,     0,
     136,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   137,     0,     0,     0,     0,     0,
     138,   139,     0,   140,   141,   142,   113,     0,   114,   115,
     116,   117,     0,   118,     0,   119,   120,   121,     0,     0,
     122,   123,   124,   125,   126,   127,   128,   129,   130,   131,
     132,   133,     0,     0,     0,     0,   135,     0,     0,     0,
       0,     0,     0,   113,     0,   114,   115,   116,   117,   245,
     118,     0,   119,   120,   121,     0,   136,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,     0,
     137,     0,   197,   135,     0,     0,   138,   139,     0,   140,
     141,   142,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   117,   136,     0,     0,     0,     0,   121,     0,
       0,   122,     0,     0,     0,     0,     0,   137,     0,   130,
    -202,     0,   133,   138,   139,     0,   140,   141,   142,   113,
       0,   114,   115,   116,   117,   246,   118,     0,   119,   120,
     121,     0,     0,   122,   123,   124,   125,   126,   127,   128,
     129,   130,   131,   132,   133,     0,     0,     0,     0,   135,
       0,   137,     0,     0,     0,     0,   113,     0,   114,   115,
     116,   117,   142,   118,     0,   119,   120,   121,     0,   136,
     122,   123,   124,   125,   126,   127,   128,   129,   130,   131,
     132,   133,     0,   137,   247,     0,   135,     0,     0,   138,
     139,     0,   140,   141,   142,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   136,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     137,     0,     0,     0,     0,     0,   138,   139,     0,   140,
     141,   142,   113,     0,   114,   115,   116,   117,   257,   118,
       0,   119,   120,   121,     0,     0,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   131,   132,   133,     0,     0,
       0,     0,   135,     0,     0,     0,     0,     0,     0,   113,
       0,   114,   115,   116,   117,   260,   118,     0,   119,   120,
     121,     0,   136,   122,   123,   124,   125,   126,   127,   128,
     129,   130,   131,   132,   133,     0,   137,     0,     0,   135,
       0,     0,   138,   139,     0,   140,   141,   142,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   136,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   137,     0,     0,     0,     0,     0,   138,
     139,     0,   140,   141,   142,   113,     0,   114,   115,   116,
     117,   261,   118,     0,   119,   120,   121,     0,     0,   122,
     123,   124,   125,   126,   127,   128,   129,   130,   131,   132,
     133,     0,     0,     0,     0,   135,     0,     0,     0,     0,
       0,     0,   113,     0,   114,   115,   116,   117,   266,   118,
       0,   119,   120,   121,     0,   136,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   131,   132,   133,     0,   137,
       0,     0,   135,     0,     0,   138,   139,     0,   140,   141,
     142,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   136,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   137,     0,     0,     0,
       0,     0,   138,   139,     0,   140,   141,   142,   113,     0,
     114,   115,   116,   117,   273,   118,     0,   119,   120,   121,
       0,     0,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,     0,     0,     0,     0,   135,     0,
       0,     0,     0,     0,     0,   113,     0,   114,   115,   116,
     117,   281,   118,     0,   119,   120,   121,     0,   136,   122,
     123,   124,   125,   126,   127,   128,   129,   130,   131,   132,
     133,     0,   137,     0,     0,   135,     0,     0,   138,   139,
       0,   140,   141,   142,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   136,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   137,
       0,     0,     0,     0,     0,   138,   139,     0,   140,   141,
     142,   113,     0,   114,   115,   116,   117,   284,   118,     0,
     119,   120,   121,     0,     0,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,     0,     0,     0,
       0,   135,     0,     0,     0,     0,     0,     0,   113,     0,
     114,   115,   116,   117,   285,   118,     0,   119,   120,   121,
       0,   136,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,     0,   137,     0,     0,   135,     0,
       0,   138,   139,     0,   140,   141,   142,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   136,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   137,     0,     0,     0,     0,     0,   138,   139,
       0,   140,   141,   142,   113,     0,   114,   115,   116,   117,
       0,   118,     0,   119,   120,   121,     0,     0,   122,   123,
     124,   125,   126,   127,   128,   129,   130,   131,   132,   133,
       0,     0,   286,     0,   135,     0,     0,     0,     0,     0,
       0,   113,     0,   114,   115,   116,   117,   289,   118,     0,
     119,   120,   121,     0,   136,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,     0,   137,     0,
       0,   135,     0,     0,   138,   139,     0,   140,   141,   142,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   136,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   137,     0,     0,     0,     0,
       0,   138,   139,     0,   140,   141,   142,   113,     0,   114,
     115,   116,   117,     0,   118,     0,   119,   120,   121,     0,
       0,   122,   123,   124,   125,   126,   127,   128,   129,   130,
     131,   132,   133,     0,     0,   296,     0,   135,     0,     0,
       0,     0,     0,     0,   113,     0,   114,   115,   116,   117,
     301,   118,     0,   119,   120,   121,     0,   136,   122,   123,
     124,   125,   126,   127,   128,   129,   130,   131,   132,   133,
       0,   137,     0,     0,   135,     0,     0,   138,   139,     0,
     140,   141,   142,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   136,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   137,     0,
       0,     0,     0,     0,   138,   139,     0,   140,   141,   142,
     113,     0,   114,   115,   116,   117,     0,   118,     0,   119,
     120,   121,     0,   302,   122,   123,   124,   125,   126,   127,
     128,   129,   130,   131,   132,   133,     0,     0,     0,     0,
     135,     0,     0,     0,     0,     0,     0,   113,     0,   114,
     115,   116,   117,     0,   118,     0,   119,   120,   121,     0,
     136,   122,   123,   124,   125,   126,   127,   128,   129,   130,
     131,   132,   133,     0,   137,     0,     0,   135,     0,     0,
     138,   139,     0,   140,   141,   142,     0,     0,     0,     0,
     113,   325,     0,     0,     0,   117,     0,   136,     0,   119,
     120,   121,     0,     0,   122,     0,     0,     0,     0,     0,
       0,   137,   130,   131,     0,   133,     0,   138,   139,     0,
     140,   141,   142,   113,     0,   114,   115,   116,   117,     0,
     118,     0,   119,   120,   121,     0,     0,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,     0,
       0,     0,     0,   135,   137,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   141,   142,     0,     0,     0,     0,
       0,     0,     0,   136,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   330,     0,     0,     0,   137,     0,     0,
       0,     0,     0,   138,   139,     0,   140,   141,   142,   113,
       0,   114,   115,   116,   117,     0,   118,     0,   119,   120,
     121,     0,     0,   122,   123,   124,   125,   126,   127,   128,
     129,   130,   131,   132,   133,     0,     0,     0,   357,   135,
       0,     0,     0,     0,     0,     0,   113,     0,   114,   115,
     116,   117,     0,   118,     0,   119,   120,   121,     0,   136,
     122,   123,   124,   125,   126,   127,   128,   129,   130,   131,
     132,   133,     0,   137,     0,     0,   135,     0,     0,   138,
     139,     0,   140,   141,   142,     0,     0,     0,     0,     0,
       0,   369,     0,     0,     0,     0,   136,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     137,     0,     0,     0,     0,     0,   138,   139,     0,   140,
     141,   142,   113,     0,   114,   115,   116,   117,     0,   118,
       0,   119,   120,   121,     0,     0,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   131,   132,   133,     0,     0,
       0,   371,   135,     0,     0,     0,     0,     0,     0,   113,
       0,   114,   115,   116,   117,   373,   118,     0,   119,   120,
     121,     0,   136,   122,   123,   124,   125,   126,   127,   128,
     129,   130,   131,   132,   133,     0,   137,     0,     0,   135,
       0,     0,   138,   139,     0,   140,   141,   142,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   136,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   137,     0,     0,     0,     0,     0,   138,
     139,     0,   140,   141,   142,   113,     0,   114,   115,   116,
     117,     0,   118,     0,   119,   120,   121,     0,     0,   122,
     123,   124,   125,   126,   127,   128,   129,   130,   131,   132,
     133,     0,     0,   383,     0,   135,     0,     0,     0,     0,
       0,     0,     0,   113,     0,   114,   115,   116,   117,     0,
     118,     0,   119,   120,   121,   136,     0,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   137,
       0,     0,     0,   135,     0,   138,   139,   394,   140,   141,
     142,   113,     0,   114,   115,   116,   117,     0,   118,     0,
     119,   120,   121,   136,     0,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,   137,     0,     0,
     419,   135,     0,   138,   139,     0,   140,   141,   142,   113,
       0,   114,   115,   116,   117,     0,   118,     0,   119,   120,
     121,   136,     0,   122,   123,   124,   125,   126,   127,   128,
     129,   130,   131,   132,   133,   137,     0,   432,     0,   135,
       0,   138,   139,     0,   140,   141,   142,   113,     0,   114,
     115,   116,   117,     0,   118,     0,   119,   120,   121,   136,
       0,   122,   123,   124,   125,   126,   127,   128,   129,   130,
     131,   132,   133,   137,     0,     0,   442,   135,     0,   138,
     139,     0,   140,   141,   142,   113,     0,   114,   115,   116,
     117,     0,   118,     0,   119,   120,   121,   136,     0,   122,
     123,   124,   125,   126,   127,   128,   129,   130,   131,   132,
     133,   137,     0,     0,     0,   135,     0,   138,   139,     0,
     140,   141,   142,   113,     0,   198,   115,   116,   117,     0,
     118,     0,   119,   120,   121,   136,     0,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   137,
       0,     0,     0,   135,     0,   138,   139,     0,   140,   141,
     142,   113,     0,   114,   115,   116,   117,     0,   118,     0,
     119,   120,   121,   136,     0,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,   137,     0,     0,
       0,   135,     0,   138,   139,     0,   140,   141,   142,   113,
       0,     0,   115,   116,   117,     0,   118,     0,   119,   120,
     121,   136,     0,   122,   123,   124,   125,   126,   127,   128,
     129,   130,   131,   132,   133,   137,     0,     0,     0,   135,
       0,     0,  -202,     0,   140,   141,   142,   113,     0,   114,
     115,   116,   117,     0,   118,     0,   119,   120,   121,   136,
       0,   122,   123,   124,   125,   126,   127,   128,   129,   130,
     131,   132,   133,   137,     0,     0,     0,   135,     0,   138,
     139,     0,   140,   141,   142,   113,     0,     0,   115,   116,
     117,     0,   118,     0,   119,   120,   121,   136,     0,   122,
     123,   124,   125,   126,   127,   128,   129,   130,   131,   132,
     133,   137,     0,     0,     0,   135,     0,     0,     0,     0,
     140,   141,   142,   113,     0,     0,     0,   116,   117,     0,
     118,     0,   119,   120,   121,     0,     0,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   137,
       0,     0,     0,   135,     0,   138,   139,     0,   140,   141,
     142,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   136,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   137,     0,     0,
       0,   336,     0,     0,     0,   113,   140,   141,   142,   116,
     117,     0,   118,     0,   119,   120,   121,     0,     0,   122,
     123,   124,   125,   126,   127,   128,   129,   130,   131,   132,
     133,     0,     0,     0,     0,   135,     0,     0,     0,     0,
       0,     0,   113,     0,     0,     0,   116,   117,     0,   118,
       0,   119,   120,   121,     0,   136,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   131,   132,   133,     0,   137,
       0,     0,   135,     0,     0,     0,     0,     0,   140,   141,
     142,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   136,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   137,     0,     0,     0,
       0,     0,     0,     0,   113,  -202,   141,   142,   116,   117,
       0,   118,     0,   119,   120,   121,     0,     0,   122,   123,
     124,   125,   126,   127,   128,   129,   130,   131,   132,   133,
       0,     0,     0,     0,   135,     0,     0,     0,     0,     0,
       0,   113,     0,     0,     0,  -202,   117,     0,   118,     0,
     119,   120,   121,     0,   136,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,     0,   137,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   141,   142,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   136,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   113,     0,   137,     0,     0,   117,     0,
     118,     0,   119,   120,   121,   141,   142,   122,   123,   124,
     125,   126,   127,   128,   113,   130,   131,   132,   133,   117,
       0,   118,     0,   119,   120,   121,     0,     0,   122,   123,
     124,   125,   126,   127,   128,     0,   130,   131,   132,   133,
       0,     0,     0,   136,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   137,     0,     0,
       0,     0,     0,     0,  -202,     0,     0,   141,   142,     0,
       0,     0,     0,     0,     0,     0,   113,     0,   137,     0,
       0,   117,     0,   118,     0,   119,   120,   121,   141,   142,
     122,   123,   124,   125,   126,   127,   128,   113,   130,   131,
       0,   133,   117,     0,   118,     0,   119,   120,   121,     0,
       0,   122,     0,     0,   125,   126,   127,   128,     0,   130,
     131,     0,   133,     0,   113,     0,     0,     0,     0,   117,
       0,   118,     0,   119,   120,   121,     0,     0,   122,     0,
     137,   125,     0,   127,  -202,     0,   130,   131,     0,   133,
     141,   142,   113,     0,     0,     0,     0,   117,     0,   113,
       0,   137,   120,   121,   117,     0,   122,     0,     0,   120,
     121,   141,   142,   122,   130,   131,     0,   133,     0,     0,
       0,   130,   131,     0,   133,     0,     0,     0,   137,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   141,   142,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   137,     0,     0,     0,
       0,     0,     0,   137,     0,     0,  -202,   142,     0,     0,
       0,     0,     0,   141,   142
};

static const yytype_int16 yycheck[] =
{
       0,    27,   267,    23,   253,    17,    96,    15,    34,    15,
      10,   313,    12,    13,     4,    15,    16,    17,    14,    17,
      20,    21,    28,    23,    15,     4,    26,   117,    28,    27,
      15,    10,    17,    43,    13,    25,    36,    28,    38,    39,
      40,    41,    42,     4,     5,    45,    25,    18,    54,    10,
      62,    42,    43,   143,    14,   357,    56,    57,    58,    59,
     309,    69,    50,    51,    17,    65,    62,    87,    68,    43,
      17,    17,    43,     3,     4,    75,     4,    77,    78,    79,
      10,    41,    43,    13,    37,    85,     4,    17,    62,    89,
      37,    37,    92,   282,     4,    95,    96,   123,    98,   116,
     100,   101,   367,   193,    17,    43,   132,   107,   108,   109,
      89,    17,   138,    43,   114,   115,   116,   117,   118,   119,
     120,   121,    17,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,   134,   135,   136,    71,   164,   139,
     140,   141,    37,   143,   144,    41,   335,    17,    17,    17,
     167,   151,     3,     4,    84,    93,     0,   346,   248,    89,
      56,   161,    13,    17,     3,     4,    17,     6,    17,     4,
      43,    10,    62,    40,    13,    14,   176,   177,   178,   179,
      19,    67,    21,    17,   184,    17,    62,   376,   188,    23,
       3,    17,    43,   193,     3,   384,    52,    55,   198,    41,
      34,    17,   202,    37,    43,    43,     3,     4,   298,    71,
      18,    18,    18,    10,    47,    56,    13,    18,    23,    18,
      17,   311,    18,    47,   224,    18,    41,   253,    40,   167,
       3,     3,    42,    84,   172,    72,   236,    18,    42,   276,
     257,   134,    76,    82,   182,    84,    43,    86,   248,   187,
     276,    40,   357,    87,   420,   361,   273,   257,   258,   434,
      -1,   199,   366,    -1,   264,    -1,    -1,   267,    -1,    -1,
      -1,    -1,   272,   273,   274,    -1,    -1,    -1,   278,    -1,
     280,    -1,    -1,   309,    -1,    -1,    -1,    84,   288,    -1,
      -1,    -1,   292,   293,   294,    -1,    -1,    -1,   298,   299,
      -1,    -1,    -1,   320,   304,   305,   306,   307,    12,    -1,
     310,   311,    -1,    17,   314,    19,    -1,    21,    22,    23,
      -1,    -1,    26,    -1,    -1,    29,    -1,    12,   345,    -1,
      34,    35,    17,    37,    19,    -1,    21,    22,    23,    -1,
      -1,    26,   359,   343,    29,   345,    31,    32,    -1,    34,
      35,    -1,    37,    -1,    -1,    -1,   373,    -1,   384,   359,
      -1,   361,    -1,    -1,    -1,    -1,   366,   367,   368,   369,
      -1,    -1,    76,    -1,    -1,   375,    -1,    -1,   378,   379,
     397,   319,    86,    87,   322,    -1,    -1,   325,    -1,    -1,
     390,    76,    -1,    -1,    -1,    -1,   334,   335,    -1,   337,
      -1,    86,    87,    -1,   421,    -1,   423,    -1,    -1,   409,
      -1,    -1,   412,    -1,    -1,    -1,    -1,   417,    -1,    -1,
     420,    -1,    -1,   423,   424,   425,    -1,   427,    -1,    -1,
     430,    -1,    -1,    -1,    -1,    -1,   374,    -1,    -1,   377,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   387,
      -1,    -1,    -1,    -1,    -1,    -1,   394,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   402,    -1,    -1,    -1,    -1,   407,
      -1,    -1,    -1,   411,    -1,    -1,    -1,     1,   416,     3,
       4,     5,     6,     7,     8,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    -1,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    -1,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    -1,    60,    61,    62,    -1,
      64,    65,    66,    -1,    68,    69,    70,    -1,    72,    73,
      -1,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,     1,    91,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    -1,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    -1,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    -1,    60,    61,    62,    -1,    64,
      65,    66,    -1,    68,    69,    70,    -1,    72,    73,    -1,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,     1,    91,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    -1,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    -1,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    -1,    60,    61,    62,    -1,    64,    65,
      66,    -1,    68,    69,    70,    -1,    72,    73,    -1,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,     1,    91,     3,     4,     5,     6,
       7,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    -1,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    -1,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    -1,    60,    61,    62,    -1,    64,    65,    66,
      -1,    68,    69,    70,    -1,    72,    -1,    -1,    75,    76,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,    88,    89,     1,    91,     3,     4,     5,     6,     7,
      -1,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      -1,    19,    -1,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      -1,    39,    -1,    -1,    42,    43,    44,    -1,    46,    -1,
      48,    49,    50,    -1,    -1,    53,    -1,    -1,    -1,    -1,
      58,    -1,    60,    -1,    62,    -1,    64,    65,    66,    -1,
      68,    69,    70,    71,    -1,    73,    -1,    75,    76,    77,
      -1,    79,    -1,    81,    82,    83,    84,    85,    86,    87,
      88,    89,     1,    91,     3,     4,     5,     6,     7,    -1,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    -1,
      19,    -1,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    -1,
      39,    -1,    -1,    42,    43,    44,    -1,    -1,    -1,    48,
      49,    50,    -1,    -1,    53,    -1,    -1,    56,    -1,    58,
      -1,    60,    -1,    62,    -1,    64,    65,    66,    -1,    68,
      69,    70,    71,    -1,    73,    -1,    75,    76,    77,    -1,
      79,    -1,    81,    82,    83,    84,    85,    86,    87,    88,
      89,     1,    91,     3,     4,     5,     6,     7,    -1,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    -1,    19,
      -1,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    -1,    39,
      -1,    -1,    42,    43,    44,    -1,    -1,    -1,    48,    49,
      50,    -1,    -1,    53,    -1,    -1,    -1,    -1,    58,    -1,
      60,    -1,    62,    -1,    64,    65,    66,    -1,    68,    69,
      70,    -1,    -1,    73,    -1,    75,    76,    77,    -1,    79,
      -1,    81,    82,    83,    84,    85,    86,    87,    88,    89,
       1,    91,     3,     4,     5,     6,     7,    -1,     9,    10,
      11,    12,    13,    -1,    -1,    -1,    17,    -1,    19,    -1,
      -1,    -1,    -1,    24,    25,    -1,    -1,    28,    29,    -1,
      -1,    -1,    -1,    34,    35,    36,    -1,    -1,    39,    -1,
      41,    -1,    43,    44,    45,    -1,    47,    48,    49,    50,
      -1,    -1,    53,    -1,    -1,    -1,    -1,    58,    -1,    60,
      -1,    -1,    -1,    64,    65,    66,    -1,    68,    69,    70,
      -1,    72,    73,    -1,    75,    -1,    77,    -1,    79,    -1,
      81,    -1,    -1,    84,    -1,    86,    -1,    88,    89,     1,
      91,     3,     4,     5,     6,     7,    -1,     9,    10,    11,
      12,    13,    -1,    -1,    -1,    17,    -1,    19,    -1,    -1,
      -1,    -1,    24,    25,    -1,    -1,    28,    29,    -1,    -1,
      -1,    -1,    34,    35,    36,    -1,    -1,    39,    -1,    41,
      -1,    43,    44,    -1,    -1,    47,    48,    49,    50,    -1,
      -1,    53,    -1,    -1,    -1,    -1,    58,    -1,    60,    -1,
      -1,    -1,    64,    65,    66,    -1,    68,    69,    70,    -1,
      72,    73,    -1,    75,    -1,    77,    -1,    79,    -1,    81,
      -1,    -1,    84,    -1,    86,    -1,    88,    89,     1,    91,
       3,     4,     5,     6,     7,    -1,     9,    10,    11,    12,
      13,    -1,    -1,    -1,    17,    -1,    19,    -1,    -1,    -1,
      -1,    24,    25,    -1,    -1,    28,    29,    -1,    -1,    -1,
      -1,    34,    35,    36,    -1,    -1,    39,    -1,    41,    -1,
      43,    44,    -1,    -1,    -1,    48,    49,    50,    51,    52,
      53,    -1,    -1,    -1,    -1,    58,    -1,    60,    -1,    -1,
      -1,    64,    65,    66,    -1,    68,    69,    70,    -1,    -1,
      73,    -1,    75,    -1,    77,    -1,    79,    -1,    81,    -1,
      -1,    84,    -1,    86,    -1,    88,    89,     1,    91,     3,
       4,     5,     6,     7,    -1,     9,    10,    11,    12,    13,
      -1,    -1,    -1,    17,    -1,    19,    -1,    -1,    -1,    -1,
      24,    25,    -1,    -1,    28,    29,    -1,    -1,    -1,    -1,
      34,    35,    36,    -1,    -1,    39,    -1,    41,    -1,    43,
      44,    -1,    -1,    47,    48,    49,    50,    -1,    -1,    53,
      -1,    -1,    -1,    -1,    58,    -1,    60,    -1,    -1,    -1,
      64,    65,    66,    -1,    68,    69,    70,    -1,    72,    73,
      -1,    75,    -1,    77,    -1,    79,    -1,    81,    -1,    -1,
      84,    -1,    86,    -1,    88,    89,     1,    91,     3,     4,
       5,     6,     7,    -1,     9,    10,    11,    12,    13,    -1,
      -1,    -1,    17,    -1,    19,    -1,    -1,    -1,    -1,    24,
      25,    -1,    -1,    28,    29,    -1,    -1,    -1,    -1,    34,
      35,    36,    -1,    -1,    39,    -1,    41,    -1,    43,    44,
      -1,    -1,    -1,    48,    49,    50,    -1,    -1,    53,    -1,
      -1,    -1,    -1,    58,    -1,    60,    61,    -1,    -1,    64,
      65,    66,    -1,    68,    69,    70,    -1,    -1,    73,    -1,
      75,    -1,    77,    -1,    79,    -1,    81,    -1,    -1,    84,
      -1,    86,    -1,    88,    89,     1,    91,     3,     4,     5,
       6,     7,    -1,     9,    10,    11,    12,    13,    -1,    -1,
      -1,    17,    -1,    19,    -1,    -1,    -1,    -1,    24,    25,
      -1,    -1,    28,    29,    -1,    -1,    -1,    -1,    34,    35,
      36,    -1,    -1,    39,    -1,    41,    -1,    43,    44,    -1,
      46,    -1,    48,    49,    50,    -1,    -1,    53,    -1,    -1,
      -1,    -1,    58,    -1,    60,    -1,    -1,    -1,    64,    65,
      66,    -1,    68,    69,    70,    -1,    -1,    73,    -1,    75,
      -1,    77,    -1,    79,    -1,    81,    -1,    -1,    84,    -1,
      86,    -1,    88,    89,     1,    91,     3,     4,     5,     6,
       7,    -1,     9,    10,    11,    12,    13,    -1,    -1,    -1,
      17,    -1,    19,    -1,    -1,    -1,    -1,    24,    25,    -1,
      -1,    28,    29,    -1,    -1,    -1,    -1,    34,    35,    36,
      -1,    -1,    39,    -1,    41,    -1,    43,    44,    -1,    -1,
      -1,    48,    49,    50,    -1,    -1,    53,    -1,    -1,    -1,
      -1,    58,    -1,    60,    -1,    -1,    -1,    64,    65,    66,
      -1,    68,    69,    70,    -1,    72,    73,    -1,    75,    -1,
      77,    -1,    79,    -1,    81,    -1,    -1,    84,    -1,    86,
      -1,    88,    89,     1,    91,     3,     4,     5,     6,     7,
      -1,     9,    10,    11,    12,    13,    -1,    -1,    -1,    17,
      -1,    19,    -1,    -1,    -1,    -1,    24,    25,    -1,    -1,
      28,    29,    -1,    -1,    -1,    -1,    34,    35,    36,    -1,
      -1,    39,    -1,    41,    -1,    43,    44,    -1,    -1,    -1,
      48,    49,    50,    -1,    52,    53,    -1,    -1,    -1,    -1,
      58,    -1,    60,    -1,    -1,    -1,    64,    65,    66,    -1,
      68,    69,    70,    -1,    -1,    73,    -1,    75,    -1,    77,
      -1,    79,    -1,    81,    -1,    -1,    84,    -1,    86,    -1,
      88,    89,     1,    91,     3,     4,     5,     6,     7,    -1,
       9,    10,    11,    12,    13,    -1,    -1,    -1,    17,    -1,
      19,    -1,    -1,    -1,    -1,    24,    25,    -1,    -1,    28,
      29,    -1,    -1,    -1,    -1,    34,    35,    36,    -1,    -1,
      39,    -1,    41,    -1,    43,    44,    -1,    -1,    -1,    48,
      49,    50,    -1,    -1,    53,    -1,    -1,    -1,    -1,    58,
      -1,    60,    -1,    -1,    -1,    64,    65,    66,    -1,    68,
      69,    70,    -1,    72,    73,    -1,    75,    -1,    77,    -1,
      79,    -1,    81,    -1,    -1,    84,    -1,    86,    -1,    88,
      89,     1,    91,     3,     4,     5,     6,     7,    -1,     9,
      10,    11,    12,    13,    -1,    -1,    -1,    17,    -1,    19,
      -1,    -1,    -1,    -1,    24,    25,    -1,    -1,    28,    29,
      -1,    -1,    -1,    -1,    34,    35,    36,    -1,    -1,    39,
      -1,    41,    -1,    43,    44,    -1,    -1,    -1,    48,    49,
      50,    -1,    -1,    53,    -1,    -1,    -1,    -1,    58,    -1,
      60,    -1,    -1,    -1,    64,    65,    66,    -1,    68,    69,
      70,    -1,    72,    73,    -1,    75,    -1,    77,    -1,    79,
      -1,    81,    -1,    -1,    84,    -1,    86,    -1,    88,    89,
       1,    91,     3,     4,     5,     6,     7,    -1,     9,    10,
      11,    12,    13,    -1,    -1,    -1,    17,    -1,    19,    -1,
      -1,    -1,    -1,    24,    25,    -1,    -1,    28,    29,    -1,
      -1,    -1,    -1,    34,    35,    36,    -1,    -1,    39,    -1,
      -1,    -1,    43,    44,    -1,    -1,    -1,    48,    49,    50,
      -1,    -1,    53,    -1,    -1,    -1,    -1,    58,    -1,    60,
      -1,    -1,    -1,    64,    65,    66,    -1,    68,    69,    70,
      71,    -1,    73,    74,    75,    -1,    77,    -1,    79,    -1,
      81,    -1,    -1,    84,    -1,    86,    -1,    88,    89,     1,
      91,     3,     4,     5,     6,     7,    -1,     9,    10,    11,
      12,    13,    -1,    -1,    -1,    17,    -1,    19,    -1,    -1,
      -1,    -1,    24,    25,    -1,    -1,    28,    29,    -1,    -1,
      -1,    -1,    34,    35,    36,    -1,    -1,    39,    -1,    -1,
      -1,    43,    44,    -1,    -1,    -1,    48,    49,    50,    -1,
      -1,    53,    -1,    -1,    -1,    -1,    58,    -1,    60,    -1,
      -1,    -1,    64,    65,    66,    -1,    68,    69,    70,    71,
      -1,    73,    74,    75,    -1,    77,    -1,    79,    -1,    81,
      -1,    -1,    84,    -1,    86,    -1,    88,    89,     1,    91,
       3,     4,     5,     6,     7,    -1,     9,    10,    11,    12,
      13,    -1,    -1,    -1,    17,    -1,    19,    -1,    -1,    -1,
      -1,    24,    25,    -1,    -1,    28,    29,    -1,    -1,    -1,
      -1,    34,    35,    36,    -1,    -1,    39,    -1,    41,    -1,
      43,    44,    -1,    -1,    -1,    48,    49,    50,    -1,    -1,
      53,    -1,    -1,    -1,    -1,    58,    -1,    60,    -1,    -1,
      -1,    64,    65,    66,    -1,    68,    69,    70,    -1,    72,
      73,    -1,    75,    -1,    77,    -1,    79,    -1,    81,    -1,
      -1,    84,    -1,    86,    -1,    88,    89,     1,    91,     3,
       4,     5,     6,     7,    -1,     9,    10,    11,    12,    13,
      -1,    -1,    -1,    17,    -1,    19,    -1,    -1,    -1,    -1,
      24,    25,    -1,    -1,    28,    29,    -1,    -1,    -1,    -1,
      34,    35,    36,    -1,    -1,    39,    -1,    41,    -1,    43,
      44,    -1,    -1,    -1,    48,    49,    50,    -1,    -1,    53,
      -1,    -1,    -1,    -1,    58,    -1,    60,    -1,    -1,    -1,
      64,    65,    66,    -1,    68,    69,    70,    -1,    72,    73,
      -1,    75,    -1,    77,    -1,    79,    -1,    81,    -1,    -1,
      84,    -1,    86,    -1,    88,    89,     1,    91,     3,     4,
       5,     6,     7,    -1,     9,    10,    11,    12,    13,    -1,
      -1,    -1,    17,    -1,    19,    -1,    -1,    -1,    -1,    24,
      25,    -1,    -1,    28,    29,    -1,    -1,    -1,    -1,    34,
      35,    36,    -1,    -1,    39,    -1,    41,    -1,    43,    44,
      -1,    -1,    -1,    48,    49,    50,    -1,    -1,    53,    -1,
      -1,    -1,    -1,    58,    -1,    60,    -1,    -1,    -1,    64,
      65,    66,    -1,    68,    69,    70,    -1,    72,    73,    -1,
      75,    -1,    77,    -1,    79,    -1,    81,    -1,    -1,    84,
      -1,    86,    -1,    88,    89,     1,    91,     3,     4,     5,
       6,     7,    -1,     9,    10,    11,    12,    13,    -1,    -1,
      -1,    17,    -1,    19,    -1,    -1,    -1,    -1,    24,    25,
      -1,    -1,    28,    29,    -1,    -1,    -1,    -1,    34,    35,
      36,    -1,    -1,    39,    -1,    41,    -1,    43,    44,    -1,
      -1,    -1,    48,    49,    50,    -1,    -1,    53,    -1,    -1,
      -1,    -1,    58,    -1,    60,    -1,    -1,    -1,    64,    65,
      66,    -1,    68,    69,    70,    -1,    72,    73,    -1,    75,
      -1,    77,    -1,    79,    -1,    81,    -1,    -1,    84,    -1,
      86,    -1,    88,    89,     1,    91,     3,     4,     5,     6,
       7,    -1,     9,    10,    11,    12,    13,    -1,    -1,    -1,
      17,    -1,    19,    -1,    -1,    -1,    -1,    24,    25,    -1,
      -1,    28,    29,    -1,    -1,    -1,    -1,    34,    35,    36,
      -1,    -1,    39,    -1,    41,    -1,    43,    44,    -1,    -1,
      -1,    48,    49,    50,    -1,    -1,    53,    -1,    -1,    -1,
      -1,    58,    -1,    60,    -1,    -1,    -1,    64,    65,    66,
      -1,    68,    69,    70,    -1,    72,    73,    -1,    75,    -1,
      77,    -1,    79,    -1,    81,    -1,    -1,    84,    -1,    86,
      -1,    88,    89,     1,    91,     3,     4,     5,     6,     7,
      -1,     9,    10,    11,    12,    13,    -1,    -1,    -1,    17,
      -1,    19,    -1,    -1,    -1,    -1,    24,    25,    -1,    -1,
      28,    29,    -1,    -1,    -1,    -1,    34,    35,    36,    -1,
      -1,    39,    -1,    41,    -1,    43,    44,    -1,    -1,    -1,
      48,    49,    50,    -1,    -1,    53,    -1,    -1,    -1,    -1,
      58,    -1,    60,    -1,    -1,    -1,    64,    65,    66,    -1,
      68,    69,    70,    -1,    72,    73,    -1,    75,    -1,    77,
      -1,    79,    -1,    81,    -1,    -1,    84,    -1,    86,    -1,
      88,    89,     1,    91,     3,     4,     5,     6,     7,    -1,
       9,    10,    11,    12,    13,    -1,    -1,    -1,    17,    -1,
      19,    -1,    -1,    -1,    -1,    24,    25,    -1,    -1,    28,
      29,    -1,    -1,    -1,    -1,    34,    35,    36,    -1,    -1,
      39,    -1,    41,    -1,    43,    44,    -1,    -1,    -1,    48,
      49,    50,    -1,    -1,    53,    -1,    -1,    -1,    -1,    58,
      -1,    60,    -1,    -1,    -1,    64,    65,    66,    -1,    68,
      69,    70,    -1,    72,    73,    -1,    75,    -1,    77,    -1,
      79,    -1,    81,    -1,    -1,    84,    -1,    86,    -1,    88,
      89,     1,    91,     3,     4,     5,     6,     7,    -1,     9,
      10,    11,    12,    13,    -1,    -1,    -1,    17,    -1,    19,
      -1,    -1,    -1,    -1,    24,    25,    -1,    -1,    28,    29,
      -1,    -1,    -1,    -1,    34,    35,    36,    -1,    -1,    39,
      -1,    41,    -1,    43,    44,    -1,    -1,    -1,    48,    49,
      50,    -1,    -1,    53,    -1,    -1,    -1,    -1,    58,    -1,
      60,    -1,    -1,    -1,    64,    65,    66,    -1,    68,    69,
      70,    -1,    72,    73,    -1,    75,    -1,    77,    -1,    79,
      -1,    81,    -1,    -1,    84,    -1,    86,    -1,    88,    89,
       1,    91,     3,     4,     5,     6,     7,    -1,     9,    10,
      11,    12,    13,    -1,    -1,    -1,    17,    -1,    19,    -1,
      -1,    -1,    -1,    24,    25,    -1,    -1,    28,    29,    -1,
      -1,    -1,    -1,    34,    35,    36,    -1,    -1,    39,    -1,
      41,    -1,    43,    44,    -1,    -1,    -1,    48,    49,    50,
      -1,    -1,    53,    -1,    -1,    -1,    -1,    58,    -1,    60,
      -1,    -1,    -1,    64,    65,    66,    -1,    68,    69,    70,
      -1,    72,    73,    -1,    75,    -1,    77,    -1,    79,    -1,
      81,    -1,    -1,    84,    -1,    86,    -1,    88,    89,     1,
      91,     3,     4,     5,     6,     7,    -1,     9,    10,    11,
      12,    13,    -1,    -1,    -1,    17,    -1,    19,    -1,    -1,
      -1,    -1,    24,    25,    -1,    -1,    28,    29,    -1,    -1,
      -1,    -1,    34,    35,    36,    -1,    -1,    39,    -1,    41,
      -1,    43,    44,    -1,    -1,    -1,    48,    49,    50,    -1,
      -1,    53,    -1,    -1,    -1,    -1,    58,    -1,    60,    -1,
      -1,    -1,    64,    65,    66,    -1,    68,    69,    70,    -1,
      72,    73,    -1,    75,    -1,    77,    -1,    79,    -1,    81,
      -1,    -1,    84,    -1,    86,    -1,    88,    89,     1,    91,
       3,     4,     5,     6,     7,    -1,     9,    10,    11,    12,
      13,    -1,    -1,    -1,    17,    -1,    19,    -1,    -1,    -1,
      -1,    24,    25,    -1,    -1,    28,    29,    -1,    -1,    -1,
      -1,    34,    35,    36,    -1,    -1,    39,    -1,    41,    -1,
      43,    44,    -1,    -1,    -1,    48,    49,    50,    -1,    -1,
      53,    -1,    -1,    -1,    -1,    58,    -1,    60,    -1,    -1,
      -1,    64,    65,    66,    -1,    68,    69,    70,    -1,    72,
      73,    -1,    75,    -1,    77,    -1,    79,    -1,    81,    -1,
      -1,    84,    -1,    86,    -1,    88,    89,     1,    91,     3,
       4,     5,     6,     7,    -1,     9,    10,    11,    12,    13,
      14,    -1,    -1,    17,    -1,    19,    -1,    -1,    -1,    -1,
      24,    25,    -1,    -1,    28,    29,    -1,    -1,    -1,    -1,
      34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,    43,
      44,    -1,    -1,    -1,    48,    49,    50,    -1,    -1,    53,
      -1,    -1,    -1,    -1,    58,    -1,    60,    -1,    -1,    -1,
      64,    65,    66,    -1,    68,    69,    70,    -1,    -1,    73,
      -1,    75,    -1,    77,    -1,    79,    -1,    81,    -1,    -1,
      84,    -1,    86,    -1,    88,    89,     1,    91,     3,     4,
       5,     6,     7,    -1,     9,    10,    11,    12,    13,    -1,
      -1,    -1,    17,    -1,    19,    -1,    -1,    -1,    -1,    24,
      25,    -1,    -1,    28,    29,    -1,    -1,    -1,    -1,    34,
      35,    36,    -1,    -1,    39,    40,    -1,    -1,    43,    44,
      -1,    -1,    -1,    48,    49,    50,    -1,    -1,    53,    -1,
      -1,    -1,    -1,    58,    -1,    60,    -1,    -1,    -1,    64,
      65,    66,    -1,    68,    69,    70,    -1,    -1,    73,    -1,
      75,    -1,    77,    -1,    79,    -1,    81,    -1,    -1,    84,
      -1,    86,    -1,    88,    89,     1,    91,     3,     4,     5,
       6,     7,    -1,     9,    10,    11,    12,    13,    -1,    -1,
      -1,    17,    18,    19,    -1,    -1,    -1,    -1,    24,    25,
      -1,    -1,    28,    29,    -1,    -1,    -1,    -1,    34,    35,
      36,    -1,    -1,    39,    -1,    -1,    -1,    43,    44,    -1,
      -1,    -1,    48,    49,    50,    -1,    -1,    53,    -1,    -1,
      -1,    -1,    58,    -1,    60,    -1,    -1,    -1,    64,    65,
      66,    -1,    68,    69,    70,    -1,    -1,    73,    -1,    75,
      -1,    77,    -1,    79,    -1,    81,    -1,    -1,    84,    -1,
      86,    -1,    88,    89,     1,    91,     3,     4,     5,     6,
       7,    -1,     9,    10,    11,    12,    13,    -1,    -1,    -1,
      17,    18,    19,    -1,    -1,    -1,    -1,    24,    25,    -1,
      -1,    28,    29,    -1,    -1,    -1,    -1,    34,    35,    36,
      -1,    -1,    39,    -1,    -1,    -1,    43,    44,    -1,    -1,
      -1,    48,    49,    50,    -1,    -1,    53,    -1,    -1,    -1,
      -1,    58,    -1,    60,    -1,    -1,    -1,    64,    65,    66,
      -1,    68,    69,    70,    -1,    -1,    73,    -1,    75,    -1,
      77,    -1,    79,    -1,    81,    -1,    -1,    84,    -1,    86,
      -1,    88,    89,     1,    91,     3,     4,     5,     6,     7,
      -1,     9,    10,    11,    12,    13,    -1,    -1,    -1,    17,
      -1,    19,    -1,    -1,    -1,    -1,    24,    25,    -1,    -1,
      28,    29,    -1,    -1,    -1,    -1,    34,    35,    36,    -1,
      -1,    39,    -1,    41,    -1,    43,    44,    -1,    -1,    -1,
      48,    49,    50,    -1,    -1,    53,    -1,    -1,    -1,    -1,
      58,    -1,    60,    -1,    -1,    -1,    64,    65,    66,    -1,
      68,    69,    70,    -1,    -1,    73,    -1,    75,    -1,    77,
      -1,    79,    -1,    81,    -1,    -1,    84,    -1,    86,    -1,
      88,    89,     1,    91,     3,     4,     5,     6,     7,    -1,
       9,    10,    11,    12,    13,    -1,    -1,    -1,    17,    -1,
      19,    -1,    -1,    -1,    -1,    24,    25,    -1,    -1,    28,
      29,    -1,    -1,    -1,    -1,    34,    35,    36,    -1,    -1,
      39,    -1,    -1,    -1,    43,    44,    -1,    -1,    -1,    48,
      49,    50,    -1,    -1,    53,    -1,    -1,    -1,    -1,    58,
      -1,    60,    -1,    -1,    -1,    64,    65,    66,    -1,    68,
      69,    70,    -1,    -1,    73,    74,    75,    -1,    77,    -1,
      79,    -1,    81,    -1,    -1,    84,    -1,    86,    -1,    88,
      89,     1,    91,     3,     4,     5,     6,     7,    -1,     9,
      10,    11,    12,    13,    -1,    -1,    -1,    17,    18,    19,
      -1,    -1,    -1,    -1,    24,    25,    -1,    -1,    28,    29,
      -1,    -1,    -1,    -1,    34,    35,    36,    -1,    -1,    39,
      -1,    -1,    -1,    43,    44,    -1,    -1,    -1,    48,    49,
      50,    -1,    -1,    53,    -1,    -1,    -1,    -1,    58,    -1,
      60,    -1,    -1,    -1,    64,    65,    66,    -1,    68,    69,
      70,    -1,    -1,    73,    -1,    75,    -1,    77,    -1,    79,
      -1,    81,    -1,    -1,    84,    -1,    86,    -1,    88,    89,
       1,    91,     3,     4,     5,     6,     7,    -1,     9,    10,
      11,    12,    13,    -1,    -1,    -1,    17,    18,    19,    -1,
      -1,    -1,    -1,    24,    25,    -1,    -1,    28,    29,    -1,
      -1,    -1,    -1,    34,    35,    36,    -1,    -1,    39,    -1,
      -1,    -1,    43,    44,    -1,    -1,    -1,    48,    49,    50,
      -1,    -1,    53,    -1,    -1,    -1,    -1,    58,    -1,    60,
      -1,    -1,    -1,    64,    65,    66,    -1,    68,    69,    70,
      -1,    -1,    73,    -1,    75,    -1,    77,    -1,    79,    -1,
      81,    -1,    -1,    84,    -1,    86,    -1,    88,    89,     1,
      91,     3,     4,     5,     6,     7,    -1,     9,    10,    11,
      12,    13,    -1,    -1,    -1,    17,    -1,    19,    -1,    -1,
      -1,    -1,    24,    25,    -1,    -1,    28,    29,    -1,    -1,
      -1,    -1,    34,    35,    36,    -1,    -1,    39,    -1,    -1,
      -1,    43,    44,    -1,    -1,    -1,    48,    49,    50,    -1,
      -1,    53,    -1,    -1,    -1,    -1,    58,    -1,    60,    -1,
      -1,    -1,    64,    65,    66,    -1,    68,    69,    70,    71,
      -1,    73,    -1,    75,    -1,    77,    -1,    79,    -1,    81,
      -1,    -1,    84,    -1,    86,    -1,    88,    89,     1,    91,
       3,     4,     5,     6,     7,     8,     9,    10,    11,    12,
      13,    -1,    -1,    -1,    17,    -1,    19,    -1,    -1,    -1,
      -1,    24,    25,    -1,    -1,    28,    29,    -1,    -1,    -1,
      -1,    34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,
      43,    44,    -1,    -1,    -1,    48,    49,    50,    -1,    -1,
      53,    -1,    -1,    -1,    -1,    58,    -1,    60,    -1,    -1,
      -1,    64,    65,    66,    -1,    68,    69,    70,    -1,    -1,
      73,    -1,    75,    -1,    77,    -1,    79,    -1,    81,    -1,
      -1,    84,    -1,    86,    -1,    88,    89,     1,    91,     3,
       4,     5,     6,     7,    -1,     9,    10,    11,    12,    13,
      -1,    -1,    -1,    17,    -1,    19,    -1,    -1,    -1,    -1,
      24,    25,    -1,    -1,    28,    29,    -1,    -1,    -1,    -1,
      34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,    43,
      44,    -1,    -1,    -1,    48,    49,    50,    -1,    -1,    53,
      -1,    -1,    -1,    -1,    58,    -1,    60,    -1,    -1,    -1,
      64,    65,    66,    -1,    68,    69,    70,    -1,    -1,    73,
      74,    75,    -1,    77,    -1,    79,    -1,    81,    -1,    -1,
      84,    -1,    86,    -1,    88,    89,     1,    91,     3,     4,
       5,     6,     7,    -1,     9,    10,    11,    12,    13,    -1,
      -1,    -1,    17,    -1,    19,    -1,    -1,    -1,    -1,    24,
      25,    -1,    -1,    28,    29,    -1,    -1,    -1,    -1,    34,
      35,    36,    -1,    -1,    39,    -1,    -1,    -1,    43,    44,
      -1,    -1,    -1,    48,    49,    50,    -1,    -1,    53,    -1,
      -1,    -1,    -1,    58,    -1,    60,    -1,    -1,    -1,    64,
      65,    66,    -1,    68,    69,    70,    71,    -1,    73,    -1,
      75,    -1,    77,    -1,    79,    -1,    81,    -1,    -1,    84,
      -1,    86,    -1,    88,    89,     1,    91,     3,     4,     5,
       6,     7,    -1,     9,    10,    11,    12,    13,    -1,    -1,
      -1,    17,    18,    19,    -1,    -1,    -1,    -1,    24,    25,
      -1,    -1,    28,    29,    -1,    -1,    -1,    -1,    34,    35,
      36,    -1,    -1,    39,    -1,    -1,    -1,    43,    44,    -1,
      -1,    -1,    48,    49,    50,    -1,    -1,    53,    -1,    -1,
      -1,    -1,    58,    -1,    60,    -1,    -1,    -1,    64,    65,
      66,    -1,    68,    69,    70,    -1,    -1,    73,    -1,    75,
      -1,    77,    -1,    79,    -1,    81,    -1,    -1,    84,    -1,
      86,    -1,    88,    89,     1,    91,     3,     4,     5,     6,
       7,    -1,     9,    10,    11,    12,    13,    -1,    -1,    -1,
      17,    -1,    19,    -1,    -1,    -1,    -1,    24,    25,    -1,
      -1,    28,    29,    -1,    -1,    -1,    -1,    34,    35,    36,
      -1,    -1,    39,    -1,    -1,    -1,    43,    44,    -1,    -1,
      -1,    48,    49,    50,    -1,    -1,    53,    -1,    -1,    -1,
      -1,    58,    -1,    60,    -1,    -1,    -1,    64,    65,    66,
      -1,    68,    69,    70,    -1,    -1,    73,    -1,    75,    -1,
      77,    -1,    79,    -1,    81,    -1,    -1,    84,    -1,    86,
      -1,    88,    89,     1,    91,     3,     4,     5,     6,     7,
      -1,     9,    10,    11,    12,    13,    -1,    -1,    -1,    17,
      -1,    19,    -1,    -1,    -1,    -1,    24,    25,    -1,    -1,
      28,    29,    -1,    -1,    -1,    -1,    34,    35,    36,    -1,
      -1,    39,    -1,    -1,    -1,    43,    44,    -1,    -1,    -1,
      48,    49,    50,    -1,    -1,    53,    -1,    -1,    -1,    -1,
      58,    -1,    60,    -1,    -1,    -1,    64,    65,    66,    -1,
      68,    69,    70,    -1,    -1,    73,    -1,    75,    -1,    77,
      -1,    79,    -1,    81,    -1,    -1,    84,    -1,    86,    -1,
      88,    89,     1,    91,     3,     4,     5,     6,     7,    -1,
       9,    10,    11,    12,    13,    -1,    -1,    -1,    17,    -1,
      19,    -1,    -1,    -1,    -1,    24,    25,    -1,    -1,    28,
      29,    -1,    -1,    -1,    -1,    34,    35,    36,    -1,    -1,
      39,    -1,    -1,    -1,    43,    44,    -1,    -1,    -1,    48,
      49,    50,    -1,    -1,    53,    -1,    -1,    -1,    -1,    58,
      -1,    60,    -1,    -1,    -1,    64,    65,    66,    -1,    68,
      69,    70,    -1,    -1,    73,    -1,    75,    -1,    77,    -1,
      79,    -1,    81,    -1,    -1,    84,    -1,    86,    -1,    88,
      89,     1,    91,     3,     4,     5,     6,     7,    -1,     9,
      10,    11,    12,    13,    -1,    -1,    -1,    17,    -1,    19,
      -1,    -1,    -1,    -1,    24,    25,    -1,    -1,    28,    29,
      -1,    -1,    -1,    -1,    34,    35,    36,    -1,    -1,    39,
      -1,    -1,    -1,    43,    44,    -1,    -1,    -1,    48,    49,
      50,    -1,    -1,    53,    -1,    -1,    -1,    -1,    58,    -1,
      60,    -1,    -1,    -1,    64,    65,    66,    -1,    68,    69,
      70,    -1,    -1,    73,    -1,    75,    -1,    77,    -1,    79,
      -1,    81,    -1,    -1,    84,    -1,    86,    -1,    88,    89,
       1,    91,     3,     4,     5,     6,     7,    -1,     9,    10,
      11,    12,    13,    -1,    -1,    -1,    17,    -1,    19,    -1,
      -1,    -1,    -1,    24,    25,    -1,    -1,    28,    29,    -1,
      -1,    -1,    -1,    34,    35,    36,    -1,    -1,    39,    -1,
      -1,    -1,    43,    44,    -1,    -1,    -1,    48,    49,    50,
      -1,    -1,    53,    -1,    -1,    -1,    -1,    58,    -1,    60,
      -1,    -1,    -1,    64,    65,    66,    -1,    68,    69,    70,
      -1,    -1,    73,    -1,    75,    -1,    77,    -1,    79,    -1,
      81,    -1,    -1,    84,    -1,    86,    -1,    88,    89,     1,
      91,     3,     4,     5,     6,     7,    -1,     9,    10,    11,
      12,    13,    -1,    -1,    -1,    17,    -1,    19,    -1,    -1,
      -1,    -1,    24,    25,    -1,    -1,    28,    29,    -1,    -1,
      -1,    -1,    34,    35,    36,    -1,    -1,    39,    -1,    -1,
      -1,    43,    44,    -1,    -1,    -1,    48,    49,    50,    -1,
      -1,    53,    -1,    -1,    -1,    -1,    58,    -1,    60,    -1,
      -1,    -1,    64,    65,    66,    -1,    68,    69,    70,    -1,
      -1,    73,    -1,    75,    -1,    77,    -1,    79,    -1,    81,
      -1,    -1,    84,    -1,    86,    -1,    88,    89,     1,    91,
       3,     4,     5,     6,     7,    -1,     9,    10,    11,    12,
      13,    -1,    -1,    -1,    17,    -1,    19,    -1,    -1,    -1,
      -1,    24,    25,    -1,    -1,    28,    29,    -1,    -1,    -1,
      -1,    34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,
      43,    44,    -1,    -1,    -1,    48,    49,    50,    -1,    -1,
      53,    -1,    -1,    -1,    -1,    58,    -1,    60,    -1,    -1,
      -1,    64,    65,    66,    -1,    68,    69,    70,    -1,    -1,
      73,    -1,    75,    -1,    77,    -1,    79,    -1,    81,    -1,
      -1,    84,    -1,    86,    -1,    88,    89,     1,    91,     3,
       4,     5,     6,     7,    -1,     9,    10,    11,    12,    13,
      -1,    -1,    -1,    17,    -1,    19,    -1,    -1,    -1,    -1,
      24,    25,    -1,    -1,    28,    29,    -1,    -1,    -1,    -1,
      34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,    43,
      44,    -1,    -1,    -1,    48,    49,    50,    -1,    -1,    53,
      -1,    -1,    -1,    -1,    58,    -1,    60,    -1,    -1,    -1,
      64,    65,    66,    -1,    68,    69,    70,    -1,    -1,    73,
      -1,    75,    -1,    77,    -1,    79,    -1,    81,    -1,    -1,
      84,    -1,    86,    -1,    88,    89,     1,    91,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    -1,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    -1,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    -1,    60,    61,    62,    -1,    64,
      65,    66,    -1,    68,    69,    70,    -1,    72,    73,    -1,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,     8,    -1,    -1,    -1,    12,
      -1,    14,    15,    16,    17,    -1,    19,    -1,    21,    22,
      23,    -1,    -1,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    -1,    -1,    -1,    41,    42,
      -1,    -1,    -1,    -1,    -1,    11,    12,    -1,    14,    15,
      16,    17,    -1,    19,    -1,    21,    22,    23,    -1,    62,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    -1,    76,    -1,    -1,    42,    -1,    -1,    82,
      83,    -1,    85,    86,    87,    51,    -1,    -1,    -1,    -1,
      12,    -1,    -1,    -1,    -1,    17,    62,    19,    -1,    21,
      22,    23,    -1,    -1,    26,    -1,    -1,    -1,    -1,    -1,
      76,    -1,    34,    35,    -1,    37,    82,    83,    -1,    85,
      86,    87,    12,    -1,    14,    15,    16,    17,    -1,    19,
      -1,    21,    22,    23,    -1,    -1,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    -1,    -1,
      40,    -1,    42,    -1,    76,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    53,    86,    87,    -1,    -1,    -1,    -1,
      -1,    -1,    62,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    76,    -1,    -1,    -1,
      -1,    -1,    82,    83,    -1,    85,    86,    87,    12,    -1,
      14,    15,    16,    17,    -1,    19,    -1,    21,    22,    23,
      -1,    -1,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    -1,    -1,    40,    -1,    42,    -1,
      44,    -1,    -1,    -1,    -1,    12,    -1,    14,    15,    16,
      17,    -1,    19,    -1,    21,    22,    23,    -1,    62,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    -1,    76,    40,    -1,    42,    -1,    -1,    82,    83,
      -1,    85,    86,    87,    -1,    12,    53,    -1,    -1,    -1,
      17,    -1,    -1,    -1,    -1,    62,    23,    -1,    -1,    26,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    34,    35,    76,
      37,    -1,    -1,    -1,    -1,    82,    83,    -1,    85,    86,
      87,    12,    -1,    14,    15,    16,    17,    18,    19,    -1,
      21,    22,    23,    -1,    -1,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    -1,    -1,    76,
      -1,    42,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    12,
      87,    14,    15,    16,    17,    -1,    19,    -1,    21,    22,
      23,    62,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    76,    -1,    -1,    -1,    42,
      -1,    82,    83,    -1,    85,    86,    87,    12,    -1,    14,
      15,    16,    17,    -1,    19,    -1,    21,    22,    23,    62,
      -1,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    76,    -1,    -1,    -1,    42,    -1,    82,
      83,    -1,    85,    86,    87,    -1,    -1,    -1,    53,    -1,
      -1,    12,    -1,    -1,    -1,    -1,    17,    62,    -1,    -1,
      -1,    22,    23,    -1,    -1,    26,    -1,    -1,    -1,    -1,
      -1,    76,    -1,    34,    35,    -1,    37,    82,    83,    -1,
      85,    86,    87,    12,    -1,    14,    15,    16,    17,    -1,
      19,    -1,    21,    22,    23,    -1,    -1,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    -1,
      -1,    -1,    -1,    42,    -1,    76,    -1,    46,    -1,    -1,
      12,    -1,    14,    15,    16,    17,    87,    19,    -1,    21,
      22,    23,    -1,    62,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    -1,    76,    -1,    -1,
      42,    -1,    -1,    82,    83,    -1,    85,    86,    87,    -1,
      -1,    -1,    -1,    -1,    56,    -1,    -1,    -1,    -1,    -1,
      62,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,
      82,    83,    -1,    85,    86,    87,    12,    -1,    14,    15,
      16,    17,    -1,    19,    -1,    21,    22,    23,    -1,    -1,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    -1,    -1,    -1,    -1,    42,    -1,    -1,    -1,
      -1,    -1,    -1,    12,    -1,    14,    15,    16,    17,    18,
      19,    -1,    21,    22,    23,    -1,    62,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    -1,
      76,    -1,    78,    42,    -1,    -1,    82,    83,    -1,    85,
      86,    87,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    17,    62,    -1,    -1,    -1,    -1,    23,    -1,
      -1,    26,    -1,    -1,    -1,    -1,    -1,    76,    -1,    34,
      35,    -1,    37,    82,    83,    -1,    85,    86,    87,    12,
      -1,    14,    15,    16,    17,    18,    19,    -1,    21,    22,
      23,    -1,    -1,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    -1,    -1,    -1,    -1,    42,
      -1,    76,    -1,    -1,    -1,    -1,    12,    -1,    14,    15,
      16,    17,    87,    19,    -1,    21,    22,    23,    -1,    62,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    -1,    76,    40,    -1,    42,    -1,    -1,    82,
      83,    -1,    85,    86,    87,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    62,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      76,    -1,    -1,    -1,    -1,    -1,    82,    83,    -1,    85,
      86,    87,    12,    -1,    14,    15,    16,    17,    18,    19,
      -1,    21,    22,    23,    -1,    -1,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    -1,    -1,
      -1,    -1,    42,    -1,    -1,    -1,    -1,    -1,    -1,    12,
      -1,    14,    15,    16,    17,    18,    19,    -1,    21,    22,
      23,    -1,    62,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    -1,    76,    -1,    -1,    42,
      -1,    -1,    82,    83,    -1,    85,    86,    87,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    62,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,    82,
      83,    -1,    85,    86,    87,    12,    -1,    14,    15,    16,
      17,    18,    19,    -1,    21,    22,    23,    -1,    -1,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    -1,    -1,    -1,    -1,    42,    -1,    -1,    -1,    -1,
      -1,    -1,    12,    -1,    14,    15,    16,    17,    18,    19,
      -1,    21,    22,    23,    -1,    62,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    -1,    76,
      -1,    -1,    42,    -1,    -1,    82,    83,    -1,    85,    86,
      87,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    62,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    76,    -1,    -1,    -1,
      -1,    -1,    82,    83,    -1,    85,    86,    87,    12,    -1,
      14,    15,    16,    17,    18,    19,    -1,    21,    22,    23,
      -1,    -1,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    -1,    -1,    -1,    -1,    42,    -1,
      -1,    -1,    -1,    -1,    -1,    12,    -1,    14,    15,    16,
      17,    18,    19,    -1,    21,    22,    23,    -1,    62,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    -1,    76,    -1,    -1,    42,    -1,    -1,    82,    83,
      -1,    85,    86,    87,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    62,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    76,
      -1,    -1,    -1,    -1,    -1,    82,    83,    -1,    85,    86,
      87,    12,    -1,    14,    15,    16,    17,    18,    19,    -1,
      21,    22,    23,    -1,    -1,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    -1,    -1,    -1,
      -1,    42,    -1,    -1,    -1,    -1,    -1,    -1,    12,    -1,
      14,    15,    16,    17,    18,    19,    -1,    21,    22,    23,
      -1,    62,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    -1,    76,    -1,    -1,    42,    -1,
      -1,    82,    83,    -1,    85,    86,    87,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    62,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,    82,    83,
      -1,    85,    86,    87,    12,    -1,    14,    15,    16,    17,
      -1,    19,    -1,    21,    22,    23,    -1,    -1,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      -1,    -1,    40,    -1,    42,    -1,    -1,    -1,    -1,    -1,
      -1,    12,    -1,    14,    15,    16,    17,    18,    19,    -1,
      21,    22,    23,    -1,    62,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    -1,    76,    -1,
      -1,    42,    -1,    -1,    82,    83,    -1,    85,    86,    87,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    62,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,
      -1,    82,    83,    -1,    85,    86,    87,    12,    -1,    14,
      15,    16,    17,    -1,    19,    -1,    21,    22,    23,    -1,
      -1,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    -1,    -1,    40,    -1,    42,    -1,    -1,
      -1,    -1,    -1,    -1,    12,    -1,    14,    15,    16,    17,
      18,    19,    -1,    21,    22,    23,    -1,    62,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      -1,    76,    -1,    -1,    42,    -1,    -1,    82,    83,    -1,
      85,    86,    87,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    62,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    76,    -1,
      -1,    -1,    -1,    -1,    82,    83,    -1,    85,    86,    87,
      12,    -1,    14,    15,    16,    17,    -1,    19,    -1,    21,
      22,    23,    -1,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    -1,    -1,    -1,    -1,
      42,    -1,    -1,    -1,    -1,    -1,    -1,    12,    -1,    14,
      15,    16,    17,    -1,    19,    -1,    21,    22,    23,    -1,
      62,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    -1,    76,    -1,    -1,    42,    -1,    -1,
      82,    83,    -1,    85,    86,    87,    -1,    -1,    -1,    -1,
      12,    56,    -1,    -1,    -1,    17,    -1,    62,    -1,    21,
      22,    23,    -1,    -1,    26,    -1,    -1,    -1,    -1,    -1,
      -1,    76,    34,    35,    -1,    37,    -1,    82,    83,    -1,
      85,    86,    87,    12,    -1,    14,    15,    16,    17,    -1,
      19,    -1,    21,    22,    23,    -1,    -1,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    -1,
      -1,    -1,    -1,    42,    76,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    86,    87,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    62,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    72,    -1,    -1,    -1,    76,    -1,    -1,
      -1,    -1,    -1,    82,    83,    -1,    85,    86,    87,    12,
      -1,    14,    15,    16,    17,    -1,    19,    -1,    21,    22,
      23,    -1,    -1,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    -1,    -1,    -1,    41,    42,
      -1,    -1,    -1,    -1,    -1,    -1,    12,    -1,    14,    15,
      16,    17,    -1,    19,    -1,    21,    22,    23,    -1,    62,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    -1,    76,    -1,    -1,    42,    -1,    -1,    82,
      83,    -1,    85,    86,    87,    -1,    -1,    -1,    -1,    -1,
      -1,    57,    -1,    -1,    -1,    -1,    62,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      76,    -1,    -1,    -1,    -1,    -1,    82,    83,    -1,    85,
      86,    87,    12,    -1,    14,    15,    16,    17,    -1,    19,
      -1,    21,    22,    23,    -1,    -1,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    -1,    -1,
      -1,    41,    42,    -1,    -1,    -1,    -1,    -1,    -1,    12,
      -1,    14,    15,    16,    17,    18,    19,    -1,    21,    22,
      23,    -1,    62,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    -1,    76,    -1,    -1,    42,
      -1,    -1,    82,    83,    -1,    85,    86,    87,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    62,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    76,    -1,    -1,    -1,    -1,    -1,    82,
      83,    -1,    85,    86,    87,    12,    -1,    14,    15,    16,
      17,    -1,    19,    -1,    21,    22,    23,    -1,    -1,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    -1,    -1,    40,    -1,    42,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    12,    -1,    14,    15,    16,    17,    -1,
      19,    -1,    21,    22,    23,    62,    -1,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    76,
      -1,    -1,    -1,    42,    -1,    82,    83,    46,    85,    86,
      87,    12,    -1,    14,    15,    16,    17,    -1,    19,    -1,
      21,    22,    23,    62,    -1,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    76,    -1,    -1,
      41,    42,    -1,    82,    83,    -1,    85,    86,    87,    12,
      -1,    14,    15,    16,    17,    -1,    19,    -1,    21,    22,
      23,    62,    -1,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    76,    -1,    40,    -1,    42,
      -1,    82,    83,    -1,    85,    86,    87,    12,    -1,    14,
      15,    16,    17,    -1,    19,    -1,    21,    22,    23,    62,
      -1,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    76,    -1,    -1,    41,    42,    -1,    82,
      83,    -1,    85,    86,    87,    12,    -1,    14,    15,    16,
      17,    -1,    19,    -1,    21,    22,    23,    62,    -1,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    76,    -1,    -1,    -1,    42,    -1,    82,    83,    -1,
      85,    86,    87,    12,    -1,    14,    15,    16,    17,    -1,
      19,    -1,    21,    22,    23,    62,    -1,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    76,
      -1,    -1,    -1,    42,    -1,    82,    83,    -1,    85,    86,
      87,    12,    -1,    14,    15,    16,    17,    -1,    19,    -1,
      21,    22,    23,    62,    -1,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    76,    -1,    -1,
      -1,    42,    -1,    82,    83,    -1,    85,    86,    87,    12,
      -1,    -1,    15,    16,    17,    -1,    19,    -1,    21,    22,
      23,    62,    -1,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    76,    -1,    -1,    -1,    42,
      -1,    -1,    83,    -1,    85,    86,    87,    12,    -1,    14,
      15,    16,    17,    -1,    19,    -1,    21,    22,    23,    62,
      -1,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    76,    -1,    -1,    -1,    42,    -1,    82,
      83,    -1,    85,    86,    87,    12,    -1,    -1,    15,    16,
      17,    -1,    19,    -1,    21,    22,    23,    62,    -1,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    76,    -1,    -1,    -1,    42,    -1,    -1,    -1,    -1,
      85,    86,    87,    12,    -1,    -1,    -1,    16,    17,    -1,
      19,    -1,    21,    22,    23,    -1,    -1,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    76,
      -1,    -1,    -1,    42,    -1,    82,    83,    -1,    85,    86,
      87,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    62,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    76,    -1,    -1,
      -1,    80,    -1,    -1,    -1,    12,    85,    86,    87,    16,
      17,    -1,    19,    -1,    21,    22,    23,    -1,    -1,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    -1,    -1,    -1,    -1,    42,    -1,    -1,    -1,    -1,
      -1,    -1,    12,    -1,    -1,    -1,    16,    17,    -1,    19,
      -1,    21,    22,    23,    -1,    62,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    -1,    76,
      -1,    -1,    42,    -1,    -1,    -1,    -1,    -1,    85,    86,
      87,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    62,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    76,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    12,    85,    86,    87,    16,    17,
      -1,    19,    -1,    21,    22,    23,    -1,    -1,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      -1,    -1,    -1,    -1,    42,    -1,    -1,    -1,    -1,    -1,
      -1,    12,    -1,    -1,    -1,    16,    17,    -1,    19,    -1,
      21,    22,    23,    -1,    62,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    -1,    76,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    86,    87,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    62,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    12,    -1,    76,    -1,    -1,    17,    -1,
      19,    -1,    21,    22,    23,    86,    87,    26,    27,    28,
      29,    30,    31,    32,    12,    34,    35,    36,    37,    17,
      -1,    19,    -1,    21,    22,    23,    -1,    -1,    26,    27,
      28,    29,    30,    31,    32,    -1,    34,    35,    36,    37,
      -1,    -1,    -1,    62,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    76,    -1,    -1,
      -1,    -1,    -1,    -1,    62,    -1,    -1,    86,    87,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    12,    -1,    76,    -1,
      -1,    17,    -1,    19,    -1,    21,    22,    23,    86,    87,
      26,    27,    28,    29,    30,    31,    32,    12,    34,    35,
      -1,    37,    17,    -1,    19,    -1,    21,    22,    23,    -1,
      -1,    26,    -1,    -1,    29,    30,    31,    32,    -1,    34,
      35,    -1,    37,    -1,    12,    -1,    -1,    -1,    -1,    17,
      -1,    19,    -1,    21,    22,    23,    -1,    -1,    26,    -1,
      76,    29,    -1,    31,    32,    -1,    34,    35,    -1,    37,
      86,    87,    12,    -1,    -1,    -1,    -1,    17,    -1,    12,
      -1,    76,    22,    23,    17,    -1,    26,    -1,    -1,    22,
      23,    86,    87,    26,    34,    35,    -1,    37,    -1,    -1,
      -1,    34,    35,    -1,    37,    -1,    -1,    -1,    76,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    86,    87,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    76,    -1,    -1,    -1,
      -1,    -1,    -1,    76,    -1,    -1,    86,    87,    -1,    -1,
      -1,    -1,    -1,    86,    87
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     1,     3,     4,     5,     6,     7,     9,    10,    11,
      12,    13,    17,    19,    24,    25,    28,    29,    34,    35,
      36,    39,    43,    44,    48,    49,    50,    53,    58,    60,
      64,    65,    66,    68,    69,    70,    73,    75,    77,    79,
      81,    84,    86,    88,    89,    91,    95,    96,    97,    99,
       4,     5,    10,    43,   100,    43,    15,    17,    17,    37,
      97,     4,    97,    97,    13,    73,    97,    97,    14,    97,
       4,    97,    97,   106,     4,    17,    97,    17,    17,    17,
      97,     3,     4,    10,    13,    17,    89,    98,    99,    17,
      97,    97,   107,    71,   112,    17,    17,    99,    17,    97,
      17,    37,    97,    97,    97,    97,    97,   107,    17,    17,
      97,     0,     8,    12,    14,    15,    16,    17,    19,    21,
      22,    23,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    41,    42,    62,    76,    82,    83,
      85,    86,    87,    17,    17,     4,    23,    87,     4,    10,
      13,    25,    89,    97,    18,    97,    18,    97,    97,    18,
      25,    25,    25,    97,    53,    40,    97,    46,    97,    97,
       4,    11,    51,   117,    97,   105,    62,    15,    28,    54,
     109,    97,    56,    41,    61,    97,   118,   101,   107,    67,
      97,    97,   106,    17,    97,    97,    97,    78,    14,    46,
      97,    18,    62,    97,    97,    97,   112,   106,    97,    97,
      97,    97,    97,    99,    97,    97,    97,    97,    97,    97,
      97,    97,    97,    99,    39,    97,     8,    96,    97,    97,
       6,    10,    14,    19,    21,    82,    86,    99,    97,    97,
      97,   106,    97,     3,    97,    18,    18,    40,    17,     3,
       4,    10,    13,    17,    99,   103,   104,    18,   107,   112,
      18,    18,    18,     3,   107,    52,    18,    41,    97,    97,
      97,    97,    55,    18,   107,    97,    74,   102,   107,    72,
      17,    18,    18,   106,    18,    18,    40,    97,   107,    18,
      97,    18,    27,    22,    62,    97,    40,    97,    17,    37,
      18,    18,    25,   106,    15,    28,    42,    43,   104,    14,
      62,    17,    97,   112,    45,    47,    72,   113,   114,   115,
      47,    71,    56,    97,   105,    56,    97,    97,   112,    72,
      72,   103,    72,    97,   101,    18,    80,    47,    72,    97,
      97,    40,   106,    39,    97,    15,    69,    23,    18,    97,
      97,    97,    97,    18,   104,    97,    97,    41,   111,   115,
      97,   107,   112,    50,    51,   116,   107,    41,   107,    57,
     108,    41,    41,    18,    71,   107,    71,   101,   107,   107,
      40,    18,    97,    40,    69,    97,   112,   101,     3,    40,
      44,   111,    97,   112,    46,   114,     3,    42,    72,    11,
     117,   105,    47,    72,    97,    41,    56,   110,   112,   107,
      72,   101,   107,    72,    72,    40,   101,   107,    97,    41,
     107,    42,   112,    18,   107,   107,    72,   107,    72,    40,
     107,    72,    40,   113,   112,    97,   112,    72,    72,    72,
      72,   116,    41
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    94,    95,    96,    96,    96,    97,    97,    97,    97,
      97,    97,    97,    97,    97,    97,    97,    97,    97,    97,
      97,    97,    97,    97,    97,    97,    97,    97,    97,    97,
      97,    97,    97,    97,    97,    97,    97,    97,    97,    97,
      97,    97,    97,    97,    97,    97,    97,    97,    97,    97,
      97,    97,    97,    97,    97,    97,    97,    97,    97,    97,
      97,    97,    97,    97,    97,    97,    97,    97,    97,    97,
      97,    97,    97,    97,    97,    97,    97,    97,    97,    97,
      97,    97,    97,    97,    97,    97,    97,    97,    97,    97,
      97,    97,    97,    97,    97,    97,    97,    97,    97,    97,
      97,    97,    97,    97,    97,    97,    97,    97,    97,    97,
      97,    97,    97,    97,    97,    97,    97,    97,    97,    97,
      97,    97,    97,    97,    97,    97,    97,    97,    97,    97,
      97,    97,    97,    97,    97,    97,    97,    97,    97,    97,
      97,    97,    97,    98,    98,    98,    99,    99,    99,    99,
      99,    99,    99,    99,    99,    99,   100,   100,   101,   101,
     102,   103,   103,   104,   104,   104,   104,   104,   104,   104,
     104,   104,   105,   105,   106,   106,   107,   107,   107,   108,
     108,   109,   109,   109,   109,   110,   110,   111,   111,   111,
     112,   112,   113,   113,   113,   114,   115,   116,   116,   116,
     117,   117,   117,   118
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     2,     3,     3,     1,     2,     4,     3,
       5,     7,     1,     1,     6,     6,     6,     6,     8,     3,
       3,     3,     3,     3,     3,     4,     4,     4,     1,     1,
       1,     3,     4,     3,     3,     3,     5,     3,     2,     3,
       3,     3,     3,     3,     2,     2,     3,     3,     3,     2,
       5,     3,     1,     3,     2,     4,     4,     1,     4,     4,
       4,     3,     1,     2,     2,     6,     4,     5,     4,     1,
       3,     3,     4,     1,     2,     1,     3,     2,     1,     3,
       1,     1,     7,     9,     9,     1,     4,     5,     7,     5,
       1,     3,     2,     5,     3,     3,     2,     3,     1,     3,
       4,     3,     3,     3,     1,     3,     4,     6,     6,     3,
       3,     3,     2,     3,     2,     3,     2,     1,     6,     7,
       7,     8,     7,     9,     8,     8,     9,     9,    10,     4,
       3,     4,     7,     9,     5,     6,     5,     7,     4,     1,
       7,     4,     4,     1,     1,     1,     1,     3,     3,     3,
       3,     5,     2,     3,     2,     2,     1,     1,     0,     2,
       3,     1,     3,     1,     3,     3,     3,     3,     1,     3,
       1,     1,     0,     1,     0,     1,     1,     2,     2,     0,
       2,     0,     2,     2,     2,     1,     1,     0,     3,     2,
       3,     4,     1,     3,     5,     1,     1,     0,     3,     5,
       0,     2,     5,     1
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
do                                                              \
  if (yychar == YYEMPTY)                                        \
    {                                                           \
      yychar = (Token);                                         \
      yylval = (Value);                                         \
      YYPOPSTACK (yylen);                                       \
      yystate = *yyssp;                                         \
      goto yybackup;                                            \
    }                                                           \
  else                                                          \
    {                                                           \
      yyerror (scanner, YY_("syntax error: cannot back up")); \
      YYERROR;                                                  \
    }                                                           \
while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256



/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value, scanner); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, void * scanner)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  YYUSE (scanner);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, void * scanner)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep, scanner);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, int yyrule, void * scanner)
{
  unsigned long int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &(yyvsp[(yyi + 1) - (yynrhs)])
                                              , scanner);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule, scanner); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
yystrlen (const char *yystr)
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            /* Fall through.  */
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, void * scanner)
{
  YYUSE (yyvaluep);
  YYUSE (scanner);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/*----------.
| yyparse.  |
`----------*/

int
yyparse (void * scanner)
{
/* The lookahead symbol.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

    /* Number of syntax errors so far.  */
    int yynerrs;

    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        YYSTYPE *yyvs1 = yyvs;
        yytype_int16 *yyss1 = yyss;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * sizeof (*yyssp),
                    &yyvs1, yysize * sizeof (*yyvsp),
                    &yystacksize);

        yyss = yyss1;
        yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yytype_int16 *yyss1 = yyss;
        union yyalloc *yyptr =
          (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
                  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex (&yylval,YYLEX_PARAM);
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:
#line 171 "input_parser.yy" /* yacc.c:1646  */
    {   const giac::context * contextptr = giac_yyget_extra(scanner);
			    if ((yyvsp[0])._VECTptr->size()==1)
			     parsed_gen((yyvsp[0])._VECTptr->front(),contextptr);
                          else
			     parsed_gen(gen(*(yyvsp[0])._VECTptr,_SEQ__VECT),contextptr);
			 }
#line 3270 "y.tab.c" /* yacc.c:1646  */
    break;

  case 3:
#line 179 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=vecteur(1,(yyvsp[-1])); }
#line 3276 "y.tab.c" /* yacc.c:1646  */
    break;

  case 4:
#line 180 "input_parser.yy" /* yacc.c:1646  */
    { if ((yyvsp[-1]).val==1) (yyval)=vecteur(1,symbolic(at_nodisp,(yyvsp[-2]))); else (yyval)=vecteur(1,(yyvsp[-2])); }
#line 3282 "y.tab.c" /* yacc.c:1646  */
    break;

  case 5:
#line 181 "input_parser.yy" /* yacc.c:1646  */
    { if ((yyvsp[-1]).val==1) (yyval)=mergevecteur(makevecteur(symbolic(at_nodisp,(yyvsp[-2]))),*(yyvsp[0])._VECTptr); else (yyval)=mergevecteur(makevecteur((yyvsp[-2])),*(yyvsp[0])._VECTptr); }
#line 3288 "y.tab.c" /* yacc.c:1646  */
    break;

  case 6:
#line 184 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = (yyvsp[0]);}
#line 3294 "y.tab.c" /* yacc.c:1646  */
    break;

  case 7:
#line 185 "input_parser.yy" /* yacc.c:1646  */
    {if (is_one((yyvsp[-1]))) (yyval)=(yyvsp[0]); else (yyval)=symbolic(at_prod,gen(makevecteur((yyvsp[-1]),(yyvsp[0])),_SEQ__VECT));}
#line 3300 "y.tab.c" /* yacc.c:1646  */
    break;

  case 8:
#line 186 "input_parser.yy" /* yacc.c:1646  */
    {if (is_one((yyvsp[-3]))) (yyval)=symb_pow((yyvsp[-2]),(yyvsp[0])); else (yyval)=symbolic(at_prod,gen(makevecteur((yyvsp[-3]),symb_pow((yyvsp[-2]),(yyvsp[0]))),_SEQ__VECT));}
#line 3306 "y.tab.c" /* yacc.c:1646  */
    break;

  case 9:
#line 187 "input_parser.yy" /* yacc.c:1646  */
    {(yyval)=symbolic(at_prod,gen(makevecteur((yyvsp[-2]),symb_pow((yyvsp[-1]),(yyvsp[0]))) ,_SEQ__VECT));}
#line 3312 "y.tab.c" /* yacc.c:1646  */
    break;

  case 10:
#line 188 "input_parser.yy" /* yacc.c:1646  */
    { (yyval) =(yyvsp[-4])*symbolic(*(yyvsp[-3])._FUNCptr,python_compat(giac_yyget_extra(scanner))?denest_sto((yyvsp[-1])):(yyvsp[-1])); }
#line 3318 "y.tab.c" /* yacc.c:1646  */
    break;

  case 11:
#line 189 "input_parser.yy" /* yacc.c:1646  */
    { (yyval) =(yyvsp[-6])*symb_pow(symbolic(*(yyvsp[-5])._FUNCptr,python_compat(giac_yyget_extra(scanner))?denest_sto((yyvsp[-3])):(yyvsp[-3])),(yyvsp[0])); }
#line 3324 "y.tab.c" /* yacc.c:1646  */
    break;

  case 12:
#line 191 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=(yyvsp[0]); }
#line 3330 "y.tab.c" /* yacc.c:1646  */
    break;

  case 13:
#line 192 "input_parser.yy" /* yacc.c:1646  */
    { if ((yyvsp[0]).type==_FUNC) (yyval)=symbolic(*(yyvsp[0])._FUNCptr,gen(vecteur(0),_SEQ__VECT)); else (yyval)=(yyvsp[0]); }
#line 3336 "y.tab.c" /* yacc.c:1646  */
    break;

  case 14:
#line 195 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = symb_program_sto((yyvsp[-3]),(yyvsp[-3])*zero,(yyvsp[0]),(yyvsp[-5]),false,giac_yyget_extra(scanner));}
#line 3342 "y.tab.c" /* yacc.c:1646  */
    break;

  case 15:
#line 196 "input_parser.yy" /* yacc.c:1646  */
    {if (is_array_index((yyvsp[-5]),(yyvsp[-3]),giac_yyget_extra(scanner)) || (abs_calc_mode(giac_yyget_extra(scanner))==38 && (yyvsp[-5]).type==_IDNT && strlen((yyvsp[-5])._IDNTptr->id_name)==2 && check_vect_38((yyvsp[-5])._IDNTptr->id_name))) (yyval)=symbolic(at_sto,makesequence((yyvsp[0]),symbolic(at_of,makesequence((yyvsp[-5]),(yyvsp[-3]))))); else { (yyval) = symb_program_sto((yyvsp[-3]),(yyvsp[-3])*zero,(yyvsp[0]),(yyvsp[-5]),true,giac_yyget_extra(scanner)); (yyval)._SYMBptr->feuille.subtype=_SORTED__VECT;  } }
#line 3348 "y.tab.c" /* yacc.c:1646  */
    break;

  case 16:
#line 197 "input_parser.yy" /* yacc.c:1646  */
    {if (is_array_index((yyvsp[-3]),(yyvsp[-1]),giac_yyget_extra(scanner)) || (abs_calc_mode(giac_yyget_extra(scanner))==38 && (yyvsp[-3]).type==_IDNT && check_vect_38((yyvsp[-3])._IDNTptr->id_name))) (yyval)=symbolic(at_sto,makesequence((yyvsp[-5]),symbolic(at_of,makesequence((yyvsp[-3]),(yyvsp[-1]))))); else (yyval) = symb_program_sto((yyvsp[-1]),(yyvsp[-1])*zero,(yyvsp[-5]),(yyvsp[-3]),false,giac_yyget_extra(scanner));}
#line 3354 "y.tab.c" /* yacc.c:1646  */
    break;

  case 17:
#line 198 "input_parser.yy" /* yacc.c:1646  */
    { 
         const giac::context * contextptr = giac_yyget_extra(scanner);
         gen g=symb_at((yyvsp[-3]),(yyvsp[-1]),contextptr); (yyval)=symb_sto((yyvsp[-5]),g); 
        }
#line 3363 "y.tab.c" /* yacc.c:1646  */
    break;

  case 18:
#line 202 "input_parser.yy" /* yacc.c:1646  */
    { 
         const giac::context * contextptr = giac_yyget_extra(scanner);
         gen g=symbolic(at_of,makesequence((yyvsp[-5]),(yyvsp[-2]))); (yyval)=symb_sto((yyvsp[-7]),g); 
        }
#line 3372 "y.tab.c" /* yacc.c:1646  */
    break;

  case 19:
#line 206 "input_parser.yy" /* yacc.c:1646  */
    { if ((yyvsp[0]).type==_IDNT) { const char * ch=(yyvsp[0]).print(context0).c_str(); if (ch[0]=='_' && unit_conversion_map().find(ch+1) != unit_conversion_map().end()) (yyval)=symbolic(at_convert,gen(makevecteur((yyvsp[-2]),symbolic(at_unit,makevecteur(1,(yyvsp[0])))) ,_SEQ__VECT)); else (yyval)=symb_sto((yyvsp[-2]),(yyvsp[0])); } else (yyval)=symb_sto((yyvsp[-2]),(yyvsp[0])); }
#line 3378 "y.tab.c" /* yacc.c:1646  */
    break;

  case 20:
#line 207 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symbolic(at_convert,makesequence((yyvsp[-2]),(yyvsp[0]))); }
#line 3384 "y.tab.c" /* yacc.c:1646  */
    break;

  case 21:
#line 208 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symbolic(at_convert,makesequence((yyvsp[-2]),(yyvsp[0]))); }
#line 3390 "y.tab.c" /* yacc.c:1646  */
    break;

  case 22:
#line 209 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symbolic(at_convert,makesequence((yyvsp[-2]),(yyvsp[0]))); }
#line 3396 "y.tab.c" /* yacc.c:1646  */
    break;

  case 23:
#line 210 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symbolic(at_time,(yyvsp[-2]));}
#line 3402 "y.tab.c" /* yacc.c:1646  */
    break;

  case 24:
#line 211 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symbolic(at_solve,symb_equal((yyvsp[-2]),0));}
#line 3408 "y.tab.c" /* yacc.c:1646  */
    break;

  case 25:
#line 212 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symbolic(at_convert,gen(makevecteur((yyvsp[-3]),symb_unit(gen(1),(yyvsp[0]),giac_yyget_extra(scanner))),_SEQ__VECT)); opened_quote(giac_yyget_extra(scanner)) &= 0x7ffffffd;}
#line 3414 "y.tab.c" /* yacc.c:1646  */
    break;

  case 26:
#line 213 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = check_symb_of((yyvsp[-3]),python_compat(giac_yyget_extra(scanner))?denest_sto((yyvsp[-1])):(yyvsp[-1]),giac_yyget_extra(scanner));}
#line 3420 "y.tab.c" /* yacc.c:1646  */
    break;

  case 27:
#line 214 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = check_symb_of((yyvsp[-3]),python_compat(giac_yyget_extra(scanner))?denest_sto((yyvsp[-1])):(yyvsp[-1]),giac_yyget_extra(scanner));}
#line 3426 "y.tab.c" /* yacc.c:1646  */
    break;

  case 28:
#line 215 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = (yyvsp[0]);}
#line 3432 "y.tab.c" /* yacc.c:1646  */
    break;

  case 29:
#line 216 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = (yyvsp[0]);}
#line 3438 "y.tab.c" /* yacc.c:1646  */
    break;

  case 30:
#line 217 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = (yyvsp[0]);}
#line 3444 "y.tab.c" /* yacc.c:1646  */
    break;

  case 31:
#line 218 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = symbolic(*(yyvsp[-2])._FUNCptr,(yyvsp[0]));}
#line 3450 "y.tab.c" /* yacc.c:1646  */
    break;

  case 32:
#line 219 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = symbolic(*(yyvsp[-3])._FUNCptr,(yyvsp[-1]));}
#line 3456 "y.tab.c" /* yacc.c:1646  */
    break;

  case 33:
#line 220 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = symbolic(*(yyvsp[-2])._FUNCptr,gen(vecteur(0),_SEQ__VECT));}
#line 3462 "y.tab.c" /* yacc.c:1646  */
    break;

  case 34:
#line 221 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = symbolic(*(yyvsp[0])._FUNCptr,(yyvsp[-2]));}
#line 3468 "y.tab.c" /* yacc.c:1646  */
    break;

  case 35:
#line 222 "input_parser.yy" /* yacc.c:1646  */
    {if (is_inequation((yyvsp[-2])) || ((yyvsp[-2]).is_symb_of_sommet(at_same) && ((yyvsp[0]).type!=_INT_ || (yyvsp[0]).subtype!=_INT_BOOLEAN))){ (yyval) = symb_and((yyvsp[-2]),symbolic(*(yyvsp[-1])._FUNCptr,makesequence((yyvsp[-2])._SYMBptr->feuille[1],(yyvsp[0]))));} else (yyval) = symbolic(*(yyvsp[-1])._FUNCptr,makesequence((yyvsp[-2]),(yyvsp[0])));}
#line 3474 "y.tab.c" /* yacc.c:1646  */
    break;

  case 36:
#line 223 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = symb_and(symbolic(*(yyvsp[-3])._FUNCptr,makesequence((yyvsp[-4]),(yyvsp[-2]))),symbolic(*(yyvsp[-1])._FUNCptr,makesequence((yyvsp[-2]),(yyvsp[0]))));}
#line 3480 "y.tab.c" /* yacc.c:1646  */
    break;

  case 37:
#line 224 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = symbolic(*(yyvsp[-1])._FUNCptr,makesequence((yyvsp[-2]),(yyvsp[0]))); }
#line 3486 "y.tab.c" /* yacc.c:1646  */
    break;

  case 38:
#line 225 "input_parser.yy" /* yacc.c:1646  */
    { 
	if ((yyvsp[0]).type==_SYMB) (yyval)=(yyvsp[0]); else (yyval)=symbolic(at_nop,(yyvsp[0])); 
	(yyval).change_subtype(_SPREAD__SYMB); 
        const giac::context * contextptr = giac_yyget_extra(scanner);
       spread_formula(false,contextptr); 
	}
#line 3497 "y.tab.c" /* yacc.c:1646  */
    break;

  case 39:
#line 231 "input_parser.yy" /* yacc.c:1646  */
    { if ((yyvsp[-2]).is_symb_of_sommet(at_plus) && (yyvsp[-2])._SYMBptr->feuille.type==_VECT && (yyvsp[-1])==at_plus){ (yyvsp[-2])._SYMBptr->feuille._VECTptr->push_back((yyvsp[0])); (yyval)=(yyvsp[-2]); } else
  (yyval) =symbolic(*(yyvsp[-1])._FUNCptr,makesequence((yyvsp[-2]),(yyvsp[0])));}
#line 3504 "y.tab.c" /* yacc.c:1646  */
    break;

  case 40:
#line 233 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) =symbolic(*(yyvsp[-1])._FUNCptr,makesequence((yyvsp[-2]),(yyvsp[0])));}
#line 3510 "y.tab.c" /* yacc.c:1646  */
    break;

  case 41:
#line 234 "input_parser.yy" /* yacc.c:1646  */
    {if ((yyvsp[-2])==symbolic(at_exp,1) && (yyvsp[-1])==at_pow) (yyval)=symbolic(at_exp,(yyvsp[0])); else (yyval) =symbolic(*(yyvsp[-1])._FUNCptr,makesequence((yyvsp[-2]),(yyvsp[0])));}
#line 3516 "y.tab.c" /* yacc.c:1646  */
    break;

  case 42:
#line 235 "input_parser.yy" /* yacc.c:1646  */
    {if ((yyvsp[-1]).type==_FUNC) (yyval)=symbolic(*(yyvsp[-1])._FUNCptr,makesequence((yyvsp[-2]),(yyvsp[0]))); else (yyval) = symbolic(at_normalmod,makesequence((yyvsp[-2]),(yyvsp[0])));}
#line 3522 "y.tab.c" /* yacc.c:1646  */
    break;

  case 43:
#line 236 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = symbolic(*(yyvsp[-1])._FUNCptr,makesequence((yyvsp[-2]),(yyvsp[0]))); }
#line 3528 "y.tab.c" /* yacc.c:1646  */
    break;

  case 44:
#line 237 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = symbolic(*(yyvsp[0])._FUNCptr,makesequence((yyvsp[-1]),RAND_MAX)); }
#line 3534 "y.tab.c" /* yacc.c:1646  */
    break;

  case 45:
#line 238 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = symbolic(*(yyvsp[-1])._FUNCptr,makesequence(0,(yyvsp[0]))); }
#line 3540 "y.tab.c" /* yacc.c:1646  */
    break;

  case 46:
#line 239 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = makesequence(symbolic(*(yyvsp[-2])._FUNCptr,makesequence(0,RAND_MAX)),(yyvsp[0])); }
#line 3546 "y.tab.c" /* yacc.c:1646  */
    break;

  case 47:
#line 241 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = symbolic(*(yyvsp[-1])._FUNCptr,makesequence((yyvsp[-2]),(yyvsp[0])));}
#line 3552 "y.tab.c" /* yacc.c:1646  */
    break;

  case 48:
#line 242 "input_parser.yy" /* yacc.c:1646  */
    {(yyval)= symbolic(at_deuxpoints,makesequence((yyvsp[-2]),(yyvsp[0])) );}
#line 3558 "y.tab.c" /* yacc.c:1646  */
    break;

  case 49:
#line 243 "input_parser.yy" /* yacc.c:1646  */
    { 
					if ((yyvsp[0])==unsigned_inf || (yyvsp[0])==plus_inf){
            (yyval) = (yyvsp[-1])==at_binary_minus?minus_inf:plus_inf;
					}
					else {
           if ((yyvsp[-1])==at_binary_minus)
             if ((yyvsp[0]).type==_INT_) (yyval)=(-(yyvsp[0]).val); else { if ((yyvsp[0]).type==_DOUBLE_) (yyval)=(-(yyvsp[0])._DOUBLE_val); else (yyval)=symbolic(at_neg,(yyvsp[0])); }
           else
             (yyval)=(yyvsp[0]);
					   }
				}
#line 3572 "y.tab.c" /* yacc.c:1646  */
    break;

  case 50:
#line 252 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = polynome_or_sparse_poly1(eval((yyvsp[-3]),1, giac_yyget_extra(scanner)),(yyvsp[-1]));}
#line 3578 "y.tab.c" /* yacc.c:1646  */
    break;

  case 51:
#line 253 "input_parser.yy" /* yacc.c:1646  */
    { 
           if ( ((yyvsp[-1]).type==_SYMB) && ((yyvsp[-1])._SYMBptr->sommet==at_deuxpoints) )
             (yyval) = algebraic_EXTension((yyvsp[-1])._SYMBptr->feuille._VECTptr->front(),(yyvsp[-1])._SYMBptr->feuille._VECTptr->back());
           else (yyval)=(yyvsp[-1]);
        }
#line 3588 "y.tab.c" /* yacc.c:1646  */
    break;

  case 52:
#line 258 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=gen(at_of,2); }
#line 3594 "y.tab.c" /* yacc.c:1646  */
    break;

  case 53:
#line 259 "input_parser.yy" /* yacc.c:1646  */
    {if ((yyvsp[-2]).type==_FUNC) *logptr(giac_yyget_extra(scanner))<< ("Warning: "+(yyvsp[-2]).print(context0)+" is a reserved word")<<endl; if ((yyvsp[-2]).type==_INT_) (yyval)=symb_equal((yyvsp[-2]),(yyvsp[0])); else {(yyval) = symb_sto((yyvsp[0]),(yyvsp[-2]),(yyvsp[-1])==at_array_sto); if ((yyvsp[0]).is_symb_of_sommet(at_program)) *logptr(giac_yyget_extra(scanner))<<"// End defining "<<(yyvsp[-2])<<endl;}}
#line 3600 "y.tab.c" /* yacc.c:1646  */
    break;

  case 54:
#line 260 "input_parser.yy" /* yacc.c:1646  */
    { (yyval) = symbolic(*(yyvsp[-1])._FUNCptr,(yyvsp[0]));}
#line 3606 "y.tab.c" /* yacc.c:1646  */
    break;

  case 55:
#line 261 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = symb_args((yyvsp[-1]));}
#line 3612 "y.tab.c" /* yacc.c:1646  */
    break;

  case 56:
#line 262 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = symb_args((yyvsp[-1]));}
#line 3618 "y.tab.c" /* yacc.c:1646  */
    break;

  case 57:
#line 263 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symb_args(vecteur(0)); }
#line 3624 "y.tab.c" /* yacc.c:1646  */
    break;

  case 58:
#line 264 "input_parser.yy" /* yacc.c:1646  */
    {
	gen tmp=python_compat(giac_yyget_extra(scanner))?denest_sto((yyvsp[-1])):(yyvsp[-1]);
	// CERR << python_compat(giac_yyget_extra(scanner)) << tmp << endl;
	(yyval) = symbolic(*(yyvsp[-3])._FUNCptr,tmp);
        const giac::context * contextptr = giac_yyget_extra(scanner);
	if (*(yyvsp[-3])._FUNCptr==at_maple_mode ||*(yyvsp[-3])._FUNCptr==at_xcas_mode ){
          xcas_mode(contextptr)=(yyvsp[-1]).val;
        }
	}
#line 3638 "y.tab.c" /* yacc.c:1646  */
    break;

  case 59:
#line 273 "input_parser.yy" /* yacc.c:1646  */
    {
	if ((yyvsp[-1]).type==_VECT && (yyvsp[-1])._VECTptr->empty())
          giac_yyerror(scanner,"void argument");
	(yyval) = symbolic(*(yyvsp[-3])._FUNCptr,python_compat(giac_yyget_extra(scanner))?denest_sto((yyvsp[-1])):(yyvsp[-1]));	
	}
#line 3648 "y.tab.c" /* yacc.c:1646  */
    break;

  case 60:
#line 278 "input_parser.yy" /* yacc.c:1646  */
    { 
          const giac::context * contextptr = giac_yyget_extra(scanner);
          (yyval)=symb_at((yyvsp[-3]),(yyvsp[-1]),contextptr);
        }
#line 3657 "y.tab.c" /* yacc.c:1646  */
    break;

  case 61:
#line 282 "input_parser.yy" /* yacc.c:1646  */
    {
	(yyval) = symbolic(*(yyvsp[-2])._FUNCptr,gen(vecteur(0),_SEQ__VECT));
	if (*(yyvsp[-2])._FUNCptr==at_rpn)
          rpn_mode(giac_yyget_extra(scanner))=1;
	if (*(yyvsp[-2])._FUNCptr==at_alg)
          rpn_mode(giac_yyget_extra(scanner))=0;
	}
#line 3669 "y.tab.c" /* yacc.c:1646  */
    break;

  case 62:
#line 289 "input_parser.yy" /* yacc.c:1646  */
    {
	(yyval) = (yyvsp[0]);
	}
#line 3677 "y.tab.c" /* yacc.c:1646  */
    break;

  case 63:
#line 292 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = symbolic(at_derive,(yyvsp[-1]));}
#line 3683 "y.tab.c" /* yacc.c:1646  */
    break;

  case 64:
#line 293 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symbolic(*(yyvsp[0])._FUNCptr,(yyvsp[-1])); }
#line 3689 "y.tab.c" /* yacc.c:1646  */
    break;

  case 65:
#line 294 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = symbolic((yyvsp[-5]).type==_FUNC?*(yyvsp[-5])._FUNCptr:*at_ifte,makevecteur(equaltosame((yyvsp[-4])),symb_bloc((yyvsp[-2])),symb_bloc((yyvsp[0]))));}
#line 3695 "y.tab.c" /* yacc.c:1646  */
    break;

  case 66:
#line 295 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = symbolic((yyvsp[-3]).type==_FUNC?*(yyvsp[-3])._FUNCptr:*at_ifte,makevecteur(equaltosame((yyvsp[-2])),(yyvsp[0]),0));}
#line 3701 "y.tab.c" /* yacc.c:1646  */
    break;

  case 67:
#line 296 "input_parser.yy" /* yacc.c:1646  */
    {
	(yyval) = symbolic((yyvsp[-4]).type==_FUNC?*(yyvsp[-4])._FUNCptr:*at_ifte,makevecteur(equaltosame((yyvsp[-3])),symb_bloc((yyvsp[-1])),(yyvsp[0])));
	}
#line 3709 "y.tab.c" /* yacc.c:1646  */
    break;

  case 68:
#line 299 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = symbolic((yyvsp[-3]).type==_FUNC?*(yyvsp[-3])._FUNCptr:*at_ifte,(yyvsp[-1]));}
#line 3715 "y.tab.c" /* yacc.c:1646  */
    break;

  case 69:
#line 300 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = (yyvsp[0]);}
#line 3721 "y.tab.c" /* yacc.c:1646  */
    break;

  case 70:
#line 301 "input_parser.yy" /* yacc.c:1646  */
    {
          const giac::context * contextptr = giac_yyget_extra(scanner);
         (yyval) = symb_program((yyvsp[-2]),zero*(yyvsp[-2]),(yyvsp[0]),contextptr);
        }
#line 3730 "y.tab.c" /* yacc.c:1646  */
    break;

  case 71:
#line 305 "input_parser.yy" /* yacc.c:1646  */
    {
          const giac::context * contextptr = giac_yyget_extra(scanner);
             if ((yyvsp[0]).type==_VECT) 
                (yyval) = symb_program((yyvsp[-2]),zero*(yyvsp[-2]),symb_bloc(makevecteur(at_nop,(yyvsp[0]))),contextptr); 
             else 
                (yyval) = symb_program((yyvsp[-2]),zero*(yyvsp[-2]),(yyvsp[0]),contextptr);
		}
#line 3742 "y.tab.c" /* yacc.c:1646  */
    break;

  case 72:
#line 312 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = symb_bloc((yyvsp[-1]));}
#line 3748 "y.tab.c" /* yacc.c:1646  */
    break;

  case 73:
#line 313 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = at_bloc;}
#line 3754 "y.tab.c" /* yacc.c:1646  */
    break;

  case 74:
#line 314 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symbolic(*(yyvsp[-1])._FUNCptr,(yyvsp[0])); }
#line 3760 "y.tab.c" /* yacc.c:1646  */
    break;

  case 75:
#line 315 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = symbolic(*(yyvsp[0])._FUNCptr,gen(vecteur(0),_SEQ__VECT));}
#line 3766 "y.tab.c" /* yacc.c:1646  */
    break;

  case 76:
#line 316 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = symbolic(*(yyvsp[-2])._FUNCptr,gen(vecteur(0),_SEQ__VECT));}
#line 3772 "y.tab.c" /* yacc.c:1646  */
    break;

  case 77:
#line 318 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symbolic(*(yyvsp[-1])._FUNCptr,(yyvsp[0])); }
#line 3778 "y.tab.c" /* yacc.c:1646  */
    break;

  case 78:
#line 320 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = gen(*(yyvsp[0])._FUNCptr,0);}
#line 3784 "y.tab.c" /* yacc.c:1646  */
    break;

  case 79:
#line 321 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=(yyvsp[-1]);}
#line 3790 "y.tab.c" /* yacc.c:1646  */
    break;

  case 80:
#line 323 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = symbolic(at_break,zero);}
#line 3796 "y.tab.c" /* yacc.c:1646  */
    break;

  case 81:
#line 324 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = symbolic(at_continue,zero);}
#line 3802 "y.tab.c" /* yacc.c:1646  */
    break;

  case 82:
#line 325 "input_parser.yy" /* yacc.c:1646  */
    { 
          if ((yyvsp[0]).type==_INT_ && (yyvsp[0]).val && (yyvsp[0]).val!=2 && (yyvsp[0]).val!=9)
	    giac_yyerror(scanner,"missing loop end delimiter");
 	  bool rg=(yyvsp[-3]).is_symb_of_sommet(at_range);
          gen f=(yyvsp[-3]).type==_SYMB?(yyvsp[-3])._SYMBptr->feuille:0,inc=1;
          if (rg){
            if (f.type!=_VECT) f=makesequence(0,f);
            vecteur v=*f._VECTptr;
            if (v.size()>=2) f=makesequence(v.front(),v[1]-1);
            if (v.size()==3) inc=v[2];
          }
          if (inc.type==_INT_  && inc.val!=0 && f.type==_VECT && f._VECTptr->size()==2 && (rg || ((yyvsp[-3]).is_symb_of_sommet(at_interval) 
	  // && f._VECTptr->front().type==_INT_ && f._VECTptr->back().type==_INT_ 
	  )))
            (yyval)=symbolic((yyvsp[-6]).type==_FUNC?*(yyvsp[-6])._FUNCptr:*at_for,makevecteur(symb_sto(f._VECTptr->front(),(yyvsp[-5])),inc.val>0?symb_inferieur_egal((yyvsp[-5]),f._VECTptr->back()):symb_superieur_egal((yyvsp[-5]),f._VECTptr->back()),symb_sto(symb_plus((yyvsp[-5]),inc),(yyvsp[-5])),symb_bloc((yyvsp[-1]))));
          else 
            (yyval)=symbolic((yyvsp[-6]).type==_FUNC?*(yyvsp[-6])._FUNCptr:*at_for,makevecteur(1,symbolic(*(yyvsp[-6])._FUNCptr,makevecteur((yyvsp[-5]),(yyvsp[-3]))),1,symb_bloc((yyvsp[-1]))));
	  }
#line 3825 "y.tab.c" /* yacc.c:1646  */
    break;

  case 83:
#line 343 "input_parser.yy" /* yacc.c:1646  */
    { 
          if ((yyvsp[0]).type==_INT_ && (yyvsp[0]).val && (yyvsp[0]).val!=2 && (yyvsp[0]).val!=9)
	    giac_yyerror(scanner,"missing loop end delimiter");
	  (yyval)=symbolic((yyvsp[-8]).type==_FUNC?*(yyvsp[-8])._FUNCptr:*at_for,makevecteur(1,symbolic(*(yyvsp[-8])._FUNCptr,makevecteur((yyvsp[-7]),(yyvsp[-5]),symb_bloc((yyvsp[-1])))),1,symb_bloc((yyvsp[-3]))));
	  }
#line 3835 "y.tab.c" /* yacc.c:1646  */
    break;

  case 84:
#line 348 "input_parser.yy" /* yacc.c:1646  */
    { 
          if ((yyvsp[0]).type==_INT_ && (yyvsp[0]).val && (yyvsp[0]).val!=2 && (yyvsp[0]).val!=9) giac_yyerror(scanner,"missing loop end delimiter");
          gen tmp,st=(yyvsp[-3]);  
       if (st==1 && (yyvsp[-5])!=1) st=(yyvsp[-5]);
          const giac::context * contextptr = giac_yyget_extra(scanner);
	  if (!lidnt(st).empty())
            *logptr(contextptr) << "Warning, step is not numeric " << st << std::endl;
          bool b=has_evalf(st,tmp,1,context0);
          if (!b || is_positive(tmp,context0)) 
             (yyval)=symbolic((yyvsp[-8]).type==_FUNC?*(yyvsp[-8])._FUNCptr:*at_for,makevecteur(symb_sto((yyvsp[-6]),(yyvsp[-7])),symb_inferieur_egal((yyvsp[-7]),(yyvsp[-4])),symb_sto(symb_plus((yyvsp[-7]),b?abs(st,context0):symb_abs(st)),(yyvsp[-7])),symb_bloc((yyvsp[-1])))); 
          else 
            (yyval)=symbolic((yyvsp[-8]).type==_FUNC?*(yyvsp[-8])._FUNCptr:*at_for,makevecteur(symb_sto((yyvsp[-6]),(yyvsp[-7])),symb_superieur_egal((yyvsp[-7]),(yyvsp[-4])),symb_sto(symb_plus((yyvsp[-7]),st),(yyvsp[-7])),symb_bloc((yyvsp[-1])))); 
        }
#line 3853 "y.tab.c" /* yacc.c:1646  */
    break;

  case 85:
#line 361 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = gen((yyvsp[0]).type==_FUNC?*(yyvsp[0])._FUNCptr:*at_for,4);}
#line 3859 "y.tab.c" /* yacc.c:1646  */
    break;

  case 86:
#line 362 "input_parser.yy" /* yacc.c:1646  */
    { 
        vecteur v=gen2vecteur((yyvsp[-2]));
        v.push_back(symb_ifte(equaltosame((yyvsp[0])),symbolic(at_break,zero),0));
	(yyval)=symbolic(*(yyvsp[-3])._FUNCptr,makevecteur(zero,1,zero,symb_bloc(v))); 
	}
#line 3869 "y.tab.c" /* yacc.c:1646  */
    break;

  case 87:
#line 367 "input_parser.yy" /* yacc.c:1646  */
    { 
        if ((yyvsp[0]).type==_INT_ && (yyvsp[0]).val && (yyvsp[0]).val!=2 && (yyvsp[0]).val!=9) giac_yyerror(scanner,"missing loop end delimiter");
        vecteur v=gen2vecteur((yyvsp[-3]));
        v.push_back(symb_ifte(equaltosame((yyvsp[-1])),symbolic(at_break,zero),0));
	(yyval)=symbolic(*(yyvsp[-4])._FUNCptr,makevecteur(zero,1,zero,symb_bloc(v))); 
	}
#line 3880 "y.tab.c" /* yacc.c:1646  */
    break;

  case 88:
#line 373 "input_parser.yy" /* yacc.c:1646  */
    {
          if ((yyvsp[0]).type==_INT_ && (yyvsp[0]).val && (yyvsp[0]).val!=4) giac_yyerror(scanner,"missing iferr end delimiter");
           (yyval)=symbolic(at_try_catch,makevecteur(symb_bloc((yyvsp[-5])),0,symb_bloc((yyvsp[-3])),symb_bloc((yyvsp[-1]))));
        }
#line 3889 "y.tab.c" /* yacc.c:1646  */
    break;

  case 89:
#line 377 "input_parser.yy" /* yacc.c:1646  */
    {
          if ((yyvsp[0]).type==_INT_ && (yyvsp[0]).val && (yyvsp[0]).val!=4) giac_yyerror(scanner,"missing iferr end delimiter");
           (yyval)=symbolic(at_try_catch,makevecteur(symb_bloc((yyvsp[-3])),0,symb_bloc((yyvsp[-1])),symb_bloc(0)));
        }
#line 3898 "y.tab.c" /* yacc.c:1646  */
    break;

  case 90:
#line 381 "input_parser.yy" /* yacc.c:1646  */
    { 
	(yyval)=(yyvsp[0]); 
	// $$.subtype=1; 
	}
#line 3907 "y.tab.c" /* yacc.c:1646  */
    break;

  case 91:
#line 385 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=(yyvsp[-1]); /* $$.subtype=1; */ }
#line 3913 "y.tab.c" /* yacc.c:1646  */
    break;

  case 92:
#line 386 "input_parser.yy" /* yacc.c:1646  */
    { (yyval) = symb_dollar((yyvsp[0])); }
#line 3919 "y.tab.c" /* yacc.c:1646  */
    break;

  case 93:
#line 387 "input_parser.yy" /* yacc.c:1646  */
    {(yyval)=symb_dollar(makesequence((yyvsp[-4]),(yyvsp[-2]),(yyvsp[0])));}
#line 3925 "y.tab.c" /* yacc.c:1646  */
    break;

  case 94:
#line 388 "input_parser.yy" /* yacc.c:1646  */
    { (yyval) = symb_dollar(makesequence((yyvsp[-2]),(yyvsp[0]))); }
#line 3931 "y.tab.c" /* yacc.c:1646  */
    break;

  case 95:
#line 389 "input_parser.yy" /* yacc.c:1646  */
    { (yyval) = symb_dollar(makesequence((yyvsp[-2]),(yyvsp[0]))); }
#line 3937 "y.tab.c" /* yacc.c:1646  */
    break;

  case 96:
#line 390 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symb_dollar((yyvsp[0])); }
#line 3943 "y.tab.c" /* yacc.c:1646  */
    break;

  case 97:
#line 391 "input_parser.yy" /* yacc.c:1646  */
    {  //CERR << $1 << " compose " << $2 << $3 << endl;
(yyval) = symbolic(*(yyvsp[-1])._FUNCptr,makesequence((yyvsp[-2]),python_compat(giac_yyget_extra(scanner))?denest_sto((yyvsp[0])):(yyvsp[0])) ); }
#line 3950 "y.tab.c" /* yacc.c:1646  */
    break;

  case 98:
#line 393 "input_parser.yy" /* yacc.c:1646  */
    {(yyval)=symbolic(at_ans,-1);}
#line 3956 "y.tab.c" /* yacc.c:1646  */
    break;

  case 99:
#line 394 "input_parser.yy" /* yacc.c:1646  */
    { (yyval) = symbolic(((yyvsp[-1]).type==_FUNC?*(yyvsp[-1])._FUNCptr:*at_union),makesequence((yyvsp[-2]),(yyvsp[0]))); }
#line 3962 "y.tab.c" /* yacc.c:1646  */
    break;

  case 100:
#line 395 "input_parser.yy" /* yacc.c:1646  */
    { (yyval) = symbolic(((yyvsp[-2]).type==_FUNC?*(yyvsp[-2])._FUNCptr:*at_union),gen(makevecteur((yyvsp[-3]),(yyvsp[-3])*(yyvsp[-1])/100) ,_SEQ__VECT)); }
#line 3968 "y.tab.c" /* yacc.c:1646  */
    break;

  case 101:
#line 396 "input_parser.yy" /* yacc.c:1646  */
    { (yyval) = symb_intersect(makesequence((yyvsp[-2]),(yyvsp[0]))); }
#line 3974 "y.tab.c" /* yacc.c:1646  */
    break;

  case 102:
#line 397 "input_parser.yy" /* yacc.c:1646  */
    { (yyval) = symb_minus(makesequence((yyvsp[-2]),(yyvsp[0]))); }
#line 3980 "y.tab.c" /* yacc.c:1646  */
    break;

  case 103:
#line 398 "input_parser.yy" /* yacc.c:1646  */
    { 
	(yyval)=symbolic(*(yyvsp[-1])._FUNCptr,makesequence((yyvsp[-2]),(yyvsp[0])) ); 
	}
#line 3988 "y.tab.c" /* yacc.c:1646  */
    break;

  case 104:
#line 401 "input_parser.yy" /* yacc.c:1646  */
    { (yyval) = (yyvsp[0]); }
#line 3994 "y.tab.c" /* yacc.c:1646  */
    break;

  case 105:
#line 402 "input_parser.yy" /* yacc.c:1646  */
    {if ((yyvsp[-1]).type==_FUNC) (yyval)=(yyvsp[-1]); else { 
          // const giac::context * contextptr = giac_yyget_extra(scanner);
          (yyval)=symb_quote((yyvsp[-1]));
          } 
        }
#line 4004 "y.tab.c" /* yacc.c:1646  */
    break;

  case 106:
#line 407 "input_parser.yy" /* yacc.c:1646  */
    {
          const giac::context * contextptr = giac_yyget_extra(scanner);
	  (yyval) = symb_at((yyvsp[-3]),(yyvsp[-1]),contextptr);
        }
#line 4013 "y.tab.c" /* yacc.c:1646  */
    break;

  case 107:
#line 411 "input_parser.yy" /* yacc.c:1646  */
    {
          const giac::context * contextptr = giac_yyget_extra(scanner);
	  (yyval) = symbolic(at_of,makesequence((yyvsp[-5]),(yyvsp[-2])));
        }
#line 4022 "y.tab.c" /* yacc.c:1646  */
    break;

  case 108:
#line 415 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = check_symb_of((yyvsp[-4]),(yyvsp[-1]),giac_yyget_extra(scanner));}
#line 4028 "y.tab.c" /* yacc.c:1646  */
    break;

  case 109:
#line 416 "input_parser.yy" /* yacc.c:1646  */
    {
	if ((yyvsp[-2])==_LIST__VECT && python_compat(giac_yyget_extra(scanner))){
           (yyval)=symbolic(at_python_list,(yyvsp[-1]));
        }
        else {
 	 if (abs_calc_mode(giac_yyget_extra(scanner))==38 && (yyvsp[-1]).type==_VECT && (yyvsp[-1]).subtype==_SEQ__VECT && (yyvsp[-1])._VECTptr->size()==2 && ((yyvsp[-1])._VECTptr->front().type<=_DOUBLE_ || (yyvsp[-1])._VECTptr->front().type==_FLOAT_) && ((yyvsp[-1])._VECTptr->back().type<=_DOUBLE_ || (yyvsp[-1])._VECTptr->back().type==_FLOAT_)){ 
           const giac::context * contextptr = giac_yyget_extra(scanner);
	   gen a=evalf((yyvsp[-1])._VECTptr->front(),1,contextptr),
	       b=evalf((yyvsp[-1])._VECTptr->back(),1,contextptr);
	   if ( (a.type==_DOUBLE_ || a.type==_FLOAT_) &&
                (b.type==_DOUBLE_ || b.type==_FLOAT_))
             (yyval)= a+b*cst_i; 
           else (yyval)=(yyvsp[-1]);
  	 } else {
              if (calc_mode(giac_yyget_extra(scanner))==1 && (yyvsp[-1]).type==_VECT && (yyvsp[-2])!=_LIST__VECT &&
	      (yyvsp[-1]).subtype==_SEQ__VECT && ((yyvsp[-1])._VECTptr->size()==2 || (yyvsp[-1])._VECTptr->size()==3) )
                (yyval) = gen(*(yyvsp[-1])._VECTptr,_GGB__VECT);
              else
                (yyval)=(yyvsp[-1]);
          }
	 }
        }
#line 4055 "y.tab.c" /* yacc.c:1646  */
    break;

  case 110:
#line 438 "input_parser.yy" /* yacc.c:1646  */
    { 
        //cerr << $1 << " " << $2 << endl;
        (yyval) = gen(*((yyvsp[-1])._VECTptr),(yyvsp[-2]).val);
        // cerr << $$ << endl;

        }
#line 4066 "y.tab.c" /* yacc.c:1646  */
    break;

  case 111:
#line 444 "input_parser.yy" /* yacc.c:1646  */
    { 
         if ((yyvsp[-2]).type==_VECT && (yyvsp[-2]).subtype==_SEQ__VECT && !((yyvsp[0]).type==_VECT && (yyvsp[-1]).subtype==_SEQ__VECT)){ (yyval)=(yyvsp[-2]); (yyval)._VECTptr->push_back((yyvsp[0])); }
	 else
           (yyval) = makesuite((yyvsp[-2]),(yyvsp[0])); 

        }
#line 4077 "y.tab.c" /* yacc.c:1646  */
    break;

  case 112:
#line 450 "input_parser.yy" /* yacc.c:1646  */
    {(yyval)=symb_findhelp((yyvsp[0]));}
#line 4083 "y.tab.c" /* yacc.c:1646  */
    break;

  case 113:
#line 451 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symb_interrogation((yyvsp[-2]),(yyvsp[0])); }
#line 4089 "y.tab.c" /* yacc.c:1646  */
    break;

  case 114:
#line 452 "input_parser.yy" /* yacc.c:1646  */
    {
          const giac::context * contextptr = giac_yyget_extra(scanner);
          (yyval)=symb_unit(gen(1),(yyvsp[0]),contextptr); 
          opened_quote(giac_yyget_extra(scanner)) &= 0x7ffffffd;	
        }
#line 4099 "y.tab.c" /* yacc.c:1646  */
    break;

  case 115:
#line 457 "input_parser.yy" /* yacc.c:1646  */
    {
          const giac::context * contextptr = giac_yyget_extra(scanner);
          (yyval)=symb_unit((yyvsp[-2]),(yyvsp[0]),contextptr); 
          opened_quote(giac_yyget_extra(scanner)) &= 0x7ffffffd;        }
#line 4108 "y.tab.c" /* yacc.c:1646  */
    break;

  case 116:
#line 461 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symb_pow((yyvsp[-1]),(yyvsp[0])); }
#line 4114 "y.tab.c" /* yacc.c:1646  */
    break;

  case 117:
#line 462 "input_parser.yy" /* yacc.c:1646  */
    { 
        const giac::context * contextptr = giac_yyget_extra(scanner);
#ifdef HAVE_SIGNAL_H_OLD
	messages_to_print += parser_filename(contextptr) + parser_error(contextptr); 
	/* *logptr(giac_yyget_extra(scanner)) << messages_to_print; */
#endif
	(yyval)=undef;
        spread_formula(false,contextptr); 
	}
#line 4128 "y.tab.c" /* yacc.c:1646  */
    break;

  case 118:
#line 471 "input_parser.yy" /* yacc.c:1646  */
    {
	(yyval) = symbolic(*(yyvsp[-5])._FUNCptr,makevecteur(equaltosame((yyvsp[-3])),symb_bloc((yyvsp[-1])),(yyvsp[0])));
	}
#line 4136 "y.tab.c" /* yacc.c:1646  */
    break;

  case 119:
#line 474 "input_parser.yy" /* yacc.c:1646  */
    {
        vecteur v=makevecteur(equaltosame((yyvsp[-4])),(yyvsp[-2]),(yyvsp[0]));
	// *logptr(giac_yyget_extra(scanner)) << v << endl;
	(yyval) = symbolic(*(yyvsp[-6])._FUNCptr,v);
	}
#line 4146 "y.tab.c" /* yacc.c:1646  */
    break;

  case 120:
#line 479 "input_parser.yy" /* yacc.c:1646  */
    { 
          if ((yyvsp[0]).type==_INT_ && (yyvsp[0]).val && (yyvsp[0]).val!=3) giac_yyerror(scanner,"missing func/prog/proc end delimiter");
          const giac::context * contextptr = giac_yyget_extra(scanner);
           (yyval)=symb_program((yyvsp[-4]),zero*(yyvsp[-4]),symb_local((yyvsp[-2]),(yyvsp[-1]),contextptr),contextptr); 
        }
#line 4156 "y.tab.c" /* yacc.c:1646  */
    break;

  case 121:
#line 484 "input_parser.yy" /* yacc.c:1646  */
    { 
          if ((yyvsp[0]).type==_INT_ && (yyvsp[0]).val && (yyvsp[0]).val!=3) giac_yyerror(scanner,"missing func/prog/proc end delimiter");
          const giac::context * contextptr = giac_yyget_extra(scanner);
           (yyval)=symb_program_sto((yyvsp[-4]),zero*(yyvsp[-4]),symb_local((yyvsp[-2]),(yyvsp[-1]),contextptr),(yyvsp[-6]),false,contextptr); 
        }
#line 4166 "y.tab.c" /* yacc.c:1646  */
    break;

  case 122:
#line 489 "input_parser.yy" /* yacc.c:1646  */
    { 
          if ((yyvsp[0]).type==_INT_ && (yyvsp[0]).val && (yyvsp[0]).val!=3) giac_yyerror(scanner,"missing func/prog/proc end delimiter");
          const giac::context * contextptr = giac_yyget_extra(scanner);
           (yyval)=symb_program_sto((yyvsp[-3]),zero*(yyvsp[-3]),symb_bloc((yyvsp[-1])),(yyvsp[-5]),false,contextptr); 
        }
#line 4176 "y.tab.c" /* yacc.c:1646  */
    break;

  case 123:
#line 494 "input_parser.yy" /* yacc.c:1646  */
    { 
          if ((yyvsp[0]).type==_INT_ && (yyvsp[0]).val && (yyvsp[0]).val!=3) giac_yyerror(scanner,"missing func/prog/proc end delimiter");
          const giac::context * contextptr = giac_yyget_extra(scanner);
           (yyval)=symb_program_sto((yyvsp[-5]),zero*(yyvsp[-5]),symb_local((yyvsp[-2]),(yyvsp[-1]),contextptr),(yyvsp[-7]),false,contextptr); 
        }
#line 4186 "y.tab.c" /* yacc.c:1646  */
    break;

  case 124:
#line 499 "input_parser.yy" /* yacc.c:1646  */
    { 
          if ((yyvsp[0]).type==_INT_ && (yyvsp[0]).val && (yyvsp[0]).val!=3) giac_yyerror(scanner,"missing func/prog/proc end delimiter");
          const giac::context * contextptr = giac_yyget_extra(scanner);
         (yyval)=symb_program((yyvsp[-5]),zero*(yyvsp[-5]),symb_local((yyvsp[-3]),(yyvsp[-1]),contextptr),contextptr); 
        }
#line 4196 "y.tab.c" /* yacc.c:1646  */
    break;

  case 125:
#line 504 "input_parser.yy" /* yacc.c:1646  */
    { 
          if ((yyvsp[0]).type==_INT_ && (yyvsp[0]).val && (yyvsp[0]).val!=3) giac_yyerror(scanner,"missing func/prog/proc end delimiter");
          const giac::context * contextptr = giac_yyget_extra(scanner);
           (yyval)=symb_program_sto((yyvsp[-5]),zero*(yyvsp[-5]),symb_local((yyvsp[-2]),(yyvsp[-1]),contextptr),(yyvsp[-7]),false,contextptr); 
        }
#line 4206 "y.tab.c" /* yacc.c:1646  */
    break;

  case 126:
#line 509 "input_parser.yy" /* yacc.c:1646  */
    { 
          if ((yyvsp[0]).type==_INT_ && (yyvsp[0]).val && (yyvsp[0]).val!=3) giac_yyerror(scanner,"missing func/prog/proc end delimiter");
          const giac::context * contextptr = giac_yyget_extra(scanner);
           (yyval)=symb_program_sto((yyvsp[-6]),zero*(yyvsp[-6]),symb_local((yyvsp[-2]),(yyvsp[-1]),contextptr),(yyvsp[-8]),false,contextptr); 
        }
#line 4216 "y.tab.c" /* yacc.c:1646  */
    break;

  case 127:
#line 514 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = symbolic((yyvsp[-8]).type==_FUNC?*(yyvsp[-8])._FUNCptr:*at_for,makevecteur((yyvsp[-6]),equaltosame((yyvsp[-4])),(yyvsp[-2]),symb_bloc((yyvsp[0]))));}
#line 4222 "y.tab.c" /* yacc.c:1646  */
    break;

  case 128:
#line 515 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = symbolic((yyvsp[-9]).type==_FUNC?*(yyvsp[-9])._FUNCptr:*at_for,makevecteur((yyvsp[-7]),equaltosame((yyvsp[-5])),(yyvsp[-3]),(yyvsp[-1])));}
#line 4228 "y.tab.c" /* yacc.c:1646  */
    break;

  case 129:
#line 516 "input_parser.yy" /* yacc.c:1646  */
    {(yyval) = symbolic((yyvsp[-3]).type==_FUNC?*(yyvsp[-3])._FUNCptr:*at_for,gen2vecteur((yyvsp[-1])));}
#line 4234 "y.tab.c" /* yacc.c:1646  */
    break;

  case 130:
#line 517 "input_parser.yy" /* yacc.c:1646  */
    {(yyval)=symbolic(at_member,makesequence((yyvsp[-2]),(yyvsp[0]))); if ((yyvsp[-1])==at_not) (yyval)=symbolic(at_not,(yyval));}
#line 4240 "y.tab.c" /* yacc.c:1646  */
    break;

  case 131:
#line 518 "input_parser.yy" /* yacc.c:1646  */
    {(yyval)=symbolic(at_not,symbolic(at_member,makesequence((yyvsp[-3]),(yyvsp[0]))));}
#line 4246 "y.tab.c" /* yacc.c:1646  */
    break;

  case 132:
#line 519 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symbolic(at_apply,makesequence(symbolic(at_program,makesequence((yyvsp[-3]),0*(yyvsp[-3]),vecteur(1,(yyvsp[-5])))),(yyvsp[-1]))); if ((yyvsp[-6])==_TABLE__VECT) (yyval)=symbolic(at_table,(yyval));}
#line 4252 "y.tab.c" /* yacc.c:1646  */
    break;

  case 133:
#line 520 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symbolic(at_apply,symbolic(at_program,makesequence((yyvsp[-5]),0*(yyvsp[-5]),vecteur(1,(yyvsp[-7])))),symbolic(at_select,makesequence(symbolic(at_program,makesequence((yyvsp[-5]),0*(yyvsp[-5]),(yyvsp[-1]))),(yyvsp[-3])))); if ((yyvsp[-8])==_TABLE__VECT) (yyval)=symbolic(at_table,(yyval));}
#line 4258 "y.tab.c" /* yacc.c:1646  */
    break;

  case 134:
#line 521 "input_parser.yy" /* yacc.c:1646  */
    { 
	vecteur v=makevecteur(zero,equaltosame((yyvsp[-2])),zero,symb_bloc((yyvsp[0])));
	(yyval)=symbolic((yyvsp[-4]).type==_FUNC?*(yyvsp[-4])._FUNCptr:*at_for,v); 
	}
#line 4267 "y.tab.c" /* yacc.c:1646  */
    break;

  case 135:
#line 525 "input_parser.yy" /* yacc.c:1646  */
    { 
	(yyval)=symbolic((yyvsp[-5]).type==_FUNC?*(yyvsp[-5])._FUNCptr:*at_for,makevecteur(zero,equaltosame((yyvsp[-3])),zero,(yyvsp[-1]))); 
	}
#line 4275 "y.tab.c" /* yacc.c:1646  */
    break;

  case 136:
#line 528 "input_parser.yy" /* yacc.c:1646  */
    { 
          if ((yyvsp[0]).type==_INT_ && (yyvsp[0]).val && (yyvsp[0]).val!=9 && (yyvsp[0]).val!=8) giac_yyerror(scanner,"missing loop end delimiter");
	  (yyval)=symbolic((yyvsp[-4]).type==_FUNC?*(yyvsp[-4])._FUNCptr:*at_for,makevecteur(zero,equaltosame((yyvsp[-3])),zero,symb_bloc((yyvsp[-1])))); 
        }
#line 4284 "y.tab.c" /* yacc.c:1646  */
    break;

  case 137:
#line 532 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symb_try_catch(makevecteur(symb_bloc((yyvsp[-5])),(yyvsp[-2]),symb_bloc((yyvsp[0]))));}
#line 4290 "y.tab.c" /* yacc.c:1646  */
    break;

  case 138:
#line 533 "input_parser.yy" /* yacc.c:1646  */
    {(yyval)=symb_try_catch(gen2vecteur((yyvsp[-1])));}
#line 4296 "y.tab.c" /* yacc.c:1646  */
    break;

  case 139:
#line 534 "input_parser.yy" /* yacc.c:1646  */
    {(yyval)=gen(at_try_catch,3);}
#line 4302 "y.tab.c" /* yacc.c:1646  */
    break;

  case 140:
#line 535 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symb_case((yyvsp[-4]),(yyvsp[-1])); }
#line 4308 "y.tab.c" /* yacc.c:1646  */
    break;

  case 141:
#line 536 "input_parser.yy" /* yacc.c:1646  */
    { (yyval) = symb_case((yyvsp[-1])); }
#line 4314 "y.tab.c" /* yacc.c:1646  */
    break;

  case 142:
#line 537 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symb_case((yyvsp[-2]),(yyvsp[-1])); }
#line 4320 "y.tab.c" /* yacc.c:1646  */
    break;

  case 143:
#line 540 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=(yyvsp[0]); }
#line 4326 "y.tab.c" /* yacc.c:1646  */
    break;

  case 144:
#line 541 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=(yyvsp[0]); }
#line 4332 "y.tab.c" /* yacc.c:1646  */
    break;

  case 145:
#line 542 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=(yyvsp[0]); }
#line 4338 "y.tab.c" /* yacc.c:1646  */
    break;

  case 146:
#line 545 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=(yyvsp[0]); }
#line 4344 "y.tab.c" /* yacc.c:1646  */
    break;

  case 147:
#line 546 "input_parser.yy" /* yacc.c:1646  */
    { 
	       gen tmp((yyvsp[0])); 
	       // tmp.subtype=1; 
	       (yyval)=symb_check_type(makevecteur(tmp,(yyvsp[-2])),context0); 
          }
#line 4354 "y.tab.c" /* yacc.c:1646  */
    break;

  case 148:
#line 551 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symb_double_deux_points(makevecteur((yyvsp[-2]),(yyvsp[0]))); }
#line 4360 "y.tab.c" /* yacc.c:1646  */
    break;

  case 149:
#line 552 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symb_double_deux_points(makevecteur((yyvsp[-2]),(yyvsp[0]))); }
#line 4366 "y.tab.c" /* yacc.c:1646  */
    break;

  case 150:
#line 553 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symb_double_deux_points(makevecteur((yyvsp[-2]),(yyvsp[0]))); }
#line 4372 "y.tab.c" /* yacc.c:1646  */
    break;

  case 151:
#line 554 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symb_double_deux_points(makevecteur((yyvsp[-4]),(yyvsp[-1]))); }
#line 4378 "y.tab.c" /* yacc.c:1646  */
    break;

  case 152:
#line 555 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symb_double_deux_points(makevecteur(0,(yyvsp[0]))); }
#line 4384 "y.tab.c" /* yacc.c:1646  */
    break;

  case 153:
#line 556 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symb_double_deux_points(makevecteur((yyvsp[-2]),(yyvsp[0]))); }
#line 4390 "y.tab.c" /* yacc.c:1646  */
    break;

  case 154:
#line 564 "input_parser.yy" /* yacc.c:1646  */
    { 
	  gen tmp((yyvsp[-1])); 
	  // tmp.subtype=1; 
	  (yyval)=symb_check_type(makevecteur(tmp,(yyvsp[0])),context0); 
	  }
#line 4400 "y.tab.c" /* yacc.c:1646  */
    break;

  case 155:
#line 569 "input_parser.yy" /* yacc.c:1646  */
    {(yyval)=symbolic(*(yyvsp[-1])._FUNCptr,(yyvsp[0])); }
#line 4406 "y.tab.c" /* yacc.c:1646  */
    break;

  case 156:
#line 572 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=(yyvsp[0]); }
#line 4412 "y.tab.c" /* yacc.c:1646  */
    break;

  case 157:
#line 573 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=(yyvsp[0]); }
#line 4418 "y.tab.c" /* yacc.c:1646  */
    break;

  case 158:
#line 576 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=makevecteur(vecteur(0),vecteur(0)); }
#line 4424 "y.tab.c" /* yacc.c:1646  */
    break;

  case 159:
#line 577 "input_parser.yy" /* yacc.c:1646  */
    { vecteur v1 =gen2vecteur((yyvsp[-1])); vecteur v2=gen2vecteur((yyvsp[0])); (yyval)=makevecteur(mergevecteur(gen2vecteur(v1[0]),gen2vecteur(v2[0])),mergevecteur(gen2vecteur(v1[1]),gen2vecteur(v2[1]))); }
#line 4430 "y.tab.c" /* yacc.c:1646  */
    break;

  case 160:
#line 581 "input_parser.yy" /* yacc.c:1646  */
    { if (!(yyvsp[-2]).val) (yyval)=makevecteur((yyvsp[-1]),vecteur(0)); else (yyval)=makevecteur(vecteur(0),(yyvsp[-1]));}
#line 4436 "y.tab.c" /* yacc.c:1646  */
    break;

  case 161:
#line 584 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=gen(vecteur(1,(yyvsp[0])),_SEQ__VECT); }
#line 4442 "y.tab.c" /* yacc.c:1646  */
    break;

  case 162:
#line 585 "input_parser.yy" /* yacc.c:1646  */
    { 
	       vecteur v=*(yyvsp[-2])._VECTptr;
	       v.push_back((yyvsp[0]));
	       (yyval)=gen(v,_SEQ__VECT);
	     }
#line 4452 "y.tab.c" /* yacc.c:1646  */
    break;

  case 163:
#line 592 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=(yyvsp[0]); }
#line 4458 "y.tab.c" /* yacc.c:1646  */
    break;

  case 164:
#line 593 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symb_sto((yyvsp[0]),(yyvsp[-2]),(yyvsp[-1])==at_array_sto); }
#line 4464 "y.tab.c" /* yacc.c:1646  */
    break;

  case 165:
#line 594 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symb_equal((yyvsp[-2]),(yyvsp[0])); }
#line 4470 "y.tab.c" /* yacc.c:1646  */
    break;

  case 166:
#line 595 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symbolic(at_deuxpoints,makesequence((yyvsp[-2]),(yyvsp[0])));  }
#line 4476 "y.tab.c" /* yacc.c:1646  */
    break;

  case 167:
#line 596 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=(yyvsp[-1]); }
#line 4482 "y.tab.c" /* yacc.c:1646  */
    break;

  case 168:
#line 597 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=(yyvsp[0]); *logptr(giac_yyget_extra(scanner)) << "Error: reserved word "<< (yyvsp[0]) <<endl;}
#line 4488 "y.tab.c" /* yacc.c:1646  */
    break;

  case 169:
#line 598 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symb_double_deux_points(makevecteur((yyvsp[-2]),(yyvsp[0]))); *logptr(giac_yyget_extra(scanner)) << "Error: reserved word "<< (yyvsp[-2]) <<endl; }
#line 4494 "y.tab.c" /* yacc.c:1646  */
    break;

  case 170:
#line 599 "input_parser.yy" /* yacc.c:1646  */
    { 
  const giac::context * contextptr = giac_yyget_extra(scanner);
  (yyval)=string2gen("_"+(yyvsp[0]).print(contextptr),false); 
  if (!giac::first_error_line(contextptr)){
    giac::first_error_line(giac::lexer_line_number(contextptr),contextptr);
    giac:: error_token_name((yyvsp[0]).print(contextptr)+ " (reserved word)",contextptr);
  }
}
#line 4507 "y.tab.c" /* yacc.c:1646  */
    break;

  case 171:
#line 607 "input_parser.yy" /* yacc.c:1646  */
    { 
  const giac::context * contextptr = giac_yyget_extra(scanner);
  (yyval)=string2gen("_"+(yyvsp[0]).print(contextptr),false);
  if (!giac::first_error_line(contextptr)){
    giac::first_error_line(giac::lexer_line_number(contextptr),contextptr);
    giac:: error_token_name((yyvsp[0]).print(contextptr)+ " reserved word",contextptr);
  }
}
#line 4520 "y.tab.c" /* yacc.c:1646  */
    break;

  case 172:
#line 617 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=plus_one;}
#line 4526 "y.tab.c" /* yacc.c:1646  */
    break;

  case 173:
#line 618 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=(yyvsp[0]); }
#line 4532 "y.tab.c" /* yacc.c:1646  */
    break;

  case 174:
#line 621 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=gen(vecteur(0),_SEQ__VECT); }
#line 4538 "y.tab.c" /* yacc.c:1646  */
    break;

  case 175:
#line 622 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=makesuite((yyvsp[0])); }
#line 4544 "y.tab.c" /* yacc.c:1646  */
    break;

  case 176:
#line 625 "input_parser.yy" /* yacc.c:1646  */
    { (yyval) = makevecteur((yyvsp[0])); }
#line 4550 "y.tab.c" /* yacc.c:1646  */
    break;

  case 177:
#line 627 "input_parser.yy" /* yacc.c:1646  */
    { vecteur v(1,(yyvsp[-1])); 
			  if ((yyvsp[-1]).type==_VECT) v=*((yyvsp[-1])._VECTptr); 
			  v.push_back((yyvsp[0])); 
			  (yyval) = v;
			}
#line 4560 "y.tab.c" /* yacc.c:1646  */
    break;

  case 178:
#line 632 "input_parser.yy" /* yacc.c:1646  */
    { (yyval) = (yyvsp[-1]);}
#line 4566 "y.tab.c" /* yacc.c:1646  */
    break;

  case 179:
#line 636 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=plus_one; }
#line 4572 "y.tab.c" /* yacc.c:1646  */
    break;

  case 180:
#line 637 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=(yyvsp[0]); }
#line 4578 "y.tab.c" /* yacc.c:1646  */
    break;

  case 181:
#line 640 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=plus_one; }
#line 4584 "y.tab.c" /* yacc.c:1646  */
    break;

  case 182:
#line 641 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=(yyvsp[0]); }
#line 4590 "y.tab.c" /* yacc.c:1646  */
    break;

  case 183:
#line 642 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=(yyvsp[0]); }
#line 4596 "y.tab.c" /* yacc.c:1646  */
    break;

  case 184:
#line 643 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=(yyvsp[0]); }
#line 4602 "y.tab.c" /* yacc.c:1646  */
    break;

  case 185:
#line 646 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=plus_one; }
#line 4608 "y.tab.c" /* yacc.c:1646  */
    break;

  case 186:
#line 647 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=(yyvsp[0]); }
#line 4614 "y.tab.c" /* yacc.c:1646  */
    break;

  case 187:
#line 650 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=0; }
#line 4620 "y.tab.c" /* yacc.c:1646  */
    break;

  case 188:
#line 651 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=(yyvsp[-1]); }
#line 4626 "y.tab.c" /* yacc.c:1646  */
    break;

  case 189:
#line 652 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=symb_bloc((yyvsp[0])); }
#line 4632 "y.tab.c" /* yacc.c:1646  */
    break;

  case 190:
#line 655 "input_parser.yy" /* yacc.c:1646  */
    { 
	(yyval) = (yyvsp[-1]);
	}
#line 4640 "y.tab.c" /* yacc.c:1646  */
    break;

  case 191:
#line 658 "input_parser.yy" /* yacc.c:1646  */
    {
          const giac::context * contextptr = giac_yyget_extra(scanner);
          (yyval) = symb_local((yyvsp[-2]),(yyvsp[-1]),contextptr);
         }
#line 4649 "y.tab.c" /* yacc.c:1646  */
    break;

  case 192:
#line 665 "input_parser.yy" /* yacc.c:1646  */
    { if ((yyvsp[0]).type==_INT_ && (yyvsp[0]).val && (yyvsp[0]).val!=4) giac_yyerror(scanner,"missing test end delimiter"); (yyval)=0; }
#line 4655 "y.tab.c" /* yacc.c:1646  */
    break;

  case 193:
#line 666 "input_parser.yy" /* yacc.c:1646  */
    {
          if ((yyvsp[0]).type==_INT_ && (yyvsp[0]).val && (yyvsp[0]).val!=4) giac_yyerror(scanner,"missing test end delimiter");
	(yyval)=symb_bloc((yyvsp[-1])); 
	}
#line 4664 "y.tab.c" /* yacc.c:1646  */
    break;

  case 194:
#line 670 "input_parser.yy" /* yacc.c:1646  */
    { 
	  (yyval)=symb_ifte(equaltosame((yyvsp[-3])),symb_bloc((yyvsp[-1])),(yyvsp[0]));
	  }
#line 4672 "y.tab.c" /* yacc.c:1646  */
    break;

  case 195:
#line 675 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=(yyvsp[0]); }
#line 4678 "y.tab.c" /* yacc.c:1646  */
    break;

  case 196:
#line 678 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=0; }
#line 4684 "y.tab.c" /* yacc.c:1646  */
    break;

  case 197:
#line 681 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=vecteur(0); }
#line 4690 "y.tab.c" /* yacc.c:1646  */
    break;

  case 198:
#line 682 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=makevecteur(symb_bloc((yyvsp[0])));}
#line 4696 "y.tab.c" /* yacc.c:1646  */
    break;

  case 199:
#line 683 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=mergevecteur(makevecteur((yyvsp[-3]),symb_bloc((yyvsp[-1]))),*((yyvsp[0])._VECTptr));}
#line 4702 "y.tab.c" /* yacc.c:1646  */
    break;

  case 200:
#line 686 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=vecteur(0); }
#line 4708 "y.tab.c" /* yacc.c:1646  */
    break;

  case 201:
#line 687 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=vecteur(1,symb_bloc((yyvsp[0]))); }
#line 4714 "y.tab.c" /* yacc.c:1646  */
    break;

  case 202:
#line 688 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=mergevecteur(makevecteur((yyvsp[-3]),symb_bloc((yyvsp[-1]))),*((yyvsp[0])._VECTptr));}
#line 4720 "y.tab.c" /* yacc.c:1646  */
    break;

  case 203:
#line 691 "input_parser.yy" /* yacc.c:1646  */
    { (yyval)=(yyvsp[0]); }
#line 4726 "y.tab.c" /* yacc.c:1646  */
    break;


#line 4730 "y.tab.c" /* yacc.c:1646  */
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (scanner, YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (scanner, yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, scanner);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYTERROR;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  yystos[yystate], yyvsp, scanner);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (scanner, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, scanner);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[*yyssp], yyvsp, scanner);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  return yyresult;
}
#line 698 "input_parser.yy" /* yacc.c:1906  */


#ifndef NO_NAMESPACE_GIAC
} // namespace giac


#endif // ndef NO_NAMESPACE_GIAC
int giac_yyget_column  (yyscan_t yyscanner);

// Error print routine (store error string in parser_error)
#if 1
int giac_yyerror(yyscan_t scanner,const char *s) {
 const giac::context * contextptr = giac_yyget_extra(scanner);
 int col = giac_yyget_column(scanner);
 int line = giac::lexer_line_number(contextptr);
 const char * scanb=giac::currently_scanned(contextptr);
 std::string curline;
 if (scanb){
  for (int i=1;i<line;++i){
   for (;*scanb;++scanb){
     if (*scanb=='\n'){
       ++scanb;
       break;
     }
   }
  }
  const char * scane=scanb;
  for (;*scane;++scane){
    if (*scane=='\n') break;
  }
  curline=std::string (scanb,scane);
 }
 std::string token_name=string(giac_yyget_text(scanner));
 bool is_at_end = (token_name.size()==2 && (token_name[0]==char(0xC3)) && (token_name[1]==char(0xBF)));
 std::string suffix = " (reserved word)";
 if (token_name.size()>suffix.size() && token_name.compare(token_name.size()-suffix.size(),suffix.size(),suffix)) {
  if (col>=token_name.size()-suffix.size()) {
   col -= token_name.size()-suffix.size();
  }
 } else if (col>=token_name.size()) {
   col -= token_name.size();
 }
 giac::lexer_column_number(contextptr)=col;
 string sy("syntax error ");
 if (0 && strlen(s)){
   sy += ": ";
   sy += s;
   sy +=", ";
 }
 if (is_at_end) {
  parser_error(":" + giac::print_INT_(line) + ": " +sy + " at end of input\n",contextptr); // string(s) replaced with syntax error
  giac::parsed_gen(giac::undef,contextptr);
 } else {
 parser_error( ":" + giac::print_INT_(line) + ": " + sy + " line " + giac::print_INT_(line) + " col " + giac::print_INT_(col) + " at " + token_name +" in "+curline+" \n",contextptr); // string(s) replaced with syntax error
 giac::parsed_gen(giac::string2gen(token_name,false),contextptr);
 }
 if (!giac::first_error_line(contextptr)) {
  giac::first_error_line(line,contextptr);
  if (is_at_end) {
   token_name="end of input";
  }
  giac:: error_token_name(token_name,contextptr);
 }
 return line;
}

#else

int giac_yyerror(yyscan_t scanner,const char *s)
{
  const giac::context * contextptr = giac_yyget_extra(scanner);
  int col= giac_yyget_column(scanner);
  giac::lexer_column_number(contextptr)=col;
  if ( (*giac_yyget_text( scanner )) && (giac_yyget_text( scanner )[0]!=-61) && (giac_yyget_text( scanner )[1]!=-65)){
    std::string txt=giac_yyget_text( scanner );
    parser_error( ":" + giac::print_INT_(giac::lexer_line_number(contextptr)) + ": " + string(s) + " line " + giac::print_INT_(giac::lexer_line_number(contextptr)) + " col " + giac::print_INT_(col) + " at " + txt +"\n",contextptr);
     giac::parsed_gen(giac::string2gen(txt,false),contextptr);
  }
  else {
    parser_error(":" + giac::print_INT_(giac::lexer_line_number(contextptr)) + ": " +string(s) + " at end of input\n",contextptr);
    giac::parsed_gen(giac::undef,contextptr);
  }
  if (!giac::first_error_line(contextptr)){
    giac::first_error_line(giac::lexer_line_number(contextptr),contextptr);
    std::string s=string(giac_yyget_text( scanner ));
    if (s.size()==2 && s[0]==-61 && s[1]==-65)
      s="end of input";
    giac:: error_token_name(s,contextptr);
  }
  return giac::lexer_line_number(contextptr);
}
#endif
