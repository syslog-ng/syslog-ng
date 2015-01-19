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
  GList *string_delimiters;
} LogCSVParser;

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

void
log_csv_parser_append_string_delimiter(LogColumnParser *s, const gchar *string_delimiter)
{
  LogCSVParser *self = (LogCSVParser *) s;

  self->string_delimiters = g_list_prepend(self->string_delimiters, (gpointer)string_delimiter);
}

typedef struct _StrLstPrivData {
  char *first_occurence;
  int delim_length;
  const char *string;
} StrLstPrivData;

void
_find_string(gpointer data, gpointer u_data)
{
  const char *item = (const char*) data;
  StrLstPrivData *user_data = (StrLstPrivData*) u_data;

  if (!user_data->first_occurence)
  {
    user_data->first_occurence = strstr(user_data->string, item);
    user_data->delim_length = (user_data->first_occurence) ? strlen(item) : 0;
  }
}

/* searches for str1 in list and returns the first occurence, otherwise NULL */
guchar*
strlst(const char * str1, GList *list, int *delim_length)
{
  StrLstPrivData user_data = {NULL, 0, str1};

  g_list_foreach(list, _find_string, (gpointer) &user_data);

  *delim_length = user_data.delim_length;

  return (guchar*) user_data.first_occurence;
}

static inline gboolean
_do_drop_invalid(GList *cur_column, const gchar *src, guint32 flags)
{
  return (cur_column || (src && *src)) && (flags & LOG_CSV_PARSER_DROP_INVALID);
}

static inline gboolean
_is_whitespace_char(const gchar* str)
{
  return (*str == ' ' || *str == '\t') ? TRUE : FALSE;
}

static inline void
_strip_whitespace_left(const gchar** src)
{
  while (_is_whitespace_char(*src))
    (*src)++;
}

typedef struct _UnescapedParserState {
  guchar *delim;
  guchar *delim_string;
  guchar *delim_char;
  gint delim_len;
} UnescapedParserState;

static inline guchar*
_find_delim_unescaped_quoted(LogCSVParser *self, UnescapedParserState *pstate, const gchar* src, guchar quote)
{
  guchar *delim = NULL;
  guchar *delim_string = NULL;
  guchar *delim_char = NULL;
  gint delim_len = 0;

  /* search for end of quote */
  delim = (guchar *) strchr(src, quote);

  if (delim)
    {
      delim_string = strlst((const char *)delim, self->string_delimiters, &delim_len);
      if (delim_string == (delim + 1) || (strchr(self->delimiters, *(delim + 1)) != NULL ))
        {
          /* closing quote, and then a delimiter, everything is nice */
          delim++;
        }
    }
  else if (!delim)
    {
      /* complete remaining string */
      delim = (guchar *) src + strlen(src);
    }

  pstate->delim = delim;
  pstate->delim_string = delim_string;
  pstate->delim_char = delim_char;
  pstate->delim_len = delim_len;

  return delim;
}

static inline guchar*
_find_delim_unescaped_unquoted(LogCSVParser *self, UnescapedParserState *pstate, const gchar* src)
{
  guchar *delim = NULL;
  guchar *delim_string = NULL;
  guchar *delim_char = NULL;
  gint delim_len = 0;

  if (self->string_delimiters)
    {
      delim_string = strlst(src, self->string_delimiters, &delim_len);
    }

  delim_char = (guchar *) src + strcspn(src, self->delimiters);

  // string delimeter legalabb a karakter delim helyen kezdodik
  if (delim_string && delim_string <= delim_char)
    {
      delim = delim_string;
    }
  else if (delim_char)
    {
      delim = delim_char; 
    }
  else
    {
      delim = (guchar *) src + strlen(src);
    }

  pstate->delim = delim;
  pstate->delim_string = delim_string;
  pstate->delim_char = delim_char;
  pstate->delim_len = delim_len;
  
  return delim;
}

