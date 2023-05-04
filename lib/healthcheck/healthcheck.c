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
#include "stopwatch.h"

struct _HealthCheck
{
  GAtomicCounter ref_cnt;
  gboolean running;
  HealthCheckResult result;

  HealthCheckCompletionCB completion;
  gpointer user_data;

  struct
  {
    MainLoopIOWorkerJob job;
    Stopwatch stopwatch;
  } io_worker_latency;
};

static void
healthcheck_incomplete(HealthCheck *self)
{
  self->running = FALSE;
  self->completion = NULL;
  self->user_data = NULL;
  healthcheck_unref(self);
}

static void
healthcheck_complete(HealthCheck *self)
{
  self->running = FALSE;
  self->completion(self->result, self->user_data);

  self->completion = NULL;
  self->user_data = NULL;
  healthcheck_unref(self);
}

static inline gboolean
_start_io_worker_latency(HealthCheck *self)
{
  stopwatch_start(&self->io_worker_latency.stopwatch);

  if (!main_loop_io_worker_job_submit(&self->io_worker_latency.job, NULL))
    {
      /* currently, the IO worker check is our only health check */
      healthcheck_incomplete(self);
      return FALSE;
    }

  return TRUE;
}

static gboolean
_start_health_checks(HealthCheck *self)
{
  g_assert(!self->running);
  self->running = TRUE;

  return _start_io_worker_latency(self);
}

gboolean
healthcheck_run(HealthCheck *self, HealthCheckCompletionCB completion, gpointer user_data)
{
  if (self->running || !completion)
    return FALSE;

  self->completion = completion;
  self->user_data = user_data;
  self->result = (HealthCheckResult)
  {
    0
  };
  healthcheck_ref(self);

  return _start_health_checks(self);
}

static void
_io_worker_latency(gpointer s, gpointer arg)
{
  HealthCheck *self = (HealthCheck *) s;

  self->result.io_worker_latency = stopwatch_get_elapsed_nsec(&self->io_worker_latency.stopwatch);
}

static void
_io_worker_latency_finished(gpointer s, gpointer arg)
{
  HealthCheck *self = (HealthCheck *) s;

  self->result.mainloop_io_worker_roundtrip_latency = stopwatch_get_elapsed_nsec(&self->io_worker_latency.stopwatch);

  /* currently, the IO worker check is our only health check */
  healthcheck_complete(self);
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

  main_loop_io_worker_job_init(&self->io_worker_latency.job);
  self->io_worker_latency.job.user_data = self;
  self->io_worker_latency.job.work = _io_worker_latency;
  self->io_worker_latency.job.completion = _io_worker_latency_finished;
  self->io_worker_latency.job.engage = (void (*)(gpointer)) healthcheck_ref;
  self->io_worker_latency.job.release = (void (*)(gpointer)) healthcheck_unref;

  return self;
}
