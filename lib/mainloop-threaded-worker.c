/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
 * Copyright (c) 2022 Balázs Scheidler <bazsi77@gmail.com>
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
#include "mainloop-threaded-worker.h"
#include "mainloop-call.h"

#include <iv.h>

static void
_signal_startup_finished(MainLoopThreadedWorker *self, gboolean startup_result)
{
  g_mutex_lock(&self->lock);
  self->startup.finished = TRUE;
  self->startup.result &= startup_result;
  g_cond_signal(&self->startup.cond);
  g_mutex_unlock(&self->lock);
}

static gboolean
_thread_init(MainLoopThreadedWorker *self)
{
  gboolean result = TRUE;

  main_loop_worker_thread_start(self->worker_type);

  if (self->thread_init)
    result = self->thread_init(self);

  _signal_startup_finished(self, result);
  return result;
}

static void
_thread_deinit(MainLoopThreadedWorker *self)
{
  if (self->thread_deinit)
    self->thread_deinit(self);
  main_loop_call((MainLoopTaskFunc) main_loop_worker_job_complete, NULL, TRUE);
  main_loop_worker_thread_stop();
}

static void
_request_worker_exit(gpointer st)
{
  MainLoopThreadedWorker *self = st;

  return self->request_exit(self);
}

static gpointer
_worker_thread_func(gpointer st)
{
  MainLoopThreadedWorker *self = st;

  iv_init();

  if (_thread_init(self))
    self->run(self);
  _thread_deinit(self);

  iv_deinit();

  /* NOTE: this assert aims to validate that the worker thread in fact
   * invokes main_loop_worker_invoke_batch_callbacks() during its operation.
   * Please do so every once a couple of messages, hopefully you have a
   * natural barrier that lets you decide when, the easiest would be
   * log-fetch-limit(), but other limits may also be applicable.
   */
  main_loop_worker_assert_batch_callbacks_were_processed();

  return NULL;
}

static gboolean
_wait_for_startup_finished(MainLoopThreadedWorker *self)
{
  g_mutex_lock(&self->lock);
  while (!self->startup.finished)
    g_cond_wait(&self->startup.cond, &self->lock);
  g_mutex_unlock(&self->lock);
  return self->startup.result;
}

gboolean
main_loop_threaded_worker_start(MainLoopThreadedWorker *self)
{
  /* NOTE: we can only start up once */
  g_assert(!self->startup.finished);

  self->startup.result = TRUE;
  main_loop_assert_main_thread();
  main_loop_worker_job_start();
  main_loop_worker_register_exit_notification_callback(_request_worker_exit, self);
  self->thread = g_thread_new(NULL, _worker_thread_func, self);
  return _wait_for_startup_finished(self);
}


void
main_loop_threaded_worker_init(MainLoopThreadedWorker *self,
                               MainLoopWorkerType worker_type, gpointer data)
{
  self->worker_type = worker_type;
  g_cond_init(&self->startup.cond);
  g_mutex_init(&self->lock);

  self->data = data;

  self->startup.finished = FALSE;
  self->startup.result = FALSE;
}

void
main_loop_threaded_worker_clear(MainLoopThreadedWorker *self)
{
  /* by the time main_loop_threaded_worker_clear() is called, the mainloop
   * should have terminated the thread with its request_exit() method.  The
   * mainloop also ensures that these threads actually exit.  If the code
   * hangs on this g_thread_join(), your worker probably did not respond to
   * the request_exit() call or there's a bug in mainloop.
   *
   * With that in mind, g_thread_join() would return immediately and the
   * only side effect is that it drops the reference to self->thread.
   */

  if (self->thread)
    g_thread_join(self->thread);
  g_cond_clear(&self->startup.cond);
  g_mutex_clear(&self->lock);
}
