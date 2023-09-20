/*
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

#include "logscheduler.h"
#include "template/eval.h"

static void
_reinject_message(LogPipe *front_pipe, LogMessage *msg, const LogPathOptions *path_options)
{
  if (front_pipe)
    log_pipe_queue(front_pipe, msg, path_options);
  else
    log_msg_drop(msg, path_options, AT_PROCESSED);
}

#if SYSLOG_NG_HAVE_IV_WORK_POOL_SUBMIT_CONTINUATION

/* LogSchedulerBatch */

LogSchedulerBatch *
_batch_new(struct iv_list_head *elements)
{
  LogSchedulerBatch *batch = g_new0(LogSchedulerBatch, 1);

  INIT_IV_LIST_HEAD(&batch->elements);
  INIT_IV_LIST_HEAD(&batch->list);
  iv_list_splice_tail(elements, &batch->elements);
  return batch;
}

void
_batch_free(LogSchedulerBatch *batch)
{
  g_free(batch);
}

/* LogSchedulerPartition */

static void
_work(gpointer s, gpointer arg)
{
  LogSchedulerPartition *partition = (LogSchedulerPartition *) s;
  struct iv_list_head *ilh, *next;
  struct iv_list_head *ilh2, *next2;

  /* batches_lock protects the batches list itself.  We take off partitions
   * one-by-one under the protection of the lock */

  g_mutex_lock(&partition->batches_lock);
  while (!iv_list_empty(&partition->batches))
    {
      struct iv_list_head batches = IV_LIST_HEAD_INIT(batches);
      iv_list_splice_init(&partition->batches, &batches);

      g_mutex_unlock(&partition->batches_lock);

      iv_list_for_each_safe(ilh, next, &batches)
      {
        /* remove the first batch from the batches list */
        LogSchedulerBatch *batch = iv_list_entry(ilh, LogSchedulerBatch, list);
        iv_list_del(&batch->list);

        iv_list_for_each_safe(ilh2, next2, &batch->elements)
        {
          LogMessageQueueNode *node = iv_list_entry(ilh2, LogMessageQueueNode, list);

          iv_list_del(&node->list);

          LogMessage *msg = log_msg_ref(node->msg);

          LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
          path_options.ack_needed = node->ack_needed;
          path_options.flow_control_requested = node->flow_control_requested;

          log_msg_free_queue_node(node);

          log_msg_refcache_start_consumer(msg, &path_options);
          _reinject_message(partition->front_pipe, msg, &path_options);
          log_msg_unref(msg);
          log_msg_refcache_stop();
        }
        _batch_free(batch);
      }
      g_mutex_lock(&partition->batches_lock);
    }
  g_mutex_unlock(&partition->batches_lock);
}

static void
_complete(gpointer s, gpointer arg)
{
  LogSchedulerPartition *partition = (LogSchedulerPartition *) s;

  gboolean needs_restart = FALSE;

  g_mutex_lock(&partition->batches_lock);
  if (!iv_list_empty(&partition->batches))
    {
      /* our work() function returned right before a new batch was added, let's restart */
      needs_restart = TRUE;
    }
  else
    partition->flush_running = FALSE;

  g_mutex_unlock(&partition->batches_lock);

  if (needs_restart)
    main_loop_io_worker_job_submit(&partition->io_job, NULL);
}

static void
_partition_add_batch(LogSchedulerPartition *partition, LogSchedulerBatch *batch)
{
  gboolean trigger_flush = FALSE;

  g_mutex_lock(&partition->batches_lock);
  if (!partition->flush_running &&
      iv_list_empty(&partition->batches))
    {
      trigger_flush = TRUE;
      partition->flush_running = TRUE;
    }
  iv_list_add_tail(&batch->list, &partition->batches);
  g_mutex_unlock(&partition->batches_lock);

  if (trigger_flush)
    {
      main_loop_io_worker_job_submit_continuation(&partition->io_job, NULL);
    }

}

static void
_partition_init(LogSchedulerPartition *partition, LogPipe *front_pipe)
{
  main_loop_io_worker_job_init(&partition->io_job);
  partition->io_job.user_data = partition;
  partition->io_job.work = _work;
  partition->io_job.completion = _complete;
  partition->io_job.engage = NULL;
  partition->io_job.release = NULL;

  partition->front_pipe = front_pipe;

  INIT_IV_LIST_HEAD(&partition->batches);
  g_mutex_init(&partition->batches_lock);
}

void
_partition_clear(LogSchedulerPartition *partition)
{
  g_mutex_clear(&partition->batches_lock);
}

/* LogSchedulerThreadState */

static guint
_get_partition_index(LogScheduler *self, LogSchedulerThreadState *thread_state, LogMessage *msg)
{
  if (!self->options->partition_key)
    {
      gint partition_index = thread_state->last_partition;
      thread_state->last_partition = (thread_state->last_partition + 1) % self->options->num_partitions;
      return partition_index;
    }
  else
    {
      LogTemplateEvalOptions options = DEFAULT_TEMPLATE_EVAL_OPTIONS;
      return log_template_hash(self->options->partition_key, msg, &options) % self->options->num_partitions;
    }
}

