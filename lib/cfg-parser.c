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

#include <string.h>
#include <stdlib.h>

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
  { "add_prefix",         KW_ADD_PREFIX },
  { "replace",            KW_REPLACE_PREFIX, KWS_OBSOLETE, "replace_prefix" },
  { "replace_prefix",     KW_REPLACE_PREFIX },

  /* option items */
  { "flags",              KW_FLAGS },
  { "pad_size",           KW_PAD_SIZE },
  { "mark_freq",          KW_MARK_FREQ },
  { "mark",               KW_MARK_FREQ, KWS_OBSOLETE, "mark_freq" },
  { "mark_mode",          KW_MARK_MODE },
  { "stats_freq",         KW_STATS_FREQ },
  { "stats_lifetime",     KW_STATS_LIFETIME },
  { "stats_level",        KW_STATS_LEVEL },
  { "stats",              KW_STATS_FREQ, KWS_OBSOLETE, "stats_freq" },
  { "stats_max_dynamics", KW_STATS_MAX_DYNAMIC },
  { "flush_lines",        KW_FLUSH_LINES },
  { "flush_timeout",      KW_FLUSH_TIMEOUT },
  { "suppress",           KW_SUPPRESS },
  { "sync_freq",          KW_FLUSH_LINES, KWS_OBSOLETE, "flush_lines" },
  { "sync",               KW_FLUSH_LINES, KWS_OBSOLETE, "flush_lines" },
  { "long_hostnames",     KW_CHAIN_HOSTNAMES, KWS_OBSOLETE, "chain_hostnames" },
  { "chain_hostnames",    KW_CHAIN_HOSTNAMES },
  { "normalize_hostnames",KW_NORMALIZE_HOSTNAMES },
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
  { "default_level",      KW_DEFAULT_LEVEL },
  { "default_priority",   KW_DEFAULT_LEVEL },
  { "default_facility",   KW_DEFAULT_FACILITY },
  { "threaded",           KW_THREADED },
  { "use_rcptid",         KW_USE_RCPTID, KWS_OBSOLETE, "This has been deprecated, try use_uniqid() instead" },
  { "use_uniqid",         KW_USE_UNIQID },

  { "log_fifo_size",      KW_LOG_FIFO_SIZE },
  { "log_fetch_limit",    KW_LOG_FETCH_LIMIT },
  { "log_iw_size",        KW_LOG_IW_SIZE },
  { "log_msg_size",       KW_LOG_MSG_SIZE },
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
  { "pass_unix_credentials",   KW_PASS_UNIX_CREDENTIALS },
  { "persist_name",            KW_PERSIST_NAME, VERSION_VALUE_3_8 },

  { "retries",            KW_RETRIES },

  { "read_old_records",   KW_READ_OLD_RECORDS},
  /* filter items */
  { "type",               KW_TYPE },
  { "tags",               KW_TAGS },

  /* on/off switches */
  { "yes",                KW_YES },
  { "on",                 KW_YES },
  { "no",                 KW_NO },
  { "off",                KW_NO },
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

CFG_PARSER_IMPLEMENT_LEXER_BINDING(main_, gpointer *)

/* display CONTEXT lines before and after the offending line */
#define CONTEXT 5

static void
_format_source_prefix(gchar *line_prefix, gsize line_prefix_len, gint lineno, gboolean error_location)
{
  g_snprintf(line_prefix, line_prefix_len, "%d", lineno);
  if (error_location)
    {
      for (gint i = strlen(line_prefix); i < 6; i++)
        g_strlcat(line_prefix, "-", line_prefix_len);

      g_strlcat(line_prefix, ">", line_prefix_len);
    }
}

static void
_print_underline(const gchar *line, gint whitespace_before, gint number_of_carets)
{
  for (gint i = 0; line[i] && i < whitespace_before; i++)
    {
      fprintf(stderr, "%c", line[i] == '\t' ? '\t' : ' ');
    }

  /* NOTE: sometimes the yylloc has zero characters, print a caret even in
   * this case, that's why i == 0 is there */

  for (gint i = 0; i == 0 || i < number_of_carets; i++)
    fprintf(stderr, "^");
  fprintf(stderr, "\n");
}

static void
_print_underlined_source_block(YYLTYPE *yylloc, gchar **lines, gint error_index)
{
  gint line_ndx;
  gchar line_prefix[12];

  for (line_ndx = 0; lines[line_ndx]; line_ndx++)
    {
      gint lineno = yylloc->first_line + line_ndx - error_index;
      const gchar *line = lines[line_ndx];
      gint line_len = strlen(line);
      gboolean line_ends_with_newline = line_len > 0 && line[line_len - 1] == '\n';

      _format_source_prefix(line_prefix, sizeof(line_prefix), lineno, line_ndx == error_index);

      fprintf(stderr, "%-8s%s%s", line_prefix, line, line_ends_with_newline ? "" : "\n");

      if (line_ndx == error_index)
        {
          /* print the underline right below the source line we just printed */
          fprintf(stderr, "%-8s", line_prefix);

          gboolean multi_line = yylloc->first_line != yylloc->last_line;

          _print_underline(line, yylloc->first_column - 1,
                           multi_line ? strlen(&line[yylloc->first_column]) + 1
                                      : yylloc->last_column - yylloc->first_column);
        }
      else if (line_ndx >= error_index + CONTEXT)
        break;
    }
}

