/*
 * Copyright (c) 2002, 2003, 2004 BalaBit IT Ltd, Budapest, Hungary
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

#include "afinter.h"
#include "logreader.h"

#include "messages.h"

extern GQueue *internal_msg_queue;

static gboolean
afinter_source_prepare(GSource *source G_GNUC_UNUSED, gint *timeout)
{
  *timeout = -1;
  return !g_queue_is_empty(internal_msg_queue);
}

static gboolean
afinter_source_check(GSource *source G_GNUC_UNUSED)
{
  return !g_queue_is_empty(internal_msg_queue);
}

static gboolean
afinter_source_dispatch(GSource *source,
                        GSourceFunc callback,
                        gpointer user_data)
{
  LogMessage *msg = g_queue_pop_head(internal_msg_queue);
  if (msg)
    ((void (*)(LogPipe *, LogMessage *))callback) ((LogPipe *) user_data, msg);
  return TRUE;
}

static void
afinter_source_finalize(GSource *source)
{
}

GSourceFuncs afinter_source_watch_funcs =
{
  afinter_source_prepare,
  afinter_source_check,
  afinter_source_dispatch,
  afinter_source_finalize
};

static void
afinter_source_dispatch_msg(LogPipe *pipe, LogMessage *msg)
{
  log_pipe_queue(pipe, msg, 0);
}

static inline GSource *
afinter_source_watch_new(LogPipe *pipe)
{
  GSource *self = g_source_new(&afinter_source_watch_funcs, sizeof(GSource));
  g_source_set_callback(self, (GSourceFunc) afinter_source_dispatch_msg, log_pipe_ref(pipe), (GDestroyNotify) log_pipe_unref);
  return self;
}

typedef struct _AFInterSource
{
  LogSource super;
  GSource *watch;
} AFInterSource;

static gboolean
afinter_source_init(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFInterSource *self = (AFInterSource *) s;
  
  /* the source added below references this logreader, it will be unref'd
     when the source is destroyed */ 
  self->watch = afinter_source_watch_new(&self->super.super);
  g_source_attach(self->watch, NULL);
  return TRUE;
}

static gboolean
afinter_source_deinit(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFInterSource *self = (AFInterSource *) s;
  
  if (self->watch)
    {
      g_source_destroy(self->watch);
      g_source_unref(self->watch);
      self->watch = NULL;
    }
  return TRUE;
}

static LogSource *
afinter_source_new(LogSourceOptions *options)
{
  AFInterSource *self = g_new0(AFInterSource, 1);
  
  log_source_init_instance(&self->super, options);
  self->super.super.init = afinter_source_init;
  self->super.super.deinit = afinter_source_deinit;
  return &self->super;
}

typedef struct _AFInterSourceDriver
{
  LogDriver super;
  LogSource *source;
  LogSourceOptions source_options;
} AFInterSourceDriver;

static gboolean
afinter_sd_init(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFInterSourceDriver *self = (AFInterSourceDriver *) s;

  log_source_options_init(&self->source_options, cfg);
  self->source = afinter_source_new(&self->source_options);
  log_pipe_append(&self->source->super, s);
  log_pipe_init(&self->source->super, NULL, NULL);
  return TRUE;
}

static gboolean
afinter_sd_deinit(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFInterSourceDriver *self = (AFInterSourceDriver *) s;
  
  if (self->source)
    {
      log_pipe_deinit(&self->source->super, NULL, NULL);
      /* break circular reference created during _init */
      log_pipe_unref(&self->source->super);
      self->source = NULL;
    }
  return TRUE;
}

static void
afinter_sd_free(LogPipe *s)
{
  AFInterSourceDriver *self = (AFInterSourceDriver *) s;
  
  g_assert(!self->source);
  log_drv_free_instance(&self->super);
  g_free(self);
}

LogDriver *
afinter_sd_new(void)
{
  AFInterSourceDriver *self = g_new0(AFInterSourceDriver, 1);

  log_drv_init_instance(&self->super);
  self->super.super.init = afinter_sd_init;
  self->super.super.deinit = afinter_sd_deinit;
  self->super.super.free_fn = afinter_sd_free;
  log_source_options_defaults(&self->source_options);
  return &self->super;
}

