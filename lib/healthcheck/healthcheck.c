/*
 * Copyright (c) 2023 László Várady <laszlo.varady93@gmail.com>
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

#include "healthcheck.h"

#include "atomic.h"
#include "mainloop-io-worker.h"

struct _HealthCheck
{
  GAtomicCounter ref_cnt;
  gboolean running;
  HealthCheckResult result;

  HealthCheckCompletionCB completion;
  gpointer user_data;
};

gboolean
healthcheck_run(HealthCheck *self, HealthCheckCompletionCB completion, gpointer user_data)
{
  if (self->running || !completion)
    return FALSE;

  self->completion = completion;
  self->user_data = user_data;
  self->result = (HealthCheckResult){0};

  return TRUE;
}

static void
healthcheck_free(HealthCheck *self)
{
  g_assert(!self->running);
  g_free(self);
}

HealthCheck *
healthcheck_ref(HealthCheck *self)
{
  if (!self)
    return NULL;

  g_atomic_counter_inc(&self->ref_cnt);
  return self;
}

void
healthcheck_unref(HealthCheck *self)
{
  if (!self)
    return;

  if (g_atomic_counter_dec_and_test(&self->ref_cnt))
    healthcheck_free(self);
}

HealthCheck *
healthcheck_new(void)
{
  HealthCheck *self = g_new0(HealthCheck, 1);
  g_atomic_counter_set(&self->ref_cnt, 1);

  self->running = FALSE;

  return self;
}
