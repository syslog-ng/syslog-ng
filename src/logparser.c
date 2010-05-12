/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "logparser.h"
#include "templates.h"
#include "misc.h"
#include "logmatcher.h"
#include "radix.h"
#include "logpatterns.h"
#include "apphook.h"

#include <string.h>
#include <sys/stat.h>

/* NOTE: consumes template */
void
log_parser_set_template(LogParser *self, LogTemplate *template)
{
  log_template_unref(self->template);
  self->template = template;
}

gboolean
log_parser_process(LogParser *self, LogMessage *msg)
{
  gboolean success;

  if (G_LIKELY(!self->template))
    {
      NVTable *payload = nv_table_ref(msg->payload);
      success = self->process(self, msg, log_msg_get_value(msg, LM_V_MESSAGE, NULL));
      nv_table_unref(payload);
    }
  else
    {
      GString *input = g_string_sized_new(256);
      
      log_template_format(self->template, msg, 0, TS_FMT_ISO, NULL, 0, 0, input);
      success = self->process(self, msg, input->str);
      g_string_free(input, TRUE);
    }
  return success;
}


/*
 * Abstract class that has a column list to parse fields into.
 */

void
log_column_parser_set_columns(LogColumnParser *s, GList *columns)
{
  LogColumnParser *self = (LogColumnParser *) s;
  
  string_list_free(self->columns);
  self->columns = columns;
}

void
log_column_parser_free(LogParser *s)
{
  LogColumnParser *self = (LogColumnParser *) s;
  
  string_list_free(self->columns);
}

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
log_csv_parser_process(LogParser *s, LogMessage *msg, const gchar *input)
{
  LogCSVParser *self = (LogCSVParser *) s;
  const gchar *src;
  GList *cur_column = self->super.columns;
  gint len;
  
  src = input;
  if (self->flags & LOG_CSV_PARSER_ESCAPE_NONE)
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
            
          if (*delim == 0)
            break;
          src = (gchar *) delim + 1;
          cur_column = cur_column->next;

          if (cur_column && cur_column->next == NULL && self->flags & LOG_CSV_PARSER_GREEDY)
            {
              log_msg_set_value(msg, log_msg_get_value_handle((gchar *) cur_column->data), src, -1);
              cur_column = NULL;
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
                  log_msg_set_value(msg, log_msg_get_value_handle((gchar *) cur_column->data), src, -1);
                  cur_column = NULL;
                }
            }
        }
      g_string_free(current_value, TRUE);
    }
  if (!cur_column && (self->flags & LOG_CSV_PARSER_DROP_INVALID))
    return FALSE;
  return TRUE;
}

static void
log_csv_parser_free(LogParser *s)
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
  log_column_parser_free(s);
}

/*
 * Parse comma-separated values from a log message.
 */
LogColumnParser *
log_csv_parser_new(void)
{
  LogCSVParser *self = g_new0(LogCSVParser, 1);
  
  self->super.super.process = log_csv_parser_process;
  self->super.super.free_fn = log_csv_parser_free;
  log_csv_parser_set_delimiters(&self->super, " ");
  log_csv_parser_set_quote_pairs(&self->super, "\"\"''");
  self->flags = LOG_CSV_PARSER_STRIP_WHITESPACE | LOG_CSV_PARSER_ESCAPE_NONE;
  return &self->super;
}

gint
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


struct _LogDBParser
{
  LogParser super;
  LogPatternDatabase db;
  gchar *db_file;
  time_t db_file_last_check;
  ino_t db_file_inode;
  time_t db_file_mtime;
};

static void
log_db_parser_reload_database(LogDBParser *self)
{
  struct stat st;
  LogPatternDatabase db_old;
  gchar *db_file_old;

  if (stat(self->db_file, &st) < 0)
    {
      msg_error("Error stating pattern database file, no automatic reload will be performed",
                evt_tag_str("error", g_strerror(errno)),
                NULL);
      return;
    }
  if ((self->db_file_inode == st.st_ino && self->db_file_mtime == st.st_mtime))
    {
      return;
    }
  self->db_file_inode = st.st_ino;
  self->db_file_mtime = st.st_mtime;
  db_old = self->db;
  db_file_old = self->db_file;

  if (!log_pattern_database_load(&self->db, self->db_file))
    {
      msg_error("Error reloading pattern database, no automatic reload will be performed", NULL);
      /* restore the old object pointers */
      self->db = db_old;
      self->db_file = db_file_old;
    }
  else
    {
      msg_notice("Log pattern database reloaded", 
                 evt_tag_str("file", self->db_file),
                 evt_tag_str("version", self->db.version),
                 evt_tag_str("pub_date", self->db.pub_date),
                 NULL);
      /* free the old database if the new was loaded successfully */
      log_pattern_database_free(&db_old);
    }

}

