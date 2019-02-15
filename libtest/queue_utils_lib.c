/*
 * Copyright (c) 2002-2016 Balabit
 * Copyright (c) 2016 Viktor Juhasz <viktor.juhasz@balabit.com>
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

#include "queue_utils_lib.h"

#include <stdlib.h>
#include <string.h>
#include <iv.h>
#include <iv_thread.h>

int acked_messages = 0;
int fed_messages = 0;

void
test_ack(LogMessage *msg, AckType ack_type)
{
  acked_messages++;
}

void
feed_some_messages(LogQueue *q, int n)
{
  MsgFormatOptions format_options;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  LogMessage *msg;
  gint i;

  format_options.format_handler = NULL;
  path_options.ack_needed = q->use_backlog;
  path_options.flow_control_requested = TRUE;
  for (i = 0; i < n; i++)
    {
      gchar *msg_str =
        g_strdup_printf("<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]: árvíztűrőtükörfúrógép ID :%08d",i);
      GSockAddr *test_addr = g_sockaddr_inet_new("10.10.10.10", 1010);
      msg = log_msg_new(msg_str, strlen(msg_str), test_addr, &format_options);
      g_sockaddr_unref(test_addr);
      g_free(msg_str);

      log_msg_add_ack(msg, &path_options);
      msg->ack_func = test_ack;
      log_queue_push_tail(q, msg, &path_options);
      fed_messages++;
    }
}

void
send_some_messages(LogQueue *q, gint n)
{
  gint i;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

  for (i = 0; i < n; i++)
    {
      LogMessage *msg = log_queue_pop_head(q, &path_options);
      if (q->use_backlog)
        {
          log_msg_ack(msg, &path_options, AT_PROCESSED);
        }
      log_msg_unref(msg);
    }
}

void
app_rewind_some_messages(LogQueue *q, guint n)
{
  log_queue_rewind_backlog(q,n);
}

void
app_ack_some_messages(LogQueue *q, guint n)
{
  log_queue_ack_backlog(q, n);
}

void
rewind_messages(LogQueue *q)
{
  log_queue_rewind_backlog_all(q);
}
