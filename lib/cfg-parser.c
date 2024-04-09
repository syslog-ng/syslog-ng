/*
 * Copyright (c) 2002-2017 Balabit
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

#include "cfg-parser.h"
#include "cfg-grammar.h"
#include "cfg-source.h"

#include <string.h>
#include <stdlib.h>
#include "str-utils.h"

extern int main_debug;

/* defined in the parser */
int main_parse(CfgLexer *lexer, gpointer *dummy, gpointer arg);

/*
 * PLEASE: if at all possible avoid adding keywords here, as these
 * keywords might collide with user-defined identifiers (e.g. source,
 * destination & block names).
 *
 * Add the keyword & the token to the parser of the module in question
 * instead, even if there are multiple modules using the same
 * keywords.
 *
 * Tokens (e.g. KW_xxx) associated with core keywords (e.g. those
 * listed here) should be declared only once and only in cfg-grammar.y
 * and NOT in external modules.
 */
static CfgLexerKeyword main_keywords[] =
{
  /* statements */
  { "source",             KW_SOURCE },
  { "filter",             KW_FILTER },
  { "filterx",            KW_FILTERX },
  { "parser",             KW_PARSER },
  { "rewrite",            KW_REWRITE },
  { "destination",        KW_DESTINATION },
  { "log",                KW_LOG },
  { "junction",           KW_JUNCTION },
  { "channel",            KW_CHANNEL },
  { "options",            KW_OPTIONS },
  { "include",            KW_INCLUDE, },
  { "block",              KW_BLOCK },
  { "if",                 KW_IF },
  { "else",               KW_ELSE },
  { "elif",               KW_ELIF },

  /* source or destination items */
  { "internal",           KW_INTERNAL },

  /* value pairs */
  { "value_pairs",        KW_VALUE_PAIRS },
  { "exclude",            KW_EXCLUDE },
  { "pair",               KW_PAIR },
  { "key",                KW_KEY },
  { "scope",              KW_SCOPE },
  { "rekey",              KW_REKEY },
  { "shift",              KW_SHIFT },
  { "shift_levels",       KW_SHIFT_LEVELS },
  { "add_prefix",         KW_ADD_PREFIX },
  { "replace",            KW_REPLACE_PREFIX, KWS_OBSOLETE, "replace_prefix" },
  { "replace_prefix",     KW_REPLACE_PREFIX },
  { "cast",               KW_CAST },
  { "upper",              KW_UPPER },
  { "lower",              KW_LOWER },
  { "include_bytes",      KW_INCLUDE_BYTES },

  /* option items */
  { "flags",              KW_FLAGS },
  { "pad_size",           KW_PAD_SIZE },
  { "truncate_size",      KW_TRUNCATE_SIZE },
  { "mark_freq",          KW_MARK_FREQ },
  { "mark",               KW_MARK_FREQ, KWS_OBSOLETE, "mark_freq" },
  { "mark_mode",          KW_MARK_MODE },
  { "stats_freq",         KW_STATS_FREQ, KWS_OBSOLETE, "Use the stats() block. E.g. stats(freq(1));" },
  { "stats_lifetime",     KW_STATS_LIFETIME, KWS_OBSOLETE, "Use the stats() block. E.g. stats(lifetime(10));" },
  { "stats_level",        KW_STATS_LEVEL, KWS_OBSOLETE, "Use the stats() block. E.g. stats(level(1));" },
  { "stats",              KW_STATS },
  { "stats_max_dynamics", KW_STATS_MAX_DYNAMIC, KWS_OBSOLETE, "Use the stats() block. E.g. stats(max-dynamics(10000));" },
  { "freq",               KW_FREQ },
  { "level",              KW_LEVEL },
  { "lifetime",           KW_LIFETIME },
  { "max_dynamics",       KW_MAX_DYNAMIC },
  { "syslog_stats",       KW_SYSLOG_STATS },
  { "healthcheck_freq",   KW_HEALTHCHECK_FREQ},
  { "min_iw_size_per_reader", KW_MIN_IW_SIZE_PER_READER },
  { "flush_lines",        KW_FLUSH_LINES },
  { "flush_timeout",      KW_FLUSH_TIMEOUT, KWS_OBSOLETE, "Some drivers support batch-timeout() instead that you can specify at the destination level." },
  { "suppress",           KW_SUPPRESS },
  { "sync_freq",          KW_FLUSH_LINES, KWS_OBSOLETE, "flush_lines" },
  { "sync",               KW_FLUSH_LINES, KWS_OBSOLETE, "flush_lines" },
  { "long_hostnames",     KW_CHAIN_HOSTNAMES, KWS_OBSOLETE, "chain_hostnames" },
  { "chain_hostnames",    KW_CHAIN_HOSTNAMES },
  { "normalize_hostnames", KW_NORMALIZE_HOSTNAMES },
  { "keep_hostname",      KW_KEEP_HOSTNAME },
  { "check_hostname",     KW_CHECK_HOSTNAME },
  { "bad_hostname",       KW_BAD_HOSTNAME },
  { "custom_domain",      KW_CUSTOM_DOMAIN },
  { "keep_timestamp",     KW_KEEP_TIMESTAMP },
  { "encoding",           KW_ENCODING },
  { "ts_format",          KW_TS_FORMAT },
  { "frac_digits",        KW_FRAC_DIGITS },
  { "time_zone",          KW_TIME_ZONE },
  { "recv_time_zone",     KW_RECV_TIME_ZONE },
  { "send_time_zone",     KW_SEND_TIME_ZONE },
  { "local_time_zone",    KW_LOCAL_TIME_ZONE },
  { "format",             KW_FORMAT },
  { "use_time_recvd",     KW_USE_TIME_RECVD, KWS_OBSOLETE, "Use R_ or S_ prefixed macros in templates or keep_timestamp(no)" },
  { "use_fqdn",           KW_USE_FQDN },
  { "use_dns",            KW_USE_DNS },
  { "time_reopen",        KW_TIME_REOPEN },
  { "time_reap",          KW_TIME_REAP },
  { "time_sleep",         KW_TIME_SLEEP, KWS_OBSOLETE, "time_sleep() has been deprecated" },
  { "file_template",      KW_FILE_TEMPLATE },
  { "proto_template",     KW_PROTO_TEMPLATE },
  { "default_level",      KW_DEFAULT_SEVERITY },
  { "default_priority",   KW_DEFAULT_SEVERITY },
  { "default_severity",   KW_DEFAULT_SEVERITY },
  { "default_facility",   KW_DEFAULT_FACILITY },
  { "sdata_prefix",       KW_SDATA_PREFIX },
  { "threaded",           KW_THREADED },
  { "use_rcptid",         KW_USE_RCPTID, KWS_OBSOLETE, "This has been deprecated, try use_uniqid() instead" },
  { "use_uniqid",         KW_USE_UNIQID },
  { "log_level",          KW_LOG_LEVEL },

  { "log_fifo_size",      KW_LOG_FIFO_SIZE },
  { "log_fetch_limit",    KW_LOG_FETCH_LIMIT },
  { "log_iw_size",        KW_LOG_IW_SIZE },
  { "log_msg_size",       KW_LOG_MSG_SIZE },
  { "trim_large_messages", KW_TRIM_LARGE_MESSAGES },
  { "log_prefix",         KW_LOG_PREFIX, KWS_OBSOLETE, "program_override" },
  { "program_override",   KW_PROGRAM_OVERRIDE },
  { "host_override",      KW_HOST_OVERRIDE },
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
  { "template_function",  KW_TEMPLATE_FUNCTION },
  { "on_error",           KW_ON_ERROR },
  { "persist_only",       KW_PERSIST_ONLY },
  { "dns_cache_hosts",    KW_DNS_CACHE_HOSTS },
  { "dns_cache",          KW_DNS_CACHE },
  { "dns_cache_size",     KW_DNS_CACHE_SIZE },
  { "dns_cache_expire",   KW_DNS_CACHE_EXPIRE },
  { "dns_cache_expire_failed", KW_DNS_CACHE_EXPIRE_FAILED },
  {
    "pass_unix_credentials",   KW_PASS_UNIX_CREDENTIALS, KWS_OBSOLETE,
    "The use of pass-unix-credentials() has been deprecated in " VERSION_3_35 " in favour of "
    "the 'so-passcred()' source option or the 'ignore-aux-data' source flag"
  },

  { "persist_name",            KW_PERSIST_NAME, VERSION_VALUE_3_8 },

  { "retries",            KW_RETRIES },
  { "workers",            KW_WORKERS },
  { "worker_partition_key", KW_WORKER_PARTITION_KEY },
  { "batch_lines",        KW_BATCH_LINES },
  { "batch_timeout",      KW_BATCH_TIMEOUT },

  { "read_old_records",   KW_READ_OLD_RECORDS},
  { "use_syslogng_pid",   KW_USE_SYSLOGNG_PID },
  { "fetch_no_data_delay", KW_FETCH_NO_DATA_DELAY},

  /* multi-line */
  { "multi_line_mode",    KW_MULTI_LINE_MODE  },
  { "multi_line_prefix",  KW_MULTI_LINE_PREFIX },
  { "multi_line_garbage", KW_MULTI_LINE_GARBAGE },
  { "multi_line_suffix",  KW_MULTI_LINE_GARBAGE },
  { "parallelize",        KW_PARALLELIZE },
  { "partitions",         KW_PARTITIONS },
  { "partition_key",      KW_PARTITION_KEY },

  /* filter items */
  { "type",               KW_TYPE },
  { "tags",               KW_TAGS },

  /* on/off switches */
  { "yes",                KW_YES },
  { "on",                 KW_YES },
  { "no",                 KW_NO },
  { "off",                KW_NO },
  { "auto",               KW_AUTO },
  /* rewrite rules */
  { "condition",          KW_CONDITION },
  { "value",              KW_VALUE },

  { NULL, 0 }
};


