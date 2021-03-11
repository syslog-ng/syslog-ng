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

#include "msg-format.h"
#include "cfg.h"
#include "plugin.h"
#include "plugin-types.h"
#include "find-crlf.h"
#include "scratch-buffers.h"

static gsize
_rstripped_message_length(const guchar *data, gsize length)
{
  while (length > 0 && (data[length - 1] == '\n' || data[length - 1] == '\0'))
    length--;
  return length;
}

static void
msg_format_inject_parse_error(LogMessage *msg, const guchar *data, gsize length, gint problem_position)
{
  GString *buf = scratch_buffers_alloc();

  log_msg_clear(msg);

  msg->timestamps[LM_TS_STAMP] = msg->timestamps[LM_TS_RECVD];
  log_msg_set_value(msg, LM_V_HOST, "", 0);

  if (problem_position > 0)
    g_string_printf(buf, "Error processing log message: %.*s>@<%.*s", (gint) problem_position-1,
                    data, (gint) (length-problem_position+1), data+problem_position-1);
  else
    g_string_printf(buf, "Error processing log message: %.*s", (gint) length, data);

  log_msg_set_value(msg, LM_V_MESSAGE, buf->str, buf->len);
  log_msg_set_value(msg, LM_V_PROGRAM, "syslog-ng", 9);
  g_string_printf(buf, "%d", (int) getpid());
  log_msg_set_value(msg, LM_V_PID, buf->str, buf->len);

  msg->flags |= LF_LOCAL;
  msg->pri = LOG_SYSLOG | LOG_ERR;
}

static void
msg_format_preprocess_message(MsgFormatOptions *options, LogMessage *msg,
                              const guchar *data, gsize length)
{
  if (options->flags & LP_STORE_RAW_MESSAGE)
    {
      log_msg_set_value(msg, LOG_MSG_GET_VALUE_HANDLE_STATIC("RAWMSG"),
                        (gchar *) data, _rstripped_message_length(data, length));
    }
}

static void
msg_format_postprocess_message(MsgFormatOptions *options, LogMessage *msg,
                               const guchar *data, gsize length)
{
  if (options->flags & LP_NO_PARSE_DATE)
    {
      msg->timestamps[LM_TS_STAMP] = msg->timestamps[LM_TS_RECVD];
      unix_time_set_timezone(&msg->timestamps[LM_TS_STAMP],
                             time_zone_info_get_offset(options->recv_time_zone_info,
                                                       msg->timestamps[LM_TS_RECVD].ut_sec));
    }
  if (G_UNLIKELY(options->flags & LP_NO_MULTI_LINE))
    {
      gssize msg_len;
      gchar *msg_text;
      gchar *p;

      p = msg_text = (gchar *) log_msg_get_value(msg, LM_V_MESSAGE, &msg_len);
      while ((p = find_cr_or_lf(p, msg_text + msg_len - p)))
        {
          *p = ' ';
          p++;
        }
    }
  if (options->flags & LP_LOCAL)
    msg->flags |= LF_LOCAL;
  if (options->flags & LP_ASSUME_UTF8)
    msg->flags |= LF_UTF8;
}

static gboolean
msg_format_process_message(MsgFormatOptions *options, LogMessage *msg,
                           const guchar *data, gsize length,
                           gsize *problem_position)
{
  if ((options->flags & LP_NOPARSE) == 0)
    {
      return options->format_handler->parse(options, msg, data, length, problem_position);
    }
  else
    {
      log_msg_set_value(msg, LM_V_MESSAGE, (gchar *) data, _rstripped_message_length(data, length));
      msg->pri = options->default_pri;
      return TRUE;
    }
}

gboolean
msg_format_parse_conditional(MsgFormatOptions *options, LogMessage *msg,
                             const guchar *data, gsize length,
                             gsize *problem_position)
{
  if (G_UNLIKELY(!options->format_handler))
    {
      gchar buf[256];

      g_snprintf(buf, sizeof(buf), "Error parsing message, format module %s is not loaded", options->format);
      log_msg_set_value(msg, LM_V_MESSAGE, buf, -1);
      return FALSE;
    }

  msg_format_preprocess_message(options, msg, data, length);

  if (!msg_format_process_message(options, msg, data, length, problem_position))
    return FALSE;

  msg_format_postprocess_message(options, msg, data, length);
  return TRUE;
}

