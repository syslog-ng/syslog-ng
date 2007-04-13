/* A Bison parser, made by GNU Bison 1.875a.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* Written by Richard Stallman by simplifying the original so called
   ``semantic'' parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Using locations.  */
#define YYLSP_NEEDED 0



/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     KW_SOURCE = 258,
     KW_DESTINATION = 259,
     KW_LOG = 260,
     KW_OPTIONS = 261,
     KW_FILTER = 262,
     KW_INTERNAL = 263,
     KW_FILE = 264,
     KW_PIPE = 265,
     KW_UNIX_STREAM = 266,
     KW_UNIX_DGRAM = 267,
     KW_UDP = 268,
     KW_TCP = 269,
     KW_USER = 270,
     KW_DOOR = 271,
     KW_SUN_STREAMS = 272,
     KW_PROGRAM = 273,
     KW_FSYNC = 274,
     KW_MARK_FREQ = 275,
     KW_SYNC_FREQ = 276,
     KW_LOG_MSG_SIZE = 277,
     KW_FILE_TEMPLATE = 278,
     KW_PROTO_TEMPLATE = 279,
     KW_CHAIN_HOSTNAMES = 280,
     KW_KEEP_HOSTNAME = 281,
     KW_KEEP_TIMESTAMP = 282,
     KW_USE_DNS = 283,
     KW_USE_FQDN = 284,
     KW_DNS_CACHE = 285,
     KW_DNS_CACHE_SIZE = 286,
     KW_DNS_CACHE_EXPIRE = 287,
     KW_DNS_CACHE_EXPIRE_FAILED = 288,
     KW_TZ_CONVERT = 289,
     KW_TS_FORMAT = 290,
     KW_LOG_FIFO_SIZE = 291,
     KW_LOG_FETCH_LIMIT = 292,
     KW_LOG_IW_SIZE = 293,
     KW_FLAGS = 294,
     KW_CATCHALL = 295,
     KW_FALLBACK = 296,
     KW_FINAL = 297,
     KW_FLOW_CONTROL = 298,
     KW_NO_PARSE = 299,
     KW_PADDING = 300,
     KW_TIME_ZONE = 301,
     KW_TIME_REOPEN = 302,
     KW_TIME_REAP = 303,
     KW_TMPL_ESCAPE = 304,
     KW_CREATE_DIRS = 305,
     KW_OWNER = 306,
     KW_GROUP = 307,
     KW_PERM = 308,
     KW_DIR_OWNER = 309,
     KW_DIR_GROUP = 310,
     KW_DIR_PERM = 311,
     KW_TEMPLATE = 312,
     KW_TEMPLATE_ESCAPE = 313,
     KW_FOLLOW_FREQ = 314,
     KW_KEEP_ALIVE = 315,
     KW_MAX_CONNECTIONS = 316,
     KW_LOCALIP = 317,
     KW_IP = 318,
     KW_LOCALPORT = 319,
     KW_PORT = 320,
     KW_DESTPORT = 321,
     KW_USE_TIME_RECVD = 322,
     KW_FACILITY = 323,
     KW_LEVEL = 324,
     KW_HOST = 325,
     KW_MATCH = 326,
     KW_YES = 327,
     KW_NO = 328,
     KW_REQUIRED = 329,
     KW_ALLOW = 330,
     KW_DENY = 331,
     KW_GC_IDLE_THRESHOLD = 332,
     KW_GC_BUSY_THRESHOLD = 333,
     KW_COMPRESS = 334,
     KW_MAC = 335,
     KW_AUTH = 336,
     KW_ENCRYPT = 337,
     DOTDOT = 338,
     IDENTIFIER = 339,
     NUMBER = 340,
     STRING = 341,
     KW_OR = 342,
     KW_AND = 343,
     KW_NOT = 344
   };
#endif
#define KW_SOURCE 258
#define KW_DESTINATION 259
#define KW_LOG 260
#define KW_OPTIONS 261
#define KW_FILTER 262
#define KW_INTERNAL 263
#define KW_FILE 264
#define KW_PIPE 265
#define KW_UNIX_STREAM 266
#define KW_UNIX_DGRAM 267
#define KW_UDP 268
#define KW_TCP 269
#define KW_USER 270
#define KW_DOOR 271
#define KW_SUN_STREAMS 272
#define KW_PROGRAM 273
#define KW_FSYNC 274
#define KW_MARK_FREQ 275
#define KW_SYNC_FREQ 276
#define KW_LOG_MSG_SIZE 277
#define KW_FILE_TEMPLATE 278
#define KW_PROTO_TEMPLATE 279
#define KW_CHAIN_HOSTNAMES 280
#define KW_KEEP_HOSTNAME 281
#define KW_KEEP_TIMESTAMP 282
#define KW_USE_DNS 283
#define KW_USE_FQDN 284
#define KW_DNS_CACHE 285
#define KW_DNS_CACHE_SIZE 286
#define KW_DNS_CACHE_EXPIRE 287
#define KW_DNS_CACHE_EXPIRE_FAILED 288
#define KW_TZ_CONVERT 289
#define KW_TS_FORMAT 290
#define KW_LOG_FIFO_SIZE 291
#define KW_LOG_FETCH_LIMIT 292
#define KW_LOG_IW_SIZE 293
#define KW_FLAGS 294
#define KW_CATCHALL 295
#define KW_FALLBACK 296
#define KW_FINAL 297
#define KW_FLOW_CONTROL 298
#define KW_NO_PARSE 299
#define KW_PADDING 300
#define KW_TIME_ZONE 301
#define KW_TIME_REOPEN 302
#define KW_TIME_REAP 303
#define KW_TMPL_ESCAPE 304
#define KW_CREATE_DIRS 305
#define KW_OWNER 306
#define KW_GROUP 307
#define KW_PERM 308
#define KW_DIR_OWNER 309
#define KW_DIR_GROUP 310
#define KW_DIR_PERM 311
#define KW_TEMPLATE 312
#define KW_TEMPLATE_ESCAPE 313
#define KW_FOLLOW_FREQ 314
#define KW_KEEP_ALIVE 315
#define KW_MAX_CONNECTIONS 316
#define KW_LOCALIP 317
#define KW_IP 318
#define KW_LOCALPORT 319
#define KW_PORT 320
#define KW_DESTPORT 321
#define KW_USE_TIME_RECVD 322
#define KW_FACILITY 323
#define KW_LEVEL 324
#define KW_HOST 325
#define KW_MATCH 326
#define KW_YES 327
#define KW_NO 328
#define KW_REQUIRED 329
#define KW_ALLOW 330
#define KW_DENY 331
#define KW_GC_IDLE_THRESHOLD 332
#define KW_GC_BUSY_THRESHOLD 333
#define KW_COMPRESS 334
#define KW_MAC 335
#define KW_AUTH 336
#define KW_ENCRYPT 337
#define DOTDOT 338
#define IDENTIFIER 339
#define NUMBER 340
#define STRING 341
#define KW_OR 342
#define KW_AND 343
#define KW_NOT 344




/* Copy the first part of user declarations.  */
#line 1 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"


#include "syslog-ng.h"
#include "cfg.h"
#include "sgroup.h"
#include "dgroup.h"
#include "center.h"
#include "filter.h"
#include "templates.h"
#include "logreader.h"

#include "affile.h"
#include "afinter.h"
#include "afsocket.h"
#include "afinet.h"
#include "afunix.h"
#include "afstreams.h"
#include "afuser.h"
#include "afprog.h"

#include "messages.h"

#include "syslog-names.h"

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>

void yyerror(char *msg);
int yylex();

LogDriver *last_driver;
LogReaderOptions *last_reader_options;
LogWriterOptions *last_writer_options;
LogTemplate *last_template;



/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 39 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
typedef union YYSTYPE {
	guint num;
	char *cptr;
	void *ptr;
	FilterExprNode *node;
} YYSTYPE;
/* Line 191 of yacc.c.  */
#line 299 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.c"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 214 of yacc.c.  */
#line 311 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.c"

#if ! defined (yyoverflow) || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# if YYSTACK_USE_ALLOCA
#  define YYSTACK_ALLOC alloca
# else
#  ifndef YYSTACK_USE_ALLOCA
#   if defined (alloca) || defined (_ALLOCA_H)
#    define YYSTACK_ALLOC alloca
#   else
#    ifdef __GNUC__
#     define YYSTACK_ALLOC __builtin_alloca
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning. */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
# else
#  if defined (__STDC__) || defined (__cplusplus)
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   define YYSIZE_T size_t
#  endif
#  define YYSTACK_ALLOC malloc
#  define YYSTACK_FREE free
# endif
#endif /* ! defined (yyoverflow) || YYERROR_VERBOSE */


#if (! defined (yyoverflow) \
     && (! defined (__cplusplus) \
	 || (YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  short yyss;
  YYSTYPE yyvs;
  };

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE))				\
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  register YYSIZE_T yyi;		\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (0)
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (0)

#endif

#if defined (__STDC__) || defined (__cplusplus)
   typedef signed char yysigned_char;
#else
   typedef short yysigned_char;
#endif

/* YYFINAL -- State number of the termination state. */
#define YYFINAL  24
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   617

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  95
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  97
/* YYNRULES -- Number of rules. */
#define YYNRULES  240
/* YYNRULES -- Number of states. */
#define YYNSTATES  615

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   344

