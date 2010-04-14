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

  /* parser specific keywords to be pushed to the lexer */
  CfgLexerKeyword *keywords;

  /* the parser entry point, returns the parsed object in *instance */
  gint (*parse)(CfgLexer *lexer, gpointer *instance);

  /* in case of parse failure and instance != NULL, this should free instance */
  void (*cleanup)(gpointer instance);
} CfgParser;


/* the debug flag for the main parser will be used for all parsers */
extern int yydebug;

static inline gboolean
cfg_parser_parse(CfgParser *self, CfgLexer *lexer, gpointer *instance)
{
  (*self->debug_flag) = yydebug;
  return self->parse(lexer, instance) == 0;
}

static inline void
cfg_parser_cleanup(CfgParser *self, gpointer instance)
{
  if (instance && self->cleanup)
    self->cleanup(instance);
}

extern CfgParser main_parser;

#define CFG_PARSER_DECLARE_LEXER_BINDING(parser_prefix, root_type)             \
    int                                                                        \
    parser_prefix ## lex(YYSTYPE *yylval, YYLTYPE *yylloc, CfgLexer *lexer);   \
                                                                               \
    void                                                                       \
    parser_prefix ## error(YYLTYPE *yylloc, CfgLexer *lexer, root_type instance, const char *msg);


#define CFG_PARSER_IMPLEMENT_LEXER_BINDING(parser_prefix, root_type)          \
    int                                                                       \
    parser_prefix ## lex(YYSTYPE *yylval, YYLTYPE *yylloc, CfgLexer *lexer)   \
    {                                                                         \
      int token;                                                              \
                                                                              \
      cfg_lexer_set_current_keywords(lexer, parser_prefix ## parser.keywords);\
      token = cfg_lexer_lex(lexer, yylval, yylloc);                           \
      cfg_lexer_set_current_keywords(lexer, NULL);                            \
      return token;                                                           \
    }                                                                         \
                                                                              \
    void                                                                      \
    parser_prefix ## error(YYLTYPE *yylloc, CfgLexer *lexer, root_type instance, const char *msg) \
    {                                                                                             \
      report_syntax_error(yylloc, cfg_lexer_get_context_description(lexer), msg);                 \
    }

void report_syntax_error(YYLTYPE *yylloc, const char *what, const char *msg);

CFG_PARSER_DECLARE_LEXER_BINDING(main_, gpointer *)

#endif