static void
_report_file_location(const gchar *filename, YYLTYPE *yylloc)
{
  FILE *f;
  gint lineno = 0;
  gchar buf[1024];
  GPtrArray *context = g_ptr_array_new();
  gint error_index = 0;

  f = fopen(filename, "r");
  if (f)
    {
      while (fgets(buf, sizeof(buf), f))
        {
          lineno++;
          if (lineno > (gint) yylloc->first_line + CONTEXT)
            break;
          else if (lineno < (gint) yylloc->first_line - CONTEXT)
            continue;
          else if (lineno == yylloc->first_line)
            error_index = context->len;
          g_ptr_array_add(context, g_strdup(buf));
        }
      /* NOTE: do we have the appropriate number of lines? */
      if (lineno <= yylloc->first_line)
        goto exit;
      g_ptr_array_add(context, NULL);
      fclose(f);
    }
  _print_underlined_source_block(yylloc, (gchar **) context->pdata, error_index);

 exit:
  g_ptr_array_foreach(context, (GFunc) g_free, NULL);
  g_ptr_array_free(context, TRUE);
}

static void
_report_buffer_location(const gchar *buffer_content, YYLTYPE *yylloc)
{
  gchar **lines = g_strsplit(buffer_content, "\n", yylloc->first_line + CONTEXT + 1);
  gint num_lines = g_strv_length(lines);

  if (num_lines <= yylloc->first_line)
    goto exit;

  gint start = yylloc->first_line - 1 - CONTEXT;
  gint error_index = CONTEXT;
  if (start < 0)
    {
      error_index += start;
      start = 0;
    }
  _print_underlined_source_block(yylloc, &lines[start], error_index);

 exit:
  g_strfreev(lines);
}

void
report_syntax_error(CfgLexer *lexer, YYLTYPE *yylloc, const char *what, const char *msg)
{
  CfgIncludeLevel *level = yylloc->level, *from;

  for (from = level; from >= lexer->include_stack; from--)
    {
      if (from == level)
        {
          fprintf(stderr, "Error parsing %s, %s in %s:\n",
                  what,
                  msg,
                  yylloc->level->name);
        }
      else
        {
          fprintf(stderr, "Included from %s:\n", from->name);
        }
      if (from->include_type == CFGI_FILE)
        {
          _report_file_location(from->name, &from->lloc);
        }
      else if (from->include_type == CFGI_BUFFER)
        {
          _report_buffer_location(from->buffer.original_content, &from->lloc);
        }
      fprintf(stderr, "\n");
    }

  fprintf(stderr, "\nsyslog-ng documentation: https://www.balabit.com/support/documentation?product=%s\n"
          "contact: %s\n", PRODUCT_NAME, PRODUCT_CONTACT);

}

/* the debug flag for the main parser will be used for all parsers */
extern int cfg_parser_debug;


gboolean
cfg_parser_parse(CfgParser *self, CfgLexer *lexer, gpointer *instance, gpointer arg)
{
  gboolean success;

  if (cfg_parser_debug)
    {
      fprintf(stderr, "\n\nStarting parser %s\n", self->name);
    }
  if (self->debug_flag)
    (*self->debug_flag) = cfg_parser_debug;
  cfg_lexer_push_context(lexer, self->context, self->keywords, self->name);
  success = (self->parse(lexer, instance, arg) == 0);
  cfg_lexer_pop_context(lexer);
  if (cfg_parser_debug)
    {
      fprintf(stderr, "\nStopping parser %s, result: %d\n", self->name, success);
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
  gint h;
  gchar flag[32];

  for (h = 0; flag_[h] && h < sizeof(flag); h++)
    {
      if (flag_[h] == '_')
        flag[h] = '-';
      else
        flag[h] = flag_[h];
    }
  flag[h] = 0;

  for (h = 0; handlers[h].name; h++)
    {
      CfgFlagHandler *handler = &handlers[h];

      if (strcmp(handlers[h].name, flag) == 0)
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
              return TRUE;
            case CFH_CLEAR:
              /* set the bitfield to zero */

              if (handler->mask)
                *field = (*field) & ~handler->mask;
              else
                *field = (*field) & ~handler->param;
              return TRUE;
            }
        }
    }
  return FALSE;
}

gboolean
cfg_process_yesno(const gchar *yesno)
{
  if (strcasecmp(yesno, "yes") == 0 || atoi(yesno) > 0)
    return TRUE;
  return FALSE;
}
