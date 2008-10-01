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

