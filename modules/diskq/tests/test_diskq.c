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

#include "logqueue.h"
#include "logqueue-fifo.h"
#include "logqueue-disk.h"
#include "logqueue-disk-reliable.h"
#include "logqueue-disk-non-reliable.h"
#include "diskq.h"
#include "logpipe.h"
#include "apphook.h"
#include "plugin.h"
#include "mainloop.h"
#include "mainloop-call.h"
#include "mainloop-io-worker.h"
#include "tls-support.h"
#include "queue_utils_lib.h"
#include "test_diskq_tools.h"
#include <criterion/criterion.h>
#include <criterion/parameterized.h>
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

  q = log_queue_disk_reliable_new(&options, NULL);
  log_queue_set_use_backlog(q, TRUE);

  filename = g_string_sized_new(32);
  g_string_sprintf(filename,"test-normal_acks.qf");
  unlink(filename->str);
  log_queue_disk_load_queue(q,filename->str);
  fed_messages = 0;
  acked_messages = 0;
  for (i = 0; i < 10; i++)
    feed_some_messages(q, 10);

  send_some_messages(q, fed_messages);
  log_queue_ack_backlog(q, fed_messages);
  cr_assert_eq(fed_messages, acked_messages,
               "%s: did not receive enough acknowledgements: fed_messages=%d, acked_messages=%d\n", __FUNCTION__, fed_messages,
               acked_messages);

  log_queue_unref(q);
  unlink(filename->str);
  g_string_free(filename,TRUE);
  disk_queue_options_destroy(&options);
}

Test(diskq, testcase_zero_diskbuf_alternating_send_acks)
{
  LogQueue *q;
  gint i;
  GString *filename;
  DiskQueueOptions options = {0};

  _construct_options(&options, 10000000, 100000, TRUE);

  q = log_queue_disk_reliable_new(&options, NULL);
  log_queue_set_use_backlog(q, TRUE);

  filename = g_string_sized_new(32);
  g_string_sprintf(filename,"test-send_acks.qf");
  unlink(filename->str);
  log_queue_disk_load_queue(q,filename->str);
  fed_messages = 0;
  acked_messages = 0;
  for (i = 0; i < 10; i++)
    {
      feed_some_messages(q, 10);
      send_some_messages(q, 10);
      log_queue_ack_backlog(q, 10);
    }

  cr_assert_eq(fed_messages, acked_messages,
               "%s: did not receive enough acknowledgements: fed_messages=%d, acked_messages=%d\n", __FUNCTION__, fed_messages,
               acked_messages);
  log_queue_unref(q);
  unlink(filename->str);
  g_string_free(filename,TRUE);
  disk_queue_options_destroy(&options);
}

Test(diskq, testcase_ack_and_rewind_messages)
{
  LogQueue *q;
  gint i;
  GString *filename;
  DiskQueueOptions options = {0};

  _construct_options(&options, 10000000, 100000, TRUE);

  q = log_queue_disk_reliable_new(&options, NULL);
  log_queue_set_use_backlog(q, TRUE);

  StatsClusterKey sc_key;
  stats_cluster_logpipe_key_set(&sc_key, SCS_DESTINATION, "queued messages", NULL );
  stats_lock();
  stats_register_counter(0, &sc_key, SC_TYPE_QUEUED, &q->queued_messages);
  stats_unlock();

  cr_assert_eq(stats_counter_get(q->queued_messages), 0, "queued messages: %d", __LINE__);

  filename = g_string_sized_new(32);
  g_string_sprintf(filename,"test-rewind_and_acks.qf");
  unlink(filename->str);
  log_queue_disk_load_queue(q,filename->str);

  fed_messages = 0;
  acked_messages = 0;
  feed_some_messages(q, 1000);
  cr_assert_eq(stats_counter_get(q->queued_messages), 1000, "queued messages: %d", __LINE__);

  for(i = 0; i < 10; i++)
    {
      send_some_messages(q,1);
      cr_assert_eq(stats_counter_get(q->queued_messages), 999, "queued messages wrong number %d", __LINE__);
      log_queue_rewind_backlog(q,1);
      cr_assert_eq(stats_counter_get(q->queued_messages), 1000, "queued messages wrong number: %d", __LINE__);
    }
  send_some_messages(q,1000);
  cr_assert_eq(stats_counter_get(q->queued_messages), 0, "queued messages: %d", __LINE__);
  log_queue_ack_backlog(q,500);
  log_queue_rewind_backlog(q,500);
  cr_assert_eq(stats_counter_get(q->queued_messages), 500, "queued messages: %d", __LINE__);
  send_some_messages(q,500);
  cr_assert_eq(stats_counter_get(q->queued_messages), 0, "queued messages: %d", __LINE__);
  log_queue_ack_backlog(q,500);
  log_queue_unref(q);
  unlink(filename->str);
  g_string_free(filename,TRUE);
  disk_queue_options_destroy(&options);
}

