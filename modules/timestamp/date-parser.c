/*
 * Copyright (c) 2015 Balabit
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
#include "str-utils.h"
#include "string-list.h"
#include "timeutils/wallclocktime.h"
#include "timeutils/cache.h"
#include "timeutils/conv.h"

enum
{
  DPF_GUESS_TIMEZONE = 0x0001,
};

typedef struct _DateParser
{
  LogParser super;
  GList *date_formats;
  gchar *date_tz;
  LogMessageTimeStamp time_stamp;
  TimeZoneInfo *date_tz_info;
  guint32 flags;
} DateParser;

void
date_parser_set_formats(LogParser *s, GList *formats)
{
  DateParser *self = (DateParser *) s;

  string_list_free(self->date_formats);
  self->date_formats = formats;
}

void
date_parser_set_timezone(LogParser *s, gchar *tz)
{
  DateParser *self = (DateParser *) s;

  g_free(self->date_tz);
  self->date_tz = g_strdup(tz);
}

void
date_parser_set_time_stamp(LogParser *s, LogMessageTimeStamp time_stamp)
{
  DateParser *self = (DateParser *) s;

  self->time_stamp = time_stamp;
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

/* NOTE: tm is initialized with the current time and date */
static gboolean
_parse_timestamp_and_deduce_missing_parts(DateParser *self, WallClockTime *wct, const gchar *input,
                                          const gchar *date_format)
{
  const gchar *remainder;

  msg_trace("date-parser message processing for",
            evt_tag_str("input", input),
            evt_tag_str("date_format", date_format));

  remainder = wall_clock_time_strptime(wct, date_format, input);

  if (!remainder || remainder[0])
    return FALSE;

  /* hopefully _parse_timestamp will fill all necessary information, if
   * not, we are going to guess the missing_fields heuristically */

  wall_clock_time_guess_missing_fields(wct);
  return TRUE;
}

static gboolean
_parse_timestamp_against_date_format_list(DateParser *self, WallClockTime *wct, const gchar *input)
{
  for (GList *item = self->date_formats; item; item = item->next)
    {
      if (_parse_timestamp_and_deduce_missing_parts(self, wct, input, item->data))
        return TRUE;
    }

  return FALSE;
}

static gboolean
_convert_timestamp_to_logstamp(DateParser *self, time_t now, UnixTime *target, const gchar *input)
{
  WallClockTime wct = WALL_CLOCK_TIME_INIT;

  if (!_parse_timestamp_against_date_format_list(self, &wct, input))
    return FALSE;

  convert_and_normalize_wall_clock_time_to_unix_time_with_tz_hint(&wct, target,
      time_zone_info_get_offset(self->date_tz_info, now));

  if ((self->flags & DPF_GUESS_TIMEZONE) != 0)
    unix_time_fix_timezone_assuming_the_time_matches_real_time(target);

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
  LogMessage *msg = log_msg_make_writable(pmsg, path_options);
  msg_trace("date-parser message processing started",
            evt_tag_str("input", input),
            evt_tag_msg_reference(*pmsg));

  /* this macro ensures zero termination by copying input to a
   * g_alloca()-d buffer if necessary. In most cases it's not though.
   */

  APPEND_ZERO(input, input, input_len);
  gboolean res = _convert_timestamp_to_logstamp(self,
                                                msg->timestamps[LM_TS_RECVD].ut_sec,
                                                &msg->timestamps[self->time_stamp],
                                                input);

  return res;
}

static LogPipe *
date_parser_clone(LogPipe *s)
{
  DateParser *self = (DateParser *) s;
  LogParser *cloned;

  cloned = date_parser_new(log_pipe_get_config(&self->super.super));
  date_parser_set_formats(cloned, string_list_clone(self->date_formats));
  date_parser_set_timezone(cloned, self->date_tz);
  date_parser_set_time_stamp(cloned, self->time_stamp);
  log_parser_set_template(cloned, log_template_ref(self->super.template));

  return &cloned->super;
}

static void
date_parser_free(LogPipe *s)
{
  DateParser *self = (DateParser *)s;

  string_list_free(self->date_formats);
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
  self->time_stamp = LM_TS_STAMP;

  date_parser_set_formats(&self->super, g_list_prepend(NULL, g_strdup("%FT%T%z")));
  return &self->super;
}

CfgFlagHandler date_parser_flags[] =
{
  /* NOTE: underscores are automatically converted to dashes */

  /* LogReaderOptions */
  { "guess-timezone",             CFH_SET, offsetof(DateParser, flags),  DPF_GUESS_TIMEZONE },
  { NULL },
};

gboolean
date_parser_process_flag(LogParser *s, gchar *flag)
{
  DateParser *self = (DateParser *) s;

  return cfg_process_flag(date_parser_flags, self, flag);
}
