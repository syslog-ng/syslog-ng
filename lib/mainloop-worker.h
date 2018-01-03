/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
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
#ifndef MAINLOOP_WORKER_H_INCLUDED
#define MAINLOOP_WORKER_H_INCLUDED 1

#include "mainloop.h"

#include <iv_list.h>

#define MAIN_LOOP_MIN_WORKER_THREADS 2
#define MAIN_LOOP_MAX_WORKER_THREADS 64


/*
 * A batch callback is registered during the processing of messages in a
 * given thread.  Batch callbacks are invoked when the batch is complete.
 *
 * This mechanism is used by LogQueueFifo implementation, that consumes
 * messages into a per-thread, unlocked queue first and once the whole batch
 * is in, it grabs a lock and propagates the elements towards the consumer.
 */

typedef struct _WorkerBatchCallback
{
  struct iv_list_head list;
  MainLoopTaskFunc func;
  gpointer user_data;
} WorkerBatchCallback;

typedef struct _WorkerOptions
{
  gboolean is_output_thread;
  gboolean is_external_input;
} WorkerOptions;

static inline void
worker_batch_callback_init(WorkerBatchCallback *self)
{
  INIT_IV_LIST_HEAD(&self->list);
}

void main_loop_worker_register_batch_callback(WorkerBatchCallback *cb);
void main_loop_worker_invoke_batch_callbacks(void);

typedef void (*WorkerThreadFunc)(gpointer user_data);
typedef void (*WorkerExitNotificationFunc)(gpointer user_data);

void main_loop_worker_set_thread_id(gint id);
gint main_loop_worker_get_thread_id(void);

void main_loop_worker_job_start(void);
void main_loop_worker_job_complete(void);

void main_loop_worker_thread_start(void *cookie);
void main_loop_worker_thread_stop(void);
void main_loop_worker_run_gc(void);

void main_loop_create_worker_thread(WorkerThreadFunc func, WorkerExitNotificationFunc terminate_func, gpointer data,
                                    WorkerOptions *worker_options);

void main_loop_worker_sync_call(void (*func)(void *user_data), void *user_data);

void main_loop_worker_init(void);
void main_loop_worker_deinit(void);

extern volatile gboolean main_loop_workers_quit;
extern volatile gboolean is_reloading_scheduled;

static inline gboolean
main_loop_worker_job_quit(void)
{
  return main_loop_workers_quit;
}


#endif
