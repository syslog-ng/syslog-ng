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

typedef struct _MainLoopThreadedWorker
{
  WorkerThreadFunc func;
  gpointer data;
  MainLoopWorkerType worker_type;
} MainLoopThreadedWorker;

static gpointer
_worker_thread_func(gpointer st)
{
  MainLoopThreadedWorker *p = st;

  iv_init();
  main_loop_worker_thread_start(p->worker_type);
  p->func(p->data);
  main_loop_call((MainLoopTaskFunc) main_loop_worker_job_complete, NULL, TRUE);
  main_loop_worker_thread_stop();
  iv_deinit();

  /* NOTE: this assert aims to validate that the worker thread in fact
   * invokes main_loop_worker_invoke_batch_callbacks() during its operation.
   * Please do so every once a couple of messages, hopefully you have a
   * natural barrier that lets you decide when, the easiest would be
   * log-fetch-limit(), but other limits may also be applicable.
   */
  main_loop_worker_assert_batch_callbacks_were_processed();

  g_free(st);
  return NULL;
}

void
main_loop_create_worker_thread(WorkerThreadFunc func, WorkerExitNotificationFunc terminate_func, gpointer data,
                               MainLoopWorkerType worker_type)
{
  MainLoopThreadedWorker *p;

  main_loop_assert_main_thread();

  p = g_new0(MainLoopThreadedWorker, 1);
  p->func = func;
  p->data = data;
  p->worker_type = worker_type;

  main_loop_worker_job_start();
  if (terminate_func)
    main_loop_worker_register_exit_notification_callback(terminate_func, data);
  g_thread_new(NULL, _worker_thread_func, p);
}