static gint
_get_column_length_unescaped(LogCSVParser *self, const guchar *delim, const gchar *src, guchar quote)
{
  gint len;

  // az oszlop hossza?
  len = delim - (guchar *) src;
  /* move in front of the terminating quote character */
  if (quote && len > 0 && src[len - 1] == quote)
    len--;
  if (len > 0 && self->flags & LOG_CSV_PARSER_STRIP_WHITESPACE)
    {
      while (len > 0 && (_is_whitespace_char(src + len - 1)))
        len--;
    }

  return len;
}

static inline void
_move_to_next_column_unescaped(UnescapedParserState *pstate, const gchar** src)
{
  *src = (gchar *) pstate->delim;

  if (pstate->delim_len && (pstate->delim == pstate->delim_string))
    *src += pstate->delim_len;
  else if (**src)
    (*src)++;
}

static guchar
_get_current_quote_unescaped(LogCSVParser *self, const gchar** src)
{
  guchar *quote = (guchar *) strchr(self->quotes_start, **src);

  if (quote != NULL)
    {
      /* ok, quote character found */
      (*src)++;
      return self->quotes_end[quote - (guchar *) self->quotes_start];
    }
  else
    {
      /* we didn't start with a quote character, no need for escaping, delimiter terminates */
      return  0;
    }
}

static inline guchar*
_find_delim_unescaped(LogCSVParser *self, UnescapedParserState *pstate, const gchar* src, guchar quote)
{
  if (quote)
    {
      return _find_delim_unescaped_quoted(self, pstate, src, quote);
    }
  else
    {
      return _find_delim_unescaped_unquoted(self, pstate, src);
    }
}

static inline gboolean
_check_and_handle_greedy_mode(LogCSVParser *self, GList **cur_column, LogMessage *msg, const gchar **src)
{
 if (*cur_column && (*cur_column)->next == NULL && self->flags & LOG_CSV_PARSER_GREEDY)
   {
     /* greedy mode, the last column gets it all, without taking escaping, quotes or anything into account */
     log_msg_set_value_by_name(msg, (gchar *) (*cur_column)->data, *src, -1);
     *cur_column = NULL;
     *src = NULL;
     return TRUE;
   }
  return FALSE;
}

static gboolean
log_csv_parser_process_unescaped(LogCSVParser *self, LogMessage *msg, const gchar* src)
{
  GList *cur_column = self->super.columns;
  gint len;
  /* no escaping, no need to keep state, we split input and trim if necessary */

  while (cur_column && *src)
    {
      UnescapedParserState pstate = {NULL, NULL, NULL}; 
      guchar current_quote;
      guchar *delim=NULL;

      current_quote = _get_current_quote_unescaped(self, &src);

      if (self->flags & LOG_CSV_PARSER_STRIP_WHITESPACE)
        {
          _strip_whitespace_left(&src);
        }
        
      delim = _find_delim_unescaped(self, &pstate, src, current_quote);

      len = _get_column_length_unescaped(self, delim, src, current_quote);

      if (self->null_value && strncmp(src, self->null_value, len) == 0)
        log_msg_set_value_by_name(msg, (gchar *) cur_column->data, "", 0);
      else
        log_msg_set_value_by_name(msg, (gchar *) cur_column->data, src, len);

      _move_to_next_column_unescaped(&pstate, &src);
      cur_column = cur_column->next;
      _check_and_handle_greedy_mode(self, &cur_column, msg, &src);
    }

  if (_do_drop_invalid(cur_column, src, self->flags))
    {
      /* there are unfilled variables, OR not all of the input was processed
       * and "drop-invalid" flag is specified */
      return FALSE;
    }
  return TRUE;
}

static inline gchar
_get_current_quote_escaped(LogCSVParser *self, const gchar *src)
{
  gchar *quote = strchr(self->quotes_start, *src);

  if (quote != NULL)
    {
      /* ok, quote character found */
      return self->quotes_end[quote - self->quotes_start];
    }
  else
    {
      /* we didn't start with a quote character, no need for escaping, delimiter terminates */
      return 0;
    }
}

