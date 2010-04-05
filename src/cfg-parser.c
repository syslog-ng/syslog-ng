
#include "cfg-parser.h"
#include "cfg-lexer.h"
#include "cfg-grammar.h"

extern int main_debug;

/* defined in the parser */
int main_parse(CfgLexer *lexer, gpointer *dummy);

CfgParser main_parser =
{
  .debug_flag = &main_debug,
  .parse = (int (*)(CfgLexer *, gpointer *)) main_parse,
};

int
main_lex(YYSTYPE *yylval, YYLTYPE *yylloc, CfgLexer *lexer)
{
  return cfg_lexer_lex(lexer, yylval, yylloc);
}

void
main_error(YYLTYPE *yylloc, CfgLexer *lexer, gpointer *configuration, const char *msg)
{
  fprintf(stderr, "%s in %s at line %d, column %d\n\n"
                  "syslog-ng documentation: http://www.balabit.com/support/documentation/?product=syslog-ng\n"
                  "mailing list: https://lists.balabit.hu/mailman/listinfo/syslog-ng\n",
                  msg,
                  yylloc->filename,
                  yylloc->first_line,
                  yylloc->first_column);
}
