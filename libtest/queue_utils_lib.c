#include "queue_utils_lib.h"

#include <stdlib.h>
#include <string.h>
#include <iv.h>
#include <iv_thread.h>

int acked_messages = 0;
int fed_messages = 0;

void
test_ack(LogMessage *msg, gpointer user_data, gboolean pos_tracking)
{
  acked_messages++;
}

void
feed_some_messages(LogQueue *q, int n, gboolean ack_needed, MsgFormatOptions *po)
{
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  LogMessage *msg;
  gint i;

  path_options.ack_needed = ack_needed;
  path_options.flow_control_requested = TRUE;
  for (i = 0; i < n; i++)
    {
      gchar *msg_str = g_strdup_printf("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: árvíztűrőtükörfúrógép ID :%08d",i);
      GSockAddr *s_addr = g_sockaddr_inet_new("10.10.10.10", 1010);
      msg = log_msg_new(msg_str, strlen(msg_str), s_addr, po);
      g_sockaddr_unref(s_addr);
      g_free(msg_str);

      log_msg_add_ack(msg, &path_options);
      msg->ack_func = test_ack;
      log_queue_push_tail(q, msg, &path_options);
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
      log_queue_pop_head(q, &msg, &path_options, FALSE, use_app_acks);
      if (!use_app_acks)
        {
          log_msg_ack(msg, &path_options, TRUE);
        }
      log_msg_unref(msg);
    }
}

void
app_rewind_some_messages(LogQueue *q, gint n)
{
  log_queue_rewind_backlog(q,n);
}

void
app_ack_some_messages(LogQueue *q, gint n)
{
  log_queue_ack_backlog(q, n);
}

void
rewind_messages(LogQueue *q)
{
  log_queue_rewind_backlog(q, -1);
}