enum
  {
    PS_COLUMN_START,
    PS_WHITESPACE,
    PS_VALUE,
    PS_DELIMITER,
    PS_EOS
  };

typedef struct _EscapedParserState
{
  LogMessage *msg;
  const gchar *src;
  gint state;
  GString *current_value;
  GList *current_column;
  gchar current_quote;
  gint delim_len;
  gboolean store_value;
} EscapedParserState;

static inline void
_escaped_parser_state_init(EscapedParserState *self, LogMessage *msg, GList *cur_column, const gchar *src)
{
  self->msg = msg;
  self->src = src;
  self->state = PS_COLUMN_START;
  self->current_value = g_string_sized_new(128);
  self->current_quote = 0;
  self->current_column = cur_column;
  self->delim_len = 0;
  self->store_value = FALSE;
}

static inline void
_process_escaped_VALUE_quoted(LogCSVParser *self, EscapedParserState *pstate)
{
  /* quoted value */
  if ((self->flags & LOG_CSV_PARSER_ESCAPE_BACKSLASH) && *pstate->src == '\\' && *(pstate->src+1))
    {
      pstate->src++;
      g_string_append_c(pstate->current_value, *pstate->src);
      return;
    }
  else if (self->flags & LOG_CSV_PARSER_ESCAPE_DOUBLE_CHAR && *pstate->src == pstate->current_quote && *(pstate->src+1) == pstate->current_quote)
    {
      (pstate->src)++;
      g_string_append_c(pstate->current_value, *pstate->src);
      return;
    }
  if (*pstate->src == pstate->current_quote)
    {
      /* end of column */
      pstate->current_quote = 0;
      pstate->state = PS_DELIMITER;

      if (self->string_delimiters && strlst(pstate->src, self->string_delimiters, &pstate->delim_len) != ((guchar*)pstate->src + 1))
        pstate->delim_len = 0;

      return;
    }
  g_string_append_c(pstate->current_value, *pstate->src);
}

static gboolean
_is_delimiter_escaped(LogCSVParser *self, EscapedParserState *pstate)
{
  return (self->string_delimiters && strlst(pstate->src, self->string_delimiters, &pstate->delim_len) == (guchar*)pstate->src) || strchr(self->delimiters, *pstate->src);
}

static inline gint
_get_column_length_escaped(LogCSVParser *self, GString *current_value)
{
  gint len = current_value->len;

  if (self->flags & LOG_CSV_PARSER_STRIP_WHITESPACE)
    {
      while (len > 0 && _is_whitespace_char(current_value->str + len -1))
        len--;
    }

  return len;
}

static inline void
_store_value_escaped(LogCSVParser *self, EscapedParserState *ps)
{
  gint len = _get_column_length_escaped(self, ps->current_value);
  if (self->null_value && strcmp(ps->current_value->str, self->null_value) == 0)
    log_msg_set_value_by_name(ps->msg, (gchar *) ps->current_column->data, "", 0);
  else
    log_msg_set_value_by_name(ps->msg, (gchar *) ps->current_column->data, ps->current_value->str, len);
}

static inline void
_reset_variables(LogCSVParser *self, EscapedParserState *ps)
{
  g_string_truncate(ps->current_value, 0);
  ps->current_column = ps->current_column->next;
  ps->state = PS_COLUMN_START;
  ps->store_value = FALSE;

  if (ps->delim_len > 0)
    ps->src += ps->delim_len - 1;

  ps->delim_len = 0;
}

#define has_more_data(ps) (ps.current_column && *ps.src)

