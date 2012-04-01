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
  
#include "driver.h"
#include "logqueue-fifo.h"

/* LogDriverPlugin */

void
log_driver_plugin_free_method(LogDriverPlugin *self)
{
  g_free(self);
}

void
log_driver_plugin_init_instance(LogDriverPlugin *self)
{
  self->free_fn = log_driver_plugin_free_method;
}

/* LogDriver */

void
log_driver_add_plugin(LogDriver *self, LogDriverPlugin *plugin)
{
  self->plugins = g_list_append(self->plugins, plugin);
}

void
log_driver_append(LogDriver *self, LogDriver *next)
{
  if (self->drv_next)
    log_pipe_unref(&self->drv_next->super);
  self->drv_next = (LogDriver *) log_pipe_ref(&next->super);
}

gboolean
log_driver_init_method(LogPipe *s)
{
  LogDriver *self = (LogDriver *) s;
  gboolean success = TRUE;
  GList *l;

  for (l = self->plugins; l; l = l->next)
    {
      if (!log_driver_plugin_attach((LogDriverPlugin *) l->data, self))
        success = FALSE;
    }
  return success;
}

gboolean
log_driver_deinit_method(LogPipe *s)
{
  LogDriver *self = (LogDriver *) s;
  gboolean success = TRUE;
  GList *l;

  for (l = self->plugins; l; l = l->next)
    {
      log_driver_plugin_detach((LogDriverPlugin *) l->data, self);
    }
  return success;
}

/* NOTE: intentionally static, as only LogSrcDriver or LogDestDriver will derive from LogDriver */
static void
log_driver_free(LogPipe *s)
{
  LogDriver *self = (LogDriver *) s;
  GList *l;
  
  for (l = self->plugins; l; l = l->next)
    {
      log_driver_plugin_free((LogDriverPlugin *) l->data);
    }
  log_pipe_unref(&self->drv_next->super);
  self->drv_next = NULL;
  if (self->group)
    g_free(self->group);
  if (self->id)
    g_free(self->id);
  log_pipe_free_method(s);
}

/* NOTE: intentionally static, as only LogSrcDriver or LogDestDriver will derive from LogDriver */
static void
log_driver_init_instance(LogDriver *self)
{
  log_pipe_init_instance(&self->super);
  self->super.free_fn = log_driver_free;
  self->super.init = log_driver_init_method;
  self->super.deinit = log_driver_deinit_method;
}

/* LogSrcDriver */

void
log_src_driver_init_instance(LogSrcDriver *self)
{
  log_driver_init_instance(&self->super);
}

void
log_src_driver_free(LogPipe *s)
{
  log_driver_free(s);
}

/* LogDestDriver */

/* returns a reference */
static LogQueue *
log_dest_driver_acquire_queue_method(LogDestDriver *self, gchar *persist_name, gpointer user_data)
{
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super);
  LogQueue *queue = NULL;

  g_assert(user_data == NULL);

  if (persist_name)
    queue = cfg_persist_config_fetch(cfg, persist_name);

  if (!queue)
    {
      queue = log_queue_fifo_new(self->log_fifo_size < 0 ? cfg->log_fifo_size : self->log_fifo_size, persist_name);
      log_queue_set_throttle(queue, self->throttle);
    }
  return queue;
}

/* consumes the reference in @q */
static void
log_dest_driver_release_queue_method(LogDestDriver *self, LogQueue *q, gpointer user_data)
{
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super);

  /* we only save the LogQueue instance if it contains data */
  if (q->persist_name && log_queue_keep_on_reload(q) > 0)
    cfg_persist_config_add(cfg, q->persist_name, q, (GDestroyNotify) log_queue_unref, FALSE);
  else
    log_queue_unref(q);
}

gboolean
log_dest_driver_deinit_method(LogPipe *s)
{
  LogDestDriver *self = (LogDestDriver *) s;
  GList *l, *l_next;

  for (l = self->queues; l; l = l_next)
    {
      LogQueue *q = (LogQueue *) l->data;

      /* the GList struct will be freed by log_dest_driver_release_queue */
      l_next = l->next;

      /* we have to pass a reference to log_dest_driver_release_queue(),
       * which automatically frees the ref on the list too */
      log_dest_driver_release_queue(self, log_queue_ref(q));
    }
  g_assert(self->queues == NULL);

  if (!log_driver_deinit_method(s))
    return FALSE;
  return TRUE;
}

void
log_dest_driver_init_instance(LogDestDriver *self)
{
  log_driver_init_instance(&self->super);
  self->acquire_queue = log_dest_driver_acquire_queue_method;
  self->release_queue = log_dest_driver_release_queue_method;
  self->log_fifo_size = -1;
  self->throttle = 0;
}

void
log_dest_driver_free(LogPipe *s)
{
  LogDestDriver *self = (LogDestDriver *) s;
  GList *l;

  for (l = self->queues; l; l = l->next)
    {
      log_queue_unref((LogQueue *) l->data);
    }
  g_list_free(self->queues);
  log_driver_free(s);
}
