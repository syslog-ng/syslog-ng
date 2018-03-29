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

/************************************************************************************
 * I/O worker threads
 ************************************************************************************/

static struct iv_work_pool main_loop_io_workers;

/* NOTE: runs in the main thread */
void
main_loop_io_worker_job_submit(MainLoopIOWorkerJob *self)
{
  g_assert(self->working == FALSE);
  if (main_loop_workers_quit)
    return;
  main_loop_worker_job_start();
  self->working = TRUE;
  iv_work_pool_submit_work(&main_loop_io_workers, &self->work_item);
}

/* NOTE: runs in the actual worker thread spawned by the
 * main_loop_io_workers thread pool */
static void
_work(MainLoopIOWorkerJob *self)
{
  self->work(self->user_data);
  main_loop_worker_invoke_batch_callbacks();
  main_loop_worker_run_gc();
}

/* NOTE: runs in the main thread */
static void
_complete(MainLoopIOWorkerJob *self)
{
  self->working = FALSE;
  self->completion(self->user_data);
  main_loop_worker_job_complete();
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

void
main_loop_io_worker_init(void)
{
  if (main_loop_io_workers.max_threads == 0)
    {
      main_loop_io_workers.max_threads = MIN(MAX(MAIN_LOOP_MIN_WORKER_THREADS, get_processor_count()),
                                             MAIN_LOOP_MAX_WORKER_THREADS);
    }

  main_loop_io_workers.thread_start = (void (*)(void *)) main_loop_worker_thread_start;
  main_loop_io_workers.thread_stop = (void (*)(void *)) main_loop_worker_thread_stop;
  iv_work_pool_create(&main_loop_io_workers);

  log_queue_set_max_threads(MIN(main_loop_io_workers.max_threads, MAIN_LOOP_MAX_WORKER_THREADS));
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
