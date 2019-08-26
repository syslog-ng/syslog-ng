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
#include "logmsg/logmsg-serialize.h"

#include <stdlib.h>
#include <string.h>
#include <iv.h>
#include <iv_thread.h>
#include <criterion/criterion.h>

int acked_messages = 0;
int fed_messages = 0;

gsize
get_one_message_serialized_size(void)
{
  GString *serialized = g_string_sized_new(256);
  SerializeArchive *sa = serialize_string_archive_new(serialized);
  LogMessage *msg = log_msg_new_empty();

  log_msg_serialize(msg, sa);
  gsize message_length = serialized->len;

  log_msg_unref(msg);
  g_string_free(serialized, TRUE);
  serialize_archive_free(sa);
  return message_length;
}

void
test_ack(LogMessage *msg, AckType ack_type)
{
  acked_messages++;
}

void
feed_empty_messages(LogQueue *q, const LogPathOptions *path_options, gint n)
{
  for (gint i = 0; i < n; i++)
    {
      LogMessage *msg = log_msg_new_empty();

      log_msg_add_ack(msg, path_options);
      msg->ack_func = test_ack;
      log_queue_push_tail(q, msg, path_options);
      fed_messages++;
    }
}

void
feed_some_messages(LogQueue *q, int n)
{
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

  path_options.ack_needed = q->use_backlog;
  path_options.flow_control_requested = TRUE;

  feed_empty_messages(q, &path_options, n);
}

void
send_some_messages(LogQueue *q, gint n)
{
  gint i;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

  for (i = 0; i < n; i++)
    {
      LogMessage *msg = log_queue_pop_head(q, &path_options);
      cr_assert_not_null(msg);
      if (q->use_backlog)
        {
          log_msg_ack(msg, &path_options, AT_PROCESSED);
        }
      log_msg_unref(msg);
    }
}
