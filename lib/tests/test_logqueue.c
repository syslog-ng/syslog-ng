/*
 * Copyright (c) 2008-2016 Balabit
 * Copyright (c) 2019 One Identity
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

#include <criterion/criterion.h>
#include "libtest/msg_parse_lib.h"
#include "libtest/queue_utils_lib.h"

#include "logqueue.h"
#include "logqueue-fifo.h"
#include "logpipe.h"
#include "apphook.h"
#include "plugin.h"
#include "mainloop.h"
#include "mainloop-io-worker.h"
#include "timeutils/misc.h"
#include "stats/stats-cluster-single.h"

#include <stdlib.h>
#include <string.h>
#include <iv.h>
#include <iv_list.h>
#include <iv_thread.h>

#define OVERFLOW_SIZE 10000
#define FEEDERS 1
#define MESSAGES_PER_FEEDER 30000
#define MESSAGES_SUM (FEEDERS * MESSAGES_PER_FEEDER)
#define TEST_RUNS 10

GMutex tlock;
glong sum_time;

static gpointer
_threaded_feed(gpointer args)
{
  LogQueue *q = args;
  gint i;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  LogMessage *msg, *tmpl;
  struct timespec start, end;
  glong diff;

  iv_init();
  /* emulate main loop for LogQueue */
  main_loop_worker_thread_start(MLW_ASYNC_WORKER);

  tmpl = log_msg_new_empty();

  clock_gettime(CLOCK_MONOTONIC, &start);
  for (i = 0; i < MESSAGES_PER_FEEDER; i++)
    {
      msg = log_msg_clone_cow(tmpl, &path_options);
      log_msg_add_ack(msg, &path_options);
      msg->ack_func = test_ack;

      log_queue_push_tail(q, msg, &path_options);

      if ((i & 0xFF) == 0)
        main_loop_worker_invoke_batch_callbacks();
    }
  main_loop_worker_invoke_batch_callbacks();
  clock_gettime(CLOCK_MONOTONIC, &end);
  diff = timespec_diff_usec(&end, &start);
  g_mutex_lock(&tlock);
  sum_time += diff;
  g_mutex_unlock(&tlock);
  log_msg_unref(tmpl);
  main_loop_worker_thread_stop();
  iv_deinit();
  return NULL;
}

static gpointer
_threaded_consume(gpointer st)
{
  LogQueue *q = (LogQueue *) st;
  LogMessage *msg;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  gint msg_count = 0;

  iv_init();
  /* just to make sure time is properly cached */
  while (msg_count < MESSAGES_SUM)
    {
      gint slept = 0;
      msg = NULL;

      while((msg = log_queue_pop_head(q, &path_options)) == NULL)
        {
          struct timespec ns;

          /* sleep 1 msec */
          ns.tv_sec = 0;
          ns.tv_nsec = 1000000;
          nanosleep(&ns, NULL);
          slept++;
          if (slept > 10000)
            {
              /* slept for more than 10 seconds */
              fprintf(stderr, "The wait for messages took too much time, msg_count=%d\n", msg_count);
              return GUINT_TO_POINTER(1);
            }
        }

      log_msg_ack(msg, &path_options, AT_PROCESSED);
      log_msg_unref(msg);
      msg_count++;
    }

  iv_deinit();
  return NULL;
}

static gpointer
_output_thread(gpointer args)
{
  iv_init();
  main_loop_worker_thread_start(MLW_THREADED_OUTPUT_WORKER);
  struct timespec ns;

  /* sleep 1 msec */
  ns.tv_sec = 0;
  ns.tv_nsec = 1000000;
  nanosleep(&ns, NULL);
  main_loop_worker_thread_stop();
  iv_deinit();
  return NULL;
}


void
setup(void)
{
  app_startup();
  setenv("TZ", "MET-1METDST", TRUE);
  tzset();
  configuration = cfg_new_snippet();
  configuration->stats_options.level = 1;
  cr_assert(cfg_init(configuration), "cfg_init failed!");
}

void
teardown(void)
{
  cfg_free(configuration);
  app_shutdown();
}

TestSuite(logqueue, .init = setup, .fini = teardown);

