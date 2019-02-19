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

#include <iv.h>

typedef enum { GENERAL_THREAD = 0, OUTPUT_THREAD, EXTERNAL_INPUT_THREAD, MAIN_LOOP_WORKER_TYPE_MAX} MainLoopWorkerType;

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
static gint main_loop_jobs_running;

static struct iv_task main_loop_workers_reenable_jobs_task;

/* thread ID allocation */
static GStaticMutex main_loop_workers_idmap_lock = G_STATIC_MUTEX_INIT;

static guint64 main_loop_workers_idmap[MAIN_LOOP_WORKER_TYPE_MAX];

static void
_allocate_thread_id(void)
{
  gint id;

  g_static_mutex_lock(&main_loop_workers_idmap_lock);

  /* NOTE: this algorithm limits the number of I/O worker threads to 64,
   * since the ID map is stored in a single 64 bit integer.  If we ever need
   * more threads than that, we can generalize this algorithm further. */

  main_loop_worker_id = 0;

  if(main_loop_worker_type != EXTERNAL_INPUT_THREAD)
    {
      for (id = 0; id < MAIN_LOOP_MAX_WORKER_THREADS; id++)
        {
          if ((main_loop_workers_idmap[main_loop_worker_type] & (1ULL << id)) == 0)
            {
              /* id not yet used */

              main_loop_worker_id = (id + 1)  + (main_loop_worker_type * MAIN_LOOP_MAX_WORKER_THREADS);
              main_loop_workers_idmap[main_loop_worker_type] |= (1ULL << id);
              break;
            }
        }
    }
  g_static_mutex_unlock(&main_loop_workers_idmap_lock);
}

static void
_release_thread_id(void)
{
  g_static_mutex_lock(&main_loop_workers_idmap_lock);
  if (main_loop_worker_id)
    {
      const gint id = main_loop_worker_id & (sizeof(guint64) * CHAR_BIT - 1);
      main_loop_workers_idmap[main_loop_worker_type] &= ~(1ULL << (id - 1));
      main_loop_worker_id = 0;
    }
  g_static_mutex_unlock(&main_loop_workers_idmap_lock);
}

/* NOTE: only used by the unit test program to emulate worker threads with
 * LogQueue, other threads acquire a thread id when they start up. */
void
main_loop_worker_set_thread_id(gint id)
{
  main_loop_worker_id = id + 1;
}

gint
main_loop_worker_get_thread_id(void)
{
  return main_loop_worker_id - 1;
}

typedef struct _WorkerExitNotification
{
  WorkerExitNotificationFunc func;
  gpointer user_data;
} WorkerExitNotification;

static GList *exit_notification_list = NULL;

static void
_register_exit_notification_callback(WorkerExitNotificationFunc func, gpointer user_data)
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
main_loop_worker_thread_start(void *cookie)
{
  WorkerOptions *worker_options = cookie;
  main_loop_worker_type = GENERAL_THREAD;

  if (worker_options && worker_options->is_output_thread)
    {
      main_loop_worker_type = OUTPUT_THREAD;
    }
  else if(worker_options && worker_options->is_external_input)
    {
      main_loop_worker_type = EXTERNAL_INPUT_THREAD;
    }

  _allocate_thread_id();
  INIT_IV_LIST_HEAD(&batch_callbacks);

  g_static_mutex_lock(&workers_running_lock);
  main_loop_workers_running++;
  g_static_mutex_unlock(&workers_running_lock);

  app_thread_start();
}

/* Call this function from worker threads, when you stop */
void
main_loop_worker_thread_stop(void)
{
  app_thread_stop();
  _release_thread_id();

  g_static_mutex_lock(&workers_running_lock);
  main_loop_workers_running--;
  g_cond_signal(thread_halt_cond);
  g_static_mutex_unlock(&workers_running_lock);
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
  main_loop_assert_main_thread();

  main_loop_jobs_running++;
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

  main_loop_jobs_running--;
  if (main_loop_workers_quit && main_loop_jobs_running == 0)
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
      _invoke_sync_call_actions();
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

typedef struct _WorkerThreadParams
{
  WorkerThreadFunc func;
  gpointer data;
  WorkerOptions *worker_options;
} WorkerThreadParams;

static gpointer
_worker_thread_func(gpointer st)
{
  WorkerThreadParams *p = st;

  main_loop_worker_thread_start(p->worker_options);
  p->func(p->data);
  main_loop_call((MainLoopTaskFunc) main_loop_worker_job_complete, NULL, TRUE);
  main_loop_worker_thread_stop();


  /* NOTE: this assert aims to validate that the worker thread in fact
   * invokes main_loop_worker_invoke_batch_callbacks() during its operation.
   * Please do so every once a couple of messages, hopefully you have a
   * natural barrier that let's you decide when, the easiest would be
   * log-fetch-limit(), but other limits may also be applicable.
   */
  g_assert(iv_list_empty(&batch_callbacks));

  g_free(st);
  return NULL;
}

void
main_loop_create_worker_thread(WorkerThreadFunc func, WorkerExitNotificationFunc terminate_func, gpointer data,
                               WorkerOptions *worker_options)
{
  GThread *h;
  WorkerThreadParams *p;

  main_loop_assert_main_thread();

  p = g_new0(WorkerThreadParams, 1);
  p->func = func;
  p->data = data;
  p->worker_options = worker_options;

  main_loop_worker_job_start();
  if (terminate_func)
    _register_exit_notification_callback(terminate_func, data);
  h = g_thread_create_full(_worker_thread_func, p, 1024 * 1024, FALSE, TRUE, G_THREAD_PRIORITY_NORMAL, NULL);
  g_assert(h != NULL);
}

static void
_reenable_worker_jobs(void *s)
{
  main_loop_workers_quit = FALSE;
  if (is_reloading_scheduled)
    msg_notice("Configuration reload finished");
  is_reloading_scheduled = FALSE;
}

void
main_loop_worker_sync_call(void (*func)(gpointer user_data), gpointer user_data)
{
  _register_sync_call_action(&sync_call_actions, func, user_data);

  if (main_loop_jobs_running == 0)
    {
      _invoke_sync_call_actions();
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
  if (main_loop_jobs_running == 0)
    return;

  IV_TASK_INIT(&request_exit);
  request_exit.handler = (void (*)(void *)) _request_all_threads_to_exit;
  iv_task_register(&request_exit);
  _register_sync_call_action(&sync_call_actions, (void (*)(gpointer user_data)) iv_quit, NULL);
  iv_main();
}

void
main_loop_worker_init(void)
{
  IV_TASK_INIT(&main_loop_workers_reenable_jobs_task);
  main_loop_workers_reenable_jobs_task.handler = _reenable_worker_jobs;

}

void
main_loop_worker_deinit(void)
{
}