CfgParser main_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &main_debug,
#endif
  .name = "config",
  .context = LL_CONTEXT_ROOT,
  .keywords = main_keywords,
  .parse = main_parse,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(main_, MAIN_, gpointer *)


void
report_syntax_error(CfgLexer *lexer, const CFG_LTYPE *yylloc, const char *what, const char *msg,
                    gboolean in_main_grammar)
{
  CfgIncludeLevel *level = &lexer->include_stack[lexer->include_depth], *from;

  for (from = level; from >= lexer->include_stack; from--)
    {
      const CFG_LTYPE *from_lloc;

      if (from == level)
        {
          /* the location on the initial level is the one we get as
           * argument, instead of what we have in the lexer's state.  This
           * is because the lexer might be one token in advance (because of
           * LALR) and the grammar is kind enough to pass us the original
           * location.  */

          from_lloc = yylloc;
          fprintf(stderr, "Error parsing %s, %s in %s:%d:%d-%d:%d:\n",
                  what,
                  msg,
                  from_lloc->name,
                  from_lloc->first_line,
                  from_lloc->first_column,
                  from_lloc->last_line,
                  from_lloc->last_column);
        }
      else
        {
          from_lloc = &from->lloc;
          fprintf(stderr, "Included from %s:%d:%d-%d:%d:\n", from_lloc->name,
                  from_lloc->first_line,
                  from_lloc->first_column,
                  from_lloc->last_line,
                  from_lloc->last_column);
        }
      cfg_source_print_source_context(lexer, from, from_lloc);
      fprintf(stderr, "\n");
    }

  if (in_main_grammar)
    fprintf(stderr, "\nsyslog-ng documentation: %s\n"
            "contact: %s\n", PRODUCT_DOCUMENTATION, PRODUCT_CONTACT);

}

