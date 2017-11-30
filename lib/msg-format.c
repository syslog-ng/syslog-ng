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

void
msg_format_inject_parse_error(LogMessage *msg, const guchar *data, gsize length, gint problem_position)
{
  gchar buf[2048];

  log_msg_clear(msg);

  msg->timestamps[LM_TS_STAMP] = msg->timestamps[LM_TS_RECVD];
  log_msg_set_value(msg, LM_V_HOST, "", 0);

  g_snprintf(buf, sizeof(buf), "Error processing log message: %.*s>@<%.*s", (gint) problem_position-1,
             data,(gint) (length-problem_position+1), data+problem_position-1);

  log_msg_set_value(msg, LM_V_MESSAGE, buf, -1);
  log_msg_set_value(msg, LM_V_PROGRAM, "syslog-ng", 9);
  g_snprintf(buf, sizeof(buf), "%d", (int) getpid());
  log_msg_set_value(msg, LM_V_PID, buf, -1);

  msg->pri = LOG_SYSLOG | LOG_ERR;
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

  { NULL },
};

gboolean
msg_format_options_process_flag(MsgFormatOptions *options, gchar *flag)
{
  return cfg_process_flag(msg_format_flag_handlers, options, flag);
}
