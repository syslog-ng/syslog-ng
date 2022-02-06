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
#include "mainloop-io-worker.h"
#include "mainloop-worker.h"
#include "mainloop-call.h"
#include "logqueue.h"
#include "apphook.h"

/************************************************************************************
 * I/O worker threads
 ************************************************************************************/

static struct iv_work_pool main_loop_io_workers;

static void
_release(MainLoopIOWorkerJob *self)
{
  if (self->release)
    self->release(self->user_data);
}

static void
_engage(MainLoopIOWorkerJob *self)
{
  if (self->engage)
    self->engage(self->user_data);
}

gboolean
main_loop_io_worker_job_submit(MainLoopIOWorkerJob *self, gpointer arg)
{
  main_loop_assert_main_thread();

  g_assert(self->working == FALSE);
  if (main_loop_workers_quit)
    return FALSE;

  _engage(self);
  main_loop_worker_job_start();
  self->working = TRUE;
  self->arg = arg;
  iv_work_pool_submit_work(&main_loop_io_workers, &self->work_item);
  return TRUE;
}

#if SYSLOG_NG_HAVE_IV_WORK_POOL_SUBMIT_CONTINUATION
void
main_loop_io_worker_job_submit_continuation(MainLoopIOWorkerJob *self, gpointer arg)
{
  main_loop_assert_worker_thread();
  g_assert(self->working == FALSE);

  _engage(self);
  main_loop_worker_job_start();
  self->working = TRUE;
  self->arg = arg;

  iv_work_pool_submit_continuation(&main_loop_io_workers, &self->work_item);
}
#endif

/* NOTE: runs in the actual worker thread spawned by the
 * main_loop_io_workers thread pool */
static void
_work(MainLoopIOWorkerJob *self)
{
  self->work(self->user_data, self->arg);
  main_loop_worker_invoke_batch_callbacks();
  main_loop_worker_run_gc();
}

/* NOTE: runs in the main thread */
static void
_complete(MainLoopIOWorkerJob *self)
{
  self->working = FALSE;
  if (self->completion)
    self->completion(self->user_data, self->arg);
  main_loop_worker_job_complete();
  _release(self);
}

void
main_loop_io_worker_job_init(MainLoopIOWorkerJob *self)
{
  IV_WORK_ITEM_INIT(&self->work_item);
  self->work_item.cookie = self;
  self->work_item.work = (void (*)(void *)) _work;
  self->work_item.completion = (void (*)(void *)) _complete;
}

static gint
get_processor_count(void)
{
#ifdef _SC_NPROCESSORS_ONLN
  return sysconf(_SC_NPROCESSORS_ONLN);
#else
  return -1;
#endif
}

static void
main_loop_io_worker_thread_start(void *cookie)
{
  main_loop_worker_thread_start(MLW_ASYNC_WORKER);
}

static void
main_loop_io_worker_thread_stop(void *cookie)
{
  main_loop_worker_thread_stop();
}

static void
__pre_pre_init_hook(gint type, gpointer user_data)
{
  main_loop_worker_allocate_thread_space(main_loop_io_workers.max_threads);
}

void
main_loop_io_worker_init(void)
{
  if (main_loop_io_workers.max_threads == 0)
    {
      main_loop_io_workers.max_threads = MIN(MAX(MAIN_LOOP_MIN_WORKER_THREADS, get_processor_count()),
                                             MAIN_LOOP_MAX_WORKER_THREADS);
    }

  main_loop_io_workers.thread_start = main_loop_io_worker_thread_start;
  main_loop_io_workers.thread_stop = main_loop_io_worker_thread_stop;
  iv_work_pool_create(&main_loop_io_workers);
  register_application_hook(AH_CONFIG_PRE_PRE_INIT, __pre_pre_init_hook, NULL, AHM_RUN_REPEAT);
}

void
main_loop_io_worker_deinit(void)
{
  iv_work_pool_put(&main_loop_io_workers);
}

static GOptionEntry main_loop_io_worker_options[] =
{
  { "worker-threads",      0,         0, G_OPTION_ARG_INT, &main_loop_io_workers.max_threads, "Set the number of I/O worker threads", "<max>" },
  { NULL },
};

void
main_loop_io_worker_add_options(GOptionContext *ctx)
{
  g_option_context_add_main_entries(ctx, main_loop_io_worker_options, NULL);
}
