
#include "cfg-parser.h"
#include "cfg-lexer.h"
#include "cfg-grammar.h"

#include <string.h>

extern int main_debug;

/* defined in the parser */
int main_parse(CfgLexer *lexer, gpointer *dummy);

CfgParser main_parser =
{
  .debug_flag = &main_debug,
  .parse = (int (*)(CfgLexer *, gpointer *)) main_parse,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(main_, gpointer *)

void
report_syntax_error(YYLTYPE *yylloc, const char *what, const char *msg)
{
  fprintf(stderr, "Error parsing %s, %s in %s at line %d, column %d:\n",
                  what,
                  msg,
                  yylloc->filename,
                  yylloc->first_line,
                  yylloc->first_column);

  if (yylloc->filename)
    {
      FILE *f;

      f = fopen(yylloc->filename, "r");
      if (f)
        {
          gint lineno = 1;
          gint i;
          gchar buf[1024];

          while (fgets(buf, sizeof(buf), f) && lineno < yylloc->first_line)
            lineno++;
          if (lineno == yylloc->first_line && buf[0])
            {
              fprintf(stderr, "\n%s", buf);
              if (buf[strlen(buf) - 1] != '\n')
                fprintf(stderr, "\n");
              for (i = 0; buf[i] && i < yylloc->first_column - 1; i++)
                {
                  fprintf(stderr, "%c", buf[i] == '\t' ? '\t' : ' ');
                }
              for (i = yylloc->first_column; i < yylloc->last_column; i++)
                fprintf(stderr, "^");
              fprintf(stderr, "\n");
            }
          fclose(f);
        }
    }
  fprintf(stderr, "\nsyslog-ng documentation: http://www.balabit.com/support/documentation/?product=syslog-ng\n"
                  "mailing list: https://lists.balabit.hu/mailman/listinfo/syslog-ng\n");

}