Test(logqueue, test_zero_diskbuf_and_normal_acks)
{
  LogQueue *q;
  gint i;

  StatsClusterKeyBuilder *driver_sck_builder = stats_cluster_key_builder_new();
  StatsClusterKeyBuilder *queue_sck_builder = stats_cluster_key_builder_new();
  q = log_queue_fifo_new(OVERFLOW_SIZE, NULL, STATS_LEVEL0, driver_sck_builder, queue_sck_builder);
  stats_cluster_key_builder_free(driver_sck_builder);
  stats_cluster_key_builder_free(queue_sck_builder);

  cr_assert_eq(atomic_gssize_racy_get(&q->metrics.shared.queued_messages->value), 0);

  fed_messages = 0;
  acked_messages = 0;
  feed_some_messages(q, 1);
  cr_assert_eq(stats_counter_get(q->metrics.shared.queued_messages), 1);
  cr_assert_neq(stats_counter_get(q->metrics.shared.memory_usage), 0);
  gint size_when_single_msg = stats_counter_get(q->metrics.shared.memory_usage);

  for (i = 0; i < 10; i++)
    feed_some_messages(q, 10);

  cr_assert_eq(stats_counter_get(q->metrics.shared.queued_messages), 101);
  cr_assert_eq(stats_counter_get(q->metrics.shared.memory_usage), 101*size_when_single_msg);

  send_some_messages(q, fed_messages, TRUE);

  cr_assert_eq(fed_messages, acked_messages,
               "did not receive enough acknowledgements: fed_messages=%d, acked_messages=%d",
               fed_messages, acked_messages);

  log_queue_unref(q);
}

Test(logqueue, test_zero_diskbuf_alternating_send_acks)
{
  LogQueue *q;
  gint i;

  q = log_queue_fifo_new(OVERFLOW_SIZE, NULL, STATS_LEVEL0, NULL, NULL);

  fed_messages = 0;
  acked_messages = 0;
  for (i = 0; i < 10; i++)
    {
      feed_some_messages(q, 10);
      send_some_messages(q, 10, TRUE);
    }

  cr_assert_eq(fed_messages, acked_messages,
               "did not receive enough acknowledgements: fed_messages=%d, acked_messages=%d",
               fed_messages, acked_messages);

  log_queue_unref(q);
}

Test(logqueue, test_with_threads)
{
  LogQueue *q;
  GThread *thread_feed[FEEDERS], *thread_consume;
  GThread *other_threads[FEEDERS];
  gint i, j;

  main_loop_worker_allocate_thread_space(FEEDERS * 2);
  main_loop_worker_finalize_thread_space();
  for (i = 0; i < TEST_RUNS; i++)
    {
      fprintf(stderr, "starting testrun: %d\n", i);
      q = log_queue_fifo_new(MESSAGES_SUM, NULL, STATS_LEVEL0, NULL, NULL);

      for (j = 0; j < FEEDERS; j++)
        {
          fprintf(stderr, "starting feed thread %d\n", j);
          other_threads[j] = g_thread_new(NULL, _output_thread, NULL);
          thread_feed[j] = g_thread_new(NULL, _threaded_feed, q);
        }

      thread_consume = g_thread_new(NULL, _threaded_consume, q);

      for (j = 0; j < FEEDERS; j++)
        {
          fprintf(stderr, "waiting for feed thread %d\n", j);
          g_thread_join(thread_feed[j]);
          g_thread_join(other_threads[j]);
        }
      g_thread_join(thread_consume);

      log_queue_unref(q);
    }
  fprintf(stderr, "Feed speed: %.2lf\n", (double) TEST_RUNS * MESSAGES_SUM * 1000000 / sum_time);
}

Test(logqueue, log_queue_fifo_rewind_all_and_memory_usage)
{
  StatsClusterKeyBuilder *driver_sck_builder = stats_cluster_key_builder_new();
  StatsClusterKeyBuilder *queue_sck_builder = stats_cluster_key_builder_new();
  LogQueue *q = log_queue_fifo_new(OVERFLOW_SIZE, NULL, STATS_LEVEL0, driver_sck_builder, queue_sck_builder);
  stats_cluster_key_builder_free(driver_sck_builder);
  stats_cluster_key_builder_free(queue_sck_builder);

  feed_some_messages(q, 1);
  gint size_when_single_msg = stats_counter_get(q->metrics.shared.memory_usage);

  feed_some_messages(q, 9);
  cr_assert_eq(stats_counter_get(q->metrics.shared.memory_usage), 10*size_when_single_msg);

  send_some_messages(q, 10, FALSE);
  cr_assert_eq(stats_counter_get(q->metrics.shared.memory_usage), 0);
  log_queue_rewind_backlog_all(q);
  cr_assert_eq(stats_counter_get(q->metrics.shared.memory_usage), 10*size_when_single_msg);

  log_queue_unref(q);
}

