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
  
#ifndef DGROUP_H_INCLUDED
#define DGROUP_H_INCLUDED

#include "syslog-ng.h"
#include "logpipe.h"
#include "driver.h"
#include "stats.h"

typedef struct _LogDestGroup
{
  LogPipe super;
  gchar *name;
  LogDriver *drivers;
  StatsCounterItem *processed_messages;
} LogDestGroup;

static inline LogDestGroup *
log_dest_group_ref(LogDestGroup *self)
{
  return (LogDestGroup *) log_pipe_ref(&self->super);
}

static inline void
log_dest_group_unref(LogDestGroup *self)
{
  log_pipe_unref(&self->super);
}

LogDestGroup *log_dest_group_new(gchar *name, LogDriver *drivers);

#endif
