/*
 * Copyright (c) 2015 BalaBit
 * Copyright (c) 2015 Vincent Bernat <Vincent.Bernat@exoscale.ch>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "date-parser.h"
#include "misc.h"

typedef struct _DateParser
{
  LogParser super;
  gchar *date_format;
  gchar *date_tz;
  TimeZoneInfo *date_tz_info;
} DateParser;

void
date_parser_set_format(LogParser *s, gchar *format)
{
  DateParser *self = (DateParser *) s;

  g_free(self->date_format);
  self->date_format = g_strdup(format);
}

void
date_parser_set_timezone(LogParser *s, gchar *tz)
{
  DateParser *self = (DateParser *) s;

  g_free(self->date_tz);
  self->date_tz = g_strdup(tz);
}

static gboolean
date_parser_init(LogPipe *s)
{
  DateParser *self = (DateParser *) s;

  if (self->date_tz_info)
    time_zone_info_free(self->date_tz_info);
  self->date_tz_info = self->date_tz ? time_zone_info_new(self->date_tz) : NULL;
  return log_parser_init_method(s);
}

static gboolean
_parse_timestamp(DateParser *self, const gchar *input, struct tm *tm)
{
  memset(tm, 0, sizeof(struct tm));
  if (!strptime(input, self->date_format, tm))
    return FALSE;
  return TRUE;
}

static gboolean
_(DateParser *self, LogMessage *msg, const gchar *input)
{
  struct tm tm;

  if (!_parse_timestamp(self, input, &tm))
    return FALSE;

  /* The date may be missing. Try to use the current date as we can't
   * do anything better... Don't try to be too smart with new year
   * eve. */
  if (tm.tm_year == 0)
    {
      /* Grab the year from the received timestamp */
      struct tm nowtm;
      LogStamp received = msg->timestamps[LM_TS_RECVD];
      cached_gmtime(&received.tv_sec, &nowtm);
      tm.tm_year = nowtm.tm_year;

      /* Adjust the year if needed. Therefore, we don't need to care
       * too much about timezones. */
      if (tm.tm_mon > nowtm.tm_mon + 1)
        tm.tm_year--;
    }

  /* mktime handles timezones horribly. It considers the time to be
     local and also alter the parsed timezone. Try to fix all that. */
  msg->timestamps[LM_TS_STAMP].tv_usec = 0;
  if (!self->date_tz_info)
    {
      msg->timestamps[LM_TS_STAMP].zone_offset = tm.tm_gmtoff;
      msg->timestamps[LM_TS_STAMP].tv_sec = mktime (&tm) - msg->timestamps[LM_TS_STAMP].zone_offset - timezone;
    }
  else
    {
      msg->timestamps[LM_TS_STAMP].tv_sec = mktime (&tm) - timezone;
      msg->timestamps[LM_TS_STAMP].zone_offset =
        time_zone_info_get_offset(self->date_tz_info,
                                  msg->timestamps[LM_TS_STAMP].tv_sec);
      if (msg->timestamps[LM_TS_STAMP].zone_offset == -1)
        {
          msg->timestamps[LM_TS_STAMP].zone_offset =
            get_local_timezone_ofs(msg->timestamps[LM_TS_STAMP].tv_sec);
        }
      msg->timestamps[LM_TS_STAMP].tv_sec -= msg->timestamps[LM_TS_STAMP].zone_offset;
    }

  return TRUE;
}

static gboolean
date_parser_process(LogParser *s,
                    LogMessage **pmsg,
                    const LogPathOptions *path_options,
                    const gchar *input,
                    gsize input_len)
{
  DateParser *self = (DateParser *) s;
  LogMessage *msg = log_msg_make_writable (pmsg, path_options);
  gchar *input_with_nul;

  APPEND_ZERO(input_with_nul, input, input_len);
  return _(self, msg, input_with_nul);
}

static LogPipe *
date_parser_clone(LogPipe *s)
{
  DateParser *self = (DateParser *) s;
  LogParser *cloned;

  cloned = date_parser_new(log_pipe_get_config(&self->super.super));
  date_parser_set_format(cloned, self->date_format);
  date_parser_set_timezone(cloned, self->date_tz);
  log_parser_set_template(cloned, log_template_ref(self->super.template));

  return &cloned->super;
}

static void
date_parser_free(LogPipe *s)
{
  DateParser *self = (DateParser *)s;

  g_free(self->date_format);
  g_free(self->date_tz);
  if (self->date_tz_info)
    time_zone_info_free(self->date_tz_info);

  log_parser_free_method(s);
}

LogParser *
date_parser_new(GlobalConfig *cfg)
{
  DateParser *self = g_new0(DateParser, 1);
  log_parser_init_instance(&self->super, cfg);
  self->super.super.init = date_parser_init;
  self->super.process = date_parser_process;
  self->super.super.clone = date_parser_clone;
  self->super.super.free_fn = date_parser_free;

  self->date_format = g_strdup("%FT%T%z");

  return &self->super;
}
