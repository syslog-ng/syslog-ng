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
static GStaticPrivate msg_context_private = G_STATIC_PRIVATE_INIT;
static GStaticMutex evtlog_lock = G_STATIC_MUTEX_INIT;

NVHandle msg_id_handle;
extern NVRegistry *logmsg_registry;

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
  if (logmsg_registry == NULL)
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
msg_send_internal_message(const char *msg, LogMessage *lm)
{
  if (G_UNLIKELY(log_stderr || (msg_post_func == NULL && (lm->pri & 0x7) <= EVT_PRI_WARNING)))
    {
      fprintf(stderr, "%s\n", msg);
      log_msg_unref(lm);
    }
  else
    {
      MsgContext *context = msg_get_context();

      if (context->recurse_count == 0)
        context->recurse_warning = FALSE;
      lm->recurse_count = context->recurse_count;
      msg_post_message(lm);
    }
}

struct __evttag
{
  EVTTAG *et_next;
  char *et_tag;
  char *et_value;
};

void evt_tag_free(EVTTAG *et);

EVTTAG *
create_tag_list(EVTTAG *first, va_list tags)
{
  EVTTAG *t;
  EVTTAG *next_t;
  t = first;
  while(t)
    {
      next_t = va_arg(tags, EVTTAG *);
      t->et_next = next_t;
      t = next_t;
    }
  return first;
}

gchar *
msg_escape_value(const gchar *unescaped, gchar escape_char)
{
  gsize unescaped_len = strlen(unescaped);
  gchar *buf = g_new0(gchar,4*unescaped_len + 1);

  int i, dst;

  for (i = 0, dst = 0; i < unescaped_len; i++)
    {
      unsigned c = (unsigned) unescaped[i];

      if (c < 32 && c != '\t')
        {
          sprintf(&buf[dst], "\\x%02x", (unsigned char) unescaped[i]);
          dst += 4;
        }
      else if (unescaped[i] == escape_char)
        {
          buf[dst++] = '\\';
          buf[dst++] = escape_char;
        }
      else
        {
          buf[dst++] = unescaped[i];
        }
      g_assert(dst <= 4*unescaped_len);
    }
  return buf;
}

GString *
msg_format_message(const gchar *desc, EVTTAG *first_tag, gchar **message_id)
{
  EVTTAG *et;
  GString *message_str = g_string_sized_new(512);
  gchar *escaped_value = msg_escape_value(desc, ';');
  gboolean first_printable_tag = TRUE;

  g_string_append(message_str, escaped_value);
  g_string_append_c(message_str, ';');
  g_free(escaped_value);

  et = first_tag;
  while(et)
    {
      EVTTAG *next_tag;
      if (strcmp(et->et_tag, "MSGID") != 0)
        {
          if (first_printable_tag)
            {
              first_printable_tag = FALSE;
            }
          else
            {
              g_string_append_c(message_str, ',');
            }
          g_string_append_c(message_str, ' ');
          g_string_append(message_str, et->et_tag);
          g_string_append(message_str, "='");

          escaped_value = msg_escape_value(et->et_value, '\'');
          g_string_append(message_str, escaped_value);
          g_free(escaped_value);
          g_string_append_c(message_str, '\'');
        }
      else
        {
          if (message_id)
            *message_id = g_strdup(et->et_value);
        }
      next_tag = et->et_next;
      evt_tag_free(et);
      et = next_tag;
    }
  return message_str;
}

LogMessage *
msg_event_create(gint prio, const gchar *desc, EVTTAG *tag1, ...)
{
  va_list va;
  gchar *message_id = NULL;
  GString *message_str = NULL;
  g_static_mutex_lock(&evtlog_lock);
  if (tag1)
    {
      va_start(va, tag1);
      tag1 = create_tag_list(tag1, va);
      va_end(va);
      message_str = msg_format_message(desc, tag1, &message_id);
    }
  else
    {
      message_str = msg_format_message(desc, NULL, NULL);
    }
  g_static_mutex_unlock(&evtlog_lock);
  LogMessage *lm = log_msg_new_internal(prio | EVT_FAC_SYSLOG, message_str->str);
  if (message_id)
    {
      log_msg_set_value(lm, LM_V_MSGID, message_id, -1);
      g_free(message_id);
    }
  g_string_free(message_str, TRUE);
  return lm;
}

void
msg_event_send(LogMessage *lm)
{
  const gchar *msg;

  msg = log_msg_get_value(lm, LM_V_MESSAGE, NULL);
  if (log_syslog)
    {
      syslog(lm->pri, "%s", msg);
    }
  else
    {
      msg_send_internal_message(msg, lm);
    }
}

void
msg_log_func(const gchar *log_domain, GLogLevelFlags log_flags, const gchar *msg, gpointer user_data)
{
  int pri = EVT_PRI_INFO;
  LogMessage *lm = NULL;

  if (log_flags & G_LOG_LEVEL_DEBUG)
    pri = EVT_PRI_DEBUG;
  else if (log_flags & G_LOG_LEVEL_WARNING)
    pri = EVT_PRI_WARNING;
  else if (log_flags & G_LOG_LEVEL_ERROR)
    pri = EVT_PRI_ERR;

  pri |= EVT_FAC_SYSLOG;
  lm = log_msg_new_internal(pri, msg);
  msg_send_internal_message(msg, lm);
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
  if (!interactive)
    {
      g_log_set_handler(G_LOG_DOMAIN, 0xff, msg_log_func, NULL);
      g_log_set_handler("GLib", 0xff, msg_log_func, NULL);
    }
  else
    {
      log_stderr = TRUE;
    }
}


void
msg_deinit()
{
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