Test(logqueue, log_queue_fifo_should_drop_only_non_flow_controlled_messages,
     .description = "Flow-controlled messages should never be dropped")
{
  LogPathOptions flow_controlled_path = LOG_PATH_OPTIONS_INIT;
  flow_controlled_path.flow_control_requested = TRUE;

  LogPathOptions non_flow_controlled_path = LOG_PATH_OPTIONS_INIT;
  non_flow_controlled_path.flow_control_requested = FALSE;

  gint fifo_size = 5;
  StatsClusterKeyBuilder *driver_sck_builder = stats_cluster_key_builder_new();
  StatsClusterKeyBuilder *queue_sck_builder = stats_cluster_key_builder_new();
  LogQueue *q = log_queue_fifo_new(fifo_size, NULL, STATS_LEVEL0, driver_sck_builder, queue_sck_builder);
  stats_cluster_key_builder_free(driver_sck_builder);
  stats_cluster_key_builder_free(queue_sck_builder);

  fed_messages = 0;
  acked_messages = 0;
  feed_empty_messages(q, &flow_controlled_path, fifo_size);
  feed_empty_messages(q, &non_flow_controlled_path, fifo_size);

  feed_empty_messages(q, &non_flow_controlled_path, 1);
  feed_empty_messages(q, &flow_controlled_path, fifo_size);
  feed_empty_messages(q, &non_flow_controlled_path, 2);
  feed_empty_messages(q, &flow_controlled_path, fifo_size);

  cr_assert_eq(stats_counter_get(q->metrics.shared.dropped_messages), 3);

  gint queued_messages = stats_counter_get(q->metrics.shared.queued_messages);
  send_some_messages(q, queued_messages, TRUE);

  cr_assert_eq(fed_messages, acked_messages,
               "did not receive enough acknowledgements: fed_messages=%d, acked_messages=%d",
               fed_messages, acked_messages);

  log_queue_unref(q);
}

static gpointer
_flow_control_feed_thread(gpointer args)
{
  LogQueue *q = args;
  gint fifo_size = 5;
  LogPathOptions flow_controlled_path = LOG_PATH_OPTIONS_INIT;
  flow_controlled_path.flow_control_requested = TRUE;

  LogPathOptions non_flow_controlled_path = LOG_PATH_OPTIONS_INIT;
  non_flow_controlled_path.flow_control_requested = FALSE;

  iv_init();

  main_loop_worker_thread_start(MLW_ASYNC_WORKER);

  fed_messages = 0;
  acked_messages = 0;
  feed_empty_messages(q, &flow_controlled_path, fifo_size);
  feed_empty_messages(q, &non_flow_controlled_path, fifo_size);

  feed_empty_messages(q, &non_flow_controlled_path, 1);
  feed_empty_messages(q, &flow_controlled_path, fifo_size);
  feed_empty_messages(q, &non_flow_controlled_path, 2);
  feed_empty_messages(q, &flow_controlled_path, fifo_size);

  main_loop_worker_invoke_batch_callbacks();
  main_loop_worker_thread_stop();
  iv_deinit();
  return NULL;
}

Test(logqueue, log_queue_fifo_should_drop_only_non_flow_controlled_messages_threaded,
     .description = "Flow-controlled messages should never be dropped (using input queues with threads")
{
  gint fifo_size = 5;

  main_loop_worker_allocate_thread_space(1);
  main_loop_worker_finalize_thread_space();

  StatsClusterKeyBuilder *driver_sck_builder = stats_cluster_key_builder_new();
  StatsClusterKeyBuilder *queue_sck_builder = stats_cluster_key_builder_new();
  LogQueue *q = log_queue_fifo_new(fifo_size, NULL, STATS_LEVEL0, driver_sck_builder, queue_sck_builder);
  stats_cluster_key_builder_free(driver_sck_builder);
  stats_cluster_key_builder_free(queue_sck_builder);

  GThread *thread = g_thread_new(NULL, _flow_control_feed_thread, q);
  g_thread_join(thread);

  cr_assert_eq(stats_counter_get(q->metrics.shared.dropped_messages), 3);

  gint queued_messages = stats_counter_get(q->metrics.shared.queued_messages);
  send_some_messages(q, queued_messages, TRUE);

  cr_assert_eq(fed_messages, acked_messages,
               "did not receive enough acknowledgements: fed_messages=%d, acked_messages=%d",
               fed_messages, acked_messages);

  log_queue_unref(q);
}

