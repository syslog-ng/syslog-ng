/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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
#include "logmsg/logmsg.h"

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <evtlog.h>

enum
{
  /* processing a non-internal message, we're definitely not recursing */
  RECURSE_STATE_OK = 0,
  /* processing an internal message currently, followup internal messages will be suppressed */
  RECURSE_STATE_WATCH = 1,
  /* suppress all internal messages */
  RECURSE_STATE_SUPPRESS = 2
};

typedef struct _MsgContext
{
  guint16 recurse_state;
  gboolean recurse_warning:1;
  gchar recurse_trigger[128];
} MsgContext;

gboolean startup_debug_flag = 0;
gboolean debug_flag = 0;
gboolean verbose_flag = 0;
gboolean trace_flag = 0;
gboolean log_stderr = FALSE;
gboolean skip_timestamp_on_stderr = FALSE;
static MsgPostFunc msg_post_func;
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
      if (msg->recursed)
        context->recurse_state = RECURSE_STATE_SUPPRESS;
      else
        context->recurse_state = RECURSE_STATE_WATCH;
    }
  else
    {
      context->recurse_state = RECURSE_STATE_OK;
    }
}

static gboolean
msg_limit_internal_message(const gchar *msg)
{
  MsgContext *context;

  if (!evt_context)
    return FALSE;

  context = msg_get_context();

  if (context->recurse_state >= RECURSE_STATE_SUPPRESS)
    {
      if (!context->recurse_warning)
        {
          msg_event_send(
            msg_event_create(EVT_PRI_WARNING,
                             "internal() messages are looping back, preventing loop by suppressing all internal messages until the current message is processed",
                             evt_tag_str("trigger-msg", context->recurse_trigger),
                             evt_tag_str("first-suppressed-msg", msg),
                             NULL));
          context->recurse_warning = TRUE;
        }
      return FALSE;
    }
  return TRUE;
}

static gchar *
msg_format_timestamp(gchar *buf, gsize buflen)
{
  struct tm tm;
  GTimeVal now;
  gint len;
  time_t now_sec;

  g_get_current_time(&now);
  now_sec = now.tv_sec;
  cached_localtime(&now_sec, &tm);
  len = strftime(buf, buflen, "%Y-%m-%dT%H:%M:%S", &tm);
  if (len < buflen)
    g_snprintf(buf + len, buflen - len, ".%06ld", now.tv_usec);
  return buf;
}

static void
msg_send_formatted_message_to_stderr(const char *msg)
{
  gchar tmtime[128];

  if (skip_timestamp_on_stderr)
    fprintf(stderr, "%s\n", msg);
  else
    fprintf(stderr, "[%s] %s\n", msg_format_timestamp(tmtime, sizeof(tmtime)), msg);
}

void
msg_send_formatted_message(int prio, const char *msg)
{
  if (G_UNLIKELY(log_stderr || (msg_post_func == NULL && (prio & 0x7) <= EVT_PRI_WARNING)))
    {
      msg_send_formatted_message_to_stderr(msg);
    }
  else if (msg_post_func)
    {
      LogMessage *m;

      MsgContext *context = msg_get_context();

      if (context->recurse_state == RECURSE_STATE_OK)
        {
          context->recurse_warning = FALSE;
          g_strlcpy(context->recurse_trigger, msg, sizeof(context->recurse_trigger));
        }
      m = log_msg_new_internal(prio, msg);
      m->recursed = context->recurse_state >= RECURSE_STATE_WATCH;
      msg_post_message(m);
    }
}

static void
msg_event_send_with_suppression(EVTREC *e, gboolean (*suppress)(const gchar *msg))
{
  gchar *msg;

  msg = evt_format(e);
  if (!suppress || suppress(msg))
    msg_send_formatted_message(evt_rec_get_syslog_pri(e) | EVT_FAC_SYSLOG, msg);
  free(msg);
  msg_event_free(e);
}

void
msg_event_send(EVTREC *e)
{
  msg_event_send_with_suppression(e, NULL);
}

void
msg_event_suppress_recursions_and_send(EVTREC *e)
{
  msg_event_send_with_suppression(e, msg_limit_internal_message);
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

EVTREC *
msg_event_create_from_desc(gint prio, const char *desc)
{
  return msg_event_create(prio, desc, NULL);
}

void
msg_event_free(EVTREC *e)
{
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
  msg_send_formatted_message(pri, msg);
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

static guint g_log_handler_id;
static guint glib_handler_id;

void
msg_init(gboolean interactive)
{
  if (evt_context)
    return;

  if (!interactive)
    {
      g_log_handler_id = g_log_set_handler(G_LOG_DOMAIN, 0xff, msg_log_func, NULL);
      glib_handler_id = g_log_set_handler("GLib", 0xff, msg_log_func, NULL);
    }
  else
    {
      log_stderr = TRUE;
      skip_timestamp_on_stderr = TRUE;
    }
  evt_context = evt_ctx_init("syslog-ng", EVT_FAC_SYSLOG);
}


void
msg_deinit(void)
{
  evt_ctx_free(evt_context);
  evt_context = NULL;
  log_stderr = TRUE;

  if (g_log_handler_id)
    {
      g_log_remove_handler(G_LOG_DOMAIN, g_log_handler_id);
      g_log_handler_id = 0;
    }
  if (glib_handler_id)
    {
      g_log_remove_handler("GLib", glib_handler_id);
      glib_handler_id = 0;
    }
}

static GOptionEntry msg_option_entries[] =
{
  { "startup-debug",     'r',         0, G_OPTION_ARG_NONE, &startup_debug_flag, "Enable debug logging during startup", NULL},
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

