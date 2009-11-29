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

#ifndef LOGPARSER_H_INCLUDED
#define LOGPARSER_H_INCLUDED

#include "logmsg.h"
#include "messages.h"
#include "logprocess.h"
#include "templates.h"
#include "logpatterns.h"

typedef struct _LogParser LogParser;
typedef struct _LogColumnParser LogColumnParser;
typedef struct _LogDBParser LogDBParser;

struct _LogParser
{
  struct _LogTemplate *template;
  gboolean (*process)(LogParser *s, LogMessage *msg, const gchar *input);
  void (*free_fn)(LogParser *s);
};

void log_parser_set_template(LogParser *self, LogTemplate *template);
gboolean log_parser_process(LogParser *self, LogMessage *msg);

static inline void
log_parser_free(LogParser *self)
{
  self->free_fn(self);
  log_template_unref(self->template);
  g_free(self);
}

struct _LogColumnParser
{
  LogParser super;
  GList *columns;
};


void log_column_parser_set_columns(LogColumnParser *s, GList *fields);

#define LOG_CSV_PARSER_ESCAPE_NONE        0x0001
#define LOG_CSV_PARSER_ESCAPE_BACKSLASH   0x0002
#define LOG_CSV_PARSER_ESCAPE_DOUBLE_CHAR 0x0004
#define LOG_CSV_PARSER_STRIP_WHITESPACE   0x0008
#define LOG_CSV_PARSER_GREEDY             0x0010
#define LOG_CSV_PARSER_DROP_INVALID       0x0020


void log_csv_parser_set_flags(LogColumnParser *s, guint32 flags);
void log_csv_parser_set_delimiters(LogColumnParser *s, const gchar *delimiters);
void log_csv_parser_set_quotes(LogColumnParser *s, const gchar *quotes);
void log_csv_parser_set_quote_pairs(LogColumnParser *s, const gchar *quote_pairs);
void log_csv_parser_set_null_value(LogColumnParser *s, const gchar *null_value);
LogColumnParser *log_csv_parser_new(void);
gint log_csv_parser_lookup_flag(const gchar *flag);

void log_db_parser_set_db_file(LogDBParser *self, const gchar *db_file);
LogParser *log_db_parser_new(void);
void log_db_parser_process_lookup(LogPatternDatabase *db, LogMessage *msg);


LogProcessRule *log_parser_rule_new(const gchar *name, LogParser *parser);

void log_db_parser_global_init(void);

#endif

