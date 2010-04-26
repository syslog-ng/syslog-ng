#ifndef CFG_LEXER_H_INCLUDED
#define CFG_LEXER_H_INCLUDED 1

#include "syslog-ng.h"
#include <stdio.h>

/* this module provides a higher level encapsulation for the configuration
 * file lexer. */

#define MAX_INCLUDE_DEPTH 256

typedef struct _CfgIncludeLevel CfgIncludeLevel;
typedef struct _CfgTokenBlock CfgTokenBlock;
typedef struct _CfgBlockGeneratorArgs CfgBlockGeneratorArgs;
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

/* a block generator is a function that includes a configuration file
 * snippet in place to the block reference.  This is used by the
 * "block" statement, but can also be used by external plugins to
 * generate configuration snippets programmatically.  That code
 * however is missing as of now.  (though would be trivial to add)
 */
typedef gboolean (*CfgBlockGeneratorFunc)(CfgLexer *lexer, gint type, const gchar *name, CfgBlockGeneratorArgs *args, gpointer user_data);
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
  GList *token_blocks;
  GList *generators;
  GString *pattern_buffer;
};

/* pattern buffer */
void cfg_lexer_unput_token(CfgLexer *self, YYSTYPE *yylval);

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

/* token blocks */
void cfg_lexer_inject_token_block(CfgLexer *self, CfgTokenBlock *block);
void cfg_lexer_register_block_generator(CfgLexer *self, gint context, const gchar *name, CfgBlockGeneratorFunc generator, gpointer user_data, GDestroyNotify user_data_free);


int cfg_lexer_lex(CfgLexer *self, YYSTYPE *yylval, YYLTYPE *yylloc);

CfgLexer *cfg_lexer_new(FILE *file, const gchar *filename, gint init_line_num);
void  cfg_lexer_free(CfgLexer *self);

gint cfg_lexer_lookup_context_type_by_name(const gchar *name);
const gchar *cfg_lexer_lookup_context_name_by_type(gint id);

/* argument list for a block generator */
void cfg_block_generator_args_set_arg(CfgBlockGeneratorArgs *self, const gchar *name, const gchar *value);
const gchar *cfg_block_generator_args_get_arg(CfgBlockGeneratorArgs *self, const gchar *name);
CfgBlockGeneratorArgs *cfg_block_generator_args_new(void);
void cfg_block_generator_args_free(CfgBlockGeneratorArgs *self);

/* token block objects */

void cfg_token_block_add_token(CfgTokenBlock *self, YYSTYPE *token);
YYSTYPE *cfg_token_block_get_token(CfgTokenBlock *self);

CfgTokenBlock *cfg_token_block_new(void);
void cfg_token_block_free(CfgTokenBlock *self);

/* user defined configuration block */

gboolean cfg_block_generate(CfgLexer *self, gint context, const gchar *name, CfgBlockGeneratorArgs *args, gpointer user_data);
CfgBlock *cfg_block_new(const gchar *content, CfgBlockGeneratorArgs *arg_defs);
void cfg_block_free(CfgBlock *self);



#endif
