#include "mainloop-io-worker.h"
#include "mainloop-worker.h"
#include "mainloop-call.h"
#include "logqueue.h"
#include "messages.h"

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

void
main_loop_io_worker_init(void)
{
  if (main_loop_io_workers.max_threads == 0)
    {
      main_loop_io_workers.max_threads = MIN(MAX(MAIN_LOOP_MIN_WORKER_THREADS, get_processor_count()), MAIN_LOOP_MAX_WORKER_THREADS);
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

void
main_loop_maximalize_worker_threads(int max_threads)
{
  if (max_threads == -1)
    return;
  if (max_threads == 0)
    {
      if (main_loop_io_workers.max_threads > 2)
      {
        main_loop_io_workers.max_threads = 2;
        msg_warning("WARNING: Maximum number of worker threads limited by license",
                    evt_tag_int("Max-Processors", 2),
                    NULL);
      }
    }
  else if (max_threads < main_loop_io_workers.max_threads)
    {
      main_loop_io_workers.max_threads = max_threads;
      msg_warning("WARNING: Maximum number of worker threads limited by license",
                    evt_tag_int("Max-Processors", max_threads),
                    NULL);
    }

  log_queue_set_max_threads(main_loop_io_workers.max_threads);
}
