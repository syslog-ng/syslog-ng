/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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
#include "cfg-args.h"
#include "cfg-block-generator.h"
#include "messages.h"

#include <stdio.h>
#include <setjmp.h>

/* this module provides a higher level encapsulation for the configuration
 * file lexer. */

#define MAX_INCLUDE_DEPTH 256

typedef struct _CfgIncludeLevel CfgIncludeLevel;
typedef struct _CfgTokenBlock CfgTokenBlock;

/*
 * YYLTYPE/YYSTYPE naming conventions
 *
 * We use multiple bison generated grammars (basically one for each plugin)
 * with a single lexer and the same location/symbol types.  Earlier we could
 * easily just define YYLTYPE and YYSTYPE here and all generated grammars
 * and the lexer used it properly.  With the advent of the `api.prefix'
 * option for the grammars (and the deprecation of the old `name-prefix'
 * behaviors), we needed to complicate things somewhat.
 *
 * We have three contexts where we need to use proper type names:
 *   - in our own code where we might need to use location information (e.g. YYLTYPE)
 *   - in the generated lexer,
 *   - in the generated grammars
 *
 * Our own code
 * ============
 * Because of the various #define/typedef games that generated code uses, I
 * decided that our own code should not use the names YYLTYPE/YYSTYPE
 * directly.  In those cases we use CFG_LTYPE and CFG_STYPE to indicate that
 * these are types related to our configuration language.  None of the
 * grammars use the "CFG_" prefix (and should not in the future either).
 *
 * The generated lexer
 * ===================
 *
 * The lexer get these types by us #define-ing YYLTYPE/YYSTYPE to
 * CFG_LTYPE/STYPE but only privately, e.g.  these definitions should not be
 * published to the rest of the codebase.  We do this by defining these in
 * implementation files and not in the headers.  This is because some of the
 * code would try to #ifdef based on the existance of these macros.
 *
 * The generated grammars
 * ======================
 * The grammars each have an api.location.type and api.value.type options in
 * their .y files, which use the names CFG_LTYPE and CFG_STYPE respectively.
 * The generated code uses YYLTYPE and YYSTYPE internally (defined as
 * macros), but because of the previous points this does not create a
 * conflict.
 */

/* the location type to carry location information from the lexer to the grammar */

typedef struct CFG_LTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;

  const gchar *name;
} CFG_LTYPE;

/* symbol type that carries token related information to the grammar */
typedef struct CFG_STYPE
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
} CFG_STYPE;

#define KWS_NORMAL        0
#define KWS_OBSOLETE      1

/* used to describe a syslog-ng keyword */
typedef struct _CfgLexerKeyword
{
  const gchar *kw_name;
  gint  kw_token;
  gint  kw_status;
  const gchar *kw_explain;
} CfgLexerKeyword;

#define CFG_KEYWORD_STOP "@!#?"



/* structure that describes a given location in the include stack */
struct _CfgIncludeLevel
{
  enum
  {
    CFGI_NONE,
    CFGI_FILE,
    CFGI_BUFFER,
  } include_type;
  union
  {
    struct
    {
      GSList *files;
      FILE *include_file;
    } file;
    struct
    {
      /* the lexer mutates content, so save it for error reporting */
      gchar *original_content;
      /* buffer for the lexer */
      gchar *content;
      gsize content_length;
    } buffer;
  };

  /* this value indicates that @line was used which changed lloc relative to
   * an actual file */
  gboolean lloc_changed_by_at_line;
  CFG_LTYPE lloc;
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
  jmp_buf fatal_error;
  CfgIncludeLevel include_stack[MAX_INCLUDE_DEPTH];
  GList *context_stack;
  gint include_depth;
  gchar block_boundary[2];
  gint brace_count;
  gint tokenize_eol;
  GList *token_blocks;
  GString *string_buffer;
  GString *preprocess_output;
  gint preprocess_suppress_tokens;
  GString *token_pretext;
  GString *token_text;
  GlobalConfig *cfg;
  guint ignore_pragma:1;
};

