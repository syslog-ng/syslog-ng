/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
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
  
#include "messages.h"
#include "logmsg.h"

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <evtlog.h>

typedef struct _MsgContext
{
  guint16 recurse_count;
  gboolean recurse_warning:1;
} MsgContext;

gboolean debug_flag = 0;
gboolean verbose_flag = 0;
gboolean trace_flag = 0;
gboolean log_stderr = FALSE;
static MsgPostFunc msg_post_func;
static gboolean log_syslog = FALSE;
static EVTCONTEXT *evt_context;
static GStaticPrivate msg_context_private = G_STATIC_PRIVATE_INIT;
static GStaticMutex evtlog_lock = G_STATIC_MUTEX_INIT;


static MsgContext *
msg_get_context(void)
{
  MsgContext *context;

  context = g_static_private_get(&msg_context_private);
  if (!context)
    {
      context = g_new0(MsgContext, 1);
      g_static_private_set(&msg_context_private, context, g_free);
    }
  return context;
}

void
msg_set_context(LogMessage *msg)
{
  MsgContext *context = msg_get_context();
  
  if (msg && (msg->flags & LF_INTERNAL))
    {
      context->recurse_count = msg->recurse_count + 1;
    }
  else
    {
      context->recurse_count = 0;
    }
}

void
msg_set_post_func(MsgPostFunc func)
{
  msg_post_func = func;
}

void
msg_post_message(LogMessage *msg)
{
  if (msg_post_func)
    msg_post_func(msg);
  else
    log_msg_unref(msg);
}

#define MAX_RECURSIONS 1

gboolean
msg_limit_internal_message(void)
{
  MsgContext *context;
  
  if (!evt_context)
    return FALSE;

  context = msg_get_context();
  
  if (context->recurse_count > MAX_RECURSIONS)
    {
      if (!context->recurse_warning)
        {
          msg_event_send(
            msg_event_create(EVT_PRI_WARNING, "syslog-ng internal() messages are looping back, preventing loop by suppressing further messages", 
                             evt_tag_int("recurse_count", context->recurse_count),
                             NULL));
          context->recurse_warning = TRUE;
        }
      return FALSE;
    }
  return TRUE;
}


static void
msg_send_internal_message(int prio, const char *msg)
{
  if (G_UNLIKELY(log_stderr || (msg_post_func == NULL && (prio & 0x7) <= EVT_PRI_WARNING)))
    {
      fprintf(stderr, "%s\n", msg);
    }
  else
    {
      LogMessage *m;
      
      MsgContext *context = msg_get_context();

      if (context->recurse_count == 0)
        context->recurse_warning = FALSE;
      m = log_msg_new_internal(prio, msg);
      m->recurse_count = context->recurse_count;
      msg_post_message(m);
    }
}


EVTREC *
msg_event_create(gint prio, const gchar *desc, EVTTAG *tag1, ...)
{
  EVTREC *e;
  va_list va;
  
  g_static_mutex_lock(&evtlog_lock);
  e = evt_rec_init(evt_context, prio, desc);
  if (tag1)
    {
      evt_rec_add_tag(e, tag1);
      va_start(va, tag1);
      evt_rec_add_tagsv(e, va);
      va_end(va);
    }
  g_static_mutex_unlock(&evtlog_lock);
  return e;
}

void
msg_event_send(EVTREC *e)
{
  gchar *msg;
  
  msg = evt_format(e);
  if (log_syslog)
    {
      syslog(evt_rec_get_syslog_pri(e), "%s", msg);
    }
  else
    {
      msg_send_internal_message(evt_rec_get_syslog_pri(e) | EVT_FAC_SYSLOG, msg); 
    }
  free(msg);
  g_static_mutex_lock(&evtlog_lock);
  evt_rec_free(e);
  g_static_mutex_unlock(&evtlog_lock);
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
msg_redirect_to_syslog(const gchar *program_name)
{
  log_syslog = TRUE;
  openlog(program_name, LOG_NDELAY | LOG_PID, LOG_SYSLOG);
}

void
msg_init(gboolean interactive)
{
  if (evt_context)
    return;

  if (!interactive)
    {
      g_log_set_handler(G_LOG_DOMAIN, 0xff, msg_log_func, NULL);
      g_log_set_handler("GLib", 0xff, msg_log_func, NULL);
    }
  else
    {
      log_stderr = TRUE;
    }
  evt_context = evt_ctx_init("syslog-ng", EVT_FAC_SYSLOG);
}


void
msg_deinit()
{
  evt_ctx_free(evt_context);
  log_stderr = TRUE;
}

static GOptionEntry msg_option_entries[] =
{
  { "verbose",           'v',         0, G_OPTION_ARG_NONE, &verbose_flag, "Be a bit more verbose", NULL },
  { "debug",             'd',         0, G_OPTION_ARG_NONE, &debug_flag, "Enable debug messages", NULL},
  { "trace",             't',         0, G_OPTION_ARG_NONE, &trace_flag, "Enable trace messages", NULL },
  { "stderr",            'e',         0, G_OPTION_ARG_NONE, &log_stderr,  "Log messages to stderr", NULL},
  { NULL }
};

void
msg_add_option_group(GOptionContext *ctx)
{
  GOptionGroup *group;

  group = g_option_group_new("log", "Log options", "Log options", NULL, NULL);
  g_option_group_add_entries(group, msg_option_entries);
  g_option_context_add_group(ctx, group);
}

