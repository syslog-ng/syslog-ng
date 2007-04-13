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

typedef struct _AFInterSourceDriver
{
  LogDriver super;
  LogPipe *reader;
  LogReaderOptions reader_options;
} AFInterSourceDriver;

static gboolean
afinter_sd_init(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFInterSourceDriver *self = (AFInterSourceDriver *) s;

  log_reader_options_init(&self->reader_options, cfg);
  if (msg_pipe[0] != -1)
    {
      self->reader = log_reader_new(fd_read_new(msg_pipe[0], FR_DONTCLOSE), LR_INTERNAL, s, &self->reader_options);
      log_pipe_append(self->reader, s);
      log_pipe_init(self->reader, NULL, NULL);
    }
  return TRUE;
}

static gboolean
afinter_sd_deinit(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFInterSourceDriver *self = (AFInterSourceDriver *) s;
  
  if (self->reader)
    {
      log_pipe_deinit(self->reader, NULL, NULL);
      /* break circular reference created during _init */
      log_pipe_unref(self->reader);
      self->reader = NULL;
    }
  return TRUE;
}

static void
afinter_sd_free(LogPipe *s)
{
  AFInterSourceDriver *self = (AFInterSourceDriver *) s;
  
  g_assert(!self->reader);
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
  log_reader_options_defaults(&self->reader_options);
  return &self->super;
}