static gpointer
_flush_batch(gpointer s)
{
  LogScheduler *self = (LogScheduler *) s;

  gint thread_index = main_loop_worker_get_thread_index();
  g_assert(thread_index >= 0);

  LogSchedulerThreadState *thread_state = &self->thread_states[thread_index];

  for (gint partition_index = 0; partition_index < self->options->num_partitions; partition_index++)
    {
      if (iv_list_empty(&thread_state->batch_by_partition[partition_index]))
        continue;

      /* form the new batch, hand over the accumulated elements in batch_by_partition */
      LogSchedulerBatch *batch = _batch_new(&thread_state->batch_by_partition[partition_index]);
      INIT_IV_LIST_HEAD(&thread_state->batch_by_partition[partition_index]);

      /* add the new batch to the target partition */

      LogSchedulerPartition *partition = &self->partitions[partition_index];

      _partition_add_batch(partition, batch);


    }
  thread_state->num_messages = 0;
  return NULL;
}

static void
_queue_thread(LogScheduler *self, LogSchedulerThreadState *thread_state, LogMessage *msg,
              const LogPathOptions *path_options)
{
  if (thread_state->num_messages == 0)
    main_loop_worker_register_batch_callback(&thread_state->batch_callback);

  guint partition_index = _get_partition_index(self, thread_state, msg);

  LogMessageQueueNode *node;
  node = log_msg_alloc_queue_node(msg, path_options);
  iv_list_add_tail(&node->list, &thread_state->batch_by_partition[partition_index]);
  thread_state->num_messages++;
  log_msg_unref(msg);
}


static void
_thread_state_init(LogScheduler *self, LogSchedulerThreadState *state)
{
  worker_batch_callback_init(&state->batch_callback);
  state->batch_callback.func = _flush_batch;
  state->batch_callback.user_data = self;

  for (gint i = 0; i < self->options->num_partitions; i++)
    INIT_IV_LIST_HEAD(&state->batch_by_partition[i]);
}

static void
_init_thread_states(LogScheduler *self)
{
  for (gint i = 0; i < self->num_threads; i++)
    {
      _thread_state_init(self, &self->thread_states[i]);
    }
}

static void
_init_partitions(LogScheduler *self)
{
  for (gint i = 0; i < self->options->num_partitions; i++)
    {
      _partition_init(&self->partitions[i], self->front_pipe);
    }
}

static void
_free_partitions(LogScheduler *self)
{
  for (gint i = 0; i < self->options->num_partitions; i++)
    {
      _partition_clear(&self->partitions[i]);
    }
}

gboolean
log_scheduler_init(LogScheduler *self)
{
  return TRUE;
}

void
log_scheduler_deinit(LogScheduler *self)
{
}

void
log_scheduler_push(LogScheduler *self, LogMessage *msg, const LogPathOptions *path_options)
{
  gint thread_index = main_loop_worker_get_thread_index();

  if (!self->front_pipe ||
      self->options->num_partitions == 0 ||
      thread_index < 0 ||
      thread_index >= self->num_threads)
    {
      _reinject_message(self->front_pipe, msg, path_options);
      return;
    }

  LogSchedulerThreadState *thread_state = &self->thread_states[thread_index];
  _queue_thread(self, thread_state, msg, path_options);
}

LogScheduler *
log_scheduler_new(LogSchedulerOptions *options, LogPipe *front_pipe)
{
  gint max_threads = main_loop_worker_get_max_number_of_threads();
  LogScheduler *self = g_malloc0(sizeof(LogScheduler) + max_threads * sizeof(LogSchedulerThreadState));
  self->num_threads = max_threads;
  self->options = options;
  self->front_pipe = log_pipe_ref(front_pipe);

  _init_thread_states(self);
  _init_partitions(self);
  return self;
}

void
log_scheduler_free(LogScheduler *self)
{
  log_pipe_unref(self->front_pipe);
  _free_partitions(self);
  g_free(self);
}

#else

gboolean
log_scheduler_init(LogScheduler *self)
{
  if (self->options->num_partitions > 0)
    {
      msg_warning("Unable to parallelize message work-load even though partitions(N) N > 1 was set, "
                  "as the ivykis dependency is too old and lacks iv_work_pool_submit_continuation() "
                  "function. Use the bundled version of ivykis or use a version that already has this "
                  "function, see https://github.com/buytenh/ivykis/compare/master...bazsi:ivykis:iv-work-pool-support-for-slave-work-items");
    }
  return TRUE;
}

void
log_scheduler_deinit(LogScheduler *self)
{
}

void
log_scheduler_push(LogScheduler *self, LogMessage *msg, const LogPathOptions *path_options)
{
  _reinject_message(self->front_pipe, msg, path_options);
}

LogScheduler *
log_scheduler_new(LogSchedulerOptions *options, LogPipe *front_pipe)
{
  LogScheduler *self = g_malloc0(sizeof(LogScheduler) + 0 * sizeof(LogSchedulerThreadState));
  self->options = options;
  self->front_pipe = log_pipe_ref(front_pipe);

  return self;
}

void
log_scheduler_free(LogScheduler *self)
{
  log_pipe_unref(self->front_pipe);
  g_free(self);
}

#endif

void
log_scheduler_options_set_partition_key_ref(LogSchedulerOptions *options, LogTemplate *partition_key)
{
  log_template_unref(options->partition_key);
  options->partition_key = partition_key;
}

void
log_scheduler_options_defaults(LogSchedulerOptions *options)
{
  options->num_partitions = -1;
  options->partition_key = NULL;
}

gboolean
log_scheduler_options_init(LogSchedulerOptions *options, GlobalConfig *cfg)
{
  if (options->num_partitions == -1)
    options->num_partitions = 0;
  if (options->num_partitions > LOGSCHEDULER_MAX_PARTITIONS)
    options->num_partitions = LOGSCHEDULER_MAX_PARTITIONS;
  return TRUE;
}

void
log_scheduler_options_destroy(LogSchedulerOptions *options)
{
  log_template_unref(options->partition_key);
}
