#ifndef CFG_LEXER_H_INCLUDED
#define CFG_LEXER_H_INCLUDED 1

#include "syslog-ng.h"
#include <stdio.h>

/* this module provides a higher level encapsulation for the configuration
 * file lexer. */

#define MAX_INCLUDE_DEPTH 256

typedef struct _CfgIncludeLevel CfgIncludeLevel;
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

#define MAX_REGEXP_LEN	1024

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

/*
 * A token block is a series of tokens to be injected into the tokens
 * fetched by the lexer.  It is assumed to be filled and then depleted, the
 * two operations cannot be intermixed.
 */
typedef struct _CfgTokenBlock
{
  gint pos;
  GArray *tokens;
} CfgTokenBlock;

/* Lexer class that encapsulates a flex generated lexer. This can be
 * instantiated multiple times in parallel, e.g.  doesn't use any global
 * state as we're using the "reentrant" code by flex
 */
typedef struct _CfgLexer
{
  gpointer state;
  CfgIncludeLevel include_stack[MAX_INCLUDE_DEPTH];
  GList *context_stack;
  gint include_depth;
  gint brace_count;
  GList *token_blocks;
  GString *pattern_buffer;
} CfgLexer;


/* pattern buffer */
void _cfg_lexer_force_block_state(gpointer state);

void cfg_lexer_unput_string(CfgLexer *self, const char *string);
void cfg_lexer_append_string(CfgLexer *self, int length, char *str);
void cfg_lexer_append_char(CfgLexer *self, char c);


/* keyword handling */
void cfg_lexer_set_current_keywords(CfgLexer *self, CfgLexerKeyword *keywords);
char *cfg_lexer_get_keyword_string(CfgLexer *self, int kw);
int cfg_lexer_lookup_keyword_in_table(CfgLexer *self, YYSTYPE *yylval, YYLTYPE *lloc, const char *token, CfgLexerKeyword *keywords);
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


/* */
void cfg_lexer_add_token_block(CfgLexer *self, CfgTokenBlock *block);
int cfg_lexer_lex(CfgLexer *self, YYSTYPE *yylval, YYLTYPE *yylloc);

CfgLexer *cfg_lexer_new(FILE *file, const gchar *filename, gint init_line_num);
void  cfg_lexer_free(CfgLexer *self);

void cfg_token_block_add_token(CfgTokenBlock *self, YYSTYPE *token);
YYSTYPE *cfg_token_block_get_token(CfgTokenBlock *self);

CfgTokenBlock *cfg_token_block_new(void);
void cfg_token_block_free(CfgTokenBlock *self);


#endif