Test(logqueue, log_queue_fifo_multiple_queues)
{
  const gint fifo_size = 1;
  LogPathOptions options = LOG_PATH_OPTIONS_INIT;

  StatsClusterKeyBuilder *driver_sck_builder = stats_cluster_key_builder_new();
  StatsClusterKeyBuilder *queue_sck_builder = stats_cluster_key_builder_new();
  stats_cluster_key_builder_add_label(queue_sck_builder, stats_cluster_label("log_queue_fifo_multiple_queues", "1"));
  LogQueue *queue_1 = log_queue_fifo_new(fifo_size, NULL, STATS_LEVEL0, driver_sck_builder, queue_sck_builder);
  stats_cluster_key_builder_free(queue_sck_builder);
  queue_sck_builder = stats_cluster_key_builder_new();
  stats_cluster_key_builder_add_label(queue_sck_builder, stats_cluster_label("log_queue_fifo_multiple_queues", "2"));
  LogQueue *queue_2 = log_queue_fifo_new(fifo_size, NULL, STATS_LEVEL0, driver_sck_builder, queue_sck_builder);

  cr_assert_eq(stats_counter_get(queue_1->metrics.shared.queued_messages), 0);
  cr_assert_eq(stats_counter_get(queue_1->metrics.owned.queued_messages), 0);
  cr_assert_eq(stats_counter_get(queue_2->metrics.shared.queued_messages), 0);
  cr_assert_eq(stats_counter_get(queue_2->metrics.owned.queued_messages), 0);


  log_queue_push_tail(queue_1, log_msg_new_empty(), &options);
  cr_assert_eq(stats_counter_get(queue_1->metrics.shared.queued_messages), 1);
  cr_assert_eq(stats_counter_get(queue_1->metrics.owned.queued_messages), 1);
  cr_assert_eq(stats_counter_get(queue_2->metrics.shared.queued_messages), 1);
  cr_assert_eq(stats_counter_get(queue_2->metrics.owned.queued_messages), 0);

  log_queue_push_tail(queue_2, log_msg_new_empty(), &options);
  cr_assert_eq(stats_counter_get(queue_1->metrics.shared.queued_messages), 2);
  cr_assert_eq(stats_counter_get(queue_1->metrics.owned.queued_messages), 1);
  cr_assert_eq(stats_counter_get(queue_2->metrics.shared.queued_messages), 2);
  cr_assert_eq(stats_counter_get(queue_2->metrics.owned.queued_messages), 1);

  log_queue_unref(queue_1);

  cr_assert_eq(stats_counter_get(queue_2->metrics.shared.queued_messages), 1);
  cr_assert_eq(stats_counter_get(queue_2->metrics.owned.queued_messages), 1);

  stats_cluster_key_builder_free(queue_sck_builder);
  queue_sck_builder = stats_cluster_key_builder_new();
  stats_cluster_key_builder_add_label(queue_sck_builder, stats_cluster_label("queue", "1"));
  queue_1 = log_queue_fifo_new(fifo_size, NULL, STATS_LEVEL0, driver_sck_builder, queue_sck_builder);

  cr_assert_eq(stats_counter_get(queue_1->metrics.shared.queued_messages), 1);
  cr_assert_eq(stats_counter_get(queue_1->metrics.owned.queued_messages), 0);
  cr_assert_eq(stats_counter_get(queue_2->metrics.shared.queued_messages), 1);
  cr_assert_eq(stats_counter_get(queue_2->metrics.owned.queued_messages), 1);

  log_queue_unref(queue_2);

  cr_assert_eq(stats_counter_get(queue_1->metrics.shared.queued_messages), 0);
  cr_assert_eq(stats_counter_get(queue_1->metrics.owned.queued_messages), 0);

  log_queue_unref(queue_1);

  stats_cluster_key_builder_free(driver_sck_builder);
}
