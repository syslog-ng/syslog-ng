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

struct _Tracer
{
  GQueue *waiting_breakpoints;
  GMutex breakpoint_mutex;
  GCond breakpoint_cond;

  /* this condition variable is shared between all breakpoints, but each of
   * them has its own "resume_requested" variable, this means that all
   * resumes will wake up all waiting breakpoints, but only one of them will
   * actually resume, it's not the most efficient implementation, but avoids
   * us having to free the GCond instance as a thread is terminated. */

  GCond resume_cond;
  BreakpointSite *pending_breakpoint;
  gboolean cancel_requested;
};

/* NOTE: called by workers to stop on a breakpoint, wait for the debugger to
 * do its stuff and return to continue */
void
tracer_stop_on_breakpoint(Tracer *self, BreakpointSite *breakpoint_site)
{
  g_mutex_lock(&self->breakpoint_mutex);

  if (self->cancel_requested)
    goto exit;
  breakpoint_site->resume_requested = FALSE;
  /* send break point */
  g_queue_push_tail(self->waiting_breakpoints, breakpoint_site);
  g_cond_signal(&self->breakpoint_cond);

  /* wait for resume or cancel */
  while (!(breakpoint_site->resume_requested || self->cancel_requested))
    g_cond_wait(&self->resume_cond, &self->breakpoint_mutex);

exit:
  g_mutex_unlock(&self->breakpoint_mutex);
}

void
tracer_stop_on_interrupt(Tracer *self)
{
  g_mutex_lock(&self->breakpoint_mutex);
  /* send interrupt signal as a NULL */
  g_queue_push_tail(self->waiting_breakpoints, NULL);
  g_cond_signal(&self->breakpoint_cond);
  g_mutex_unlock(&self->breakpoint_mutex);
}

/* NOTE: called by the interactive debugger to wait for a breakpoint to
 * trigger, a return of FALSE indicates that the tracing was cancelled */
gboolean
tracer_wait_for_event(Tracer *self, BreakpointSite **breakpoint_site)
{
  gboolean cancelled = FALSE;
  g_mutex_lock(&self->breakpoint_mutex);
  while (g_queue_is_empty(self->waiting_breakpoints) && !self->cancel_requested)
    g_cond_wait(&self->breakpoint_cond, &self->breakpoint_mutex);

  g_assert(self->pending_breakpoint == NULL);
  *breakpoint_site = NULL;
  if (!self->cancel_requested)
    {
      *breakpoint_site = self->pending_breakpoint = g_queue_pop_head(self->waiting_breakpoints);
    }
  else
    {
      cancelled = TRUE;

      /* cancel out threads waiting on breakpoint, e.g.  in the cancelled
       * case no need to call tracer_resume_after_breakpoint() */
      g_cond_broadcast(&self->resume_cond);
    }
  g_mutex_unlock(&self->breakpoint_mutex);
  return !cancelled;
}

/* NOTE: called by the interactive debugger to resume the worker after a breakpoint */
void
tracer_resume_after_event(Tracer *self, BreakpointSite *breakpoint_site)
{
  g_mutex_lock(&self->breakpoint_mutex);
  g_assert(self->pending_breakpoint == breakpoint_site);
  if (self->pending_breakpoint)
    {
      /* we might be returning from an interrupt in which case
       * pending_breakpoint is NULL, nothing to resume */

      self->pending_breakpoint->resume_requested = TRUE;
      self->pending_breakpoint = NULL;
      g_cond_broadcast(&self->resume_cond);
    }
  g_mutex_unlock(&self->breakpoint_mutex);
}

/* NOTE: called by any thread, not necessarily the debugger thread or worker
 * threads.  It cancels out the tracer_wait_for_breakpoint() calls */
void
tracer_cancel(Tracer *self)
{
  g_mutex_lock(&self->breakpoint_mutex);
  self->cancel_requested = TRUE;
  g_cond_signal(&self->breakpoint_cond);
  g_mutex_unlock(&self->breakpoint_mutex);
}

Tracer *
tracer_new(GlobalConfig *cfg)
{
  Tracer *self = g_new0(Tracer, 1);

  g_mutex_init(&self->breakpoint_mutex);
  g_cond_init(&self->breakpoint_cond);
  g_cond_init(&self->resume_cond);
  self->waiting_breakpoints = g_queue_new();
  return self;
}

void
tracer_free(Tracer *self)
{
  g_mutex_clear(&self->breakpoint_mutex);
  g_cond_clear(&self->breakpoint_cond);
  g_cond_clear(&self->resume_cond);
  g_queue_free(self->waiting_breakpoints);
  g_free(self);
}
