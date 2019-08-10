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

#include "logqueue.h"
#include "logqueue-fifo.h"
#include "logpipe.h"
#include "apphook.h"
#include "plugin.h"
#include "mainloop.h"
#include "mainloop-io-worker.h"
#include "libtest/queue_utils_lib.h"
#include "msg_parse_lib.h"
#include "timeutils/misc.h"

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

GStaticMutex tlock;
glong sum_time;

static gpointer
_threaded_feed(gpointer args)
{
  LogQueue *q = args;
  gint i;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  LogMessage *msg, *tmpl;
  GTimeVal start, end;
  glong diff;

  iv_init();

  /* emulate main loop for LogQueue */
  main_loop_worker_thread_start(NULL);

  tmpl = log_msg_new_empty();

  g_get_current_time(&start);
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
  g_get_current_time(&end);
  diff = g_time_val_diff(&end, &start);
  g_static_mutex_lock(&tlock);
  sum_time += diff;
  g_static_mutex_unlock(&tlock);
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
  gint loops = 0;
  gint msg_count = 0;

  /* just to make sure time is properly cached */
  iv_init();

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
              fprintf(stderr, "The wait for messages took too much time, loops=%d, msg_count=%d\n", loops, msg_count);
              return GUINT_TO_POINTER(1);
            }
        }

      if ((loops % 10) == 0)
        {
          /* push the message back to the queue */
          log_queue_push_head(q, msg, &path_options);
        }
      else
        {
          log_msg_ack(msg, &path_options, AT_PROCESSED);
          log_msg_unref(msg);
          msg_count++;
        }
      loops++;
    }

  iv_deinit();
  return NULL;
}

static gpointer
_output_thread(gpointer args)
{
  WorkerOptions wo;

  iv_init();
  wo.is_output_thread = TRUE;
  main_loop_worker_thread_start(&wo);
  struct timespec ns;

  /* sleep 1 msec */
  ns.tv_sec = 0;
  ns.tv_nsec = 1000000;
  nanosleep(&ns, NULL);
  main_loop_worker_thread_stop();
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

  q = log_queue_fifo_new(OVERFLOW_SIZE, NULL);

  StatsClusterKey sc_key;
  stats_lock();
  stats_cluster_logpipe_key_set(&sc_key, SCS_DESTINATION, q->persist_name, NULL );
  stats_register_counter(0, &sc_key, SC_TYPE_QUEUED, &q->queued_messages);
  stats_register_counter(1, &sc_key, SC_TYPE_MEMORY_USAGE, &q->memory_usage);
  stats_unlock();

  log_queue_set_use_backlog(q, TRUE);

  cr_assert_eq(atomic_gssize_racy_get(&q->queued_messages->value), 0);

  fed_messages = 0;
  acked_messages = 0;
  feed_some_messages(q, 1);
  cr_assert_eq(stats_counter_get(q->queued_messages), 1);
  cr_assert_neq(stats_counter_get(q->memory_usage), 0);
  gint size_when_single_msg = stats_counter_get(q->memory_usage);

  for (i = 0; i < 10; i++)
    feed_some_messages(q, 10);

  cr_assert_eq(stats_counter_get(q->queued_messages), 101);
  cr_assert_eq(stats_counter_get(q->memory_usage), 101*size_when_single_msg);

  send_some_messages(q, fed_messages);
  log_queue_ack_backlog(q, fed_messages);

  cr_assert_eq(fed_messages, acked_messages,
               "did not receive enough acknowledgements: fed_messages=%d, acked_messages=%d",
               fed_messages, acked_messages);

  log_queue_unref(q);
}

