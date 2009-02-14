/*
 * Copyright (c) 2002-2008 BalaBit IT Ltd, Budapest, Hungary
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
  
#include "sgroup.h"
#include "misc.h"
#include "messages.h"
#include "stats.h"
#include "afinter.h"

#include <time.h>
#include <string.h>

static gboolean
log_source_group_init(LogPipe *s)
{
  LogSourceGroup *self = (LogSourceGroup *) s;
  LogDriver *p;
  GlobalConfig *cfg = log_pipe_get_config(s);
  gint id = 0;

  for (p = self->drivers; p; p = p->drv_next)
    {
      p->group = g_strdup(self->name);
      if (!p->id)
        p->id = g_strdup_printf("%s#%d", self->name, id++);
      if (!log_pipe_init(&p->super, cfg))
        {
          msg_error("Error initializing source driver",
                    evt_tag_str("source", self->name),
                    evt_tag_str("id", p->id),
                    NULL);
          goto deinit_all;
	}
      log_pipe_append(&p->super, s);
    }
  stats_register_counter(0, SCS_SOURCE | SCS_GROUP, self->name, NULL, SC_TYPE_PROCESSED, &self->processed_messages);
  return TRUE;
  
 deinit_all:
  for (p = self->drivers; p; p = p->drv_next)
    log_pipe_deinit(&p->super);
  return FALSE;
}

static gboolean
log_source_group_deinit(LogPipe *s)
{
  LogSourceGroup *self = (LogSourceGroup *) s;
  LogDriver *p;
  gboolean success = TRUE;

  stats_unregister_counter(SCS_SOURCE | SCS_GROUP, self->name, NULL, SC_TYPE_PROCESSED, &self->processed_messages);
  for (p = self->drivers; p; p = p->drv_next)
    {
      if (!log_pipe_deinit(&p->super))
        {
          msg_error("Error deinitializing source driver",
                    evt_tag_str("source", self->name),
                    evt_tag_str("id", p->id),
                    NULL);
          success = FALSE;
	}
    }
  return success;
}

static void
log_source_group_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  LogSourceGroup *self = (LogSourceGroup *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);
  
  log_msg_set_source(msg, g_strndup(self->name, self->name_len), self->name_len);

  if (msg->flags & LF_LOCAL)
    afinter_postpone_mark(cfg->mark_freq);
  log_pipe_queue(self->super.pipe_next, msg, path_options);
  (*self->processed_messages)++;
}

static void
log_source_group_free(LogPipe *s)
{
  LogSourceGroup *self = (LogSourceGroup *) s;
  
  log_drv_unref(self->drivers);
  g_free(self->name);
  log_pipe_free(s);
}

LogSourceGroup *
log_source_group_new(gchar *name, LogDriver *drivers)
{
  LogSourceGroup *self = g_new0(LogSourceGroup, 1);

  log_pipe_init_instance(&self->super);  
  self->name = g_strdup(name);
  self->name_len = strlen(self->name);
  self->drivers = drivers;
  self->super.init = log_source_group_init;
  self->super.deinit = log_source_group_deinit;
  self->super.queue = log_source_group_queue;
  self->super.free_fn = log_source_group_free;
  return self;
}
