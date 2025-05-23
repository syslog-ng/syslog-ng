/*
 * Copyright (c) 2002-2016 Balabit
 * Copyright (c) 2016 Viktor Juhasz <viktor.juhasz@balabit.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include <criterion/criterion.h>
#include <criterion/parameterized.h>
#include "libtest/queue_utils_lib.h"
#include "test_diskq_tools.h"

#include "logqueue.h"
#include "logqueue-fifo.h"
#include "logqueue-disk.h"
#include "logqueue-disk-reliable.h"
#include "logqueue-disk-non-reliable.h"
#include "stats/stats-cluster-single.h"
#include "diskq.h"
#include "logpipe.h"
#include "apphook.h"
#include "plugin.h"
#include "mainloop.h"
#include "mainloop-call.h"
#include "mainloop-io-worker.h"
#include "tls-support.h"
#include "timeutils/misc.h"
#include <stdlib.h>
#include <string.h>
#include <iv.h>
#include <iv_thread.h>
#include <sys/stat.h>

#define OVERFLOW_SIZE 10000
#ifdef PATH_QDISK
#undef PATH_QDISK
#endif
#define PATH_QDISK "./"

Test(diskq, testcase_zero_diskbuf_and_normal_acks)
{
  LogQueue *q;
  gint i;
  GString *filename;
  DiskQueueOptions options = {0};

  _construct_options(&options, 10000000, 100000, TRUE);

  filename = g_string_sized_new(32);
  g_string_printf(filename, "test-normal_acks.qf");
  unlink(filename->str);
  q = log_queue_disk_reliable_new(&options, filename->str, NULL, STATS_LEVEL0, NULL, NULL);

  log_queue_disk_start(q);
  fed_messages = 0;
  acked_messages = 0;
  for (i = 0; i < 10; i++)
    feed_some_messages(q, 10);

  send_some_messages(q, fed_messages, TRUE);
  cr_assert_eq(fed_messages, acked_messages,
               "%s: did not receive enough acknowledgements: fed_messages=%d, acked_messages=%d\n", __FUNCTION__, fed_messages,
               acked_messages);

  gboolean persistent;
  log_queue_disk_stop(q, &persistent);
  log_queue_unref(q);
  unlink(filename->str);
  g_string_free(filename, TRUE);
  disk_queue_options_destroy(&options);
}

Test(diskq, testcase_zero_diskbuf_alternating_send_acks)
{
  LogQueue *q;
  gint i;
  GString *filename;
  DiskQueueOptions options = {0};

  _construct_options(&options, 10000000, 100000, TRUE);

  filename = g_string_sized_new(32);
  g_string_printf(filename, "test-send_acks.qf");
  unlink(filename->str);
  q = log_queue_disk_reliable_new(&options, filename->str, NULL, STATS_LEVEL0, NULL, NULL);
  log_queue_disk_start(q);

  fed_messages = 0;
  acked_messages = 0;
  for (i = 0; i < 10; i++)
    {
      feed_some_messages(q, 10);
      send_some_messages(q, 10, TRUE);
    }

  cr_assert_eq(fed_messages, acked_messages,
               "%s: did not receive enough acknowledgements: fed_messages=%d, acked_messages=%d\n", __FUNCTION__, fed_messages,
               acked_messages);

  gboolean persistent;
  log_queue_disk_stop(q, &persistent);
  log_queue_unref(q);
  unlink(filename->str);
  g_string_free(filename, TRUE);
  disk_queue_options_destroy(&options);
}

Test(diskq, testcase_ack_and_rewind_messages)
{
  LogQueue *q;
  gint i;
  GString *filename;
  DiskQueueOptions options = {0};

  _construct_options(&options, 10000000, 100000, TRUE);

  filename = g_string_sized_new(32);
  g_string_printf(filename, "test-rewind_and_acks.qf");
  unlink(filename->str);
  StatsClusterKeyBuilder *driver_sck_builder = stats_cluster_key_builder_new();
  StatsClusterKeyBuilder *queue_sck_builder = stats_cluster_key_builder_new();
  q = log_queue_disk_reliable_new(&options, filename->str, NULL, STATS_LEVEL0, driver_sck_builder, queue_sck_builder);
  stats_cluster_key_builder_free(driver_sck_builder);
  stats_cluster_key_builder_free(queue_sck_builder);

  cr_assert_eq(stats_counter_get(q->metrics.shared.queued_messages), 0, "queued messages: %d", __LINE__);

  log_queue_disk_start(q);

  fed_messages = 0;
  acked_messages = 0;
  feed_some_messages(q, 1000);
  cr_assert_eq(stats_counter_get(q->metrics.shared.queued_messages), 1000, "queued messages: %d", __LINE__);

  for (i = 0; i < 10; i++)
    {
      send_some_messages(q, 1, FALSE);
      cr_assert_eq(stats_counter_get(q->metrics.shared.queued_messages), 999, "queued messages wrong number %d", __LINE__);
      log_queue_rewind_backlog(q, 1);
      cr_assert_eq(stats_counter_get(q->metrics.shared.queued_messages), 1000, "queued messages wrong number: %d", __LINE__);
    }
  send_some_messages(q, 1000, FALSE);
  cr_assert_eq(stats_counter_get(q->metrics.shared.queued_messages), 0, "queued messages: %d", __LINE__);
  log_queue_ack_backlog(q, 500);
  log_queue_rewind_backlog(q, 500);
  cr_assert_eq(stats_counter_get(q->metrics.shared.queued_messages), 500, "queued messages: %d", __LINE__);
  send_some_messages(q, 500, FALSE);
  cr_assert_eq(stats_counter_get(q->metrics.shared.queued_messages), 0, "queued messages: %d", __LINE__);
  log_queue_ack_backlog(q, 500);

  gboolean persistent;
  log_queue_disk_stop(q, &persistent);
  log_queue_unref(q);
  unlink(filename->str);
  g_string_free(filename, TRUE);
  disk_queue_options_destroy(&options);
}

#define FEEDERS 1
#define MESSAGES_PER_FEEDER 10000
#define MESSAGES_SUM (FEEDERS * MESSAGES_PER_FEEDER)
#define TEST_RUNS 10

GMutex tlock;
glong sum_time;

static gpointer
threaded_feed(gpointer args)
{
  LogQueue *q = (LogQueue *)args;
  gint i;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  LogMessage *msg, *tmpl;
  struct timespec start, end;
  gint64 diff;

  iv_init();

  /* emulate main loop for LogQueue */
  main_loop_worker_thread_start(MLW_ASYNC_WORKER);

  tmpl = log_msg_new_empty();
  clock_gettime(CLOCK_MONOTONIC, &start);
  for (i = 0; i < MESSAGES_PER_FEEDER; i++)
    {
      main_loop_worker_run_gc();
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
  main_loop_worker_thread_stop();
  log_msg_unref(tmpl);
  iv_deinit();
  return NULL;
}