Test(logqueue, test_zero_diskbuf_alternating_send_acks)
{
  LogQueue *q;
  gint i;

  q = log_queue_fifo_new(OVERFLOW_SIZE, NULL);
  log_queue_set_use_backlog(q, TRUE);

  fed_messages = 0;
  acked_messages = 0;
  for (i = 0; i < 10; i++)
    {
      feed_some_messages(q, 10);
      send_some_messages(q, 10);
      log_queue_ack_backlog(q, 10);
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

  log_queue_set_max_threads(FEEDERS);
  for (i = 0; i < TEST_RUNS; i++)
    {
      fprintf(stderr,"starting testrun: %d\n",i);
      q = log_queue_fifo_new(MESSAGES_SUM, NULL);
      log_queue_set_use_backlog(q, TRUE);

      for (j = 0; j < FEEDERS; j++)
        {
          fprintf(stderr,"starting feed thread %d\n",j);
          other_threads[j] = g_thread_create(_output_thread, NULL, TRUE, NULL);
          thread_feed[j] = g_thread_create(_threaded_feed, q, TRUE, NULL);
        }

      thread_consume = g_thread_create(_threaded_consume, q, TRUE, NULL);

      for (j = 0; j < FEEDERS; j++)
        {
          fprintf(stderr,"waiting for feed thread %d\n",j);
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
  LogQueue *q = log_queue_fifo_new(OVERFLOW_SIZE, NULL);
  log_queue_set_use_backlog(q, TRUE);

  StatsClusterKey sc_key;
  stats_lock();
  stats_cluster_logpipe_key_set(&sc_key, SCS_DESTINATION, q->persist_name, NULL );
  stats_register_counter(1, &sc_key, SC_TYPE_MEMORY_USAGE, &q->memory_usage);
  stats_unlock();

  feed_some_messages(q, 1);
  gint size_when_single_msg = stats_counter_get(q->memory_usage);

  feed_some_messages(q, 9);
  cr_assert_eq(stats_counter_get(q->memory_usage), 10*size_when_single_msg);

  send_some_messages(q, 10);
  cr_assert_eq(stats_counter_get(q->memory_usage), 0);
  log_queue_rewind_backlog_all(q);
  cr_assert_eq(stats_counter_get(q->memory_usage), 10*size_when_single_msg);

  log_queue_unref(q);
}

static void
_register_stats_counters(LogQueue *q)
{
  StatsClusterKey sc_key;
  stats_cluster_logpipe_key_set(&sc_key, SCS_DESTINATION, q->persist_name, NULL);

  stats_lock();
  log_queue_register_stats_counters(q, 0, &sc_key);
  stats_unlock();
}

static void
_unregister_stats_counters(LogQueue *q)
{
  StatsClusterKey sc_key;
  stats_cluster_logpipe_key_set(&sc_key, SCS_DESTINATION, q->persist_name, NULL);

  stats_lock();
  log_queue_unregister_stats_counters(q, &sc_key);
  stats_unlock();
}

Test(logqueue, log_queue_fifo_should_drop_only_non_flow_controlled_messages,
     .description = "Flow-controlled messages should never be dropped")
{
  LogPathOptions flow_controlled_path = LOG_PATH_OPTIONS_INIT;
  flow_controlled_path.flow_control_requested = TRUE;

  LogPathOptions non_flow_controlled_path = LOG_PATH_OPTIONS_INIT;
  non_flow_controlled_path.flow_control_requested = FALSE;

  gint fifo_size = 5;
  LogQueue *q = log_queue_fifo_new(fifo_size, NULL);
  log_queue_set_use_backlog(q, TRUE);
  _register_stats_counters(q);

  fed_messages = 0;
  acked_messages = 0;
  feed_empty_messages(q, &flow_controlled_path, fifo_size);
  feed_empty_messages(q, &non_flow_controlled_path, fifo_size);

  feed_empty_messages(q, &non_flow_controlled_path, 1);
  feed_empty_messages(q, &flow_controlled_path, fifo_size);
  feed_empty_messages(q, &non_flow_controlled_path, 2);
  feed_empty_messages(q, &flow_controlled_path, fifo_size);

  cr_assert_eq(stats_counter_get(q->dropped_messages), 3);

  gint queued_messages = stats_counter_get(q->queued_messages);
  send_some_messages(q, queued_messages);
  log_queue_ack_backlog(q, queued_messages);

  cr_assert_eq(fed_messages, acked_messages,
               "did not receive enough acknowledgements: fed_messages=%d, acked_messages=%d",
               fed_messages, acked_messages);

  _unregister_stats_counters(q);
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

  main_loop_worker_thread_start(NULL);

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
  log_queue_set_max_threads(1);
  LogQueue *q = log_queue_fifo_new(fifo_size, NULL);
  log_queue_set_use_backlog(q, TRUE);
  _register_stats_counters(q);

  GThread *thread = g_thread_create(_flow_control_feed_thread, q, TRUE, NULL);
  g_thread_join(thread);

  cr_assert_eq(stats_counter_get(q->dropped_messages), 3);

  gint queued_messages = stats_counter_get(q->queued_messages);
  send_some_messages(q, queued_messages);
  log_queue_ack_backlog(q, queued_messages);

  cr_assert_eq(fed_messages, acked_messages,
               "did not receive enough acknowledgements: fed_messages=%d, acked_messages=%d",
               fed_messages, acked_messages);

  _unregister_stats_counters(q);
  log_queue_unref(q);
}
