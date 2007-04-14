/*
 * Copyright (c) 2002-2007 BalaBit IT Ltd, Budapest, Hungary                    
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

#include "dgroup.h"
#include "misc.h"
#include "stats.h"

#include <sys/time.h>

static const gchar *
log_dest_group_format_stats_name(LogDestGroup *self)
{
  static gchar stats_name[64];

  g_snprintf(stats_name, sizeof(stats_name), "destination(%s)", self->name->str);
  
  return stats_name;

}


static gboolean
log_dest_group_init(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  LogDestGroup *self = (LogDestGroup *) s;
  LogDriver *p;

  for (p = self->drivers; p; p = p->drv_next)
    {
      if (!p->super.init(&p->super, cfg, persist))
	return FALSE;
    }
  stats_register_counter(SC_TYPE_PROCESSED, log_dest_group_format_stats_name(self), &self->processed_messages, FALSE);
  return TRUE;
}

static gboolean
log_dest_group_deinit(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  LogDestGroup *self = (LogDestGroup *) s;
  LogDriver *p;

  stats_unregister_counter(SC_TYPE_PROCESSED, log_dest_group_format_stats_name(self), &self->processed_messages);
  for (p = self->drivers; p; p = p->drv_next)
    {
      if (!p->super.deinit(&p->super, cfg, persist))
	return FALSE;
    }
  return TRUE;
}

static void
log_dest_group_ack(LogMessage *msg, gpointer user_data)
{
  log_msg_ack_block_end(msg);
  log_msg_ack(msg);
  log_msg_unref(msg);
}

static void
log_dest_group_queue(LogPipe *s, LogMessage *msg, gint path_flags)
{
  LogDestGroup *self = (LogDestGroup *) s;
  LogDriver *p;
  
  if ((path_flags & PF_FLOW_CTL_OFF) == 0)
    {
      log_msg_ref(msg);
      log_msg_ack_block_start(msg, log_dest_group_ack, NULL);
    }
  for (p = self->drivers; p; p = p->drv_next)
    {
      if ((path_flags & PF_FLOW_CTL_OFF) == 0)
        log_msg_ack_block_inc(msg);
      log_pipe_queue(&p->super, log_msg_ref(msg), path_flags);
    }
  (*self->processed_messages)++;
  if ((path_flags & PF_FLOW_CTL_OFF) == 0)
    log_msg_ack(msg);
  log_msg_unref(msg);
}

static void
log_dest_group_free(LogPipe *s)
{
  LogDestGroup *self = (LogDestGroup *) s;
  
  log_drv_unref(self->drivers);
  g_string_free(self->name, TRUE);
  g_free(s);
}

LogDestGroup *
log_dest_group_new(gchar *name, LogDriver *drivers)
{
  LogDestGroup *self = g_new0(LogDestGroup, 1);

  log_pipe_init_instance(&self->super);
  self->name = g_string_new(name);
  self->drivers = drivers;
  self->super.init = log_dest_group_init;
  self->super.deinit = log_dest_group_deinit;
  self->super.queue = log_dest_group_queue;
  self->super.free_fn = log_dest_group_free;
  return self;
}
