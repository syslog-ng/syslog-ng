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
#include "mainloop-worker.h"
#include "mainloop-call.h"
#include "tls-support.h"
#include "apphook.h"
#include "messages.h"
#include "scratch-buffers.h"
#include "atomic.h"

#include <iv.h>

TLS_BLOCK_START
{
  /* Thread IDs are low numbered integers that can be used to index
   * per-thread data in an array.  IDs get reused and the smallest possible
   * ID is allocated for newly started threads.  */

  /* the thread id is shifted by one, to make 0 the uninitialized state,
   * e.g. everything that sets it adds +1, everything that queries it
   * subtracts 1 */
  gint main_loop_worker_id;
  MainLoopWorkerType main_loop_worker_type;
  struct iv_list_head batch_callbacks;
}
TLS_BLOCK_END;

#define main_loop_worker_id __tls_deref(main_loop_worker_id)
#define batch_callbacks    __tls_deref(batch_callbacks)
#define main_loop_worker_type __tls_deref(main_loop_worker_type)

GQueue sync_call_actions = G_QUEUE_INIT;

/* cause workers to stop, no new I/O jobs to be submitted */
volatile gboolean main_loop_workers_quit;
volatile gboolean is_reloading_scheduled;

/* number of I/O worker jobs running */
static GAtomicCounter main_loop_jobs_running;

static struct iv_task main_loop_workers_reenable_jobs_task;

/* thread ID allocation */
static GMutex main_loop_workers_idmap_lock;

#define MAIN_LOOP_IDMAP_BITS_PER_ROW    (sizeof(guint64)*8)
#define MAIN_LOOP_IDMAP_ROWS            (MAIN_LOOP_MAX_WORKER_THREADS / MAIN_LOOP_IDMAP_BITS_PER_ROW)

static guint64 main_loop_workers_idmap[MAIN_LOOP_IDMAP_ROWS];
static gint main_loop_max_workers = 0;
static gint main_loop_estimated_number_of_workers = 0;

/* NOTE: return a zero based index for the current thread, to be used in
 * array indexes.  -1 means that the thread does not have an ID */
gint
main_loop_worker_get_thread_index(void)
{
  return main_loop_worker_id - 1;
}

static void
_allocate_thread_id(void)
{
  g_mutex_lock(&main_loop_workers_idmap_lock);

  /* the maximum number of threads must be dividible by 64, the array
   * main_loop_workers_idmap is sized accordingly, e.g.  the remainder could
   * not be represented in the array as is.  */

  G_STATIC_ASSERT((MAIN_LOOP_MAX_WORKER_THREADS % MAIN_LOOP_IDMAP_BITS_PER_ROW) == 0);

  main_loop_worker_id = 0;

  for (gint thread_index = 0; thread_index < MAIN_LOOP_MAX_WORKER_THREADS; thread_index++)
    {
      gint row = thread_index / MAIN_LOOP_IDMAP_BITS_PER_ROW;
      gint bit_in_row = thread_index % MAIN_LOOP_IDMAP_BITS_PER_ROW;

      if ((main_loop_workers_idmap[row] & (1ULL << bit_in_row)) == 0)
        {
          /* thread_index not yet used */

          main_loop_workers_idmap[row] |= (1ULL << bit_in_row);
          main_loop_worker_id = (thread_index + 1);
          break;
        }
    }
  g_mutex_unlock(&main_loop_workers_idmap_lock);

  if (main_loop_worker_id == 0)
    {
      msg_warning_once("Unable to allocate a unique thread ID. This can only "
                       "happen if the number of syslog-ng worker threads exceeds the "
                       "compile time constant MAIN_LOOP_MAX_WORKER_THREADS. "
                       "This is not a fatal problem but can be a cause for "
                       "decreased performance. Increase this number and recompile "
                       "or contact the syslog-ng authors",
                       evt_tag_int("max-worker-threads-hard-limit", MAIN_LOOP_MAX_WORKER_THREADS));
    }

  if (main_loop_worker_id > main_loop_max_workers)
    {
      msg_warning_once("The actual number of worker threads exceeds the number of threads "
                       "estimated at startup. This indicates a bug in thread estimation, "
                       "which is not fatal but could cause decreased performance. Please "
                       "contact the syslog-ng authors with your config to help troubleshoot "
                       "this issue",
                       evt_tag_int("worker-id", main_loop_worker_id),
                       evt_tag_int("max-worker-threads", main_loop_max_workers));
      main_loop_worker_id = 0;
    }
}