static gboolean
log_csv_parser_process_escaped(LogCSVParser *self, LogMessage *msg, const gchar* src)
{
  EscapedParserState pstate;

  _escaped_parser_state_init(&pstate, msg, self->super.columns, src);

  while (has_more_data(pstate))
    {
      switch (pstate.state)
        {
        case PS_COLUMN_START:
          /* check for quote character */
          pstate.state = PS_WHITESPACE;
          pstate.current_quote = _get_current_quote_escaped(self, pstate.src);
          if (!pstate.current_quote)
            continue;

          break;
        case PS_WHITESPACE:
          if ((self->flags & LOG_CSV_PARSER_STRIP_WHITESPACE) && _is_whitespace_char(pstate.src))
            {
              break;
            }
          pstate.state = PS_VALUE;
          /* fallthrough */
        case PS_VALUE:
          if (pstate.current_quote)
            {
              _process_escaped_VALUE_quoted(self, &pstate);
              break;
            }
          else
            {
              /* unquoted value */
              if (_is_delimiter_escaped(self, &pstate))
                {
                  pstate.state = PS_DELIMITER;
                  continue;
                }
              g_string_append_c(pstate.current_value, *pstate.src);
            }
          break;
        case PS_DELIMITER:
          pstate.store_value = TRUE;
          break;
        }
      pstate.src++;
      if (*pstate.src == 0 || pstate.store_value)
        {
          _store_value_escaped(self, &pstate);
          _reset_variables(self, &pstate);
          _check_and_handle_greedy_mode(self, &pstate.current_column, pstate.msg, &pstate.src);
        }
    }

  g_string_free(pstate.current_value, TRUE);

  if (_do_drop_invalid(pstate.current_column, pstate.src, self->flags))
    {
      /* there are unfilled variables, OR not all of the input was processed
       * and "drop-invalid" flag is specified */
      return FALSE;
    }
  return TRUE;
}

static inline gboolean
_dont_escape(guint32 flags)
{
  return ((flags & LOG_CSV_PARSER_ESCAPE_NONE) ||
         ((flags & LOG_CSV_PARSER_ESCAPE_MASK) == 0));
}

static inline gboolean
_do_escape(guint32 flags)
{
  return flags & (LOG_CSV_PARSER_ESCAPE_BACKSLASH+LOG_CSV_PARSER_ESCAPE_DOUBLE_CHAR);
}

static gboolean
log_csv_parser_process(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const gchar *input, gsize input_len)
{
  LogCSVParser *self = (LogCSVParser *) s;
  LogMessage *msg = log_msg_make_writable(pmsg, path_options);
  const gchar *src = input;

  if (_dont_escape(self->flags))
    return log_csv_parser_process_unescaped(self, msg, src);
  else if (_do_escape(self->flags))
    return log_csv_parser_process_escaped(self, msg, src);
  return FALSE;
}

static LogPipe *
log_csv_parser_clone(LogPipe *s)
{
  LogCSVParser *self = (LogCSVParser *) s;
  LogCSVParser *cloned;
  GList *l;

  cloned = (LogCSVParser *) log_csv_parser_new(s->cfg);
  g_free(cloned->delimiters);
  g_free(cloned->quotes_start);
  g_free(cloned->quotes_end);
  if (cloned->string_delimiters)
    g_list_free_full(cloned->string_delimiters, g_free);

  cloned->delimiters = g_strdup(self->delimiters);
  cloned->quotes_start = g_strdup(self->quotes_start);
  cloned->quotes_end = g_strdup(self->quotes_end);
  cloned->null_value = self->null_value ? g_strdup(self->null_value) : NULL;
  cloned->flags = self->flags;
  cloned->string_delimiters = self->string_delimiters;

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
  if (self->string_delimiters)
    g_list_free_full(self->string_delimiters, g_free);
  log_column_parser_free_method(s);
}

/*
 * Parse comma-separated values from a log message.
 */
LogColumnParser *
log_csv_parser_new(GlobalConfig *cfg)
{
  LogCSVParser *self = g_new0(LogCSVParser, 1);

  log_column_parser_init_instance(&self->super, cfg);
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