/* the debug flag for the main parser will be used for all parsers */
extern int cfg_parser_debug;


gboolean
cfg_parser_parse(CfgParser *self, CfgLexer *lexer, gpointer *instance, gpointer arg)
{
  enum { OK, ERROR, MEMORY_EXHAUSTED } parse_result;
  gboolean success;

  if (cfg_parser_debug)
    {
      fprintf(stderr, "\n\nStarting parser %s\n", self->name);
    }
  if (self->debug_flag)
    (*self->debug_flag) = cfg_parser_debug;
  cfg_lexer_push_context(lexer, self->context, self->keywords, self->name);
  parse_result = self->parse(lexer, instance, arg);
  success = (parse_result == OK);
  cfg_lexer_pop_context(lexer);
  if (cfg_parser_debug)
    {
      fprintf(stderr, "\nStopping parser %s, result: %d\n", self->name, parse_result);
    }
  if (parse_result == MEMORY_EXHAUSTED)
    {
      fprintf(stderr,
              "\nToo many tokens found during parsing, consider increasing YYMAXDEPTH in lib/cfg-grammar.y and recompiling.\n");
    }
  return success;
}

void
cfg_parser_cleanup(CfgParser *self, gpointer instance)
{
  if (instance && self->cleanup)
    self->cleanup(instance);
}


/*
 * This function can be used to parse flags in a flags(...) option. It
 * makes it quite easy to write a flags parser by specifying the
 * operations to be performed in a getopt-like array.
 */
gboolean
cfg_process_flag(CfgFlagHandler *handlers, gpointer base, const gchar *flag_)
{
  gboolean result = FALSE;
  gchar *flag = normalize_flag(flag_);

  for (gsize i = 0; handlers[i].name; i++)
    {
      CfgFlagHandler *handler = &handlers[i];
      if (strcmp(handlers[i].name, flag) == 0)
        {
          guint32 *field = ((guint32 *) (((gchar *) base) + handler->ofs));
          switch (handler->op)
            {
            case CFH_SET:
              /* this works if handler->mask is unset and handler->param is a single bit only */

              if (handler->mask)
                *field = ((*field) & ~handler->mask) | handler->param;
              else
                *field = (*field) | handler->param;
              result = TRUE;
              goto finish;
            case CFH_CLEAR:
              /* set the bitfield to zero */

              if (handler->mask)
                *field = (*field) & ~handler->mask;
              else
                *field = (*field) & ~handler->param;
              result = TRUE;
              goto finish;
            default:
              g_assert_not_reached();
              break;
            }
        }
    }

finish:
  g_free(flag);
  return result;
}

gboolean
cfg_process_yesno(const gchar *yesno)
{
  if (strcasecmp(yesno, "yes") == 0 || atoi(yesno) > 0)
    return TRUE;
  return FALSE;
}
