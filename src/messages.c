/*
 * Copyright (c) 2002, 2003, 2004 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "messages.h"

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <evtlog.h>


gboolean debug_flag = 0;
gboolean verbose_flag = 0;
static gboolean log_stderr = FALSE, syslog_started = FALSE;
gint msg_pipe[2] = { -1, -1 };
EVTCONTEXT *evt_context;

static void
msg_send_internal_message(int prio, const char *msg)
{
  gchar buf[1025];
  
  if (log_stderr || !syslog_started)
    {
      fprintf(stderr, "%s\n", msg);
    }
  else
    {
      g_snprintf(buf, sizeof(buf), "<%d> syslog-ng[%d]: %s\n", prio, getpid(), msg);
      if (msg_pipe[1] == -1 || write(msg_pipe[1], buf, strlen(buf)) == -1)
        {
          fprintf(stderr, "%s\n", msg);
        }
    }
}

void
msg_event(gint prio, const char *desc, EVTTAG *tag1, ...)
{
  EVTREC *e;
  va_list va;
  gchar *msg;
  
  e = evt_rec_init(evt_context, prio, desc);
  if (tag1)
    {
      evt_rec_add_tag(e, tag1);
      va_start(va, tag1);
      evt_rec_add_tagsv(e, va);
      va_end(va);
    }
  
  msg = evt_format(e);
  
  msg_send_internal_message(evt_rec_get_syslog_pri(e), msg);
  
  evt_rec_free(e);
  free(msg);
}

void
msg_log_func(const gchar *log_domain, GLogLevelFlags log_flags, const gchar *msg, gpointer user_data)
{
  int pri = EVT_PRI_INFO;
  
  if (log_flags & G_LOG_LEVEL_DEBUG)
    pri = EVT_PRI_DEBUG;
  else if (log_flags & G_LOG_LEVEL_WARNING)
    pri = EVT_PRI_WARNING;
  else if (log_flags & G_LOG_LEVEL_ERROR)
    pri = EVT_PRI_ERR;
    
  pri |= EVT_FAC_SYSLOG;
  msg_send_internal_message(pri, msg);
}

void
msg_syslog_started(void)
{
  syslog_started = TRUE;
}

gboolean
msg_init(int use_stderr)
{
  evt_context = evt_ctx_init("syslog-ng", EVT_FAC_SYSLOG);
  if (!use_stderr)
    {
      if (pipe(msg_pipe) < 0)
        {
          msg_error("Error creating internal message pipe", 
                    evt_tag_errno(EVT_TAG_OSERROR, errno),
                    NULL);
          return FALSE;
        }
      log_stderr = FALSE;
    }
  g_log_set_handler(G_LOG_DOMAIN, 0xff, msg_log_func, NULL);
  g_log_set_handler("GLib", 0xff, msg_log_func, NULL);
  return TRUE;
}


void
msg_deinit()
{
  if (msg_pipe[0] != -1)
    close(msg_pipe[0]);
  if (msg_pipe[1] != -1)
    close(msg_pipe[1]);
  evt_ctx_free(evt_context);
}
