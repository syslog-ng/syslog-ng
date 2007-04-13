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




#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 39 "cfg-grammar.y"
typedef union YYSTYPE {
	guint num;
	char *cptr;
	void *ptr;
	FilterExprNode *node;
} YYSTYPE;
/* Line 1240 of yacc.c.  */
#line 222 "y.tab.h"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;



