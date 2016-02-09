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
#include "testutils.h"

#include <stdlib.h>
#include <string.h>
#include <iv.h>
#include <iv_thread.h>

#define OVERFLOW_SIZE 10000
#ifdef PATH_QDISK
  #undef PATH_QDISK
#endif
#define PATH_QDISK "./"

MsgFormatOptions parse_options;

static void
testcase_zero_diskbuf_and_normal_acks()
{
  LogQueue *q;
  gint i;
  GString *filename;
  DiskQueueOptions options = {0};

  _construct_options(&options, 10000000, 100000, TRUE);

  q = log_queue_disk_reliable_new(&options);
  log_queue_set_use_backlog(q, TRUE);

  filename = g_string_sized_new(32);
  g_string_sprintf(filename,"test-normal_acks.qf");
  unlink(filename->str);
  log_queue_disk_load_queue(q,filename->str);
  fed_messages = 0;
  acked_messages = 0;
  for (i = 0; i < 10; i++)
    feed_some_messages(q, 10, &parse_options);

  send_some_messages(q, fed_messages);
  app_ack_some_messages(q, fed_messages);
  assert_gint(fed_messages, acked_messages, "%s: did not receive enough acknowledgements: fed_messages=%d, acked_messages=%d\n", __FUNCTION__, fed_messages, acked_messages);

  log_queue_unref(q);
  unlink(filename->str);
  g_string_free(filename,TRUE);
  disk_queue_options_destroy(&options);
}

static void
testcase_zero_diskbuf_alternating_send_acks()
{
  LogQueue *q;
  gint i;
  GString *filename;
  DiskQueueOptions options = {0};

  _construct_options(&options, 10000000, 100000, TRUE);

  q = log_queue_disk_reliable_new(&options);
  log_queue_set_use_backlog(q, TRUE);

  filename = g_string_sized_new(32);
  g_string_sprintf(filename,"test-send_acks.qf");
  unlink(filename->str);
  log_queue_disk_load_queue(q,filename->str);
  fed_messages = 0;
  acked_messages = 0;
  for (i = 0; i < 10; i++)
    {
      feed_some_messages(q, 10, &parse_options);
      send_some_messages(q, 10);
      app_ack_some_messages(q, 10);
    }

  assert_gint(fed_messages, acked_messages, "%s: did not receive enough acknowledgements: fed_messages=%d, acked_messages=%d\n", __FUNCTION__, fed_messages, acked_messages);
  log_queue_unref(q);
  unlink(filename->str);
  g_string_free(filename,TRUE);
  disk_queue_options_destroy(&options);
}

static void
testcase_ack_and_rewind_messages()
{
  LogQueue *q;
  gint i;
  GString *filename;
  DiskQueueOptions options = {0};

  _construct_options(&options, 10000000, 100000, TRUE);

  q = log_queue_disk_reliable_new(&options);
  log_queue_set_use_backlog(q, TRUE);

  filename = g_string_sized_new(32);
  g_string_sprintf(filename,"test-rewind_and_acks.qf");
  unlink(filename->str);
  log_queue_disk_load_queue(q,filename->str);

  fed_messages = 0;
  acked_messages = 0;
  feed_some_messages(q, 1000, &parse_options);
  for(i = 0; i < 10; i++)
    {
      send_some_messages(q,1);
      app_rewind_some_messages(q,1);
    }
  send_some_messages(q,1000);
  app_ack_some_messages(q,500);
  app_rewind_some_messages(q,500);
  send_some_messages(q,500);
  app_ack_some_messages(q,500);
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
  char *msg_str = "<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: árvíztűrőtükörfúrógép";
  gint msg_len = strlen(msg_str);
  gint i;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  LogMessage *msg, *tmpl;
  GTimeVal start, end;
  GSockAddr *sa;
  glong diff;

  iv_init();

  /* emulate main loop for LogQueue */
  main_loop_worker_thread_start(NULL);

  sa = g_sockaddr_inet_new("10.10.10.10", 1010);
  tmpl = log_msg_new(msg_str, msg_len, sa, &parse_options);
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
  main_loop_worker_thread_stop();
  log_msg_unref(tmpl);
  g_sockaddr_unref(sa);
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

  for (i = 0; i < MESSAGES_SUM; i++)
    {
      LogMessage *msg = NULL;
      gint slept = 0;

      while(!msg)
        {
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

  return NULL;
}


static void
testcase_with_threads()
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

      q = log_queue_disk_reliable_new(&options);
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

int
main()
{
#if _AIX
  fprintf(stderr,"On AIX this testcase can't executed, because the overriding of main_loop_io_worker_register_finish_callback does not work\n");
  return 0;
#endif
  app_startup();
  putenv("TZ=MET-1METDST");
  tzset();

  configuration = cfg_new(0x308);
  plugin_load_module("syslogformat", configuration, NULL);
  plugin_load_module("disk-buffer", configuration, NULL);
  plugin_load_module("builtin-serializer", configuration, NULL);
  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, configuration);

  testcase_ack_and_rewind_messages();
  testcase_with_threads();

  testcase_zero_diskbuf_alternating_send_acks();
  testcase_zero_diskbuf_and_normal_acks();

  return 0;
}
