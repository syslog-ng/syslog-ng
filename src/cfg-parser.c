
#include "cfg-parser.h"
#include "cfg-lexer.h"
#include "cfg-grammar.h"

#include <string.h>

extern int main_debug;

/* defined in the parser */
int main_parse(CfgLexer *lexer, gpointer *dummy);

static CfgLexerKeyword main_keywords[] = {
  /* statements */
  { "source",             KW_SOURCE },
  { "filter",             KW_FILTER },
  { "parser",             KW_PARSER, 0x0300, },
  { "rewrite",            KW_REWRITE, 0x0300, },
  { "destination",        KW_DESTINATION },
  { "log",                KW_LOG },
  { "options",            KW_OPTIONS },
  { "include",            KW_INCLUDE, 0x0300, },
  { "block",              KW_BLOCK, 0x0302 },

  /* source or destination items */
  { "file",               KW_FILE },
  { "fifo",               KW_PIPE },
  { "pipe",               KW_PIPE },
  { "internal",           KW_INTERNAL },
  { "usertty",            KW_USERTTY },
  { "syslog",             KW_SYSLOG },
  { "program",            KW_PROGRAM },
  { "columns",            KW_COLUMNS, 0x0300 },
  { "delimiters",         KW_DELIMITERS, 0x0300 },
  { "quotes",             KW_QUOTES, 0x0300 },
  { "quote_pairs",        KW_QUOTE_PAIRS, 0x0300},
  { "null",               KW_NULL, 0x0300 },
  { "csv_parser",         KW_CSV_PARSER, 0x0300 },
  { "db_parser",          KW_DB_PARSER, 0x0300 },

  /* option items */
  { "flags",              KW_FLAGS },
  { "pad_size",           KW_PAD_SIZE },
  { "mark_freq",          KW_MARK_FREQ },
  { "mark",               KW_MARK_FREQ, 0, KWS_OBSOLETE, "mark_freq" },
  { "stats_freq",         KW_STATS_FREQ },
  { "stats_level",        KW_STATS_LEVEL },
  { "stats",              KW_STATS_FREQ, 0, KWS_OBSOLETE, "stats_freq" },
  { "flush_lines",        KW_FLUSH_LINES },
  { "flush_timeout",      KW_FLUSH_TIMEOUT },
  { "suppress",           KW_SUPPRESS },
  { "sync_freq",          KW_FLUSH_LINES, 0, KWS_OBSOLETE, "flush_lines" },
  { "sync",               KW_FLUSH_LINES, 0, KWS_OBSOLETE, "flush_lines" },
  { "fsync",              KW_FSYNC },
  { "long_hostnames",     KW_CHAIN_HOSTNAMES },
  { "chain_hostnames",    KW_CHAIN_HOSTNAMES },
  { "normalize_hostnames",KW_NORMALIZE_HOSTNAMES },
  { "keep_hostname",      KW_KEEP_HOSTNAME },
  { "check_hostname",     KW_CHECK_HOSTNAME },
  { "bad_hostname",       KW_BAD_HOSTNAME },
  { "keep_timestamp",     KW_KEEP_TIMESTAMP },
  { "encoding",           KW_ENCODING, 0x0300 },
  { "ts_format",          KW_TS_FORMAT },
  { "frac_digits",        KW_FRAC_DIGITS },
  { "time_zone",          KW_TIME_ZONE },
  { "recv_time_zone",     KW_RECV_TIME_ZONE },
  { "send_time_zone",     KW_SEND_TIME_ZONE },
  { "local_time_zone",    KW_LOCAL_TIME_ZONE, 0x0300 },
  { "use_time_recvd",     KW_USE_TIME_RECVD, 0, KWS_OBSOLETE, "Use R_ or S_ prefixed macros in templates" },
  { "use_fqdn",           KW_USE_FQDN },
  { "use_dns",            KW_USE_DNS },
  { "time_reopen",        KW_TIME_REOPEN },
  { "time_reap",          KW_TIME_REAP },
  { "time_sleep",         KW_TIME_SLEEP },
  { "follow_freq",        KW_FOLLOW_FREQ,  },
  { "remove_if_older",    KW_OVERWRITE_IF_OLDER, 0, KWS_OBSOLETE, "overwrite_if_older" },
  { "overwrite_if_older", KW_OVERWRITE_IF_OLDER },
  { "file_template",      KW_FILE_TEMPLATE },
  { "proto_template",     KW_PROTO_TEMPLATE },
  { "default_level",      KW_DEFAULT_LEVEL, 0x0300 },
  { "default_priority",   KW_DEFAULT_LEVEL, 0x0300 },
  { "default_facility",   KW_DEFAULT_FACILITY, 0x0300 },

  { "set",                KW_SET, 0x0300 },
  { "subst",              KW_SUBST, 0x0300 },
  { "value",              KW_VALUE, 0x0300 },

  { "log_fifo_size",      KW_LOG_FIFO_SIZE },
  { "log_fetch_limit",    KW_LOG_FETCH_LIMIT },
  { "log_iw_size",        KW_LOG_IW_SIZE },
  { "log_msg_size",       KW_LOG_MSG_SIZE },
  { "log_prefix",         KW_LOG_PREFIX, 0, KWS_OBSOLETE, "program_override" },
  { "program_override",   KW_PROGRAM_OVERRIDE, 0x0300 },
  { "host_override",      KW_HOST_OVERRIDE, 0x0300 },
  { "throttle",           KW_THROTTLE },

  { "create_dirs",        KW_CREATE_DIRS },
  { "optional",           KW_OPTIONAL },

  { "owner",              KW_OWNER },
  { "group",              KW_GROUP },
  { "perm",               KW_PERM },
  { "dir_owner",          KW_DIR_OWNER },
  { "dir_group",          KW_DIR_GROUP },
  { "dir_perm",           KW_DIR_PERM },
  { "template",           KW_TEMPLATE },
  { "template_escape",    KW_TEMPLATE_ESCAPE },
  { "persist_only",       KW_PERSIST_ONLY },
  { "dns_cache_hosts",    KW_DNS_CACHE_HOSTS },
  { "dns_cache",          KW_DNS_CACHE },
  { "dns_cache_size",     KW_DNS_CACHE_SIZE },
  { "dns_cache_expire",   KW_DNS_CACHE_EXPIRE },
  { "dns_cache_expire_failed", KW_DNS_CACHE_EXPIRE_FAILED },

  /* filter items */
  { "type",               KW_TYPE, 0x0300 },
  { "tags",               KW_TAGS, 0x0300 },

  /* on/off switches */
  { "yes",                KW_YES },
  { "on",                 KW_YES },
  { "no",                 KW_NO },
  { "off",                KW_NO },
  { NULL, 0 }
};


CfgParser main_parser =
{
  .debug_flag = &main_debug,
  .name = "main",
  .context = LL_CONTEXT_ROOT,
  .keywords = main_keywords,
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
