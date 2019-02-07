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
#include "timeutils/strptime-tz.h"
#include "timeutils/timeutils.h"
#include "timeutils/cache.h"

typedef struct _DateParser
{
  LogParser super;
  gchar *date_format;
  gchar *date_tz;
  LogMessageTimeStamp time_stamp;
  TimeZoneInfo *date_tz_info;
} DateParser;

void
date_parser_set_format(LogParser *s, const gchar *format)
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
_parse_timestamp_and_deduce_missing_parts(DateParser *self, struct tm *tm, glong *tm_zone_offset, const gchar *input)
{
  gint current_year;
  struct tm nowtm = *tm;
  long tm_gmtoff;
  const gchar *tm_zone = NULL;
  const gchar *remainder;

  current_year = tm->tm_year;
  tm->tm_year = 0;
  tm_gmtoff = -1;
  remainder = strptime_with_tz(input, self->date_format, tm, &tm_gmtoff, &tm_zone);
  if (!remainder || remainder[0])
    return FALSE;

  /* hopefully _parse_timestamp will fill the year information, if
   * not, we are going to need the received year to find it out
   * heuristically */

  if (tm->tm_year == 0)
    {
      /* no year information in the timestamp, deduce it from the current year */
      tm->tm_year = current_year;
      tm->tm_year = determine_year_for_month(tm->tm_mon, &nowtm);
    }

  if (tm_gmtoff != -1)
    *tm_zone_offset = tm_gmtoff;
  else
    *tm_zone_offset = -1;

  return TRUE;
}

static void
_adjust_tvsec_to_move_it_into_given_timezone(LogStamp *timestamp, gint normalized_hour, gint unnormalized_hour)
{
  timestamp->tv_sec = timestamp->tv_sec
                      + get_local_timezone_ofs(timestamp->tv_sec)
                      - (normalized_hour - unnormalized_hour) * 3600
                      - timestamp->zone_offset;
}

static glong
_get_target_zone_offset(DateParser *self, glong tm_zone_offset, time_t now)
{
  if (tm_zone_offset != -1)
    return tm_zone_offset;
  else if (self->date_tz_info)
    return time_zone_info_get_offset(self->date_tz_info, now);
  else
    return get_local_timezone_ofs(now);
}

static gboolean
_convert_struct_tm_to_logstamp(DateParser *self, time_t now, struct tm *tm, glong tm_zone_offset, LogStamp *target)
{
  gint unnormalized_hour;

  target->zone_offset = _get_target_zone_offset(self, tm_zone_offset, now);

  /* NOTE: mktime changes struct tm in the call below! For instance it
   * changes the hour value. (in daylight saving changes, and when it
   * is out of range).
   *
   * We save the hour prior to this conversion, as it is needed when
   * converting the timestamp from our local timezone to the specified
   * one. */

  /* FIRST: We convert the timestamp as it was in our local time zone. */
  unnormalized_hour = tm->tm_hour;
  target->tv_sec = cached_mktime(tm);

  /* we can't parse USEC value, as strptime() does not support that */
  target->tv_usec = 0;

  /* SECOND: adjust tv_sec as if we converted it according to our timezone. */
  _adjust_tvsec_to_move_it_into_given_timezone(target, tm->tm_hour, unnormalized_hour);
  return TRUE;
}

static gboolean
_convert_timestamp_to_logstamp(DateParser *self, time_t now, LogStamp *target, const gchar *input)
{
  struct tm tm;
  glong tm_zone_offset;

  /* initialize tm with current date, this fills in dst and other
   * fields (even non-standard ones) */

  cached_localtime(&now, &tm);

  if (!_parse_timestamp_and_deduce_missing_parts(self, &tm, &tm_zone_offset, input))
    return FALSE;

  if (!_convert_struct_tm_to_logstamp(self, now, &tm, tm_zone_offset, target))
    return FALSE;

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
            evt_tag_str ("input", input),
            evt_tag_str ("format", self->date_format),
            evt_tag_printf("msg", "%p", *pmsg));

  /* this macro ensures zero termination by copying input to a
   * g_alloca()-d buffer if necessary. In most cases it's not though.
   */

  APPEND_ZERO(input, input, input_len);
  gboolean res = _convert_timestamp_to_logstamp(self,
                                                msg->timestamps[LM_TS_RECVD].tv_sec,
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
  date_parser_set_format(cloned, self->date_format);
  date_parser_set_timezone(cloned, self->date_tz);
  date_parser_set_time_stamp(cloned, self->time_stamp);
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
  self->time_stamp = LM_TS_STAMP;

  date_parser_set_format(&self->super, "%FT%T%z");
  return &self->super;
}