/* pattern buffer */
void cfg_lexer_unput_token(CfgLexer *self, CFG_STYPE *yylval);

void cfg_lexer_start_block_state(CfgLexer *self, const gchar block_boundary[2]);
void cfg_lexer_push_filterx_state(CfgLexer *self);
void cfg_lexer_pop_filterx_state(CfgLexer *self);

void cfg_lexer_append_string(CfgLexer *self, int length, char *str);
void cfg_lexer_append_char(CfgLexer *self, char c);

/* keyword handling */
void cfg_lexer_set_current_keywords(CfgLexer *self, CfgLexerKeyword *keywords);
char *cfg_lexer_get_keyword_string(CfgLexer *self, int kw);
int cfg_lexer_map_word_to_token(CfgLexer *self, CFG_STYPE *yylval, const CFG_LTYPE *yylloc, const char *token);

/* include files */
gboolean cfg_lexer_start_next_include(CfgLexer *self);
gboolean cfg_lexer_include_file(CfgLexer *self, const gchar *filename);
gboolean cfg_lexer_include_buffer(CfgLexer *self, const gchar *name, const gchar *buffer, gssize length);
gboolean cfg_lexer_include_buffer_without_backtick_substitution(CfgLexer *self,
    const gchar *name, const gchar *buffer, gsize length);
const gchar *cfg_lexer_format_location(CfgLexer *self, const CFG_LTYPE *yylloc, gchar *buf, gsize buf_len);
void cfg_lexer_set_file_location(CfgLexer *self, const gchar *filename, gint line, gint column);
EVTTAG *cfg_lexer_format_location_tag(CfgLexer *self, const CFG_LTYPE *yylloc);

/* context tracking */
void cfg_lexer_push_context(CfgLexer *self, gint context, CfgLexerKeyword *keywords, const gchar *desc);
void cfg_lexer_pop_context(CfgLexer *self);
const gchar *cfg_lexer_get_context_description(CfgLexer *self);
gint cfg_lexer_get_context_type(CfgLexer *self);

/* token blocks */
void cfg_lexer_inject_token_block(CfgLexer *self, CfgTokenBlock *block);

int cfg_lexer_lex(CfgLexer *self, CFG_STYPE *yylval, CFG_LTYPE *yylloc);
void cfg_lexer_free_token(CFG_STYPE *token);

CfgLexer *cfg_lexer_new(GlobalConfig *cfg, FILE *file, const gchar *filename, GString *preprocess_output);
CfgLexer *cfg_lexer_new_buffer(GlobalConfig *cfg, const gchar *buffer, gsize length);
void  cfg_lexer_free(CfgLexer *self);

gint cfg_lexer_lookup_context_type_by_name(const gchar *name);
const gchar *cfg_lexer_lookup_context_name_by_type(gint id);

/* token block objects */

void cfg_token_block_add_and_consume_token(CfgTokenBlock *self, CFG_STYPE *token);
void cfg_token_block_add_token(CfgTokenBlock *self, CFG_STYPE *token);
CFG_STYPE *cfg_token_block_get_token(CfgTokenBlock *self);

CfgTokenBlock *cfg_token_block_new(void);
void cfg_token_block_free(CfgTokenBlock *self);

void cfg_lexer_register_generator_plugin(PluginContext *context, CfgBlockGenerator *gen);

#define CFG_LEXER_ERROR cfg_lexer_error_quark()

GQuark cfg_lexer_error_quark(void);

enum CfgLexerError
{
  CFG_LEXER_MISSING_BACKTICK_PAIR,
  CFG_LEXER_CANNOT_REPRESENT_APOSTROPHES_IN_QSTRINGS,
  CFG_LEXER_BACKTICKS_CANT_BE_SUBSTITUTED_AFTER_BACKSLASH,
};

#endif