#define YYTRANSLATE(YYX) 						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const unsigned char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      93,    94,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    90,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    91,     2,    92,     2,     2,     2,     2,
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
      85,    86,    87,    88,    89
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const unsigned short yyprhs[] =
{
       0,     0,     3,     5,     9,    10,    13,    16,    19,    22,
      25,    28,    33,    38,    43,    47,    48,    54,    58,    59,
      64,    69,    74,    78,    79,    81,    83,    85,    87,    91,
      96,   101,   102,   106,   107,   111,   116,   121,   126,   131,
     132,   136,   137,   141,   144,   145,   150,   155,   160,   162,
     164,   165,   168,   171,   172,   174,   179,   184,   189,   194,
     199,   204,   206,   207,   210,   213,   214,   216,   221,   226,
     231,   236,   241,   243,   248,   253,   258,   259,   263,   266,
     267,   272,   275,   276,   281,   286,   291,   296,   301,   306,
     311,   314,   315,   317,   321,   322,   324,   326,   328,   330,
     332,   337,   338,   342,   345,   346,   348,   353,   358,   363,
     368,   373,   378,   383,   388,   389,   393,   396,   397,   399,
     404,   409,   414,   419,   424,   429,   434,   435,   439,   440,
     444,   445,   449,   452,   453,   458,   463,   465,   467,   472,
     477,   482,   483,   487,   490,   491,   493,   498,   503,   508,
     513,   518,   519,   523,   526,   527,   532,   537,   542,   547,
     552,   557,   562,   567,   572,   575,   576,   578,   582,   583,
     588,   593,   598,   604,   605,   608,   609,   611,   613,   615,
     617,   621,   622,   627,   632,   637,   642,   647,   652,   657,
     662,   667,   672,   677,   682,   687,   692,   697,   702,   707,
     712,   717,   722,   727,   732,   737,   742,   747,   752,   757,
     762,   767,   772,   777,   783,   785,   788,   792,   796,   800,
     805,   810,   815,   820,   825,   830,   835,   838,   840,   842,
     845,   847,   851,   853,   855,   857,   859,   861,   863,   865,
     867
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const short yyrhs[] =
{
      96,     0,    -1,    97,    -1,    98,    90,    97,    -1,    -1,
       3,    99,    -1,     4,   100,    -1,     5,   101,    -1,     7,
     182,    -1,    57,   103,    -1,     6,   102,    -1,   191,    91,
     107,    92,    -1,   191,    91,   141,    92,    -1,    91,   175,
     177,    92,    -1,    91,   180,    92,    -1,    -1,   191,   104,
      91,   105,    92,    -1,   106,    90,   105,    -1,    -1,    57,
      93,   191,    94,    -1,    58,    93,   189,    94,    -1,    46,
      93,   191,    94,    -1,   108,    90,   107,    -1,    -1,   109,
      -1,   110,    -1,   115,    -1,   132,    -1,     8,    93,    94,
      -1,     9,    93,   111,    94,    -1,    10,    93,   113,    94,
      -1,    -1,   191,   112,   137,    -1,    -1,   191,   114,   137,
      -1,    12,    93,   116,    94,    -1,    11,    93,   118,    94,
      -1,    13,    93,   122,    94,    -1,    14,    93,   127,    94,
      -1,    -1,   191,   117,   120,    -1,    -1,   191,   119,   120,
      -1,   121,   120,    -1,    -1,    51,    93,   191,    94,    -1,
      52,    93,   191,    94,    -1,    53,    93,    85,    94,    -1,
     131,    -1,   138,    -1,    -1,   123,   124,    -1,   125,   124,
      -1,    -1,   126,    -1,    64,    93,   191,    94,    -1,    65,
      93,   191,    94,    -1,    62,    93,   191,    94,    -1,    64,
      93,    85,    94,    -1,    65,    93,    85,    94,    -1,    63,
      93,   191,    94,    -1,   138,    -1,    -1,   128,   129,    -1,
     130,   129,    -1,    -1,   126,    -1,    64,    93,   191,    94,
      -1,    65,    93,   191,    94,    -1,    81,    93,   190,    94,
      -1,    80,    93,   190,    94,    -1,    82,    93,   190,    94,
      -1,   131,    -1,    60,    93,   189,    94,    -1,    61,    93,
      85,    94,    -1,    17,    93,   133,    94,    -1,    -1,   191,
     134,   135,    -1,   136,   135,    -1,    -1,    16,    93,   191,
      94,    -1,   138,   137,    -1,    -1,    39,    93,   139,    94,
      -1,    22,    93,    85,    94,    -1,    38,    93,    85,    94,
      -1,    37,    93,    85,    94,    -1,    45,    93,    85,    94,
      -1,    59,    93,    85,    94,    -1,    46,    93,   191,    94,
      -1,   140,   139,    -1,    -1,    44,    -1,   142,    90,   141,
      -1,    -1,   143,    -1,   148,    -1,   153,    -1,   167,    -1,
     168,    -1,     9,    93,   144,    94,    -1,    -1,   191,   145,
     146,    -1,   147,   146,    -1,    -1,   172,    -1,    51,    93,
     191,    94,    -1,    52,    93,   191,    94,    -1,    53,    93,
      85,    94,    -1,    54,    93,   191,    94,    -1,    55,    93,
     191,    94,    -1,    56,    93,    85,    94,    -1,    50,    93,
     189,    94,    -1,    10,    93,   149,    94,    -1,    -1,   191,
     150,   151,    -1,   152,   151,    -1,    -1,   172,    -1,    51,
      93,   191,    94,    -1,    52,    93,   191,    94,    -1,    53,
      93,    85,    94,    -1,    12,    93,   154,    94,    -1,    11,
      93,   156,    94,    -1,    13,    93,   158,    94,    -1,    14,
      93,   163,    94,    -1,    -1,   191,   155,   171,    -1,    -1,
     191,   157,   171,    -1,    -1,   191,   159,   160,    -1,   160,
     162,    -1,    -1,    62,    93,   191,    94,    -1,    65,    93,
      85,    94,    -1,   172,    -1,   161,    -1,    64,    93,   191,
      94,    -1,    65,    93,   191,    94,    -1,    66,    93,   191,
      94,    -1,    -1,   191,   164,   165,    -1,   165,   166,    -1,
      -1,   161,    -1,    64,    93,   191,    94,    -1,    65,    93,
     191,    94,    -1,    66,    93,   191,    94,    -1,    15,    93,
     191,    94,    -1,    18,    93,   169,    94,    -1,    -1,   191,
     170,   171,    -1,   172,   171,    -1,    -1,    39,    93,   173,
      94,    -1,    36,    93,    85,    94,    -1,    21,    93,    85,
      94,    -1,    57,    93,   191,    94,    -1,    58,    93,   189,
      94,    -1,    19,    93,   189,    94,    -1,    27,    93,   189,
      94,    -1,    34,    93,   191,    94,    -1,    35,    93,   191,
      94,    -1,   174,   173,    -1,    -1,    49,    -1,   176,    90,
     175,    -1,    -1,     3,    93,   191,    94,    -1,     7,    93,
     191,    94,    -1,     4,    93,   191,    94,    -1,    39,    93,
     178,    94,    90,    -1,    -1,   179,   178,    -1,    -1,    40,
      -1,    41,    -1,    42,    -1,    43,    -1,   181,    90,   180,
      -1,    -1,    20,    93,    85,    94,    -1,    21,    93,    85,
      94,    -1,    25,    93,   189,    94,    -1,    26,    93,   189,
      94,    -1,    67,    93,   189,    94,    -1,    29,    93,   189,
      94,    -1,    28,    93,   189,    94,    -1,    47,    93,    85,
      94,    -1,    48,    93,    85,    94,    -1,    36,    93,    85,
      94,    -1,    38,    93,    85,    94,    -1,    37,    93,    85,
      94,    -1,    22,    93,    85,    94,    -1,    27,    93,   189,
      94,    -1,    34,    93,   191,    94,    -1,    35,    93,   191,
      94,    -1,    78,    93,    85,    94,    -1,    77,    93,    85,
      94,    -1,    50,    93,   189,    94,    -1,    51,    93,   191,
      94,    -1,    52,    93,   191,    94,    -1,    53,    93,    85,
      94,    -1,    54,    93,   191,    94,    -1,    55,    93,   191,
      94,    -1,    56,    93,    85,    94,    -1,    30,    93,   189,
      94,    -1,    31,    93,    85,    94,    -1,    32,    93,    85,
      94,    -1,    33,    93,    85,    94,    -1,    23,    93,   191,
      94,    -1,    24,    93,   191,    94,    -1,   191,    91,   183,
      90,    92,    -1,   184,    -1,    89,   183,    -1,   183,    87,
     183,    -1,   183,    88,   183,    -1,    93,   183,    94,    -1,
      68,    93,   185,    94,    -1,    68,    93,    85,    94,    -1,
      69,    93,   187,    94,    -1,    18,    93,   191,    94,    -1,
      70,    93,   191,    94,    -1,    71,    93,   191,    94,    -1,
       7,    93,   191,    94,    -1,   186,   185,    -1,   186,    -1,
      84,    -1,   188,   187,    -1,   188,    -1,    84,    83,    84,
      -1,    84,    -1,    72,    -1,    73,    -1,    85,    -1,    74,
      -1,    75,    -1,    76,    -1,    84,    -1,    86,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned short yyrline[] =
{
       0,   178,   178,   182,   183,   187,   188,   189,   190,   191,
     192,   196,   200,   204,   208,   213,   212,   221,   222,   226,
     227,   228,   235,   236,   240,   241,   242,   243,   247,   251,
     252,   257,   256,   267,   266,   276,   277,   278,   279,   284,
     283,   296,   295,   308,   309,   313,   314,   315,   316,   317,
     322,   322,   332,   333,   337,   338,   339,   343,   344,   345,
     346,   347,   352,   352,   362,   363,   367,   368,   369,   370,
     371,   372,   373,   377,   378,   382,   387,   386,   395,   396,
     400,   404,   405,   409,   410,   411,   412,   413,   414,   415,
     419,   420,   424,   428,   429,   433,   434,   435,   436,   437,
     441,   446,   445,   456,   457,   461,   466,   467,   468,   469,
     470,   471,   472,   476,   481,   480,   491,   492,   496,   497,
     498,   499,   504,   505,   506,   507,   512,   511,   522,   521,
     533,   532,   544,   545,   549,   550,   551,   555,   556,   557,
     558,   563,   562,   574,   575,   579,   580,   581,   582,   593,
     597,   602,   601,   611,   612,   616,   617,   618,   619,   629,
     630,   631,   632,   633,   637,   638,   642,   647,   648,   652,
     653,   654,   658,   659,   664,   665,   669,   670,   671,   672,
     676,   677,   681,   682,   683,   684,   685,   686,   687,   688,
     689,   690,   691,   692,   693,   694,   695,   696,   697,   698,
     699,   700,   701,   702,   703,   704,   705,   706,   707,   708,
     709,   711,   712,   716,   720,   721,   722,   723,   724,   728,
     729,   730,   731,   732,   733,   734,   738,   739,   743,   760,
     761,   765,   789,   806,   807,   808,   812,   813,   814,   818,
     819
};
#endif

#if YYDEBUG || YYERROR_VERBOSE
/* YYTNME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals. */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "KW_SOURCE", "KW_DESTINATION", "KW_LOG", 
  "KW_OPTIONS", "KW_FILTER", "KW_INTERNAL", "KW_FILE", "KW_PIPE", 
  "KW_UNIX_STREAM", "KW_UNIX_DGRAM", "KW_UDP", "KW_TCP", "KW_USER", 
  "KW_DOOR", "KW_SUN_STREAMS", "KW_PROGRAM", "KW_FSYNC", "KW_MARK_FREQ", 
  "KW_SYNC_FREQ", "KW_LOG_MSG_SIZE", "KW_FILE_TEMPLATE", 
  "KW_PROTO_TEMPLATE", "KW_CHAIN_HOSTNAMES", "KW_KEEP_HOSTNAME", 
  "KW_KEEP_TIMESTAMP", "KW_USE_DNS", "KW_USE_FQDN", "KW_DNS_CACHE", 
  "KW_DNS_CACHE_SIZE", "KW_DNS_CACHE_EXPIRE", 
  "KW_DNS_CACHE_EXPIRE_FAILED", "KW_TZ_CONVERT", "KW_TS_FORMAT", 
  "KW_LOG_FIFO_SIZE", "KW_LOG_FETCH_LIMIT", "KW_LOG_IW_SIZE", "KW_FLAGS", 
  "KW_CATCHALL", "KW_FALLBACK", "KW_FINAL", "KW_FLOW_CONTROL", 
  "KW_NO_PARSE", "KW_PADDING", "KW_TIME_ZONE", "KW_TIME_REOPEN", 
  "KW_TIME_REAP", "KW_TMPL_ESCAPE", "KW_CREATE_DIRS", "KW_OWNER", 
  "KW_GROUP", "KW_PERM", "KW_DIR_OWNER", "KW_DIR_GROUP", "KW_DIR_PERM", 
  "KW_TEMPLATE", "KW_TEMPLATE_ESCAPE", "KW_FOLLOW_FREQ", "KW_KEEP_ALIVE", 
  "KW_MAX_CONNECTIONS", "KW_LOCALIP", "KW_IP", "KW_LOCALPORT", "KW_PORT", 
  "KW_DESTPORT", "KW_USE_TIME_RECVD", "KW_FACILITY", "KW_LEVEL", 
  "KW_HOST", "KW_MATCH", "KW_YES", "KW_NO", "KW_REQUIRED", "KW_ALLOW", 
  "KW_DENY", "KW_GC_IDLE_THRESHOLD", "KW_GC_BUSY_THRESHOLD", 
  "KW_COMPRESS", "KW_MAC", "KW_AUTH", "KW_ENCRYPT", "DOTDOT", 
  "IDENTIFIER", "NUMBER", "STRING", "KW_OR", "KW_AND", "KW_NOT", "';'", 
  "'{'", "'}'", "'('", "')'", "$accept", "start", "stmts", "stmt", 
  "source_stmt", "dest_stmt", "log_stmt", "options_stmt", "template_stmt", 
  "@1", "template_items", "template_item", "source_items", "source_item", 
  "source_afinter", "source_affile", "source_affile_params", "@2", 
  "source_afpipe_params", "@3", "source_afsocket", 
  "source_afunix_dgram_params", "@4", "source_afunix_stream_params", "@5", 
  "source_afunix_options", "source_afunix_option", 
  "source_afinet_udp_params", "@6", "source_afinet_udp_options", 
  "source_afinet_udp_option", "source_afinet_option", 
  "source_afinet_tcp_params", "@7", "source_afinet_tcp_options", 
  "source_afinet_tcp_option", "source_afsocket_stream_params", 
  "source_afstreams", "source_afstreams_params", "@8", 
  "source_afstreams_options", "source_afstreams_option", 
  "source_reader_options", "source_reader_option", 
  "source_reader_option_flags", "source_reader_option_flag", "dest_items", 
  "dest_item", "dest_affile", "dest_affile_params", "@9", 
  "dest_affile_options", "dest_affile_option", "dest_afpipe", 
  "dest_afpipe_params", "@10", "dest_afpipe_options", 
  "dest_afpipe_option", "dest_afsocket", "dest_afunix_dgram_params", 
  "@11", "dest_afunix_stream_params", "@12", "dest_afinet_udp_params", 
  "@13", "dest_afinet_udp_options", "dest_afinet_option", 
  "dest_afinet_udp_option", "dest_afinet_tcp_params", "@14", 
  "dest_afinet_tcp_options", "dest_afinet_tcp_option", "dest_afuser", 
  "dest_afprogram", "dest_afprogram_params", "@15", "dest_writer_options", 
  "dest_writer_option", "dest_writer_options_flags", 
  "dest_writer_options_flag", "log_items", "log_item", "log_flags", 
  "log_flags_items", "log_flags_item", "options_items", "options_item", 
  "filter_stmt", "filter_expr", "filter_simple_expr", "filter_fac_list", 
  "filter_fac", "filter_level_list", "filter_level", "yesno", 
  "tripleoption", "string", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const unsigned short yytoknum[] =
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
      59,   123,   125,    40,    41
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const unsigned char yyr1[] =
{
       0,    95,    96,    97,    97,    98,    98,    98,    98,    98,
      98,    99,   100,   101,   102,   104,   103,   105,   105,   106,
     106,   106,   107,   107,   108,   108,   108,   108,   109,   110,
     110,   112,   111,   114,   113,   115,   115,   115,   115,   117,
     116,   119,   118,   120,   120,   121,   121,   121,   121,   121,
     123,   122,   124,   124,   125,   125,   125,   126,   126,   126,
     126,   126,   128,   127,   129,   129,   130,   130,   130,   130,
     130,   130,   130,   131,   131,   132,   134,   133,   135,   135,
     136,   137,   137,   138,   138,   138,   138,   138,   138,   138,
     139,   139,   140,   141,   141,   142,   142,   142,   142,   142,
     143,   145,   144,   146,   146,   147,   147,   147,   147,   147,
     147,   147,   147,   148,   150,   149,   151,   151,   152,   152,
     152,   152,   153,   153,   153,   153,   155,   154,   157,   156,
     159,   158,   160,   160,   161,   161,   161,   162,   162,   162,
     162,   164,   163,   165,   165,   166,   166,   166,   166,   167,
     168,   170,   169,   171,   171,   172,   172,   172,   172,   172,
     172,   172,   172,   172,   173,   173,   174,   175,   175,   176,
     176,   176,   177,   177,   178,   178,   179,   179,   179,   179,
     180,   180,   181,   181,   181,   181,   181,   181,   181,   181,
     181,   181,   181,   181,   181,   181,   181,   181,   181,   181,
     181,   181,   181,   181,   181,   181,   181,   181,   181,   181,
     181,   181,   181,   182,   183,   183,   183,   183,   183,   184,
     184,   184,   184,   184,   184,   184,   185,   185,   186,   187,
     187,   188,   188,   189,   189,   189,   190,   190,   190,   191,
     191
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const unsigned char yyr2[] =
{
       0,     2,     1,     3,     0,     2,     2,     2,     2,     2,
       2,     4,     4,     4,     3,     0,     5,     3,     0,     4,
       4,     4,     3,     0,     1,     1,     1,     1,     3,     4,
       4,     0,     3,     0,     3,     4,     4,     4,     4,     0,
       3,     0,     3,     2,     0,     4,     4,     4,     1,     1,
       0,     2,     2,     0,     1,     4,     4,     4,     4,     4,
       4,     1,     0,     2,     2,     0,     1,     4,     4,     4,
       4,     4,     1,     4,     4,     4,     0,     3,     2,     0,
       4,     2,     0,     4,     4,     4,     4,     4,     4,     4,
       2,     0,     1,     3,     0,     1,     1,     1,     1,     1,
       4,     0,     3,     2,     0,     1,     4,     4,     4,     4,
       4,     4,     4,     4,     0,     3,     2,     0,     1,     4,
       4,     4,     4,     4,     4,     4,     0,     3,     0,     3,
       0,     3,     2,     0,     4,     4,     1,     1,     4,     4,
       4,     0,     3,     2,     0,     1,     4,     4,     4,     4,
       4,     0,     3,     2,     0,     4,     4,     4,     4,     4,
       4,     4,     4,     4,     2,     0,     1,     3,     0,     4,
       4,     4,     5,     0,     2,     0,     1,     1,     1,     1,
       3,     0,     4,     4,     4,     4,     4,     4,     4,     4,
       4,     4,     4,     4,     4,     4,     4,     4,     4,     4,
       4,     4,     4,     4,     4,     4,     4,     4,     4,     4,
       4,     4,     4,     5,     1,     2,     3,     3,     3,     4,
       4,     4,     4,     4,     4,     4,     2,     1,     1,     2,
       1,     3,     1,     1,     1,     1,     1,     1,     1,     1,
       1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const unsigned char yydefact[] =
{
       4,     0,     0,     0,     0,     0,     0,     0,     2,     0,
     239,   240,     5,     0,     6,     0,   168,     7,   181,    10,
       8,     0,     9,    15,     1,     4,    23,    94,     0,     0,
       0,   173,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     3,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    24,
      25,    26,    27,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    95,    96,    97,    98,    99,     0,     0,
       0,     0,     0,   168,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    14,   181,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   214,    18,     0,     0,
       0,     0,     0,    50,    62,     0,    11,    23,     0,     0,
       0,     0,     0,     0,     0,     0,    12,    94,     0,     0,
       0,   175,    13,   167,     0,     0,     0,     0,     0,   233,
     234,   235,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   180,     0,
       0,     0,     0,     0,     0,   215,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    28,     0,    31,     0,    33,
       0,    41,     0,    39,     0,    53,     0,    65,     0,    76,
      22,     0,   101,     0,   114,     0,   128,     0,   126,     0,
     130,     0,   141,     0,     0,   151,    93,   169,   171,   170,
     176,   177,   178,   179,     0,   175,   182,   183,   194,   211,
     212,   184,   185,   195,   188,   187,   207,   208,   209,   210,
     196,   197,   191,   193,   192,   189,   190,   200,   201,   202,
     203,   204,   205,   206,   186,   199,   198,     0,     0,   228,
       0,     0,   227,   232,     0,   230,     0,     0,   218,   216,
     217,   213,     0,     0,     0,    16,    18,    29,    82,    30,
      82,    36,    44,    35,    44,    37,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    51,    53,    54,
      61,    38,     0,     0,     0,     0,     0,     0,     0,    66,
      63,    65,    72,    75,    79,   100,   104,   113,   117,   123,
     154,   122,   154,   124,   133,   125,   144,   149,   150,   154,
       0,   174,   225,   222,   220,   219,   226,     0,   221,   229,
     223,   224,     0,     0,     0,    17,    32,    82,    34,     0,
       0,     0,    42,    44,    48,    49,    40,     0,     0,     0,
      91,     0,     0,     0,     0,     0,     0,     0,    52,     0,
       0,     0,     0,     0,     0,     0,    64,     0,    77,    79,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   102,   104,   105,     0,
       0,     0,   115,   117,   118,   129,   154,   127,   131,   142,
     152,   172,   231,    21,    19,    20,    81,     0,     0,     0,
      43,     0,     0,     0,    92,     0,    91,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     236,   237,   238,     0,     0,     0,     0,    78,     0,     0,
       0,     0,     0,     0,   165,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   103,     0,     0,     0,   116,   153,
       0,     0,     0,     0,   137,   132,   136,     0,     0,     0,
     145,   143,     0,     0,     0,    84,    86,    85,    83,    90,
      87,    89,    88,    57,    60,    58,    55,    59,    56,    73,
      74,    67,    68,    70,    69,    71,     0,     0,     0,     0,
       0,     0,     0,   166,     0,   165,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    45,    46,    47,    80,   160,
     157,   161,   162,   163,   156,   155,   164,   112,   106,   107,
     108,   109,   110,   111,   158,   159,   119,   120,   121,     0,
       0,     0,     0,     0,     0,     0,     0,   134,   138,   135,
     139,   140,   146,   147,   148
};

/* YYDEFGOTO[NTERM-NUM]. */
static const short yydefgoto[] =
{
      -1,     7,     8,     9,    12,    14,    17,    19,    22,    67,
     223,   224,    77,    78,    79,    80,   226,   318,   228,   320,
      81,   232,   324,   230,   322,   392,   393,   234,   235,   337,
     338,   339,   236,   237,   350,   351,   394,    82,   238,   354,
     418,   419,   386,   340,   465,   466,    91,    92,    93,   241,
     356,   436,   437,    94,   243,   358,   442,   443,    95,   247,
     362,   245,   360,   249,   364,   448,   514,   515,   251,   366,
     449,   521,    96,    97,   254,   369,   445,   446,   554,   555,
      31,    32,   102,   264,   265,    64,    65,    20,   145,   146,
     301,   302,   304,   305,   182,   483,    13
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -353
static const short yypact[] =
{
      43,    17,    17,   -72,   -69,    17,    17,    29,  -353,   -50,
    -353,  -353,  -353,   -48,  -353,   -29,    64,  -353,   146,  -353,
    -353,   -12,  -353,  -353,  -353,    43,   178,   264,    14,    16,
      30,    65,    44,    40,    47,    48,    49,    50,    51,    52,
      53,    58,    59,    61,    62,    71,    72,    92,   110,   111,
     112,   113,   117,   122,   124,   125,   126,   127,   128,   133,
     135,   142,   143,   144,   147,   148,    -5,   152,  -353,   151,
     153,   157,   158,   161,   165,   169,   172,   174,   177,  -353,
    -353,  -353,  -353,   187,   191,   192,   193,   194,   197,   198,
     199,   196,   203,  -353,  -353,  -353,  -353,  -353,    17,    17,
      17,   201,   207,    64,    54,   215,   217,    17,    17,   -41,
     -41,   -41,   -41,   -41,   -41,   218,   220,   222,    17,    17,
     223,   224,   239,   240,   241,   -41,    17,    17,   242,    17,
      17,   243,   -41,   247,   249,  -353,   146,   202,   245,   248,
     256,   257,   259,    -5,    -5,   -18,  -353,   -21,   246,    17,
      17,    17,    17,  -353,  -353,    17,  -353,   178,    17,    17,
      17,    17,    17,    17,    17,    17,  -353,   264,   260,   261,
     263,    11,  -353,  -353,   265,   267,   268,   273,   274,  -353,
    -353,  -353,   277,   278,   282,   283,   285,   286,   287,   288,
     290,   292,   293,   294,   295,   297,   298,   299,   300,   301,
     305,   307,   313,   314,   317,   318,   319,   320,  -353,    17,
      17,     7,   258,    17,    17,  -353,   -49,    -5,    -5,   323,
     324,   325,   326,   328,   266,  -353,   322,  -353,   327,  -353,
     329,  -353,   330,  -353,   331,    73,   332,    -4,   333,  -353,
    -353,   334,  -353,   335,  -353,   336,  -353,   337,  -353,   338,
    -353,   340,  -353,   341,   342,  -353,  -353,  -353,  -353,  -353,
    -353,  -353,  -353,  -353,   343,    11,  -353,  -353,  -353,  -353,
    -353,  -353,  -353,  -353,  -353,  -353,  -353,  -353,  -353,  -353,
    -353,  -353,  -353,  -353,  -353,  -353,  -353,  -353,  -353,  -353,
    -353,  -353,  -353,  -353,  -353,  -353,  -353,   344,   345,  -353,
     346,   347,   269,   350,   348,   258,   349,   351,  -353,   356,
    -353,  -353,    17,    17,   -41,  -353,   -21,  -353,   210,  -353,
     210,  -353,   284,  -353,   284,  -353,   353,   354,   355,   357,
     358,   359,   360,   361,   364,   365,   366,  -353,    73,  -353,
    -353,  -353,   367,   368,   369,   370,   371,   372,   373,  -353,
    -353,    -4,  -353,  -353,   390,  -353,   262,  -353,   312,  -353,
     339,  -353,   339,  -353,  -353,  -353,  -353,  -353,  -353,   339,
     377,  -353,  -353,  -353,  -353,  -353,  -353,   384,  -353,  -353,
    -353,  -353,   375,   376,   378,  -353,  -353,   210,  -353,   380,
     381,   382,  -353,   284,  -353,  -353,  -353,   386,   391,   392,
     405,   393,    17,   394,    17,    17,   -11,     1,  -353,   -41,
     395,   -11,     1,    46,    46,    46,  -353,   388,  -353,   390,
     389,   398,   399,   400,   403,   406,   408,   409,   412,   413,
     414,   415,   416,   417,   418,   419,  -353,   262,  -353,   420,
     421,   422,  -353,   312,  -353,  -353,   339,  -353,   195,   206,
    -353,  -353,  -353,  -353,  -353,  -353,  -353,    17,    17,   401,
    -353,   423,   424,   425,  -353,   426,   405,   427,   428,   429,
     430,   431,   432,   433,   434,   435,   436,   437,   438,   439,
    -353,  -353,  -353,   440,   441,   442,    17,  -353,   -41,   402,
     -41,    17,    17,   452,   467,   -41,    17,    17,   453,    17,
      17,   454,    17,   -41,  -353,    17,    17,   455,  -353,  -353,
     448,   449,   450,   451,  -353,  -353,  -353,   456,   457,   458,
    -353,  -353,   459,   460,   461,  -353,  -353,  -353,  -353,  -353,
    -353,  -353,  -353,  -353,  -353,  -353,  -353,  -353,  -353,  -353,
    -353,  -353,  -353,  -353,  -353,  -353,   462,   463,   464,   465,
     466,   468,   469,  -353,   470,   467,   471,   479,   480,   481,
     482,   483,   484,   485,   486,   487,   488,   489,    17,    17,
      45,    17,    17,    45,    17,  -353,  -353,  -353,  -353,  -353,
    -353,  -353,  -353,  -353,  -353,  -353,  -353,  -353,  -353,  -353,
    -353,  -353,  -353,  -353,  -353,  -353,  -353,  -353,  -353,   490,
     491,   492,   493,   494,   495,   496,   497,  -353,  -353,  -353,
    -353,  -353,  -353,  -353,  -353
};

/* YYPGOTO[NTERM-NUM].  */
static const short yypgoto[] =
{
    -353,  -353,   397,  -353,  -353,  -353,  -353,  -353,  -353,  -353,
     167,  -353,   404,  -353,  -353,  -353,  -353,  -353,  -353,  -353,
    -353,  -353,  -353,  -353,  -353,  -313,  -353,  -353,  -353,   150,
    -353,  -236,  -353,  -353,   134,  -353,  -225,  -353,  -353,  -353,
     129,  -353,  -306,  -294,    79,  -353,   379,  -353,  -353,  -353,
    -353,   115,  -353,  -353,  -353,  -353,   104,  -353,  -353,  -353,
    -353,  -353,  -353,  -353,  -353,  -353,   145,  -353,  -353,  -353,
    -353,  -353,  -353,  -353,  -353,  -353,  -352,  -335,    37,  -353,
     498,  -353,  -353,   352,  -353,   472,  -353,  -353,  -128,  -353,
     291,  -353,   302,  -353,  -105,  -332,    -2
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const unsigned short yytable[] =
{
      15,   349,   137,    21,    23,   183,   184,   185,   186,   187,
     447,   396,   352,   138,   388,   215,   216,   450,   326,    16,
     198,   438,    18,   444,   387,   220,   387,   205,   395,    24,
     395,   179,   180,   327,   328,   329,   221,   222,   217,   218,
      25,   330,   331,    26,   181,   308,     1,     2,     3,     4,
       5,   260,   261,   262,   263,   332,   342,   343,   333,   334,
     344,   345,    27,   139,   140,   141,   142,    28,    29,   217,
     218,    30,   219,    10,   472,    11,   346,   347,   348,    66,
     460,   456,   484,   485,   143,    10,   474,    11,   144,   309,
     310,   299,   300,   387,   509,   326,   168,   169,   170,   395,
       6,    10,   438,    11,   101,   177,   178,    98,   444,    99,
     327,   328,   329,   516,   516,   349,   191,   192,   330,   331,
     480,   481,   482,   100,   199,   200,   352,   202,   203,    10,
     601,    11,   332,   104,   103,   333,   334,   335,   336,   174,
     105,   106,   107,   108,   109,   110,   111,   227,   229,   231,
     233,   112,   113,   239,   114,   115,   242,   244,   246,   248,
     250,   252,   253,   255,   116,   117,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,   118,    69,    70,    71,    72,
      73,    74,    75,    52,    53,    76,    54,    55,    56,    57,
      58,    59,    60,   119,   120,   121,   122,   297,   298,   384,
     123,   306,   307,    61,   420,   124,   421,   125,   126,   127,
     128,   129,   422,    62,    63,   420,   130,   421,   131,   423,
     424,   425,   326,   422,   426,   132,   133,   134,   136,   135,
     423,   424,   425,   147,   148,   426,   149,   327,   328,   329,
     150,   151,   434,   435,   152,   330,   331,   510,   153,   511,
     512,   513,   154,   434,   435,   155,   156,   157,   510,   332,
     517,   518,   519,    83,    84,    85,    86,    87,    88,    89,
     158,   420,    90,   421,   159,   160,   161,   162,   166,   422,
     163,   164,   165,   167,   171,   209,   423,   424,   425,   172,
     175,   426,   176,   188,   476,   189,   326,   190,   193,   194,
     382,   383,   427,   428,   429,   430,   431,   432,   433,   434,
     435,   327,   328,   329,   195,   196,   197,   201,   204,   330,
     331,   420,   206,   421,   207,   389,   390,   391,   210,   422,
     225,   211,   303,   332,   342,   343,   423,   424,   425,   212,
     213,   426,   214,   299,   257,   258,   316,   259,   420,   266,
     421,   267,   268,   439,   440,   441,   422,   269,   270,   434,
     435,   271,   272,   423,   424,   425,   273,   274,   426,   275,
     276,   277,   278,   547,   279,   549,   280,   281,   282,   283,
     556,   284,   285,   286,   287,   288,   434,   435,   564,   289,
     468,   290,   470,   471,   473,   475,   417,   291,   292,   478,
     479,   293,   294,   295,   296,   311,   317,   312,   313,   314,
     315,   319,    68,   321,   323,   325,   341,   353,   355,   357,
     359,   361,   363,   377,   365,   367,   368,   370,   372,   373,
     374,   375,   378,   380,   218,   381,   397,   398,   399,   464,
     400,   401,   402,   403,   404,   522,   523,   405,   406,   407,
     409,   410,   411,   412,   413,   414,   415,   451,   452,   453,
     454,   461,   455,   457,   458,   459,   462,   463,   467,   469,
     477,   486,   488,   385,   546,   416,   524,   548,   408,   550,
     551,   489,   490,   491,   557,   558,   492,   560,   561,   493,
     563,   494,   495,   565,   566,   496,   497,   498,   499,   500,
     501,   502,   503,   505,   506,   507,   553,   525,   526,   527,
     528,   530,   531,   532,   533,   534,   535,   536,   537,   538,
     539,   540,   541,   542,   543,   544,   545,   552,   559,   562,
     567,   568,   569,   570,   571,   529,   256,   508,   487,   572,
     573,   574,   504,   575,   576,   577,   578,   579,   580,   581,
     582,   240,   583,   584,   585,   587,   599,   600,   602,   603,
     604,   605,   606,   588,   589,   590,   591,   592,   593,   594,
     595,   596,   597,   598,   607,   608,   609,   610,   611,   612,
     613,   614,   586,   376,   520,     0,     0,     0,     0,     0,
       0,   173,     0,     0,     0,     0,     0,   379,   208,     0,
       0,     0,     0,     0,     0,     0,     0,   371
};

static const short yycheck[] =
{
       2,   237,     7,     5,     6,   110,   111,   112,   113,   114,
     362,   324,   237,    18,   320,   143,   144,   369,    22,    91,
     125,   356,    91,   358,   318,    46,   320,   132,   322,     0,
     324,    72,    73,    37,    38,    39,    57,    58,    87,    88,
      90,    45,    46,    91,    85,    94,     3,     4,     5,     6,
       7,    40,    41,    42,    43,    59,    60,    61,    62,    63,
      64,    65,    91,    68,    69,    70,    71,     3,     4,    87,
      88,     7,    90,    84,    85,    86,    80,    81,    82,    91,
     393,   387,   414,   415,    89,    84,    85,    86,    93,   217,
     218,    84,    85,   387,   446,    22,    98,    99,   100,   393,
      57,    84,   437,    86,    39,   107,   108,    93,   443,    93,
      37,    38,    39,   448,   449,   351,   118,   119,    45,    46,
      74,    75,    76,    93,   126,   127,   351,   129,   130,    84,
      85,    86,    59,    93,    90,    62,    63,    64,    65,    85,
      93,    93,    93,    93,    93,    93,    93,   149,   150,   151,
     152,    93,    93,   155,    93,    93,   158,   159,   160,   161,
     162,   163,   164,   165,    93,    93,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    93,     8,     9,    10,    11,
      12,    13,    14,    47,    48,    17,    50,    51,    52,    53,
      54,    55,    56,    93,    93,    93,    93,   209,   210,   314,
      93,   213,   214,    67,    19,    93,    21,    93,    93,    93,
      93,    93,    27,    77,    78,    19,    93,    21,    93,    34,
      35,    36,    22,    27,    39,    93,    93,    93,    90,    92,
      34,    35,    36,    91,    93,    39,    93,    37,    38,    39,
      93,    93,    57,    58,    93,    45,    46,    62,    93,    64,
      65,    66,    93,    57,    58,    93,    92,    90,    62,    59,
      64,    65,    66,     9,    10,    11,    12,    13,    14,    15,
      93,    19,    18,    21,    93,    93,    93,    93,    92,    27,
      93,    93,    93,    90,    93,    93,    34,    35,    36,    92,
      85,    39,    85,    85,   409,    85,    22,    85,    85,    85,
     312,   313,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    37,    38,    39,    85,    85,    85,    85,    85,    45,
      46,    19,    85,    21,    85,    51,    52,    53,    93,    27,
      94,    93,    84,    59,    60,    61,    34,    35,    36,    93,
      93,    39,    93,    84,    94,    94,    90,    94,    19,    94,
      21,    94,    94,    51,    52,    53,    27,    94,    94,    57,
      58,    94,    94,    34,    35,    36,    94,    94,    39,    94,
      94,    94,    94,   488,    94,   490,    94,    94,    94,    94,
     495,    94,    94,    94,    94,    94,    57,    58,   503,    94,
     402,    94,   404,   405,   406,   407,    16,    94,    94,   411,
     412,    94,    94,    94,    94,    92,    94,    93,    93,    93,
      92,    94,    25,    94,    94,    94,    94,    94,    94,    94,
      94,    94,    94,    83,    94,    94,    94,    94,    94,    94,
      94,    94,    94,    94,    88,    94,    93,    93,    93,    44,
      93,    93,    93,    93,    93,   457,   458,    93,    93,    93,
      93,    93,    93,    93,    93,    93,    93,    90,    84,    94,
      94,    85,    94,    93,    93,    93,    85,    85,    85,    85,
      85,    93,    93,   316,   486,   351,    85,    85,   338,   491,
     492,    93,    93,    93,   496,   497,    93,   499,   500,    93,
     502,    93,    93,   505,   506,    93,    93,    93,    93,    93,
      93,    93,    93,    93,    93,    93,    49,    94,    94,    94,
      94,    94,    94,    94,    94,    94,    94,    94,    94,    94,
      94,    94,    94,    94,    94,    94,    94,    85,    85,    85,
      85,    93,    93,    93,    93,   466,   167,   443,   419,    93,
      93,    93,   437,    94,    94,    94,    94,    94,    94,    94,
      94,   157,    94,    94,    94,    94,   568,   569,   570,   571,
     572,   573,   574,    94,    94,    94,    94,    94,    94,    94,
      94,    94,    94,    94,    94,    94,    94,    94,    94,    94,
      94,    94,   555,   302,   449,    -1,    -1,    -1,    -1,    -1,
      -1,   103,    -1,    -1,    -1,    -1,    -1,   305,   136,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   265
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned char yystos[] =
{
       0,     3,     4,     5,     6,     7,    57,    96,    97,    98,
      84,    86,    99,   191,   100,   191,    91,   101,    91,   102,
     182,   191,   103,   191,     0,    90,    91,    91,     3,     4,
       7,   175,   176,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    47,    48,    50,    51,    52,    53,    54,    55,
      56,    67,    77,    78,   180,   181,    91,   104,    97,     8,
       9,    10,    11,    12,    13,    14,    17,   107,   108,   109,
     110,   115,   132,     9,    10,    11,    12,    13,    14,    15,
      18,   141,   142,   143,   148,   153,   167,   168,    93,    93,
      93,    39,   177,    90,    93,    93,    93,    93,    93,    93,
      93,    93,    93,    93,    93,    93,    93,    93,    93,    93,
      93,    93,    93,    93,    93,    93,    93,    93,    93,    93,
      93,    93,    93,    93,    93,    92,    90,     7,    18,    68,
      69,    70,    71,    89,    93,   183,   184,    91,    93,    93,
      93,    93,    93,    93,    93,    93,    92,    90,    93,    93,
      93,    93,    93,    93,    93,    93,    92,    90,   191,   191,
     191,    93,    92,   175,    85,    85,    85,   191,   191,    72,
      73,    85,   189,   189,   189,   189,   189,   189,    85,    85,
      85,   191,   191,    85,    85,    85,    85,    85,   189,   191,
     191,    85,   191,   191,    85,   189,    85,    85,   180,    93,
      93,    93,    93,    93,    93,   183,   183,    87,    88,    90,
      46,    57,    58,   105,   106,    94,   111,   191,   113,   191,
     118,   191,   116,   191,   122,   123,   127,   128,   133,   191,
     107,   144,   191,   149,   191,   156,   191,   154,   191,   158,
     191,   163,   191,   191,   169,   191,   141,    94,    94,    94,
      40,    41,    42,    43,   178,   179,    94,    94,    94,    94,
      94,    94,    94,    94,    94,    94,    94,    94,    94,    94,
      94,    94,    94,    94,    94,    94,    94,    94,    94,    94,
      94,    94,    94,    94,    94,    94,    94,   191,   191,    84,
      85,   185,   186,    84,   187,   188,   191,   191,    94,   183,
     183,    92,    93,    93,    93,    92,    90,    94,   112,    94,
     114,    94,   119,    94,   117,    94,    22,    37,    38,    39,
      45,    46,    59,    62,    63,    64,    65,   124,   125,   126,
     138,    94,    60,    61,    64,    65,    80,    81,    82,   126,
     129,   130,   131,    94,   134,    94,   145,    94,   150,    94,
     157,    94,   155,    94,   159,    94,   164,    94,    94,   170,
      94,   178,    94,    94,    94,    94,   185,    83,    94,   187,
      94,    94,   191,   191,   189,   105,   137,   138,   137,    51,
      52,    53,   120,   121,   131,   138,   120,    93,    93,    93,
      93,    93,    93,    93,    93,    93,    93,    93,   124,    93,
      93,    93,    93,    93,    93,    93,   129,    16,   135,   136,
      19,    21,    27,    34,    35,    36,    39,    50,    51,    52,
      53,    54,    55,    56,    57,    58,   146,   147,   172,    51,
      52,    53,   151,   152,   172,   171,   172,   171,   160,   165,
     171,    90,    84,    94,    94,    94,   137,    93,    93,    93,
     120,    85,    85,    85,    44,   139,   140,    85,   191,    85,
     191,   191,    85,   191,    85,   191,   189,    85,   191,   191,
      74,    75,    76,   190,   190,   190,    93,   135,    93,    93,
      93,    93,    93,    93,    93,    93,    93,    93,    93,    93,
      93,    93,    93,    93,   146,    93,    93,    93,   151,   171,
      62,    64,    65,    66,   161,   162,   172,    64,    65,    66,
     161,   166,   191,   191,    85,    94,    94,    94,    94,   139,
      94,    94,    94,    94,    94,    94,    94,    94,    94,    94,
      94,    94,    94,    94,    94,    94,   191,   189,    85,   189,
     191,   191,    85,    49,   173,   174,   189,   191,   191,    85,
     191,   191,    85,   191,   189,   191,   191,    85,    93,    93,
      93,    93,    93,    93,    93,    94,    94,    94,    94,    94,
      94,    94,    94,    94,    94,    94,   173,    94,    94,    94,
      94,    94,    94,    94,    94,    94,    94,    94,    94,   191,
     191,    85,   191,   191,   191,   191,   191,    94,    94,    94,
      94,    94,    94,    94,    94
};

#if ! defined (YYSIZE_T) && defined (__SIZE_TYPE__)
# define YYSIZE_T __SIZE_TYPE__
#endif
#if ! defined (YYSIZE_T) && defined (size_t)
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T)
# if defined (__STDC__) || defined (__cplusplus)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# endif
#endif
#if ! defined (YYSIZE_T)
# define YYSIZE_T unsigned int
#endif

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrlab1


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { 								\
      yyerror ("syntax error: cannot back up");\
      YYERROR;							\
    }								\
while (0)

#define YYTERROR	1
#define YYERRCODE	256

/* YYLLOC_DEFAULT -- Compute the default location (before the actions
   are run).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)         \
  Current.first_line   = Rhs[1].first_line;      \
  Current.first_column = Rhs[1].first_column;    \
  Current.last_line    = Rhs[N].last_line;       \
  Current.last_column  = Rhs[N].last_column;
#endif

/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (0)

# define YYDSYMPRINT(Args)			\
do {						\
  if (yydebug)					\
    yysymprint Args;				\
} while (0)

# define YYDSYMPRINTF(Title, Token, Value, Location)		\
do {								\
  if (yydebug)							\
    {								\
      YYFPRINTF (stderr, "%s ", Title);				\
      yysymprint (stderr, 					\
                  Token, Value);	\
      YYFPRINTF (stderr, "\n");					\
    }								\
} while (0)

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (cinluded).                                                   |
`------------------------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_stack_print (short *bottom, short *top)
#else
static void
yy_stack_print (bottom, top)
    short *bottom;
    short *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (/* Nothing. */; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_reduce_print (int yyrule)
#else
static void
yy_reduce_print (yyrule)
    int yyrule;
#endif
{
  int yyi;
  unsigned int yylineno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %u), ",
             yyrule - 1, yylineno);
  /* Print the symbols being reduced, and their result.  */
  for (yyi = yyprhs[yyrule]; 0 <= yyrhs[yyi]; yyi++)
    YYFPRINTF (stderr, "%s ", yytname [yyrhs[yyi]]);
  YYFPRINTF (stderr, "-> %s\n", yytname [yyr1[yyrule]]);
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (Rule);		\
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YYDSYMPRINT(Args)
# define YYDSYMPRINTF(Title, Token, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   SIZE_MAX < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#if YYMAXDEPTH == 0
# undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined (__GLIBC__) && defined (_STRING_H)
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
#   if defined (__STDC__) || defined (__cplusplus)
yystrlen (const char *yystr)
#   else
yystrlen (yystr)
     const char *yystr;
#   endif
{
  register const char *yys = yystr;

  while (*yys++ != '\0')
    continue;

  return yys - yystr - 1;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined (__GLIBC__) && defined (_STRING_H) && defined (_GNU_SOURCE)
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
#   if defined (__STDC__) || defined (__cplusplus)
yystpcpy (char *yydest, const char *yysrc)
#   else
yystpcpy (yydest, yysrc)
     char *yydest;
     const char *yysrc;
#   endif
{
  register char *yyd = yydest;
  register const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

#endif /* !YYERROR_VERBOSE */



#if YYDEBUG
/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yysymprint (FILE *yyoutput, int yytype, YYSTYPE *yyvaluep)
#else
static void
yysymprint (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;

  if (yytype < YYNTOKENS)
    {
      YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
# ifdef YYPRINT
      YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
    }
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  switch (yytype)
    {
      default:
        break;
    }
  YYFPRINTF (yyoutput, ")");
}

#endif /* ! YYDEBUG */
/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yydestruct (int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yytype, yyvaluep)
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;

  switch (yytype)
    {

      default:
        break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM);
# else
int yyparse ();
# endif
#else /* ! YYPARSE_PARAM */
#if defined (__STDC__) || defined (__cplusplus)
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */



/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;



/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM)
# else
int yyparse (YYPARSE_PARAM)
  void *YYPARSE_PARAM;
# endif
#else /* ! YYPARSE_PARAM */
#if defined (__STDC__) || defined (__cplusplus)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
  
  register int yystate;
  register int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  short	yyssa[YYINITDEPTH];
  short *yyss = yyssa;
  register short *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  register YYSTYPE *yyvsp;



#define YYPOPSTACK   (yyvsp--, yyssp--)

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;


  /* When reducing, the number of symbols on the RHS of the reduced
     rule.  */
  int yylen;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss;
  yyvsp = yyvs;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed. so pushing a state here evens the stacks.
     */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack. Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	short *yyss1 = yyss;


	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),

		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyoverflowlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyoverflowlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	short *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyoverflowlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);

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

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YYDSYMPRINTF ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */
  YYDPRINTF ((stderr, "Shifting token %s, ", yytname[yytoken]));

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;


  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  yystate = yyn;
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
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 5:
#line 187 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { cfg_add_source(configuration, yyvsp[0].ptr); }
    break;

  case 6:
#line 188 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { cfg_add_dest(configuration, yyvsp[0].ptr); }
    break;

  case 7:
#line 189 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { cfg_add_connection(configuration, yyvsp[0].ptr); }
    break;

  case 8:
#line 190 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { cfg_add_filter(configuration, yyvsp[0].ptr); }
    break;

  case 9:
#line 191 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { cfg_add_template(configuration, yyvsp[0].ptr); }
    break;

  case 10:
#line 192 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    {  }
    break;

  case 11:
#line 196 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = log_source_group_new(yyvsp[-3].cptr, yyvsp[-1].ptr); free(yyvsp[-3].cptr); }
    break;

  case 12:
#line 200 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = log_dest_group_new(yyvsp[-3].cptr, yyvsp[-1].ptr); free(yyvsp[-3].cptr); }
    break;

  case 13:
#line 204 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = log_connection_new(yyvsp[-2].ptr, yyvsp[-1].num); }
    break;

  case 14:
#line 208 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = NULL; }
    break;

  case 15:
#line 213 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    {
	    last_template = log_template_new(yyvsp[0].cptr, NULL);
	    free(yyvsp[0].cptr);
	  }
    break;

  case 16:
#line 217 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = last_template;  }
    break;

  case 19:
#line 226 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { last_template->template = g_string_new(yyvsp[-1].cptr); free(yyvsp[-1].cptr); }
    break;

  case 20:
#line 227 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { log_template_set_escape(last_template, yyvsp[-1].num); }
    break;

  case 21:
#line 228 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { if (cfg_timezone_value(yyvsp[-1].cptr, &last_template->zone_offset)) 
	                                            last_template->flags |= LT_TZ_SET; 
	                                          free(yyvsp[-1].cptr);
	                                        }
    break;

  case 22:
#line 235 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { log_drv_append(yyvsp[-2].ptr, yyvsp[0].ptr); log_drv_unref(yyvsp[0].ptr); yyval.ptr = yyvsp[-2].ptr; }
    break;

  case 23:
#line 236 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = NULL; }
    break;

  case 24:
#line 240 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = yyvsp[0].ptr; }
    break;

  case 25:
#line 241 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = yyvsp[0].ptr; }
    break;

  case 26:
#line 242 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = yyvsp[0].ptr; }
    break;

  case 27:
#line 243 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = yyvsp[0].ptr; }
    break;

  case 28:
#line 247 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = afinter_sd_new(); }
    break;

  case 29:
#line 251 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = yyvsp[-1].ptr; }
    break;

  case 30:
#line 252 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = yyvsp[-1].ptr; }
    break;

  case 31:
#line 257 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    {
	    last_driver = affile_sd_new(yyvsp[0].cptr, 0); 
	    free(yyvsp[0].cptr); 
	    last_reader_options = &((AFFileSourceDriver *) last_driver)->reader_options;
	  }
    break;

  case 32:
#line 262 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = last_driver; }
    break;

  case 33:
#line 267 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    {
	    last_driver = affile_sd_new(yyvsp[0].cptr, AFFILE_PIPE); 
	    free(yyvsp[0].cptr); 
	    last_reader_options = &((AFFileSourceDriver *) last_driver)->reader_options;
	  }
    break;

  case 34:
#line 272 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = last_driver; }
    break;

  case 35:
#line 276 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = yyvsp[-1].ptr; }
    break;

  case 36:
#line 277 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = yyvsp[-1].ptr; }
    break;

  case 37:
#line 278 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = yyvsp[-1].ptr; }
    break;

  case 38:
#line 279 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = yyvsp[-1].ptr; }
    break;

  case 39:
#line 284 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { 
	    last_driver = afunix_sd_new(
		yyvsp[0].cptr,
		AFSOCKET_DGRAM | AFSOCKET_LOCAL); 
	    free(yyvsp[0].cptr); 
	    last_reader_options = &((AFSocketSourceDriver *) last_driver)->reader_options;
	  }
    break;

  case 40:
#line 291 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = last_driver; }
    break;

  case 41:
#line 296 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { 
	    last_driver = afunix_sd_new(
		yyvsp[0].cptr,
		AFSOCKET_STREAM | AFSOCKET_KEEP_ALIVE | AFSOCKET_LOCAL);
	    free(yyvsp[0].cptr);
	    last_reader_options = &((AFSocketSourceDriver *) last_driver)->reader_options;
	  }
    break;

  case 42:
#line 303 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = last_driver; }
    break;

  case 45:
#line 313 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { afunix_sd_set_uid(last_driver, yyvsp[-1].cptr); free(yyvsp[-1].cptr); }
    break;

  case 46:
#line 314 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { afunix_sd_set_gid(last_driver, yyvsp[-1].cptr); free(yyvsp[-1].cptr); }
    break;

  case 47:
#line 315 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { afunix_sd_set_perm(last_driver, yyvsp[-1].num); }
    break;

  case 48:
#line 316 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    {}
    break;

  case 49:
#line 317 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    {}
    break;

  case 50:
#line 322 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { 
	    last_driver = afinet_sd_new(
			"0.0.0.0", 514,
			AFSOCKET_DGRAM);
	    last_reader_options = &((AFSocketSourceDriver *) last_driver)->reader_options;
	  }
    break;

  case 51:
#line 328 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = last_driver; }
    break;

  case 55:
#line 338 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { afinet_sd_set_localport(last_driver, 0, yyvsp[-1].cptr, "udp"); free(yyvsp[-1].cptr); }
    break;

  case 56:
#line 339 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { afinet_sd_set_localport(last_driver, 0, yyvsp[-1].cptr, "udp"); free(yyvsp[-1].cptr); }
    break;

  case 57:
#line 343 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { afinet_sd_set_localip(last_driver, yyvsp[-1].cptr); free(yyvsp[-1].cptr); }
    break;

  case 58:
#line 344 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { afinet_sd_set_localport(last_driver, yyvsp[-1].num, NULL, NULL); }
    break;

  case 59:
