/*
 * Copyright (c) 2012-2018 Balabit
 * Copyright (c) 2012 Peter Gyorko
 * Copyright (c) 2012 Balázs Scheidler
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
#include "grab-logging.h"
#include "syslog-ng.h"
#include "logmsg/logmsg.h"
#include "messages.h"

GList *internal_messages = NULL;


static void
grab_message(LogMessage *msg)
{
  internal_messages = g_list_append(internal_messages, msg);
}

void
reset_grabbed_messages(void)
{
  g_list_foreach(internal_messages, (GFunc) log_msg_unref, NULL);
  g_list_free(internal_messages);
  internal_messages = NULL;
}

void
start_grabbing_messages(void)
{
  reset_grabbed_messages();
  msg_set_post_func(grab_message);
}

void
display_grabbed_messages(void)
{
  GList *l;

  if (internal_messages)
    {
      fprintf(stderr, "  # Grabbed internal messages follow:\n");
      for (l = internal_messages; l; l = l->next)
        {
          LogMessage *msg = (LogMessage *) l->data;
          const gchar *msg_text = log_msg_get_value(msg, LM_V_MESSAGE, NULL);

          fprintf(stderr, "  #\t%s\n", msg_text);
        }
    }
  else
    {
      fprintf(stderr, "  # No internal messeges grabbed!\n");
    }
}

void
stop_grabbing_messages(void)
{
  msg_set_post_func(NULL);
}

gboolean
find_grabbed_message(const gchar *pattern)
{
  GList *l;

  for (l = internal_messages; l; l = l->next)
    {
      LogMessage *msg = (LogMessage *) l->data;
      const gchar *msg_text = log_msg_get_value(msg, LM_V_MESSAGE, NULL);

      if (strstr(msg_text, pattern))
        {
          return TRUE;
        }
    }
  return FALSE;
}
