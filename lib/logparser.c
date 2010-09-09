/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
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

#include "logparser.h"
#include "templates.h"
#include "misc.h"
#include "logmatcher.h"

#include <string.h>

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
