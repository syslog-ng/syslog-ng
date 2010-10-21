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

#include "logprocess.h"
#include <string.h>

void
log_process_pipe_free_method(LogPipe *s)
{
  log_pipe_free_method(s);
}

void
log_process_pipe_init_instance(LogProcessPipe *self)
{
  log_pipe_init_instance(&self->super);
  self->super.free_fn = log_process_pipe_free_method;
}


LogProcessRule *
log_process_rule_new(const gchar *name, GList *head)
{
  LogProcessRule *self = g_new0(LogProcessRule, 1);

  self->ref_cnt = 1;
  self->head = head;
  self->name = g_strdup(name);

  return self;
}

void
log_process_rule_ref(LogProcessRule *self)
{ 
  self->ref_cnt++;
}

void
log_process_rule_unref(LogProcessRule *self)
{
  if (--self->ref_cnt == 0)
    {
      g_list_foreach(self->head, (GFunc) log_pipe_unref, NULL);
      g_list_free(self->head);
      g_free(self->name);
      g_free(self);
    }
}