NVHandle class_handle = 0;
NVHandle rule_id_handle = 0;

static inline void
log_db_parser_process_real(LogPatternDatabase *db, LogMessage *msg, GSList **dbg_list)
{
  LogDBResult *verdict;
  gint i;

  verdict = log_pattern_database_lookup(db, msg, dbg_list);
  if (verdict)
    {
      log_msg_set_value(msg, class_handle, verdict->class, -1);
      log_msg_set_value(msg, rule_id_handle, verdict->rule_id, -1);

      if (verdict->tags)
        {
          for (i = 0; i < verdict->tags->len; i++)
            log_msg_set_tag_by_id(msg, g_array_index(verdict->tags, guint, i));
        }

      if (verdict->values)
        {
          GString *result = g_string_sized_new(32);

          for (i = 0; i < verdict->values->len; i++)
            {
              log_template_format(g_ptr_array_index(verdict->values, i), msg, 0, TS_FMT_ISO, NULL, 0, 0, result);
              log_msg_set_value(msg, log_msg_get_value_handle(((LogTemplate *)g_ptr_array_index(verdict->values, i))->name), result->str, result->len);
            }
          g_string_free(result, TRUE);
        }
    }
  else
    {
      log_msg_set_value(msg, class_handle, "unknown", 7);
    }
}

static gboolean
log_db_parser_process(LogParser *s, LogMessage *msg, const char *input)
{
  LogDBParser *self = (LogDBParser *) s;

  if (G_UNLIKELY(self->db_file_last_check == 0 || self->db_file_last_check < msg->timestamps[LM_TS_RECVD].time.tv_sec - 5))
    {
      self->db_file_last_check = msg->timestamps[LM_TS_RECVD].time.tv_sec;
      log_db_parser_reload_database(self);
    }

  log_db_parser_process_real(&self->db, msg, NULL);
  return TRUE;
}

void
log_db_parser_process_lookup(LogPatternDatabase *db, LogMessage *msg, GSList **dbg_list)
{
  log_db_parser_process_real(db, msg, dbg_list);
}

void
log_db_parser_set_db_file(LogDBParser *self, const gchar *db_file)
{
  if (self->db_file)
    g_free(self->db_file);
  self->db_file = g_strdup(db_file);
}

static void
log_db_parser_post_config_hook(gint type, gpointer user_data)
{
  LogDBParser *self = (LogDBParser *) user_data;

  log_db_parser_reload_database(self);
}

static void
log_db_parser_free(LogParser *s)
{
  LogDBParser *self = (LogDBParser *) s;

  log_pattern_database_free(&self->db);

  if (self->db_file)
    g_free(self->db_file);
}

LogParser *
log_db_parser_new(void)
{
  LogDBParser *self = g_new0(LogDBParser, 1);

  self->super.process = log_db_parser_process;
  self->super.free_fn = log_db_parser_free;
  self->db_file = g_strdup(PATH_PATTERNDB_FILE);

  register_application_hook(AH_POST_CONFIG_LOADED, log_db_parser_post_config_hook, self);
  return &self->super;
}

typedef struct _LogParserRule
{
  LogProcessRule super;
  GList *parser_list;
} LogParserRule;

static gboolean
log_parser_rule_process(LogProcessRule *s, LogMessage *msg)
{
  LogParserRule *self = (LogParserRule *) s;
  GList *l;
  
  for (l = self->parser_list; l; l = l->next)
    {
      if (!log_parser_process(l->data, msg))
        return FALSE;
    }
  return TRUE;
}

static void
log_parser_rule_free(LogProcessRule *s)
{
  LogParserRule *self = (LogParserRule *) s;

  g_list_foreach(self->parser_list, (GFunc) log_parser_free, NULL);
  g_list_free(self->parser_list);
  self->parser_list = NULL;
}


/*
 * LogParserRule, a configuration block encapsulating a LogParser instance.
 */ 
LogProcessRule *
log_parser_rule_new(const gchar *name, GList *parser_list)
{
  LogParserRule *self = g_new0(LogParserRule, 1);
  
  log_process_rule_init(&self->super, name);
  self->super.free_fn = log_parser_rule_free;
  self->super.process = log_parser_rule_process;
  self->parser_list = parser_list;
  return &self->super;
}

void
log_db_parser_global_init(void)
{
  class_handle = log_msg_get_value_handle(".classifier.class");
  rule_id_handle = log_msg_get_value_handle(".classifier.rule_id");
}
