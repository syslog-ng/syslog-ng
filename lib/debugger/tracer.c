/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Balázs Scheidler
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
#include "debugger/tracer.h"

void
tracer_stop_on_breakpoint(Tracer *self)
{
  g_mutex_lock(&self->breakpoint_mutex);

  /* send break point */
  self->breakpoint_hit = TRUE;
  g_cond_signal(&self->breakpoint_cond);

  /* wait for resume */
  while (!self->resume_requested)
    g_cond_wait(&self->resume_cond, &self->breakpoint_mutex);
  self->resume_requested = FALSE;
  g_mutex_unlock(&self->breakpoint_mutex);
}

void
tracer_wait_for_breakpoint(Tracer *self)
{
  g_mutex_lock(&self->breakpoint_mutex);
  while (!self->breakpoint_hit)
    g_cond_wait(&self->breakpoint_cond, &self->breakpoint_mutex);
  self->breakpoint_hit = FALSE;
  g_mutex_unlock(&self->breakpoint_mutex);
}

void
tracer_resume_after_breakpoint(Tracer *self)
{
  g_mutex_lock(&self->breakpoint_mutex);
  self->resume_requested = TRUE;
  g_cond_signal(&self->resume_cond);
  g_mutex_unlock(&self->breakpoint_mutex);
}

Tracer *
tracer_new(GlobalConfig *cfg)
{
  Tracer *self = g_new0(Tracer, 1);

  g_mutex_init(&self->breakpoint_mutex);
  g_cond_init(&self->breakpoint_cond);
  g_cond_init(&self->resume_cond);

  return self;
}

void
tracer_free(Tracer *self)
{
  g_mutex_clear(&self->breakpoint_mutex);
  g_cond_clear(&self->breakpoint_cond);
  g_cond_clear(&self->resume_cond);
  g_free(self);
}
