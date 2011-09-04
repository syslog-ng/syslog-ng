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

static void
log_parser_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options, gpointer user_data)
{
  LogParser *self = (LogParser *) s;
  gboolean success;

  if (G_LIKELY(!self->template))
    {
      NVTable *payload = nv_table_ref(msg->payload);

      /* NOTE: the process function may set values in the LogMessage
       * instance, which in turn can trigger nv_table_realloc() to be
       * called.  However in case nv_table_realloc() finds a refcounter > 1,
       * it'll always _move_ the structure and leave the old one intact,
       * until its refcounter drops to zero.  If that wouldn't be the case,
       * nv_table_realloc() could make our payload pointer and the
       * LM_V_MESSAGE pointer we pass to process() go stale.
       */

      success = self->process(self, msg, log_msg_get_value(msg, LM_V_MESSAGE, NULL));
      nv_table_unref(payload);
    }
  else
    {
      GString *input = g_string_sized_new(256);
      
      log_template_format(self->template, msg, NULL, LTZ_LOCAL, 0, NULL, input);
      success = self->process(self, msg, input->str);
      g_string_free(input, TRUE);
    }
  if (success)
    {
      log_pipe_forward_msg(s, msg, path_options);
    }
  else
    {
      if (path_options->matched)
        (*path_options->matched) = FALSE;
      log_msg_drop(msg, path_options);
    }
}

void
log_parser_free_method(LogPipe *s)
{
  LogParser *self = (LogParser *) s;

  log_template_unref(self->template);
  log_process_pipe_free_method(s);
}

void
log_parser_init_instance(LogParser *self)
{
  log_process_pipe_init_instance(&self->super);
  self->super.super.flags |= PIF_CLONE;
  self->super.super.free_fn = log_parser_free_method;
  self->super.super.queue = log_parser_queue;
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
log_column_parser_free_method(LogPipe *s)
{
  LogColumnParser *self = (LogColumnParser *) s;
  
  string_list_free(self->columns);
  log_parser_free_method(s);
}

void
log_column_parser_init_instance(LogColumnParser *self)
{
  log_parser_init_instance(&self->super);
  self->super.super.super.free_fn = log_column_parser_free_method;
}
