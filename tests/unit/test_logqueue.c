#include "logqueue.h"
#include "logpipe.h"
#include "apphook.h"


#include <stdlib.h>

int acked_messages = 0;
int fed_messages = 0;

#define OVERFLOW_SIZE 10000

void 
test_ack(LogMessage *msg, gpointer user_data)
{
  acked_messages++;
}

void
feed_some_messages(LogQueue **q, int n, gboolean flow_control)
{
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  LogMessage *msg;
  gint i;
  
  path_options.flow_control = flow_control;
  for (i = 0; i < n; i++)
    {
      char *msg_str = "<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: árvíztűrőtükörfúrógép";
      
      msg = log_msg_new(msg_str, strlen(msg_str), g_sockaddr_inet_new("10.10.10.10", 1010), 0, NULL, -1, 0xFFFF);
      log_msg_add_ack(msg, &path_options);
      msg->ack_func = test_ack;
      if (!log_queue_push_tail((*q), msg, &path_options))
        {
          fprintf(stderr, "Queue unable to consume enough messages: %d\n", fed_messages);
          exit(1);
        }
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
      log_queue_pop_head(q, &msg, &path_options, use_app_acks);
      log_msg_ack(msg, &path_options);
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
  
  q = log_queue_new(OVERFLOW_SIZE);
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
    
  log_queue_free(q);
}

void
testcase_zero_diskbuf_alternating_send_acks()
{
  LogQueue *q;
  gint i;
  
  q = log_queue_new(OVERFLOW_SIZE);
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
    
  log_queue_free(q);
}

#if 0

/* no synchronization between the feed/consume threads, therefore it does
 * not succeed reliably. commented out for now, will fix at the next
 * logqueue related threaded issue */

GStaticMutex threaded_lock = G_STATIC_MUTEX_INIT;

gpointer
threaded_feed(gpointer st)
{
  LogQueue *q = (LogQueue *) st;
  char *msg_str = "<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: árvíztűrőtükörfúrógép";
  gint i;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  LogMessage *msg;

  for (i = 0; i < 100000; i++)
    {
      msg = log_msg_new(msg_str, strlen(msg_str), g_sockaddr_inet_new("10.10.10.10", 1010), 0, NULL, -1);
      log_msg_add_ack(msg, &path_options);
      msg->ack_func = test_ack;

      g_static_mutex_lock(&threaded_lock);
      if (!log_queue_push_tail(q, msg, &path_options))
        {
          fprintf(stderr, "Queue unable to consume enough messages: %d\n", fed_messages);
          return GUINT_TO_POINTER(1);
        }
      g_static_mutex_unlock(&threaded_lock);
    }
  return NULL;
}

gpointer
threaded_consume(gpointer st)
{
  LogQueue *q = (LogQueue *) st;
  LogMessage *msg;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  gboolean success;
  gint i;

  for (i = 0; i < 100000; i++)
    {
      g_static_mutex_lock(&threaded_lock);
      msg = NULL;
      success = log_queue_pop_head(q, &msg, &path_options, FALSE);
      g_static_mutex_unlock(&threaded_lock);

      g_assert(!success || (success && msg != NULL));
      if (!success)
        {
          fprintf(stderr, "Queue didn't return enough messages: i=%d\n", i);
          return GUINT_TO_POINTER(1);
        }

      log_msg_ack(msg, &path_options);
      log_msg_unref(msg);
    }

  return NULL;
}

void
testcase_with_threads()
{
  LogQueue *q;
  GThread *thread_feed, *thread_consume;
  gint i;

  for (i = 0; i < 100; i++)
    {
      q = log_queue_new(100000, 0, 64);
      thread_feed = g_thread_create(threaded_feed, q, TRUE, NULL);

      thread_consume = g_thread_create(threaded_consume, q, TRUE, NULL);
      g_thread_join(thread_feed);
      g_thread_join(thread_consume);

      log_queue_free(q);
    }
}
#endif

int 
main()
{
  app_startup();
  putenv("TZ=MET-1METDST");
  tzset();

  testcase_zero_diskbuf_alternating_send_acks();
  testcase_zero_diskbuf_and_normal_acks();
  return 0;
}
