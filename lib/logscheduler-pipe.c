/*
 * Copyright (c) 2023 Balázs Scheidler <balazs.scheidler@axoflow.com>
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
#include "logscheduler-pipe.h"

LogSchedulerOptions *
log_scheduler_pipe_get_scheduler_options(LogPipe *s)
{
  LogSchedulerPipe *self = (LogSchedulerPipe *) s;
  return &self->scheduler_options;
}

static gboolean
_init(LogPipe *s)
{
  LogSchedulerPipe *self = (LogSchedulerPipe *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!log_scheduler_options_init(&self->scheduler_options, cfg))
    return FALSE;

  if (!self->scheduler)
    self->scheduler = log_scheduler_new(&self->scheduler_options, self->super.pipe_next);

  log_scheduler_init(self->scheduler);

  return TRUE;
}

static gboolean
_deinit(LogPipe *s)
{
  LogSchedulerPipe *self = (LogSchedulerPipe *) s;

  log_scheduler_deinit(self->scheduler);
  return TRUE;
}

static void
_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  LogSchedulerPipe *self = (LogSchedulerPipe *) s;

  log_scheduler_push(self->scheduler, msg, path_options);
}

static void
_free(LogPipe *s)
{
  LogSchedulerPipe *self = (LogSchedulerPipe *) s;

  if (self->scheduler)
    log_scheduler_free(self->scheduler);
}

LogPipe *
log_scheduler_pipe_new(GlobalConfig *cfg)
{
  LogSchedulerPipe *self = g_new0(LogSchedulerPipe, 1);

  log_pipe_init_instance(&self->super, cfg);
  self->super.init = _init;
  self->super.deinit = _deinit;
  self->super.queue = _queue;
  self->super.free_fn = _free;
  log_scheduler_options_defaults(&self->scheduler_options);
  log_pipe_add_info(&self->super, "scheduler");
  return &self->super;
}