static void
_release_thread_id(void)
{
  g_mutex_lock(&main_loop_workers_idmap_lock);
  if (main_loop_worker_id)
    {
      const gint thread_index = main_loop_worker_get_thread_index();
      gint row = thread_index / MAIN_LOOP_IDMAP_BITS_PER_ROW;
      gint bit_in_row = thread_index % MAIN_LOOP_IDMAP_BITS_PER_ROW;

      main_loop_workers_idmap[row] &= ~(1ULL << (bit_in_row));
      main_loop_worker_id = 0;
    }
  g_mutex_unlock(&main_loop_workers_idmap_lock);
}

gboolean
main_loop_worker_is_worker_thread(void)
{
  return main_loop_worker_type > MLW_UNKNOWN;
}

typedef struct _WorkerExitNotification
{
  WorkerExitNotificationFunc func;
  gpointer user_data;
} WorkerExitNotification;

static GList *exit_notification_list = NULL;

void
main_loop_worker_register_exit_notification_callback(WorkerExitNotificationFunc func, gpointer user_data)
{
  WorkerExitNotification *cfunc = g_new(WorkerExitNotification, 1);

  cfunc->func = func;
  cfunc->user_data = user_data;

  exit_notification_list = g_list_append(exit_notification_list, cfunc);
}

static void
_invoke_worker_exit_callback(WorkerExitNotification *func)
{
  func->func(func->user_data);
}

static void
_request_all_threads_to_exit(void)
{
  g_list_foreach(exit_notification_list, (GFunc) _invoke_worker_exit_callback, NULL);
  g_list_foreach(exit_notification_list, (GFunc) g_free, NULL);
  g_list_free(exit_notification_list);
  exit_notification_list = NULL;
  main_loop_workers_quit = TRUE;
}

/* Call this function from worker threads, when you start up */
void
main_loop_worker_thread_start(MainLoopWorkerType worker_type)
{
  main_loop_worker_type = worker_type;

  _allocate_thread_id();
  INIT_IV_LIST_HEAD(&batch_callbacks);

  g_mutex_lock(&workers_running_lock);
  main_loop_workers_running++;
  g_mutex_unlock(&workers_running_lock);

  app_thread_start();
}

/* Call this function from worker threads, when you stop */
void
main_loop_worker_thread_stop(void)
{
  app_thread_stop();
  _release_thread_id();

  g_mutex_lock(&workers_running_lock);
  main_loop_workers_running--;
  g_cond_signal(&thread_halt_cond);
  g_mutex_unlock(&workers_running_lock);
}

void
main_loop_worker_run_gc(void)
{
  scratch_buffers_explicit_gc();
}

/*
 * This function is called in the main thread prior to starting the
 * processing of a work item in a worker thread.
 */
void
main_loop_worker_job_start(void)
{
  g_atomic_counter_inc(&main_loop_jobs_running);
}

typedef struct
{
  void (*func)(gpointer user_data);
  gpointer user_data;
} SyncCallAction;

void
_register_sync_call_action(GQueue *q, void (*func)(gpointer user_data), gpointer user_data)
{

  SyncCallAction *action = g_new0(SyncCallAction, 1);
  action->func = func;
  action->user_data = user_data;

  g_queue_push_tail(q, action);

}

void
_consume_action(SyncCallAction *action)
{
  action->func(action->user_data);
  g_free(action);
}

static void
_invoke_sync_call_actions(void)
{
  while (!g_queue_is_empty(&sync_call_actions))
    {
      SyncCallAction *action = g_queue_pop_head(&sync_call_actions);
      _consume_action(action);
    }
}

/*
 * This function is called in the main thread after a job was finished in
 * one of the worker threads.
 *
 * If an intrusive operation (reload, termination) is pending and the number
 * of workers has dropped to zero, it commences with the intrusive
 * operation, as in that case we can safely assume that all workers exited.
 */
void
main_loop_worker_job_complete(void)
{
  main_loop_assert_main_thread();

  gboolean reached_zero = g_atomic_counter_dec_and_test(&main_loop_jobs_running);
  if (main_loop_workers_quit && reached_zero)
    {
      /* NOTE: we can't reenable I/O jobs by setting
       * main_loop_io_workers_quit to FALSE right here, because a task
       * generated by the old config might still be sitting in the task
       * queue, to be invoked once we return from here.  Tasks cannot be
       * cancelled, thus we have to get to the end of the currently running
       * task queue.
       *
       * Thus we register another task
       * (&main_loop_io_workers_reenable_jobs_task), which is guaranteed to
       * be added to the end of the task queue, which reenables task
       * submission.
       *
       *
       * A second constraint is that any tasks submitted by the reload
       * logic (sitting behind the sync_func() call below), MUST be
       * registered after the reenable_jobs_task, because otherwise some
       * I/O events will be missed, due to main_loop_io_workers_quit being
       * TRUE.
       *
       *
       *   |OldTask1|OldTask2|OldTask3| ReenableTask |NewTask1|NewTask2|NewTask3|
       *   ^
       *   | ivykis task list
       *
       * OldTasks get dropped because _quit is TRUE, NewTasks have to be
       * executed properly, otherwise we'd hang.
       */

      iv_task_register(&main_loop_workers_reenable_jobs_task);
    }
}