static gpointer
threaded_consume(gpointer st)
{
  LogQueue *q = (LogQueue *) st;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  gint i;

  iv_init();

  /* just to make sure time is properly cached */
  main_loop_worker_thread_start(MLW_ASYNC_WORKER);

  for (i = 0; i < MESSAGES_SUM; i++)
    {
      LogMessage *msg = NULL;
      gint slept = 0;

      while (!msg)
        {
          main_loop_worker_run_gc();

          msg = log_queue_pop_head(q, &path_options);
          if (!msg)
            {
              struct timespec ns;

              /* sleep 1 msec */
              ns.tv_sec = 0;
              ns.tv_nsec = 1000000;
              nanosleep(&ns, NULL);
              slept++;
              if (slept > 10000)
                {
                  return GUINT_TO_POINTER(1);
                }
            }
        }

      log_msg_ack(msg, &path_options, AT_PROCESSED);
      log_msg_unref(msg);
    }

  main_loop_worker_thread_stop();

  iv_deinit();
  return NULL;
}


Test(diskq, testcase_with_threads)
{
  LogQueue *q;
  GThread *thread_feed[FEEDERS], *thread_consume;
  GString *filename;
  gint i, j;

  main_loop_worker_allocate_thread_space(FEEDERS);
  main_loop_worker_finalize_thread_space();

  for (i = 0; i < TEST_RUNS; i++)
    {
      DiskQueueOptions options = {0};

      _construct_options(&options, 10000000, 100000, TRUE);

      filename = g_string_sized_new(32);
      g_string_printf(filename, "test-%04d.qf", i);
      unlink(filename->str);
      q = log_queue_disk_reliable_new(&options, filename->str, NULL, STATS_LEVEL0, NULL, NULL);
      log_queue_disk_start(q);

      for (j = 0; j < FEEDERS; j++)
        {
          thread_feed[j] = g_thread_new(NULL, threaded_feed, q);
        }

      thread_consume = g_thread_new(NULL, threaded_consume, q);

      for (j = 0; j < FEEDERS; j++)
        {
          g_thread_join(thread_feed[j]);
        }
      g_thread_join(thread_consume);

      gboolean persistent;
      log_queue_disk_stop(q, &persistent);
      log_queue_unref(q);
      unlink(filename->str);
      g_string_free(filename, TRUE);
      disk_queue_options_destroy(&options);

    }
  fprintf(stderr, "Feed speed: %.2lf\n", (double) TEST_RUNS * MESSAGES_SUM * 1000000 / sum_time);
}

