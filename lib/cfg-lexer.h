/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
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

#ifndef CFG_LEXER_H_INCLUDED
#define CFG_LEXER_H_INCLUDED 1

#include "syslog-ng.h"
#include <stdio.h>

/* this module provides a higher level encapsulation for the configuration
 * file lexer. */

#define MAX_INCLUDE_DEPTH 256

typedef struct _CfgIncludeLevel CfgIncludeLevel;
typedef struct _CfgTokenBlock CfgTokenBlock;
typedef struct _CfgArgs CfgArgs;
typedef struct _CfgBlockGenerator CfgBlockGenerator;
typedef struct _CfgBlock CfgBlock;
typedef struct _CfgLexer CfgLexer;

/* the location type to carry location information from the lexer to the grammar */
#define YYLTYPE YYLTYPE
typedef struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
  CfgIncludeLevel *level;
} YYLTYPE;
#define YYLTYPE_IS_TRIVIAL 1

/* symbol type that carries token related information to the grammar */
typedef struct YYSTYPE
{
  /* one of LL_ types that indicates which field is being used */
  int type;
  union
  {
    gint token;
    gint64 num;
    double fnum;
    char *cptr;
    void *ptr;
    gpointer node;
  };
} YYSTYPE;
#define YYSTYPE_IS_TRIVIAL 1
#define YYSTYPE_IS_DECLARED 1

#define KWS_NORMAL        0
#define KWS_OBSOLETE      1

/* used to describe a syslog-ng keyword */
typedef struct _CfgLexerKeyword
{
  gchar	*kw_name;
  gint  kw_token;
  gint  kw_req_version;
  gint  kw_status;
  gchar *kw_explain;
} CfgLexerKeyword;

#define CFG_KEYWORD_STOP "@!#?"

/* a block generator is a function that includes a configuration file
 * snippet in place to the block reference.  This is used by the
 * "block" statement, but can also be used by external plugins to
 * generate configuration snippets programmatically.  That code
 * however is missing as of now.  (though would be trivial to add)
 */
typedef gboolean (*CfgBlockGeneratorFunc)(CfgLexer *lexer, gint type, const gchar *name, CfgArgs *args, gpointer user_data);

/* structure that describes a given location in the include stack */
struct _CfgIncludeLevel
{
  enum
  {
    CFGI_FILE,
    CFGI_BUFFER,
  } include_type;
  /* include file or block name */
  gchar *name;
  union
  {
    struct
    {
      GSList *files;
      FILE *include_file;
    } file;
    struct
    {
      gchar *content;
      gsize content_length;
    } buffer;
  };
  YYLTYPE lloc;
  struct yy_buffer_state *yybuf;
};

/* Lexer class that encapsulates a flex generated lexer. This can be
 * instantiated multiple times in parallel, e.g.  doesn't use any global
 * state as we're using the "reentrant" code by flex
 */
struct _CfgLexer
{
  /* flex state, not using yyscan_t as it is not defined */
  gpointer state;
  CfgIncludeLevel include_stack[MAX_INCLUDE_DEPTH];
  GList *context_stack;
  gint include_depth;
  gint brace_count;
  gint tokenize_eol;
  GList *token_blocks;
  GList *generators;
  GString *string_buffer;
  FILE *preprocess_output;
  gint preprocess_suppress_tokens;
  GString *token_pretext;
  GString *token_text;
  CfgArgs *globals;
};

/* preprocessor help */
gchar *
cfg_lexer_subst_args(CfgArgs *globals, CfgArgs *defs, CfgArgs *args, gchar *cptr, gsize *length);

/* pattern buffer */
void cfg_lexer_unput_token(CfgLexer *self, YYSTYPE *yylval);

void _cfg_lexer_force_block_state(gpointer state);

void cfg_lexer_append_string(CfgLexer *self, int length, char *str);
void cfg_lexer_append_char(CfgLexer *self, char c);


/* keyword handling */
void cfg_lexer_set_current_keywords(CfgLexer *self, CfgLexerKeyword *keywords);
char *cfg_lexer_get_keyword_string(CfgLexer *self, int kw);
int cfg_lexer_lookup_keyword(CfgLexer *self, YYSTYPE *yylval, YYLTYPE *yylloc, const char *token);

/* include files */
gboolean cfg_lexer_start_next_include(CfgLexer *self);
gboolean cfg_lexer_include_file(CfgLexer *self, const gchar *filename);
gboolean cfg_lexer_include_buffer(CfgLexer *self, const gchar *name, gchar *buffer, gsize length);

/* context tracking */
void cfg_lexer_push_context(CfgLexer *self, gint context, CfgLexerKeyword *keywords, const gchar *desc);
void cfg_lexer_pop_context(CfgLexer *self);
const gchar *cfg_lexer_get_context_description(CfgLexer *self);
gint cfg_lexer_get_context_type(CfgLexer *self);

/* token blocks */
void cfg_lexer_inject_token_block(CfgLexer *self, CfgTokenBlock *block);
gboolean cfg_lexer_register_block_generator(CfgLexer *self, gint context, const gchar *name, CfgBlockGeneratorFunc generator, gpointer user_data, GDestroyNotify user_data_free);


int cfg_lexer_lex(CfgLexer *self, YYSTYPE *yylval, YYLTYPE *yylloc);

CfgLexer *cfg_lexer_new(FILE *file, const gchar *filename, const gchar *preprocess_into);
CfgLexer *cfg_lexer_new_buffer(const gchar *buffer, gsize length);
void  cfg_lexer_free(CfgLexer *self);

gint cfg_lexer_lookup_context_type_by_name(const gchar *name);
const gchar *cfg_lexer_lookup_context_name_by_type(gint id);

/* argument list for a block generator */
gboolean cfg_args_validate(CfgArgs *self, CfgArgs *defs, const gchar *context);
void cfg_args_set(CfgArgs *self, const gchar *name, const gchar *value);
const gchar *cfg_args_get(CfgArgs *self, const gchar *name);
CfgArgs *cfg_args_new(void);
void cfg_args_free(CfgArgs *self);

/* token block objects */

void cfg_token_block_add_token(CfgTokenBlock *self, YYSTYPE *token);
YYSTYPE *cfg_token_block_get_token(CfgTokenBlock *self);

CfgTokenBlock *cfg_token_block_new(void);
void cfg_token_block_free(CfgTokenBlock *self);

/* user defined configuration block */

gboolean cfg_block_generate(CfgLexer *lexer, gint context, const gchar *name, CfgArgs *args, gpointer user_data);
CfgBlock *cfg_block_new(const gchar *content, CfgArgs *arg_defs);
void cfg_block_free(CfgBlock *self);



#endif
