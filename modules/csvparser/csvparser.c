/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2012 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "csvparser.h"
#include "parser/parser-expr.h"

#include <string.h>

typedef struct _LogCSVParser
{
  LogColumnParser super;
  gchar *delimiters;
  gchar *quotes_start;
  gchar *quotes_end;
  gchar *null_value;
  guint32 flags;
} LogCSVParser;

#define LOG_CSV_PARSER_SINGLE_CHAR_DELIM 0x0100

static inline gint
_is_escape_flag(guint32 flag)
{
  return ((flag & LOG_CSV_PARSER_ESCAPE_MASK) == flag);
}

static inline gint
_contains_escape_flag(guint32 flag)
{
  return (flag & LOG_CSV_PARSER_ESCAPE_MASK);
}

static inline guint32
_get_escape_flags(guint32 flags)
{
  return (flags & LOG_CSV_PARSER_ESCAPE_MASK);
}

static inline guint32
_remove_escape_flags(guint32 flags)
{
  return (flags & (~LOG_CSV_PARSER_ESCAPE_MASK));
}

guint32
log_csv_parser_normalize_escape_flags(LogColumnParser *s, guint32 new_flag)
{
  LogCSVParser *self = (LogCSVParser *) s;
  guint32 current_flags = self->flags;
  if (_is_escape_flag(new_flag))
    return (_remove_escape_flags(current_flags) | new_flag);

  if (!_contains_escape_flag(new_flag))
    return (_get_escape_flags(current_flags) | new_flag);

  return new_flag;
}

void
log_csv_parser_set_flags(LogColumnParser *s, guint32 flags)
{
  LogCSVParser *self = (LogCSVParser *) s;

  self->flags = flags;
}

void
log_csv_parser_set_delimiters(LogColumnParser *s, const gchar *delimiters)
{
  LogCSVParser *self = (LogCSVParser *) s;

  if (self->delimiters)
    g_free(self->delimiters);
  self->delimiters = g_strdup(delimiters);
  if (strlen(delimiters) == 1)
    self->flags |= LOG_CSV_PARSER_SINGLE_CHAR_DELIM;
  else
    self->flags &= ~LOG_CSV_PARSER_SINGLE_CHAR_DELIM;
}

void
log_csv_parser_set_quotes(LogColumnParser *s, const gchar *quotes)
{
  LogCSVParser *self = (LogCSVParser *) s;

  if (self->quotes_start)
    g_free(self->quotes_start);
  if (self->quotes_end)
    g_free(self->quotes_end);
  self->quotes_start = g_strdup(quotes);
  self->quotes_end = g_strdup(quotes);
}

void
log_csv_parser_set_quote_pairs(LogColumnParser *s, const gchar *quote_pairs)
{
  LogCSVParser *self = (LogCSVParser *) s;
  gint i;

  if (self->quotes_start)
    g_free(self->quotes_start);
  if (self->quotes_end)
    g_free(self->quotes_end);

  self->quotes_start = g_malloc((strlen(quote_pairs) / 2) + 1);
  self->quotes_end = g_malloc((strlen(quote_pairs) / 2) + 1);

  for (i = 0; quote_pairs[i] && quote_pairs[i+1]; i += 2)
    {
      self->quotes_start[i / 2] = quote_pairs[i];
      self->quotes_end[i / 2] = quote_pairs[i + 1];
    }
  self->quotes_start[i / 2] = 0;
  self->quotes_end[i / 2] = 0;
}

void
log_csv_parser_set_null_value(LogColumnParser *s, const gchar *null_value)
{
  LogCSVParser *self = (LogCSVParser *) s;

  if (self->null_value)
    g_free(self->null_value);
  self->null_value = g_strdup(null_value);
}

