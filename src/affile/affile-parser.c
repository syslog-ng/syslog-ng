#include "affile.h"
#include "cfg-parser.h"
#include "affile-grammar.h"

extern int affile_debug;

int affile_parse(CfgLexer *lexer, LogDriver **instance);

static CfgLexerKeyword affile_keywords[] = {
  { "file",               KW_FILE },
  { "fifo",               KW_PIPE },
  { "pipe",               KW_PIPE },

  { "fsync",              KW_FSYNC },
  { "remove_if_older",    KW_OVERWRITE_IF_OLDER, 0, KWS_OBSOLETE, "overwrite_if_older" },
  { "overwrite_if_older", KW_OVERWRITE_IF_OLDER },
  { "follow_freq",        KW_FOLLOW_FREQ,  },

  { NULL }
};

CfgParser affile_parser =
{
  .debug_flag = &affile_debug,
  .name = "affile",
  .keywords = affile_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *)) affile_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(affile_, LogDriver **)
