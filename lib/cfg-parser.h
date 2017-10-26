/*
 * Copyright (c) 2002-2011 Balabit
 * Copyright (c) 1998-2011 Balázs Scheidler
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#ifndef CFG_PARSER_H_INCLUDED
#define CFG_PARSER_H_INCLUDED

#include "syslog-ng.h"
#include "cfg-lexer.h"

/* high level interface to a configuration file parser, it encapsulates the
 * grammar/lexer pair. */
typedef struct _CfgParser
{
  /* how to enable bison debug in the parser */
  gint *debug_flag;
  gint context;
  const gchar *name;

  /* parser specific keywords to be pushed to the lexer */
  CfgLexerKeyword *keywords;

  /* the parser entry point, returns the parsed object in *instance */
  gint (*parse)(CfgLexer *lexer, gpointer *instance, gpointer arg);

  /* in case of parse failure and instance != NULL, this should free instance */
  void (*cleanup)(gpointer instance);
} CfgParser;

gboolean cfg_parser_parse(CfgParser *self, CfgLexer *lexer, gpointer *instance, gpointer arg);
void cfg_parser_cleanup(CfgParser *self, gpointer instance);

enum
{
  CFH_SET,
  CFH_CLEAR,
};

typedef struct _CfgFlagHandler
{
  const gchar *name;
  gint op;
  gint ofs;
  guint32 param;
  guint32 mask;
} CfgFlagHandler;

gboolean
cfg_process_flag(CfgFlagHandler *handlers, gpointer base, const gchar *flag);


extern CfgParser main_parser;

#define CFG_PARSER_DECLARE_LEXER_BINDING(parser_prefix, root_type)             \
    int                                                                        \
    parser_prefix ## lex(YYSTYPE *yylval, YYLTYPE *yylloc, CfgLexer *lexer);   \
                                                                               \
    void                                                                       \
    parser_prefix ## error(YYLTYPE *yylloc, CfgLexer *lexer, root_type instance, gpointer arg, const char *msg);


#define CFG_PARSER_IMPLEMENT_LEXER_BINDING(parser_prefix, root_type)          \
    int                                                                       \
    parser_prefix ## lex(YYSTYPE *yylval, YYLTYPE *yylloc, CfgLexer *lexer)   \
    {                                                                         \
      int token;                                                              \
                                                                              \
      token = cfg_lexer_lex(lexer, yylval, yylloc);                           \
      return token;                                                           \
    }                                                                         \
                                                                              \
    void                                                                      \
    parser_prefix ## error(YYLTYPE *yylloc, CfgLexer *lexer, root_type instance, gpointer arg, const char *msg) \
    {                                                                                             \
      report_syntax_error(lexer, yylloc, cfg_lexer_get_context_description(lexer), msg);          \
    }

void report_syntax_error(CfgLexer *lexer, YYLTYPE *yylloc, const char *what, const char *msg);

CFG_PARSER_DECLARE_LEXER_BINDING(main_, gpointer *)

#endif