static gboolean
log_csv_parser_process(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const gchar *input, gsize input_len)
{
  LogCSVParser *self = (LogCSVParser *) s;
  const gchar *src;
  GList *cur_column = self->super.columns;
  gint len;
  LogMessage *msg;

  src = input;
  msg = log_msg_make_writable(pmsg, path_options);
  if ((self->flags & LOG_CSV_PARSER_ESCAPE_NONE) || ((self->flags & LOG_CSV_PARSER_ESCAPE_MASK) == 0))
    {
      /* no escaping, no need to keep state, we split input and trim if necessary */

      while (cur_column && *src)
        {
          const guchar *delim;
          guchar *quote;
          guchar current_quote;

          quote = (guchar *) strchr(self->quotes_start, *src);
          if (quote != NULL)
            {
              /* ok, quote character found */
              current_quote = self->quotes_end[quote - (guchar *) self->quotes_start];
              src++;
            }
          else
            {
              /* we didn't start with a quote character, no need for escaping, delimiter terminates */
              current_quote = 0;
            }

          if (self->flags & LOG_CSV_PARSER_STRIP_WHITESPACE)
            {
              while (*src == ' ' || *src == '\t')
                src++;
            }

          if (current_quote)
            {
              /* search for end of quote */
              delim = (guchar *) strchr(src, current_quote);

              if (delim &&
                  (((self->flags & LOG_CSV_PARSER_SINGLE_CHAR_DELIM) && *(delim + 1) == self->delimiters[0]) ||
                     strchr(self->delimiters, *(delim + 1)) != NULL))
                {
                  /* closing quote, and then a delimiter, everything is nice */
                  delim++;
                }
              else if (!delim)
                {
                  /* complete remaining string */
                  delim = (guchar *) src + strlen(src);
                }
            }
          else
            {
              if (self->flags & LOG_CSV_PARSER_SINGLE_CHAR_DELIM)
                {
                  delim = (guchar *) strchr(src, self->delimiters[0]);
                  if (!delim)
                    delim = (guchar *) src + strlen(src);
                }
              else
                {
                  delim = (guchar *) src + strcspn(src, self->delimiters);
                }
            }


          len = delim - (guchar *) src;
          /* move in front of the terminating quote character */
          if (current_quote && len > 0 && src[len - 1] == current_quote)
            len--;
          if (len > 0 && self->flags & LOG_CSV_PARSER_STRIP_WHITESPACE)
            {
              while (len > 0 && (src[len - 1] == ' ' || src[len - 1] == '\t'))
                len--;
            }
          if (self->null_value && strncmp(src, self->null_value, len) == 0)
            log_msg_set_value(msg, log_msg_get_value_handle((gchar *) cur_column->data), "", 0);
          else
            log_msg_set_value(msg, log_msg_get_value_handle((gchar *) cur_column->data), src, len);

          src = (gchar *) delim;
          if (*src)
            src++;
          cur_column = cur_column->next;

          if (cur_column && cur_column->next == NULL && self->flags & LOG_CSV_PARSER_GREEDY)
            {
              /* greedy mode, the last column gets it all, without taking escaping, quotes or anything into account */
              log_msg_set_value(msg, log_msg_get_value_handle((gchar *) cur_column->data), src, -1);
              cur_column = NULL;
              src = NULL;
              break;
            }
        }
    }
  else if (self->flags & (LOG_CSV_PARSER_ESCAPE_BACKSLASH+LOG_CSV_PARSER_ESCAPE_DOUBLE_CHAR))
    {
      /* stateful parser */
      gint state;
      enum
        {
          PS_COLUMN_START,
          PS_WHITESPACE,
          PS_VALUE,
          PS_DELIMITER,
          PS_EOS
        };
      gchar current_quote = 0;
      GString *current_value;
      gboolean store_value = FALSE;
      gchar *quote;

      current_value = g_string_sized_new(128);

      state = PS_COLUMN_START;
      while (cur_column && *src)
        {
          switch (state)
            {
            case PS_COLUMN_START:
              /* check for quote character */
              state = PS_WHITESPACE;
              quote = strchr(self->quotes_start, *src);
              if (quote != NULL)
                {
                  /* ok, quote character found */
                  current_quote = self->quotes_end[quote - self->quotes_start];
                }
              else
                {
                  /* we didn't start with a quote character, no need for escaping, delimiter terminates */
                  current_quote = 0;
                  /* don't skip to the next character */
                  continue;
                }
              break;
            case PS_WHITESPACE:
              if ((self->flags & LOG_CSV_PARSER_STRIP_WHITESPACE) && (*src == ' ' || *src == '\t'))
                {
                  break;
                }
              state = PS_VALUE;
              /* fallthrough */
            case PS_VALUE:
              if (current_quote)
                {
                  /* quoted value */
                  if ((self->flags & LOG_CSV_PARSER_ESCAPE_BACKSLASH) && *src == '\\' && *(src+1))
                    {
                      src++;
                      g_string_append_c(current_value, *src);
                      break;
                    }
                  else if (self->flags & LOG_CSV_PARSER_ESCAPE_DOUBLE_CHAR && *src == current_quote && *(src+1) == current_quote)
                    {
                      src++;
                      g_string_append_c(current_value, *src);
                      break;
                    }
                  if (*src == current_quote)
                    {
                      /* end of column */
                      current_quote = 0;
                      state = PS_DELIMITER;
                      break;
                    }
                  g_string_append_c(current_value, *src);
                }
              else
                {
                  /* unquoted value */
                  if (strchr(self->delimiters, *src))
                    {
                      state = PS_DELIMITER;
                      continue;
                    }
                  g_string_append_c(current_value, *src);
                }
              break;
            case PS_DELIMITER:
              store_value = TRUE;
              break;
            }
          src++;
          if (*src == 0 || store_value)
            {
              len = current_value->len;
              if (self->flags & LOG_CSV_PARSER_STRIP_WHITESPACE)
                {
                  while (len > 0 && (current_value->str[len-1] == ' ' || current_value->str[len-1] == '\t'))
                    len--;
                }
              if (self->null_value && strcmp(current_value->str, self->null_value) == 0)
                log_msg_set_value(msg, log_msg_get_value_handle((gchar *) cur_column->data), "", 0);
              else
                log_msg_set_value(msg, log_msg_get_value_handle((gchar *) cur_column->data), current_value->str, len);
              g_string_truncate(current_value, 0);
              cur_column = cur_column->next;
              state = PS_COLUMN_START;
              store_value = FALSE;

              if (cur_column && cur_column->next == NULL && self->flags & LOG_CSV_PARSER_GREEDY)
                {
                  /* greedy mode, the last column gets it all, without taking escaping, quotes or anything into account */
                  log_msg_set_value(msg, log_msg_get_value_handle((gchar *) cur_column->data), src, -1);
                  cur_column = NULL;
                  src = NULL;
                  break;
                }
            }
        }
      g_string_free(current_value, TRUE);
    }
  if ((cur_column || (src && *src)) && (self->flags & LOG_CSV_PARSER_DROP_INVALID))
    {
      /* there are unfilled variables, OR not all of the input was processed
       * and "drop-invalid" flag is specified */
      return FALSE;
    }
  return TRUE;
}