typedef struct restart_test_parameters
{
  gchar *filename;
  gboolean reliable;
} restart_test_parameters;

static LogQueue *
queue_new(gboolean reliable, DiskQueueOptions *options, const gchar *filename, const gchar *persist_name)
{
  if (reliable)
    return log_queue_disk_reliable_new(options, filename, persist_name, STATS_LEVEL0, NULL, NULL);

  return log_queue_disk_non_reliable_new(options, filename, persist_name, STATS_LEVEL0, NULL, NULL);
}

ParameterizedTestParameters(diskq, testcase_diskbuffer_restart_corrupted)
{
  static restart_test_parameters test_cases[] =
  {
    {"test-diskq-restart.qf", FALSE},
    {"test-diskq-restart.rqf", TRUE},
  };

  return cr_make_param_array(restart_test_parameters, test_cases, sizeof(test_cases) / sizeof(test_cases[0]));
}

ParameterizedTest(restart_test_parameters *test_case, diskq, testcase_diskbuffer_restart_corrupted)
{
  guint64 const original_dcapacity = 1000123;
  DiskQueueOptions options;
  _construct_options(&options, original_dcapacity, 100000, test_case->reliable);
  gchar *filename = test_case->filename;
  LogQueue *q = queue_new(test_case->reliable, &options, filename, NULL);

  gchar filename_corrupted_dq[100];
  g_snprintf(filename_corrupted_dq, 100, "%s.corrupted", filename);
  unlink(filename);
  unlink(filename_corrupted_dq);

  log_queue_disk_start(q);
  fed_messages = 0;
  feed_some_messages(q, 100);
  cr_assert_eq(fed_messages, 100, "Failed to push all messages to the disk-queue!\n");

  LogQueueDisk *disk_queue = (LogQueueDisk *)q;
  log_queue_disk_restart_corrupted(disk_queue);

  struct stat file_stat;
  cr_assert_eq(stat(filename, &file_stat), 0,
               "New disk-queue file does not exists!!");
  cr_assert_eq(S_ISREG(file_stat.st_mode), TRUE,
               "New disk-queue file expected to be a regular file!! st_mode value=%04o",
               (file_stat.st_mode & S_IFMT));
  stat(filename_corrupted_dq, &file_stat);
  cr_assert_eq(S_ISREG(file_stat.st_mode), TRUE,
               "Corrupted disk-queue file does not exists!!");
  cr_assert_str_eq(qdisk_get_filename(disk_queue->qdisk), filename,
                   "New disk-queue file's name should be the same\n");
  cr_assert_eq(qdisk_get_maximum_size(disk_queue->qdisk), original_dcapacity,
               "Disk-queue option does not match the original configured value!\n");
  cr_assert_eq(qdisk_get_length(disk_queue->qdisk), 0,
               "New disk-queue file should be empty!\n");
  cr_assert_eq(qdisk_get_writer_head(disk_queue->qdisk), QDISK_RESERVED_SPACE,
               "Invalid write pointer!\n");
  cr_assert_eq(qdisk_get_reader_head(disk_queue->qdisk), QDISK_RESERVED_SPACE,
               "Invalid read pointer!\n");

  gboolean persistent;
  log_queue_disk_stop(q, &persistent);
  log_queue_unref(q);
  unlink(filename);
  unlink(filename_corrupted_dq);
  disk_queue_options_destroy(&options);
}

