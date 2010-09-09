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

void
log_drv_append(LogDriver *self, LogDriver *next)
{
  if (self->drv_next)
    log_drv_unref(self->drv_next);
  self->drv_next = log_drv_ref(next);
}

void
log_drv_init_instance(LogDriver *self)
{
  log_pipe_init_instance(&self->super);
  self->super.free_fn = log_drv_free;
}

void
log_drv_free(LogPipe *s)
{
  LogDriver *self = (LogDriver *) s;
  
  log_drv_unref(self->drv_next);
  self->drv_next = NULL;
  if (self->group)
    g_free(self->group);
  if (self->id)
    g_free(self->id);
  log_pipe_free(s);
}