static LogPipe *
log_csv_parser_clone(LogPipe *s)
{
  LogCSVParser *self = (LogCSVParser *) s;
  LogCSVParser *cloned;
  GList *l;

  cloned = (LogCSVParser *) log_csv_parser_new();
  g_free(cloned->delimiters);
  g_free(cloned->quotes_start);
  g_free(cloned->quotes_end);

  cloned->delimiters = g_strdup(self->delimiters);
  cloned->quotes_start = g_strdup(self->quotes_start);
  cloned->quotes_end = g_strdup(self->quotes_end);
  cloned->null_value = self->null_value ? g_strdup(self->null_value) : NULL;
  cloned->flags = self->flags;

  cloned->super.super.template = log_template_ref(self->super.super.template);
  for (l = self->super.columns; l; l = l->next)
    {
      cloned->super.columns = g_list_append(cloned->super.columns, g_strdup(l->data));
    }
  return &cloned->super.super.super;
}

static void
log_csv_parser_free(LogPipe *s)
{
  LogCSVParser *self = (LogCSVParser *) s;

  if (self->quotes_start)
    g_free(self->quotes_start);
  if (self->quotes_end)
    g_free(self->quotes_end);
  if (self->null_value)
    g_free(self->null_value);
  if (self->delimiters)
    g_free(self->delimiters);
  log_column_parser_free_method(s);
}

/*
 * Parse comma-separated values from a log message.
 */
LogColumnParser *
log_csv_parser_new(void)
{
  LogCSVParser *self = g_new0(LogCSVParser, 1);

  log_column_parser_init_instance(&self->super);
  self->super.super.super.free_fn = log_csv_parser_free;
  self->super.super.super.clone = log_csv_parser_clone;
  self->super.super.process = log_csv_parser_process;
  log_csv_parser_set_delimiters(&self->super, " ");
  log_csv_parser_set_quote_pairs(&self->super, "\"\"''");
  self->flags = LOG_CSV_PARSER_FLAGS_DEFAULT;
  return &self->super;
}

guint32
log_csv_parser_lookup_flag(const gchar *flag)
{
  if (strcmp(flag, "escape-none") == 0)
    return LOG_CSV_PARSER_ESCAPE_NONE;
  else if (strcmp(flag, "escape-backslash") == 0)
    return LOG_CSV_PARSER_ESCAPE_BACKSLASH;
  else if (strcmp(flag, "escape-double-char") == 0)
    return LOG_CSV_PARSER_ESCAPE_DOUBLE_CHAR;
  else if (strcmp(flag, "strip-whitespace") == 0)
    return LOG_CSV_PARSER_STRIP_WHITESPACE;
  else if (strcmp(flag, "greedy") == 0)
    return LOG_CSV_PARSER_GREEDY;
  else if (strcmp(flag, "drop-invalid") == 0)
    return LOG_CSV_PARSER_DROP_INVALID;
  msg_error("Unknown CSV parser flag", evt_tag_str("flag", flag), NULL);
  return 0;
}

guint32
log_csv_parser_get_flags(LogColumnParser *s)
{
  LogCSVParser *self = (LogCSVParser *) s;

  return self->flags;
}