static gboolean
is_valid_msg_size(guint32 one_msg_size)
{
  static gssize empty_msg_size = 0;

  if (empty_msg_size == 0)
    {
      LogMessage *msg = log_msg_new_empty();
      empty_msg_size = log_msg_get_size(msg);
      log_msg_unref(msg);
    }

  return (one_msg_size == empty_msg_size);
}

typedef struct diskq_tester_parameters
{
  guint32 disk_size;
  gboolean reliable;
  gboolean overflow_expected;
  guint32 front_cache_size;
  const gchar *filename;
} diskq_tester_parameters_t;

static void
assert_general_message_flow(LogQueue *q, gssize one_msg_size)
{
  send_some_messages(q, 1, TRUE);
  cr_assert_eq(stats_counter_get(q->metrics.shared.queued_messages), 1, "queued messages: line: %d", __LINE__);
  cr_assert_eq(stats_counter_get(q->metrics.shared.memory_usage), one_msg_size, "memory_usage: line: %d", __LINE__);

  send_some_messages(q, 1, TRUE);
  cr_assert_eq(stats_counter_get(q->metrics.shared.queued_messages), 0, "queued messages: line: %d", __LINE__);
  cr_assert_eq(stats_counter_get(q->metrics.shared.memory_usage), 0, "memory_usage: line: %d", __LINE__);

  feed_some_messages(q, 10);
  cr_assert_eq(stats_counter_get(q->metrics.shared.queued_messages), 10, "queued messages: line: %d", __LINE__);
  cr_assert_eq(stats_counter_get(q->metrics.shared.memory_usage), one_msg_size*10, "memory_usage: line: %d", __LINE__);

  send_some_messages(q, 5, TRUE);
  cr_assert_eq(stats_counter_get(q->metrics.shared.queued_messages), 5, "queued messages: line: %d", __LINE__);
  cr_assert_eq(stats_counter_get(q->metrics.shared.memory_usage), one_msg_size*5, "memory_usage: line: %d", __LINE__);
}

static LogQueue *
testcase_diskq_prepare(DiskQueueOptions *options, diskq_tester_parameters_t *parameters)
{
  LogQueue *q;

  _construct_options(options, parameters->disk_size, 100000, parameters->reliable);
  options->front_cache_size = parameters->front_cache_size;

  StatsClusterKeyBuilder *driver_sck_builder = stats_cluster_key_builder_new();
  StatsClusterKeyBuilder *queue_sck_builder = stats_cluster_key_builder_new();
  if (parameters->reliable)
    q = log_queue_disk_reliable_new(options, parameters->filename, NULL, STATS_LEVEL0, driver_sck_builder,
                                    queue_sck_builder);
  else
    q = log_queue_disk_non_reliable_new(options, parameters->filename, NULL, STATS_LEVEL0, driver_sck_builder,
                                        queue_sck_builder);
  stats_cluster_key_builder_free(driver_sck_builder);
  stats_cluster_key_builder_free(queue_sck_builder);

  cr_assert_eq(stats_counter_get(q->metrics.shared.queued_messages), 0, "queued messages: line: %d", __LINE__);
  cr_assert_eq(stats_counter_get(q->metrics.shared.memory_usage), 0, "memory_usage: line: %d", __LINE__);

  unlink(parameters->filename);
  log_queue_disk_start(q);

  return q;
}

ParameterizedTestParameters(diskq, test_diskq_statistics)
{
  static diskq_tester_parameters_t test_cases[] =
  {
    // small enough to trigger overflow
    { .disk_size = 10*1024, .reliable = TRUE, .overflow_expected = TRUE, .front_cache_size = 0, .filename = "file1.qf" },

    // no overflow
    { .disk_size = 500*1024, .reliable = TRUE, .overflow_expected = FALSE, .front_cache_size = 0, .filename = "file2.qf" },

    // nonreliable version moves msgs from flow_control_window only if there is free space in front cache: front_cache_size must be 1
    { .disk_size = 1*1024, .reliable = FALSE, .overflow_expected = TRUE, .front_cache_size = 1, .filename = "file3.qf" }
  };

  return cr_make_param_array(diskq_tester_parameters_t, test_cases, sizeof(test_cases) / sizeof(test_cases[0]));
}