/*
 * Register a function to be called back when the current I/O job is
 * finished (in the worker thread).
 *
 * NOTE: we only support one pending callback at a time, may become a list of callbacks if needed in the future
 */
void
main_loop_worker_register_batch_callback(WorkerBatchCallback *cb)
{
  iv_list_add(&cb->list, &batch_callbacks);
}

void
main_loop_worker_invoke_batch_callbacks(void)
{
  struct iv_list_head *lh, *lh2;

  iv_list_for_each_safe(lh, lh2, &batch_callbacks)
  {
    WorkerBatchCallback *cb = iv_list_entry(lh, WorkerBatchCallback, list);
    iv_list_del_init(&cb->list);

    cb->func(cb->user_data);
  }
}

void
main_loop_worker_assert_batch_callbacks_were_processed(void)
{
  g_assert(iv_list_empty(&batch_callbacks));
}

static void
_reenable_worker_jobs(void *s)
{
  _invoke_sync_call_actions();
  main_loop_workers_quit = FALSE;
  if (is_reloading_scheduled)
    msg_notice("Configuration reload finished");
  is_reloading_scheduled = FALSE;
}

void
main_loop_worker_sync_call(void (*func)(gpointer user_data), gpointer user_data)
{
  main_loop_assert_main_thread();

  _register_sync_call_action(&sync_call_actions, func, user_data);

  /*
   * This might seem racy as we are reading an atomic counter without
   * testing it for its zero value. This is safe, because:
   *
   *   - the only case we increment main_loop_jobs_running from the non-main
   *     thread is when we submit slave jobs to the worker pool
   *
   *   - slave jobs are submitted by worker jobs at a point where
   *     main_loop_jobs_running cannot be zero (since they are running)
   *
   *   - decrementing main_loop_jobs_running always happens in the main
   *     thread (in main_loop_worker_job_complete)
   *
   *    - this function is called by the main thread.
   *
   * With all this said, checking the main_loop_jobs_running is zero is not
   * in fact racy as once it reaches zero there's no concurrency.  If it's
   * non-zero, then the _complete() callbacks are yet to run, but that
   * always happens in the thread we are executing now.
   */
  if (g_atomic_counter_get(&main_loop_jobs_running) == 0)
    {
      _reenable_worker_jobs(NULL);
    }
  else
    {
      _request_all_threads_to_exit();
    }
}

/* This function is intended to be used from test programs to properly
 * synchronize threaded worker startups and then trigger everything to exit
 * and wait for that too.  This is useful in LogThreadedDestDriver test
 * cases, where the test program itself is the "main" thread and we don't
 * want to launch an entire main loop, because in that case we'd be forced
 * to feed the worker thread from ivykis callbacks, which is a lot more
 * difficult to write/maintain.
 *
 * This function clearly shows the level of ugly couplings between the
 * various mainloop components.  (e.g.  mainloop and mainloop worker).  I
 * consider that this should be part of the mainloop layer (semantically, it
 * is the main loop that we are launching in a special mode.  This is also
 * indicated by the iv_main() call below). However its implementation
 * requires access to the main_loop_workers variable.
 */
void
main_loop_sync_worker_startup_and_teardown(void)
{
  struct iv_task request_exit;
  if (g_atomic_counter_get(&main_loop_jobs_running) == 0)
    return;

  IV_TASK_INIT(&request_exit);
  request_exit.handler = (void (*)(void *)) _request_all_threads_to_exit;
  iv_task_register(&request_exit);
  _register_sync_call_action(&sync_call_actions, (void (*)(gpointer user_data)) iv_quit, NULL);
  iv_main();
}

gint
main_loop_worker_get_max_number_of_threads(void)
{
  return main_loop_max_workers;
}

void
main_loop_worker_allocate_thread_space(gint num_threads)
{
  main_loop_estimated_number_of_workers += num_threads;
}

void
main_loop_worker_finalize_thread_space(void)
{
  main_loop_max_workers = main_loop_estimated_number_of_workers;
  main_loop_estimated_number_of_workers = 0;
}

static void
__pre_init_hook(gint type, gpointer user_data)
{
  main_loop_worker_finalize_thread_space();
}

void
main_loop_worker_init(void)
{
  IV_TASK_INIT(&main_loop_workers_reenable_jobs_task);
  main_loop_workers_reenable_jobs_task.handler = _reenable_worker_jobs;
  register_application_hook(AH_CONFIG_PRE_INIT, __pre_init_hook, NULL, AHM_RUN_REPEAT);
}

void
main_loop_worker_deinit(void)
{
}