void
msg_format_parse(MsgFormatOptions *options, LogMessage *msg,
                 const guchar *data, gsize length)
{
  gsize problem_position = 0;

  if (!msg_format_parse_conditional(options, msg, data, length, &problem_position))
    {
      msg_format_inject_parse_error(msg, data, _rstripped_message_length(data, length), problem_position);

      /* the injected error message needs to be postprocessed too */
      msg_format_postprocess_message(options, msg, data, length);
    }
}

void
msg_format_options_defaults(MsgFormatOptions *options)
{
  options->flags = LP_EXPECT_HOSTNAME | LP_STORE_LEGACY_MSGHDR;
  options->recv_time_zone = NULL;
  options->recv_time_zone_info = NULL;
  options->bad_hostname = NULL;
  options->default_pri = 0xFFFF;
  options->sdata_param_value_max = 65535;
}

/* NOTE: _init needs to be idempotent when called multiple times w/o invoking _destroy */
void
msg_format_options_init(MsgFormatOptions *options, GlobalConfig *cfg)
{
  Plugin *p;

  if (options->initialized)
    return;

  if (cfg->bad_hostname_compiled)
    options->bad_hostname = &cfg->bad_hostname;
  if (options->recv_time_zone == NULL)
    options->recv_time_zone = g_strdup(cfg->recv_time_zone);
  if (options->recv_time_zone_info == NULL)
    options->recv_time_zone_info = time_zone_info_new(options->recv_time_zone);

  if (!options->format)
    options->format = g_strdup("syslog");

  p = cfg_find_plugin(cfg, LL_CONTEXT_FORMAT, options->format);
  if (p)
    options->format_handler = plugin_construct(p);
  options->initialized = TRUE;
}

void
msg_format_options_copy(MsgFormatOptions *options, const MsgFormatOptions *source)
{
  g_assert(!options->initialized);

  options->format = g_strdup(source->format);
  options->flags = source->flags;
  options->default_pri = source->default_pri;
  options->recv_time_zone = g_strdup(source->recv_time_zone);
  options->sdata_param_value_max = source->sdata_param_value_max;
}

void
msg_format_options_destroy(MsgFormatOptions *options)
{
  if (options->format)
    {
      g_free(options->format);
      options->format = NULL;
    }
  if (options->recv_time_zone)
    {
      g_free(options->recv_time_zone);
      options->recv_time_zone = NULL;
    }
  if (options->recv_time_zone_info)
    {
      time_zone_info_free(options->recv_time_zone_info);
      options->recv_time_zone_info = NULL;
    }
  options->initialized = FALSE;
}

CfgFlagHandler msg_format_flag_handlers[] =
{
  { "no-parse",                   CFH_SET, offsetof(MsgFormatOptions, flags), LP_NOPARSE },
  { "check-hostname",             CFH_SET, offsetof(MsgFormatOptions, flags), LP_CHECK_HOSTNAME },
  { "syslog-protocol",            CFH_SET, offsetof(MsgFormatOptions, flags), LP_SYSLOG_PROTOCOL },
  { "assume-utf8",                CFH_SET, offsetof(MsgFormatOptions, flags), LP_ASSUME_UTF8 },
  { "validate-utf8",              CFH_SET, offsetof(MsgFormatOptions, flags), LP_VALIDATE_UTF8 },
  { "sanitize-utf8",              CFH_SET, offsetof(MsgFormatOptions, flags), LP_SANITIZE_UTF8 },
  { "no-multi-line",              CFH_SET, offsetof(MsgFormatOptions, flags), LP_NO_MULTI_LINE },
  { "store-legacy-msghdr",        CFH_SET, offsetof(MsgFormatOptions, flags), LP_STORE_LEGACY_MSGHDR },
  { "store-raw-message",          CFH_SET, offsetof(MsgFormatOptions, flags), LP_STORE_RAW_MESSAGE },
  { "dont-store-legacy-msghdr", CFH_CLEAR, offsetof(MsgFormatOptions, flags), LP_STORE_LEGACY_MSGHDR },
  { "expect-hostname",            CFH_SET, offsetof(MsgFormatOptions, flags), LP_EXPECT_HOSTNAME },
  { "no-hostname",              CFH_CLEAR, offsetof(MsgFormatOptions, flags), LP_EXPECT_HOSTNAME },
  { "guess-timezone",             CFH_SET, offsetof(MsgFormatOptions, flags), LP_GUESS_TIMEZONE },
  { "no-header",                  CFH_SET, offsetof(MsgFormatOptions, flags), LP_NO_HEADER },

  { NULL },
};

gboolean
msg_format_options_process_flag(MsgFormatOptions *options, const gchar *flag)
{
  return cfg_process_flag(msg_format_flag_handlers, options, flag);
}
