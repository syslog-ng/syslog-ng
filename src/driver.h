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
  
#ifndef DRIVER_H_INCLUDED
#define DRIVER_H_INCLUDED

#include "syslog-ng.h"
#include "logpipe.h"
#include "cfg.h"

typedef struct _LogDriver
{
  LogPipe super;
  gboolean optional;
  gchar *group;
  gchar *id;
  struct _LogDriver *drv_next;
} LogDriver;


void log_drv_append(LogDriver *self, LogDriver *next);
void log_drv_set_next_hop(LogDriver *self, LogPipe *next_hop);
void log_drv_set_fifo_size(LogDriver *self, gint fifo_size);

void log_drv_init_instance(LogDriver *self);
void log_drv_free(LogPipe *self);

static inline LogDriver *
log_drv_ref(LogDriver *s)
{
  return (LogDriver *) log_pipe_ref(&s->super);
}

static inline void
log_drv_unref(LogDriver *s)
{
  log_pipe_unref(&s->super);
}

#endif