static inline void
assert_flow_control_window_length(diskq_tester_parameters_t *parameters, LogQueue *q, gsize sent_msgs)
{
  gsize expected_length = sent_msgs - parameters->front_cache_size;

  if (parameters->reliable)
    {
      cr_assert_eq(((LogQueueDiskReliable *)q)->flow_control_window->length, 3 * expected_length,
                   "%"G_GSIZE_FORMAT" message in flow control window: line: %d", expected_length, __LINE__);
      return;
    }

  cr_assert_eq(((LogQueueDiskNonReliable *)q)->flow_control_window->length, 2 * expected_length,
               "%"G_GSIZE_FORMAT" message in flow control window: line: %d", expected_length, __LINE__);
}

ParameterizedTest(diskq_tester_parameters_t *parameters, diskq, test_diskq_statistics)
{
  LogQueue *q;
  DiskQueueOptions options = {0};

  q = testcase_diskq_prepare(&options, parameters);

  feed_some_messages(q, 1);
  cr_assert_eq(stats_counter_get(q->metrics.shared.queued_messages), 1, "queued messages: line: %d", __LINE__);

  if (parameters->overflow_expected)
    assert_flow_control_window_length(parameters, q, 1);

  guint32 one_msg_size = stats_counter_get(q->metrics.shared.memory_usage);
  if (parameters->overflow_expected)
    /* Only when overflow. If there is no overflow, the first
       msg is put to the output queue so statistics is not increased: one_msg_size == 0 */
    cr_assert(is_valid_msg_size(one_msg_size), "one_msg_size %d: line: %d", one_msg_size, __LINE__);
  else
    cr_assert_eq(stats_counter_get(q->metrics.shared.memory_usage), 0, "queued messages: line: %d", __LINE__);

  feed_some_messages(q, 1);
  cr_assert_eq(stats_counter_get(q->metrics.shared.queued_messages), 2, "queued messages: line: %d", __LINE__);
  cr_assert_eq(stats_counter_get(q->metrics.shared.memory_usage), one_msg_size*2, "memory_usage: line: %d", __LINE__);

  if (parameters->overflow_expected)
    assert_flow_control_window_length(parameters, q, 2);

  assert_general_message_flow(q, one_msg_size);

  unlink(parameters->filename);

  gboolean persistent;
  log_queue_disk_stop(q, &persistent);
  log_queue_unref(q);
  disk_queue_options_destroy(&options);
}

gchar *
qdisk_get_next_filename(const gchar *dir, gboolean reliable)
{
  return NULL;
}

Test(diskq, test_no_next_filename_in_acquire)
{
  const gchar *queue_persist_name = "test_no_next_filename_in_acquire";
  const gchar *persist_filename = "test_no_next_filename_in_acquire.persist";
  configuration->state = persist_state_new(persist_filename);
  persist_state_start(configuration->state);

  LogDestDriver *driver = g_new0(LogDestDriver, 1);
  log_dest_driver_init_instance(driver, configuration);

  DiskQDestPlugin *plugin = diskq_dest_plugin_new();
  cr_assert(log_driver_add_plugin(&driver->super, (LogDriverPlugin *) plugin));
  cr_assert(log_pipe_init(&driver->super.super));

  cr_assert_eq(log_dest_driver_acquire_queue(driver, queue_persist_name, STATS_LEVEL0, NULL, NULL), NULL);

  cr_assert(log_pipe_deinit(&driver->super.super));
  cr_assert(log_pipe_unref(&driver->super.super));

  persist_state_cancel(configuration->state);
  unlink(persist_filename);
}

static void
setup(void)
{
#if defined (_AIX)
  fprintf(stderr,
          "On AIX this testcase can't executed, because the overriding of main_loop_io_worker_register_finish_callback does not work\n");
  return 0;
#endif
  app_startup();
  setenv("TZ", "MET-1METDST", TRUE);
  tzset();

  configuration = cfg_new_snippet();
  configuration->stats_options.level = 1;

  cr_assert(cfg_init(configuration));
}

static void
teardown(void)
{
  cfg_free(configuration);
  app_shutdown();
}

TestSuite(diskq, .init = setup, .fini = teardown);