#line 345 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { afinet_sd_set_localport(last_driver, yyvsp[-1].num, NULL, NULL); }
    break;

  case 60:
#line 346 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { afinet_sd_set_localip(last_driver, yyvsp[-1].cptr); free(yyvsp[-1].cptr); }
    break;

  case 61:
#line 347 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    {}
    break;

  case 62:
#line 352 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { 
	    last_driver = afinet_sd_new(
			"0.0.0.0", 514,
			AFSOCKET_STREAM);
	    last_reader_options = &((AFSocketSourceDriver *) last_driver)->reader_options;
	  }
    break;

  case 63:
#line 358 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = last_driver; }
    break;

  case 67:
#line 368 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { afinet_sd_set_localport(last_driver, 0, yyvsp[-1].cptr, "tcp"); free(yyvsp[-1].cptr); }
    break;

  case 68:
#line 369 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { afinet_sd_set_localport(last_driver, 0, yyvsp[-1].cptr, "tcp"); free(yyvsp[-1].cptr); }
    break;

  case 69:
#line 370 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { afinet_sd_set_auth(last_driver, yyvsp[-1].num); }
    break;

  case 70:
#line 371 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { afinet_sd_set_mac(last_driver, yyvsp[-1].num); }
    break;

  case 71:
#line 372 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { afinet_sd_set_encrypt(last_driver, yyvsp[-1].num); }
    break;

  case 72:
#line 373 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    {}
    break;

  case 73:
#line 377 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { afsocket_sd_set_keep_alive(last_driver, yyvsp[-1].num); }
    break;

  case 74:
#line 378 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { afsocket_sd_set_max_connections(last_driver, yyvsp[-1].num); }
    break;

  case 75:
#line 382 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = yyvsp[-1].ptr; }
    break;

  case 76:
#line 387 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { 
	    last_driver = afstreams_sd_new(yyvsp[0].cptr); 
	    free(yyvsp[0].cptr); 
	  }
    break;

  case 77:
#line 391 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = last_driver; }
    break;

  case 80:
#line 400 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { afstreams_sd_set_sundoor(last_driver, yyvsp[-1].cptr); free(yyvsp[-1].cptr); }
    break;

  case 83:
#line 409 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { last_reader_options->options = yyvsp[-1].num; }
    break;

  case 84:
#line 410 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { last_reader_options->msg_size = yyvsp[-1].num; }
    break;

  case 85:
#line 411 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { last_reader_options->init_window_size = yyvsp[-1].num; }
    break;

  case 86:
#line 412 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { last_reader_options->fetch_limit = yyvsp[-1].num; }
    break;

  case 87:
#line 413 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { last_reader_options->padding = yyvsp[-1].num; }
    break;

  case 88:
#line 414 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { last_reader_options->follow_freq = yyvsp[-1].num; }
    break;

  case 89:
#line 415 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { last_reader_options->zone_offset_set = cfg_timezone_value(yyvsp[-1].cptr, &last_reader_options->zone_offset); free(yyvsp[-1].cptr); }
    break;

  case 90:
#line 419 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.num = yyvsp[-1].num | yyvsp[0].num; }
    break;

  case 91:
#line 420 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.num = 0; }
    break;

  case 92:
#line 424 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.num = LRO_NOPARSE; }
    break;

  case 93:
#line 428 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { log_drv_append(yyvsp[-2].ptr, yyvsp[0].ptr); log_drv_unref(yyvsp[0].ptr); yyval.ptr = yyvsp[-2].ptr; }
    break;

  case 94:
#line 429 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = NULL; }
    break;

  case 95:
#line 433 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = yyvsp[0].ptr; }
    break;

  case 96:
#line 434 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = yyvsp[0].ptr; }
    break;

  case 97:
#line 435 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = yyvsp[0].ptr; }
    break;

  case 98:
#line 436 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = yyvsp[0].ptr; }
    break;

  case 99:
#line 437 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = yyvsp[0].ptr; }
    break;

  case 100:
#line 441 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = yyvsp[-1].ptr; }
    break;

  case 101:
#line 446 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { 
	    last_driver = affile_dd_new(yyvsp[0].cptr, 0); 
	    free(yyvsp[0].cptr); 
	    last_writer_options = &((AFFileDestDriver *) last_driver)->writer_options;
	  }
    break;

  case 102:
#line 452 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = last_driver; }
    break;

  case 106:
#line 466 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { affile_dd_set_file_uid(last_driver, yyvsp[-1].cptr); free(yyvsp[-1].cptr); }
    break;

  case 107:
#line 467 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { affile_dd_set_file_gid(last_driver, yyvsp[-1].cptr); free(yyvsp[-1].cptr); }
    break;

  case 108:
#line 468 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { affile_dd_set_file_perm(last_driver, yyvsp[-1].num); }
    break;

  case 109:
#line 469 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { affile_dd_set_dir_uid(last_driver, yyvsp[-1].cptr); free(yyvsp[-1].cptr); }
    break;

  case 110:
#line 470 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { affile_dd_set_dir_gid(last_driver, yyvsp[-1].cptr); free(yyvsp[-1].cptr); }
    break;

  case 111:
#line 471 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { affile_dd_set_dir_perm(last_driver, yyvsp[-1].num); }
    break;

  case 112:
#line 472 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { affile_dd_set_create_dirs(last_driver, yyvsp[-1].num); }
    break;

  case 113:
#line 476 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = yyvsp[-1].ptr; }
    break;

  case 114:
#line 481 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { 
	    last_driver = affile_dd_new(yyvsp[0].cptr, AFFILE_NO_EXPAND | AFFILE_PIPE);
	    affile_dd_set_sync_freq(last_driver, 0);
	    free(yyvsp[0].cptr); 
	    last_writer_options = &((AFFileDestDriver *) last_driver)->writer_options;
	  }
    break;

  case 115:
#line 487 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = last_driver; }
    break;

  case 119:
#line 497 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { affile_dd_set_file_uid(last_driver, yyvsp[-1].cptr); free(yyvsp[-1].cptr); }
    break;

  case 120:
#line 498 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { affile_dd_set_file_gid(last_driver, yyvsp[-1].cptr); free(yyvsp[-1].cptr); }
    break;

  case 121:
#line 499 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { affile_dd_set_file_perm(last_driver, yyvsp[-1].num); }
    break;

  case 122:
#line 504 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = yyvsp[-1].ptr; }
    break;

  case 123:
#line 505 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = yyvsp[-1].ptr; }
    break;

  case 124:
#line 506 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = yyvsp[-1].ptr; }
    break;

  case 125:
#line 507 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = yyvsp[-1].ptr; }
    break;

  case 126:
#line 512 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { 
	    last_driver = afunix_dd_new(yyvsp[0].cptr, AFSOCKET_DGRAM);
	    free(yyvsp[0].cptr);
	    last_writer_options = &((AFSocketDestDriver *) last_driver)->writer_options;
	  }
    break;

  case 127:
#line 517 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = last_driver; }
    break;

  case 128:
#line 522 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { 
	    last_driver = afunix_dd_new(yyvsp[0].cptr, AFSOCKET_STREAM);
	    free(yyvsp[0].cptr);
	    last_writer_options = &((AFSocketDestDriver *) last_driver)->writer_options;
	  }
    break;

  case 129:
#line 527 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = last_driver; }
    break;

  case 130:
#line 533 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { 
	    last_driver = afinet_dd_new(
			yyvsp[0].cptr, 514,
			AFSOCKET_DGRAM);
	    free(yyvsp[0].cptr);
	    last_writer_options = &((AFSocketDestDriver *) last_driver)->writer_options;
	  }
    break;

  case 131:
#line 540 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = last_driver; }
    break;

  case 134:
#line 549 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { afinet_dd_set_localip(last_driver, yyvsp[-1].cptr); free(yyvsp[-1].cptr); }
    break;

  case 135:
#line 550 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { afinet_dd_set_destport(last_driver, yyvsp[-1].num, NULL, NULL); }
    break;

  case 138:
#line 556 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { afinet_dd_set_localport(last_driver, 0, yyvsp[-1].cptr, "udp"); free(yyvsp[-1].cptr); }
    break;

  case 139:
#line 557 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { afinet_dd_set_destport(last_driver, 0, yyvsp[-1].cptr, "udp"); free(yyvsp[-1].cptr); }
    break;

  case 140:
#line 558 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { afinet_dd_set_destport(last_driver, 0, yyvsp[-1].cptr, "udp"); free(yyvsp[-1].cptr); }
    break;

  case 141:
#line 563 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { 
	    last_driver = afinet_dd_new(
			yyvsp[0].cptr, 514,
			AFSOCKET_STREAM); 
	    free(yyvsp[0].cptr);
	    last_writer_options = &((AFSocketDestDriver *) last_driver)->writer_options;
	  }
    break;

  case 142:
#line 570 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = last_driver; }
    break;

  case 146:
#line 580 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { afinet_dd_set_localport(last_driver, 0, yyvsp[-1].cptr, "tcp"); free(yyvsp[-1].cptr); }
    break;

  case 147:
#line 581 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { afinet_dd_set_destport(last_driver, 0, yyvsp[-1].cptr, "tcp"); free(yyvsp[-1].cptr); }
    break;

  case 148:
#line 582 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { afinet_dd_set_destport(last_driver, 0, yyvsp[-1].cptr, "tcp"); free(yyvsp[-1].cptr); }
    break;

  case 149:
#line 593 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = afuser_dd_new(yyvsp[-1].cptr); free(yyvsp[-1].cptr); }
    break;

  case 150:
#line 597 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = yyvsp[-1].ptr; }
    break;

  case 151:
#line 602 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { 
	    last_driver = afprogram_dd_new(yyvsp[0].cptr); 
	    free(yyvsp[0].cptr); 
	    last_writer_options = &((AFProgramDestDriver *) last_driver)->writer_options;
	  }
    break;

  case 152:
#line 607 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = last_driver; }
    break;

  case 155:
#line 616 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { last_writer_options->options = yyvsp[-1].num; }
    break;

  case 156:
#line 617 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { last_writer_options->fifo_size = yyvsp[-1].num; }
    break;

  case 157:
#line 618 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { affile_dd_set_sync_freq(last_driver, yyvsp[-1].num); }
    break;

  case 158:
#line 619 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { last_writer_options->template = cfg_lookup_template(configuration, yyvsp[-1].cptr);
	                                          if (last_writer_options->template == NULL)
	                                            {
	                                              last_writer_options->template = log_template_new(NULL, yyvsp[-1].cptr); 
	                                              last_writer_options->template->def_inline = TRUE;
	                                            }
	                                          else
	                                            log_template_ref(last_writer_options->template);
	                                          free(yyvsp[-1].cptr);
	                                        }
    break;

  case 159:
#line 629 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { affile_dd_set_template_escape(last_driver, yyvsp[-1].num); }
    break;

  case 160:
#line 630 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { affile_dd_set_fsync(last_driver, yyvsp[-1].num); }
    break;

  case 161:
#line 631 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { last_writer_options->keep_timestamp = yyvsp[-1].num; }
    break;

  case 162:
#line 632 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { last_writer_options->tz_convert = cfg_tz_convert_value(yyvsp[-1].cptr); free(yyvsp[-1].cptr); }
    break;

  case 163:
#line 633 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { last_writer_options->ts_format = cfg_ts_format_value(yyvsp[-1].cptr); free(yyvsp[-1].cptr); }
    break;

  case 164:
#line 637 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.num = yyvsp[-1].num | yyvsp[0].num; }
    break;

  case 165:
#line 638 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.num = 0; }
    break;

  case 166:
#line 642 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.num = LWO_TMPL_ESCAPE; }
    break;

  case 167:
#line 647 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { log_endpoint_append(yyvsp[-2].ptr, yyvsp[0].ptr); yyval.ptr = yyvsp[-2].ptr; }
    break;

  case 168:
#line 648 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = NULL; }
    break;

  case 169:
#line 652 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = log_endpoint_new(EP_SOURCE, yyvsp[-1].cptr); free(yyvsp[-1].cptr); }
    break;

  case 170:
#line 653 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = log_endpoint_new(EP_FILTER, yyvsp[-1].cptr); free(yyvsp[-1].cptr); }
    break;

  case 171:
#line 654 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = log_endpoint_new(EP_DESTINATION, yyvsp[-1].cptr); free(yyvsp[-1].cptr); }
    break;

  case 172:
#line 658 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.num = yyvsp[-2].num; }
    break;

  case 173:
#line 659 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.num = 0; }
    break;

  case 174:
#line 664 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.num |= yyvsp[0].num; }
    break;

  case 175:
#line 665 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.num = 0; }
    break;

  case 176:
#line 669 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.num = LC_CATCHALL; }
    break;

  case 177:
#line 670 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.num = LC_FALLBACK; }
    break;

  case 178:
#line 671 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.num = LC_FINAL; }
    break;

  case 179:
#line 672 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.num = LC_FLOW_CONTROL; }
    break;

  case 180:
#line 676 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = yyvsp[-2].ptr; }
    break;

  case 181:
#line 677 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = NULL; }
    break;

  case 182:
#line 681 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { configuration->mark_freq = yyvsp[-1].num; }
    break;

  case 183:
#line 682 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { configuration->sync_freq = yyvsp[-1].num; }
    break;

  case 184:
#line 683 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { configuration->chain_hostnames = yyvsp[-1].num; }
    break;

  case 185:
#line 684 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { configuration->keep_hostname = yyvsp[-1].num; }
    break;

  case 186:
#line 685 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { configuration->use_time_recvd = yyvsp[-1].num; }
    break;

  case 187:
#line 686 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { configuration->use_fqdn = yyvsp[-1].num; }
    break;

  case 188:
#line 687 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { configuration->use_dns = yyvsp[-1].num; }
    break;

  case 189:
#line 688 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { configuration->time_reopen = yyvsp[-1].num; }
    break;

  case 190:
#line 689 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { configuration->time_reap = yyvsp[-1].num; }
    break;

  case 191:
#line 690 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { configuration->log_fifo_size = yyvsp[-1].num; }
    break;

  case 192:
#line 691 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { configuration->log_iw_size = yyvsp[-1].num; }
    break;

  case 193:
#line 692 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { configuration->log_fetch_limit = yyvsp[-1].num; }
    break;

  case 194:
#line 693 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { configuration->log_msg_size = yyvsp[-1].num; }
    break;

  case 195:
#line 694 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { configuration->keep_timestamp = yyvsp[-1].num; }
    break;

  case 196:
#line 695 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { configuration->tz_convert = cfg_tz_convert_value(yyvsp[-1].cptr); free(yyvsp[-1].cptr); }
    break;

  case 197:
#line 696 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { configuration->ts_format = cfg_ts_format_value(yyvsp[-1].cptr); free(yyvsp[-1].cptr); }
    break;

  case 198:
#line 697 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { /* ignored */; }
    break;

  case 199:
#line 698 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { /* ignored */; }
    break;

  case 200:
#line 699 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { configuration->create_dirs = yyvsp[-1].num; }
    break;

  case 201:
#line 700 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { cfg_file_owner_set(configuration, yyvsp[-1].cptr); free(yyvsp[-1].cptr); }
    break;

  case 202:
#line 701 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { cfg_file_group_set(configuration, yyvsp[-1].cptr); free(yyvsp[-1].cptr); }
    break;

  case 203:
#line 702 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { cfg_file_perm_set(configuration, yyvsp[-1].num); }
    break;

  case 204:
#line 703 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { cfg_dir_owner_set(configuration, yyvsp[-1].cptr); free(yyvsp[-1].cptr); }
    break;

  case 205:
#line 704 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { cfg_dir_group_set(configuration, yyvsp[-1].cptr); free(yyvsp[-1].cptr); }
    break;

  case 206:
#line 705 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { cfg_dir_perm_set(configuration, yyvsp[-1].num); }
    break;

  case 207:
#line 706 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { configuration->use_dns_cache = yyvsp[-1].num; }
    break;

  case 208:
#line 707 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { configuration->dns_cache_size = yyvsp[-1].num; }
    break;

  case 209:
#line 708 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { configuration->dns_cache_expire = yyvsp[-1].num; }
    break;

  case 210:
#line 710 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { configuration->dns_cache_expire_failed = yyvsp[-1].num; }
    break;

  case 211:
#line 711 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { configuration->file_template_name = yyvsp[-1].cptr; }
    break;

  case 212:
#line 712 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { configuration->proto_template_name = yyvsp[-1].cptr; }
    break;

  case 213:
#line 716 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.ptr = log_filter_rule_new(yyvsp[-4].cptr, yyvsp[-2].node); free(yyvsp[-4].cptr); }
    break;

  case 214:
#line 720 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.node = yyvsp[0].node; if (!yyvsp[0].node) return 1; }
    break;

  case 215:
#line 721 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyvsp[0].node->comp = !(yyvsp[0].node->comp); yyval.node = yyvsp[0].node; }
    break;

  case 216:
#line 722 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.node = fop_or_new(yyvsp[-2].node, yyvsp[0].node); }
    break;

  case 217:
#line 723 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.node = fop_and_new(yyvsp[-2].node, yyvsp[0].node); }
    break;

  case 218:
#line 724 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.node = yyvsp[-1].node; }
    break;

  case 219:
#line 728 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.node = filter_facility_new(yyvsp[-1].num);  }
    break;

  case 220:
#line 729 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.node = filter_facility_new(0x80000000 | yyvsp[-1].num); }
    break;

  case 221:
#line 730 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.node = filter_level_new(yyvsp[-1].num); }
    break;

  case 222:
#line 731 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.node = filter_prog_new(yyvsp[-1].cptr); free(yyvsp[-1].cptr); }
    break;

  case 223:
#line 732 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.node = filter_host_new(yyvsp[-1].cptr); free(yyvsp[-1].cptr); }
    break;

  case 224:
#line 733 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.node = filter_match_new(yyvsp[-1].cptr); free(yyvsp[-1].cptr); }
    break;

  case 225:
#line 734 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.node = filter_call_new(yyvsp[-1].cptr, configuration); free(yyvsp[-1].cptr); }
    break;

  case 226:
#line 738 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.num = yyvsp[-1].num + yyvsp[0].num; }
    break;

  case 227:
#line 739 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.num = yyvsp[0].num; }
    break;

  case 228:
#line 744 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { 
	    int n = syslog_lookup_facility(yyvsp[0].cptr);
	    if (n == -1)
	      {
	        msg_error("Warning: Unknown facility", 
	                  evt_tag_str("facility", yyvsp[0].cptr),
	                  NULL);
	        yyval.num = 0;
	      }
	    else
	      yyval.num = (1 << n); 
	    free(yyvsp[0].cptr); 
	  }
    break;

  case 229:
#line 760 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.num = yyvsp[-1].num + yyvsp[0].num; }
    break;

  case 230:
#line 761 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.num = yyvsp[0].num; }
    break;

  case 231:
#line 766 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { 
	    int r1, r2;
	    r1 = syslog_lookup_level(yyvsp[-2].cptr);
	    if (r1 == -1)
	      msg_error("Warning: Unknown priority level",
                        evt_tag_str("priority", yyvsp[-2].cptr),
                        NULL);
	    else
	      r1 = sl_levels[r1].value;
	    r2 = syslog_lookup_level(yyvsp[0].cptr);
	    if (r2 == -1)
	      msg_error("Warning: Unknown priority level",
                        evt_tag_str("priority", yyvsp[-2].cptr),
                        NULL);
	    else
	      r2 = sl_levels[r2].value;
	    if (r1 != -1 && r2 != -1)
	      yyval.num = syslog_make_range(r1, r2); 
	    else
	      yyval.num = 0;
	    free(yyvsp[-2].cptr); 
	    free(yyvsp[0].cptr); 
	  }
    break;

  case 232:
#line 790 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { 
	    int n = syslog_lookup_level(yyvsp[0].cptr); 
	    if (n == -1)
	      {
	        msg_error("Warning: Unknown priority level",
                          evt_tag_str("priority", yyvsp[0].cptr),
                          NULL);
	        yyval.num = 0;
	      }
	    else
	      yyval.num = 1 << sl_levels[n].value; 
	    free(yyvsp[0].cptr); 
	  }
    break;

  case 233:
#line 806 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.num = 1; }
    break;

  case 234:
#line 807 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.num = 0; }
    break;

  case 235:
#line 808 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.num = yyvsp[0].num; }
    break;

  case 236:
#line 812 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.num = 2; }
    break;

  case 237:
#line 813 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.num = 1; }
    break;

  case 238:
#line 814 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"
    { yyval.num = 0; }
    break;


    }

/* Line 999 of yacc.c.  */
#line 2896 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.c"

  yyvsp -= yylen;
  yyssp -= yylen;


  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;


  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (YYPACT_NINF < yyn && yyn < YYLAST)
	{
	  YYSIZE_T yysize = 0;
	  int yytype = YYTRANSLATE (yychar);
	  char *yymsg;
	  int yyx, yycount;

	  yycount = 0;
	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  for (yyx = yyn < 0 ? -yyn : 0;
	       yyx < (int) (sizeof (yytname) / sizeof (char *)); yyx++)
	    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	      yysize += yystrlen (yytname[yyx]) + 15, yycount++;
	  yysize += yystrlen ("syntax error, unexpected ") + 1;
	  yysize += yystrlen (yytname[yytype]);
	  yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg != 0)
	    {
	      char *yyp = yystpcpy (yymsg, "syntax error, unexpected ");
	      yyp = yystpcpy (yyp, yytname[yytype]);

	      if (yycount < 5)
		{
		  yycount = 0;
		  for (yyx = yyn < 0 ? -yyn : 0;
		       yyx < (int) (sizeof (yytname) / sizeof (char *));
		       yyx++)
		    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
		      {
			const char *yyq = ! yycount ? ", expecting " : " or ";
			yyp = yystpcpy (yyp, yyq);
			yyp = yystpcpy (yyp, yytname[yyx]);
			yycount++;
		      }
		}
	      yyerror (yymsg);
	      YYSTACK_FREE (yymsg);
	    }
	  else
	    yyerror ("syntax error; also virtual memory exhausted");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror ("syntax error");
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      /* Return failure if at end of input.  */
      if (yychar == YYEOF)
        {
	  /* Pop the error token.  */
          YYPOPSTACK;
	  /* Pop the rest of the stack.  */
	  while (yyss < yyssp)
	    {
	      YYDSYMPRINTF ("Error: popping", yystos[*yyssp], yyvsp, yylsp);
	      yydestruct (yystos[*yyssp], yyvsp);
	      YYPOPSTACK;
	    }
	  YYABORT;
        }

      YYDSYMPRINTF ("Error: discarding", yytoken, &yylval, &yylloc);
      yydestruct (yytoken, &yylval);
      yychar = YYEMPTY;

    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*----------------------------------------------------.
| yyerrlab1 -- error raised explicitly by an action.  |
`----------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
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

      YYDSYMPRINTF ("Error: popping", yystos[*yyssp], yyvsp, yylsp);
      yydestruct (yystos[yystate], yyvsp);
      yyvsp--;
      yystate = *--yyssp;

      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  YYDPRINTF ((stderr, "Shifting error token, "));

  *++yyvsp = yylval;


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

#ifndef yyoverflow
/*----------------------------------------------.
| yyoverflowlab -- parser overflow comes here.  |
`----------------------------------------------*/
yyoverflowlab:
  yyerror ("parser stack overflow");
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  return yyresult;
}


#line 822 "/home/bazsi/zwa/work/syslog-ng-2.0/syslog-ng/src/cfg-grammar.y"


extern int linenum;

void yyerror(char *msg)
{
	fprintf(stderr, "%s at %d\n", msg, linenum);
}