#define FEEDERS 1
#define MESSAGES_PER_FEEDER 10000
#define MESSAGES_SUM (FEEDERS * MESSAGES_PER_FEEDER)
#define TEST_RUNS 10

GStaticMutex tlock;
glong sum_time;

static gpointer
threaded_feed(gpointer args)
{
  LogQueue *q = (LogQueue *)args;
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
      main_loop_worker_run_gc();
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
  main_loop_worker_thread_stop();
  log_msg_unref(tmpl);
  return NULL;
}

static gpointer
threaded_consume(gpointer st)
{
  LogQueue *q = (LogQueue *) st;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  gint i;

  /* just to make sure time is properly cached */
  iv_init();
  main_loop_worker_thread_start(NULL);

  for (i = 0; i < MESSAGES_SUM; i++)
    {
      LogMessage *msg = NULL;
      gint slept = 0;

      while(!msg)
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

  return NULL;
}


Test(diskq, testcase_with_threads)
{
  LogQueue *q;
  GThread *thread_feed[FEEDERS], *thread_consume;
  GString *filename;
  gint i, j;

  log_queue_set_max_threads(FEEDERS);
  for (i = 0; i < TEST_RUNS; i++)
    {
      DiskQueueOptions options = {0};

      _construct_options(&options, 10000000, 100000, TRUE);

      q = log_queue_disk_reliable_new(&options, NULL);
      filename = g_string_sized_new(32);
      g_string_sprintf(filename,"test-%04d.qf",i);
      unlink(filename->str);
      log_queue_disk_load_queue(q,filename->str);

      for (j = 0; j < FEEDERS; j++)
        {
          thread_feed[j] = g_thread_create(threaded_feed, q, TRUE, NULL);
        }

      thread_consume = g_thread_create(threaded_consume, q, TRUE, NULL);

      for (j = 0; j < FEEDERS; j++)
        {
          g_thread_join(thread_feed[j]);
        }
      g_thread_join(thread_consume);

      log_queue_unref(q);
      unlink(filename->str);
      g_string_free(filename,TRUE);
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
queue_new(gboolean reliable, DiskQueueOptions *options, const gchar *persist_name)
{
  if (reliable)
    return log_queue_disk_reliable_new(options, persist_name);

  return log_queue_disk_non_reliable_new(options, persist_name);
}

Test(diskq, testcase_diskbuffer_restart_corrupted)
{

  restart_test_parameters parameters[] =
  {
    {"test-diskq-restart.qf", FALSE},
    {"test-diskq-restart.rqf", TRUE},
  };

  guint64 const original_disk_buf_size = 1000123;
  gsize number_of_variants = sizeof(parameters)/sizeof(restart_test_parameters);
  for (gsize i = 0; i < number_of_variants; i++)
    {
      DiskQueueOptions options;
      _construct_options(&options, original_disk_buf_size, 100000, parameters[i].reliable);
      LogQueue *q = queue_new(parameters[i].reliable, &options, NULL);
      log_queue_set_use_backlog(q, FALSE);

      gchar *filename = parameters[i].filename;
      gchar filename_corrupted_dq[100];
      g_snprintf(filename_corrupted_dq, 100, "%s.corrupted", filename);
      unlink(filename);
      unlink(filename_corrupted_dq);

      log_queue_disk_load_queue(q, filename);
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
      assert_string(qdisk_get_filename(disk_queue->qdisk), filename,
                    "New disk-queue file's name should be the same\n");
      cr_assert_eq(qdisk_get_size(disk_queue->qdisk), original_disk_buf_size,
                   "Disk-queue option does not match the original configured value!\n");
      cr_assert_eq(qdisk_get_length(disk_queue->qdisk), 0,
                   "New disk-queue file should be empty!\n");
      cr_assert_eq(qdisk_get_writer_head(disk_queue->qdisk), QDISK_RESERVED_SPACE,
                   "Invalid write pointer!\n");
      cr_assert_eq(qdisk_get_reader_head(disk_queue->qdisk), QDISK_RESERVED_SPACE,
                   "Invalid read pointer!\n");

      log_queue_unref(q);
      unlink(filename);
      unlink(filename_corrupted_dq);
      disk_queue_options_destroy(&options);
    }
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

struct diskq_tester_parameters;

typedef LogQueue *(*queue_constructor_t)(DiskQueueOptions *, const gchar *);
typedef void (*first_msg_asserter_t)(LogQueue *q, struct diskq_tester_parameters *parameters);
typedef void (*second_msg_asserter_t)(LogQueue *q, struct diskq_tester_parameters *parameters, gssize one_msg_size);

typedef struct diskq_tester_parameters
{
  guint32 disk_size;
  gboolean reliable;
  gboolean overflow_expected;
  queue_constructor_t constructor;
  first_msg_asserter_t first_msg_asserter;
  second_msg_asserter_t second_msg_asserter;
  guint32 qout_size;
  const gchar *filename;
} diskq_tester_parameters_t;

void
init_statistics(LogQueue *q)
{

  StatsClusterKey sc_key1, sc_key2;
  stats_lock();
  stats_cluster_logpipe_key_set(&sc_key1, SCS_DESTINATION, "queued messages", NULL );
  stats_register_counter(0, &sc_key1, SC_TYPE_QUEUED, &q->queued_messages);
  stats_cluster_logpipe_key_set(&sc_key2, SCS_DESTINATION, "memory usage", NULL );
  stats_register_counter(1, &sc_key2, SC_TYPE_MEMORY_USAGE, &q->memory_usage);
  stats_unlock();
  stats_counter_set(q->queued_messages, 0);
  stats_counter_set(q->memory_usage, 0);
}

static void
assert_general_message_flow(LogQueue *q, gssize one_msg_size)
{
  send_some_messages(q,1);
  cr_assert_eq(stats_counter_get(q->queued_messages), 1, "queued messages: line: %d", __LINE__);
  cr_assert_eq(stats_counter_get(q->memory_usage), one_msg_size, "memory_usage: line: %d", __LINE__);

  send_some_messages(q,1);
  cr_assert_eq(stats_counter_get(q->queued_messages), 0, "queued messages: line: %d", __LINE__);
  cr_assert_eq(stats_counter_get(q->memory_usage), 0, "memory_usage: line: %d", __LINE__);

  feed_some_messages(q, 10);
  cr_assert_eq(stats_counter_get(q->queued_messages), 10, "queued messages: line: %d", __LINE__);
  cr_assert_eq(stats_counter_get(q->memory_usage), one_msg_size*10, "memory_usage: line: %d", __LINE__);

  send_some_messages(q,5);
  cr_assert_eq(stats_counter_get(q->queued_messages), 5, "queued messages: line: %d", __LINE__);
  cr_assert_eq(stats_counter_get(q->memory_usage), one_msg_size*5, "memory_usage: line: %d", __LINE__);
}

static LogQueue *
testcase_diskq_prepare(DiskQueueOptions *options, diskq_tester_parameters_t *parameters)
{
  LogQueue *q;

  _construct_options(options, parameters->disk_size, 100000, parameters->reliable);
  options->qout_size = parameters->qout_size;

  q = parameters->constructor(options, NULL);

  init_statistics(q);
  cr_assert_eq(stats_counter_get(q->queued_messages), 0, "queued messages: line: %d", __LINE__);
  cr_assert_eq(stats_counter_get(q->memory_usage), 0, "memory_usage: line: %d", __LINE__);

  unlink(parameters->filename);
  log_queue_disk_load_queue(q,parameters->filename);

  return q;
}

static void
assert_first_message_reliable(LogQueue *q, diskq_tester_parameters_t *parameters)
{
  feed_some_messages(q, 1);
  cr_assert_eq(stats_counter_get(q->queued_messages), 1, "queued messages: line: %d", __LINE__);
  /* For reliable queue we have qout = 0, so messages go to the overflow area.
     But qreliable pushes 3 elements per message: msg, options, position */
  if (parameters->overflow_expected)
    cr_assert_eq(((LogQueueDiskReliable *)q)->qreliable->length, 3, "one message on overflow area: line: %d", __LINE__);
}

static void
assert_first_message_non_reliable(LogQueue *q, diskq_tester_parameters_t *parameters)
{
  LogQueueDiskNonReliable *queue = (LogQueueDiskNonReliable *)q;
  feed_some_messages(q, 1);
  cr_assert_eq(stats_counter_get(q->queued_messages), 1, "queued messages: line: %d", __LINE__);
  cr_assert_eq(queue->qoverflow->length, 0, "one message on overflow area: line: %d", __LINE__);
}

static void
assert_second_message_reliable(LogQueue *q, diskq_tester_parameters_t *parameters, gssize one_msg_size)
{
  LogQueueDiskReliable *queue = (LogQueueDiskReliable *)q;
  feed_some_messages(q, 1);
  cr_assert_eq(stats_counter_get(q->queued_messages), 2, "queued messages: line: %d", __LINE__);
  cr_assert_eq(stats_counter_get(q->memory_usage), one_msg_size*2, "memory_usage: line: %d", __LINE__);

  if (parameters->overflow_expected)
    cr_assert_eq(queue->qreliable->length, 6, "one message on overflow area: line: %d", __LINE__);
}

static void
assert_second_message_non_reliable(LogQueue *q, diskq_tester_parameters_t *parameters, gssize one_msg_size)
{
  feed_some_messages(q, 1);
  cr_assert_eq(stats_counter_get(q->queued_messages), 2, "queued messages: line: %d", __LINE__);
  cr_assert_eq(stats_counter_get(q->memory_usage), one_msg_size*2, "memory_usage: line: %d", __LINE__);

  /* qout must be 1 for nonreliable case so we have only one message. But nonreliable pushes two elements: msg + options */
  cr_assert_eq(((LogQueueDiskNonReliable *)q)->qoverflow->length, 2, "one message on overflow area: line: %d", __LINE__);
}


static void
testcase_diskq_statistics(diskq_tester_parameters_t parameters)
{
  LogQueue *q;
  DiskQueueOptions options = {0};

  q = testcase_diskq_prepare(&options, &parameters);

  parameters.first_msg_asserter(q, &parameters);

  guint32 one_msg_size = stats_counter_get(q->memory_usage);
  if (parameters.overflow_expected)
    /* Only when overflow. If there is no overflow, the first
       msg is put to the output queue so statistics is not increased: one_msg_size == 0 */
    cr_assert(is_valid_msg_size(one_msg_size), "one_msg_size %d: line: %d", one_msg_size, __LINE__);
  else
    cr_assert_eq(stats_counter_get(q->memory_usage), 0, "queued messages: line: %d", __LINE__);

  parameters.second_msg_asserter(q, &parameters, one_msg_size);

  assert_general_message_flow(q, one_msg_size);

  unlink(parameters.filename);

  log_queue_unref(q);
  disk_queue_options_destroy(&options);
}

ParameterizedTestParameters(diskq, test_diskq_statistics)
{
  static diskq_tester_parameters_t test_cases[] =
  {
    // small enough to trigger overflow
    { 10*1024, TRUE, TRUE, log_queue_disk_reliable_new, assert_first_message_reliable, assert_second_message_reliable, 0, "file1.qf" },

    // no overflow
    { 500*1024, TRUE, FALSE, log_queue_disk_reliable_new, assert_first_message_reliable, assert_second_message_reliable, 0, "file2.qf" },

    // nonreliable version moves msgs from qoverflow only if there is free space in qout: qout_size must be 1
    { 1*1024, FALSE, TRUE, log_queue_disk_non_reliable_new, assert_first_message_non_reliable, assert_second_message_non_reliable, 1, "file3.qf" }
  };

  return cr_make_param_array(diskq_tester_parameters_t, test_cases, sizeof(test_cases) / sizeof(test_cases[0]));
}

ParameterizedTest(diskq_tester_parameters_t *test_cases, diskq, test_diskq_statistics)
{
  testcase_diskq_statistics((diskq_tester_parameters_t)
  {
    .disk_size = test_cases->disk_size,
    .reliable = test_cases->reliable,
    .overflow_expected = test_cases->overflow_expected,
    .constructor = test_cases->constructor,
    .first_msg_asserter = test_cases->first_msg_asserter,
    .second_msg_asserter = test_cases->second_msg_asserter,
    .qout_size = test_cases->qout_size,
    .filename = test_cases->filename
  });
}

static void
setup(void)
{
#if _AIX
  fprintf(stderr,
          "On AIX this testcase can't executed, because the overriding of main_loop_io_worker_register_finish_callback does not work\n");
  return 0;
#endif
  app_startup();
  setenv("TZ", "MET-1METDST", TRUE);
  tzset();

  configuration = cfg_new_snippet();
  configuration->stats_options.level = 1;

  assert_true(cfg_init(configuration), "cfg_init failed!");
}

static void
teardown(void)
{
  cfg_free(configuration);
  app_shutdown();
}

TestSuite(diskq, .init = setup, .fini = teardown);
