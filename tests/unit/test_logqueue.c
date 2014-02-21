#include "logqueue.h"
#include "logqueue-fifo.h"
#include "logpipe.h"
#include "apphook.h"
#include "plugin.h"
#include "mainloop.h"
#include "tls-support.h"
#include "mainloop-io-worker.h"

#include <stdlib.h>
#include <string.h>
#include <iv.h>
#include <iv_list.h>
#include <iv_thread.h>

int acked_messages = 0;
int fed_messages = 0;
MsgFormatOptions parse_options;

#define OVERFLOW_SIZE 10000

void
test_ack(LogMessage *msg, gpointer user_data)
{
  acked_messages++;
}

void
feed_some_messages(LogQueue **q, int n, gboolean ack_needed)
{
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  LogMessage *msg;
  gint i;

  path_options.ack_needed = ack_needed;
  for (i = 0; i < n; i++)
    {
      char *msg_str = "<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: árvíztűrőtükörfúrógép";
      GSockAddr *sa;

      sa = g_sockaddr_inet_new("10.10.10.10", 1010);
      msg = log_msg_new(msg_str, strlen(msg_str), sa, &parse_options);
      g_sockaddr_unref(sa);
      log_msg_add_ack(msg, &path_options);
      msg->ack_func = test_ack;
      log_queue_push_tail((*q), msg, &path_options);
      fed_messages++;
    }

}

void
send_some_messages(LogQueue *q, gint n, gboolean use_app_acks)
{
  gint i;
  LogMessage *msg;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

  for (i = 0; i < n; i++)
    {
      log_queue_pop_head(q, &msg, &path_options, use_app_acks, FALSE);
      log_msg_ack(msg, &path_options, AT_PROCESSED);
      log_msg_unref(msg);
    }
}

void
app_ack_some_messages(LogQueue *q, gint n)
{
  log_queue_ack_backlog(q, n);
}

void
rewind_messages(LogQueue *q)
{
  log_queue_rewind_backlog(q);
}

void
testcase_zero_diskbuf_and_normal_acks()
{
  LogQueue *q;
  gint i;

  q = log_queue_fifo_new(OVERFLOW_SIZE, NULL);
  fed_messages = 0;
  acked_messages = 0;
  for (i = 0; i < 10; i++)
    feed_some_messages(&q, 10, TRUE);

  send_some_messages(q, fed_messages, TRUE);
  app_ack_some_messages(q, fed_messages);
  if (fed_messages != acked_messages)
    {
      fprintf(stderr, "did not receive enough acknowledgements: fed_messages=%d, acked_messages=%d\n", fed_messages, acked_messages);
      exit(1);
    }

  log_queue_unref(q);
}

void
testcase_zero_diskbuf_alternating_send_acks()
{
  LogQueue *q;
  gint i;

  q = log_queue_fifo_new(OVERFLOW_SIZE, NULL);
  fed_messages = 0;
  acked_messages = 0;
  for (i = 0; i < 10; i++)
    {
      feed_some_messages(&q, 10, TRUE);
      send_some_messages(q, 10, TRUE);
      app_ack_some_messages(q, 10);
    }
  if (fed_messages != acked_messages)
    {
      fprintf(stderr, "did not receive enough acknowledgements: fed_messages=%d, acked_messages=%d\n", fed_messages, acked_messages);
      exit(1);
    }

  log_queue_unref(q);
}

#define FEEDERS 1
#define MESSAGES_PER_FEEDER 30000
#define MESSAGES_SUM (FEEDERS * MESSAGES_PER_FEEDER)
#define TEST_RUNS 10

GStaticMutex tlock;
glong sum_time;

gpointer
threaded_feed(gpointer args)
{
  LogQueue *q = args;
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
  g_sockaddr_unref(sa);

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
  iv_deinit();
  main_loop_worker_thread_stop();
  return NULL;
}

gpointer
threaded_consume(gpointer st)
{
  LogQueue *q = (LogQueue *) st;
  LogMessage *msg;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  gboolean success;
  gint loops = 0;
  gint msg_count = 0;

  /* just to make sure time is properly cached */
  iv_init();

  while (msg_count < MESSAGES_SUM)
    {
      gint slept = 0;
      msg = NULL;

      do
        {
          success = log_queue_pop_head(q, &msg, &path_options, FALSE, FALSE);
          if (!success)
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
        }
      while (!success);

      g_assert(!success || (success && msg != NULL));
      if (!success)
        {
          fprintf(stderr, "Queue didn't return enough messages: loops=%d, msg_count=%d\n", loops, msg_count);
          return GUINT_TO_POINTER(1);
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

gpointer
output_thread(gpointer args)
{
  WorkerOptions wo;
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
testcase_with_threads()
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

      for (j = 0; j < FEEDERS; j++)
        {
          fprintf(stderr,"starting feed thread %d\n",j);
          other_threads[j] = g_thread_create(output_thread, NULL, TRUE, NULL);
          thread_feed[j] = g_thread_create(threaded_feed, q, TRUE, NULL);
        }

      thread_consume = g_thread_create(threaded_consume, q, TRUE, NULL);

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

int
main()
{
  app_startup();
  putenv("TZ=MET-1METDST");
  tzset();

  configuration = cfg_new(0x0302);
  plugin_load_module("syslogformat", configuration, NULL);
  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, configuration);

  fprintf(stderr,"Start testcase_with_threads\n");
  testcase_with_threads();

#if 1
  fprintf(stderr,"Start testcase_zero_diskbuf_alternating_send_acks\n");
  testcase_zero_diskbuf_alternating_send_acks();
  fprintf(stderr,"Start testcase_zero_diskbuf_and_normal_acks\n");
  testcase_zero_diskbuf_and_normal_acks();
#endif
  return 0;
}
