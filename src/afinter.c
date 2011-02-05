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

#include "afinter.h"
#include "logreader.h"
#include "stats.h"
#include "messages.h"

typedef struct _AFInterSourceDriver
{
  LogDriver super;
  LogSource *source;
  LogSourceOptions source_options;
} AFInterSourceDriver;

static glong next_mark_target = -1;

void 
afinter_postpone_mark(gint mark_freq)
{
  if (mark_freq > 0)
    {
      GTimeVal tv;
      cached_g_current_time(&tv);
      next_mark_target = tv.tv_sec + mark_freq;
    }
}

typedef struct _AFInterWatch
{
  GSource super;
  LogSource *afinter_source;
  gint mark_freq;
} AFInterWatch;

static gboolean
afinter_source_prepare(GSource *source, gint *timeout)
{
  AFInterWatch *self = (AFInterWatch *) source;
  GTimeVal tv;
  
  *timeout = -1;

  if (!log_source_free_to_send(self->afinter_source))
    return FALSE;

  if (self->mark_freq > 0 && next_mark_target == -1)
    {
      g_source_get_current_time(source, &tv);
      next_mark_target = tv.tv_sec + self->mark_freq;
    }
    
  if (next_mark_target != -1)
    {
      g_source_get_current_time(source, &tv);
      *timeout = MAX((next_mark_target - tv.tv_sec) * 1000, 0);
    }
  else
    {
      *timeout = -1;
    }
  return msg_queue_length(internal_msg_queue) > 0;
}

static gboolean
afinter_source_check(GSource *source)
{
  AFInterWatch *self = (AFInterWatch *) source;
  GTimeVal tv;

  if (!log_source_free_to_send(self->afinter_source))
    return FALSE;

  g_source_get_current_time(source, &tv);
  
  if (next_mark_target != -1 && next_mark_target <= tv.tv_sec)
    return TRUE;
  return msg_queue_length(internal_msg_queue) > 0;
}

static gboolean
afinter_source_dispatch(GSource *source,
                        GSourceFunc callback,
                        gpointer user_data)
{
  AFInterWatch *self = (AFInterWatch *) source;
  LogMessage *msg;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  GTimeVal tv;

  if (!log_source_free_to_send(self->afinter_source))
    return FALSE;
  
  g_source_get_current_time(source, &tv);
  
  if (next_mark_target != -1 && next_mark_target <= tv.tv_sec)
    {
      msg = log_msg_new_mark();
      path_options.flow_control = FALSE;
    }
  else
    {
      msg = msg_queue_pop(internal_msg_queue);
    }


  if (msg)
    ((void (*)(LogPipe *, LogMessage *, const LogPathOptions *))callback) ((LogPipe *) user_data, msg, &path_options);
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
afinter_source_dispatch_msg(LogPipe *pipe, LogMessage *msg, const LogPathOptions *path_options)
{
  log_pipe_queue(pipe, msg, path_options);
}

static inline GSource *
afinter_source_watch_new(LogSource *afinter_source, gint mark_freq)
{
  AFInterWatch *self = (AFInterWatch *) g_source_new(&afinter_source_watch_funcs, sizeof(AFInterWatch));
  
  next_mark_target = -1;
  self->mark_freq = mark_freq;
  self->afinter_source = afinter_source;
  g_source_set_callback(&self->super, (GSourceFunc) afinter_source_dispatch_msg, log_pipe_ref(&afinter_source->super), (GDestroyNotify) log_pipe_unref);
  return &self->super;
}

typedef struct _AFInterSource
{
  LogSource super;
  GSource *watch;
} AFInterSource;

static gboolean
afinter_source_init(LogPipe *s)
{
  AFInterSource *self = (AFInterSource *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);
  
  if (!log_source_init(s))
    return FALSE;
  
  /* the source added below references this logreader, it will be unref'd
     when the source is destroyed */ 
  self->watch = afinter_source_watch_new(&self->super, cfg->mark_freq);
  g_source_attach(self->watch, NULL);
  return TRUE;
}

static gboolean
afinter_source_deinit(LogPipe *s)
{
  AFInterSource *self = (AFInterSource *) s;
  
  if (self->watch)
    {
      g_source_destroy(self->watch);
      g_source_unref(self->watch);
      self->watch = NULL;
    }
  return log_source_deinit(s);
}

static LogSource *
afinter_source_new(AFInterSourceDriver *owner, LogSourceOptions *options)
{
  AFInterSource *self = g_new0(AFInterSource, 1);
  
  log_source_init_instance(&self->super);
  log_source_set_options(&self->super, options, 0, SCS_INTERNAL, owner->super.id, NULL);
  self->super.super.init = afinter_source_init;
  self->super.super.deinit = afinter_source_deinit;
  return &self->super;
}


static gboolean
afinter_sd_init(LogPipe *s)
{
  AFInterSourceDriver *self = (AFInterSourceDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  log_source_options_init(&self->source_options, cfg, self->super.group);
  self->source = afinter_source_new(self, &self->source_options);
  log_pipe_append(&self->source->super, s);
  log_pipe_init(&self->source->super, cfg);
  return TRUE;
}

static gboolean
afinter_sd_deinit(LogPipe *s)
{
  AFInterSourceDriver *self = (AFInterSourceDriver *) s;

  if (self->source)
    {
      log_pipe_deinit(&self->source->super);
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
  log_drv_free(s);
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

