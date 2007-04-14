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
#include "logmsg.h"

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
static EVTCONTEXT *evt_context;
GQueue *internal_msg_queue = NULL;

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
      LogMessage *m;
      
      if (G_LIKELY(internal_msg_queue))
        {
          g_snprintf(buf, sizeof(buf), "<%d> syslog-ng[%d]: %s\n", prio, getpid(), msg);
          m = log_msg_new(buf, strlen(buf), NULL, LP_INTERNAL | LP_LOCAL, NULL);
          g_queue_push_tail(internal_msg_queue, m);
        }
    }
}

EVTREC *
msg_event_create(gint prio, const gchar *desc, EVTTAG *tag1, ...)
{
  EVTREC *e;
  va_list va;
  
  e = evt_rec_init(evt_context, prio, desc);
  if (tag1)
    {
      evt_rec_add_tag(e, tag1);
      va_start(va, tag1);
      evt_rec_add_tagsv(e, va);
      va_end(va);
    }
  return e;
}

void
msg_event_send(EVTREC *e)
{
  gchar *msg;
  /* this prevents infinite loops, debug messages causing 
   * internal messages causing debug messages again */
  if (evt_rec_get_syslog_pri(e) != EVT_PRI_DEBUG || log_stderr)
    {
      msg = evt_format(e);
      
      msg_send_internal_message(evt_rec_get_syslog_pri(e) | EVT_FAC_SYSLOG, msg); 
      free(msg);
    }
  evt_rec_free(e);
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
  internal_msg_queue = g_queue_new();

  log_stderr = use_stderr;
  evt_context = evt_ctx_init("syslog-ng", EVT_FAC_SYSLOG);

  g_log_set_handler(G_LOG_DOMAIN, 0xff, msg_log_func, NULL);
  g_log_set_handler("GLib", 0xff, msg_log_func, NULL);
  return TRUE;
}


void
msg_deinit()
{
  evt_ctx_free(evt_context);
  g_queue_free(internal_msg_queue);
  internal_msg_queue = NULL;
}
