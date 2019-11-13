/*
 * Copyright (c) 2002-2014 Balabit
 * Copyright (c) 1998-2014 Balázs Scheidler
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

#include "template/macros.h"
#include "template/escaping.h"
#include "timeutils/cache.h"
#include "timeutils/names.h"
#include "timeutils/unixtime.h"
#include "timeutils/format.h"
#include "timeutils/misc.h"
#include "timeutils/conv.h"
#include "messages.h"
#include "str-format.h"
#include "run-id.h"
#include "host-id.h"
#include "rcptid.h"
#include "logmsg/logmsg.h"
#include "syslog-names.h"
#include "hostname.h"
#include "template/templates.h"
#include "cfg.h"

#include <string.h>

LogMacroDef macros[] =
{
  { "FACILITY", M_FACILITY },
  { "FACILITY_NUM", M_FACILITY_NUM },
  { "PRIORITY", M_LEVEL },
  { "LEVEL", M_LEVEL },
  { "LEVEL_NUM", M_LEVEL_NUM },
  { "TAG", M_TAG },
  { "TAGS", M_TAGS },
  { "BSDTAG", M_BSDTAG },
  { "PRI", M_PRI },

  { "DATE",           M_DATE },
  { "FULLDATE",       M_FULLDATE },
  { "ISODATE",        M_ISODATE },
  { "STAMP",          M_STAMP },
  { "YEAR",           M_YEAR },
  { "YEAR_DAY",       M_YEAR_DAY },
  { "MONTH",          M_MONTH },
  { "MONTH_WEEK",     M_MONTH_WEEK },
  { "MONTH_ABBREV",   M_MONTH_ABBREV },
  { "MONTH_NAME",     M_MONTH_NAME },
  { "DAY",            M_DAY },
  { "HOUR",           M_HOUR },
  { "HOUR12",         M_HOUR12 },
  { "MIN",            M_MIN },
  { "SEC",            M_SEC },
  { "USEC",           M_USEC },
  { "MSEC",           M_MSEC },
  { "AMPM",           M_AMPM },
  { "WEEKDAY",        M_WEEK_DAY_ABBREV }, /* deprecated */
  { "WEEK_DAY",       M_WEEK_DAY },
  { "WEEK_DAY_ABBREV", M_WEEK_DAY_ABBREV },
  { "WEEK_DAY_NAME",  M_WEEK_DAY_NAME },
  { "WEEK",           M_WEEK },
  { "ISOWEEK",        M_ISOWEEK },
  { "TZOFFSET",       M_TZOFFSET },
  { "TZ",             M_TZ },
  { "SYSUPTIME",      M_SYSUPTIME },
  { "UNIXTIME",       M_UNIXTIME },

  { "R_DATE",           M_RECVD_OFS + M_DATE },
  { "R_FULLDATE",       M_RECVD_OFS + M_FULLDATE },
  { "R_ISODATE",        M_RECVD_OFS + M_ISODATE },
  { "R_STAMP",          M_RECVD_OFS + M_STAMP },
  { "R_YEAR",           M_RECVD_OFS + M_YEAR },
  { "R_YEAR_DAY",       M_RECVD_OFS + M_YEAR_DAY },
  { "R_MONTH",          M_RECVD_OFS + M_MONTH },
  { "R_MONTH_WEEK",     M_RECVD_OFS + M_MONTH_WEEK },
  { "R_MONTH_ABBREV",   M_RECVD_OFS + M_MONTH_ABBREV },
  { "R_MONTH_NAME",     M_RECVD_OFS + M_MONTH_NAME },
  { "R_DAY",            M_RECVD_OFS + M_DAY },
  { "R_HOUR",           M_RECVD_OFS + M_HOUR },
  { "R_HOUR12",         M_RECVD_OFS + M_HOUR12 },
  { "R_MIN",            M_RECVD_OFS + M_MIN },
  { "R_SEC",            M_RECVD_OFS + M_SEC },
  { "R_MSEC",           M_RECVD_OFS + M_MSEC },
  { "R_USEC",           M_RECVD_OFS + M_USEC },
  { "R_AMPM",           M_RECVD_OFS + M_AMPM },
  { "R_WEEKDAY",        M_RECVD_OFS + M_WEEK_DAY_ABBREV }, /* deprecated */
  { "R_WEEK_DAY",       M_RECVD_OFS + M_WEEK_DAY },
  { "R_WEEK_DAY_ABBREV", M_RECVD_OFS + M_WEEK_DAY_ABBREV },
  { "R_WEEK_DAY_NAME",  M_RECVD_OFS + M_WEEK_DAY_NAME },
  { "R_WEEK",           M_RECVD_OFS + M_WEEK },
  { "R_ISOWEEK",        M_RECVD_OFS + M_ISOWEEK },
  { "R_TZOFFSET",       M_RECVD_OFS + M_TZOFFSET },
  { "R_TZ",             M_RECVD_OFS + M_TZ },
  { "R_UNIXTIME",       M_RECVD_OFS + M_UNIXTIME },

  { "S_DATE",           M_STAMP_OFS + M_DATE },
  { "S_FULLDATE",       M_STAMP_OFS + M_FULLDATE },
  { "S_ISODATE",        M_STAMP_OFS + M_ISODATE },
  { "S_STAMP",          M_STAMP_OFS + M_STAMP },
  { "S_YEAR",           M_STAMP_OFS + M_YEAR },
  { "S_YEAR_DAY",       M_STAMP_OFS + M_YEAR_DAY },
  { "S_MONTH",          M_STAMP_OFS + M_MONTH },
  { "S_MONTH_WEEK",     M_STAMP_OFS + M_MONTH_WEEK },
  { "S_MONTH_ABBREV",   M_STAMP_OFS + M_MONTH_ABBREV },
  { "S_MONTH_NAME",     M_STAMP_OFS + M_MONTH_NAME },
  { "S_DAY",            M_STAMP_OFS + M_DAY },
  { "S_HOUR",           M_STAMP_OFS + M_HOUR },
  { "S_HOUR12",         M_STAMP_OFS + M_HOUR12 },
  { "S_MIN",            M_STAMP_OFS + M_MIN },
  { "S_SEC",            M_STAMP_OFS + M_SEC },
  { "S_MSEC",           M_STAMP_OFS + M_MSEC },
  { "S_USEC",           M_STAMP_OFS + M_USEC },
  { "S_AMPM",           M_STAMP_OFS + M_AMPM },
  { "S_WEEKDAY",        M_STAMP_OFS + M_WEEK_DAY_ABBREV }, /* deprecated */
  { "S_WEEK_DAY",       M_STAMP_OFS + M_WEEK_DAY },
  { "S_WEEK_DAY_ABBREV", M_STAMP_OFS + M_WEEK_DAY_ABBREV },
  { "S_WEEK_DAY_NAME",  M_STAMP_OFS + M_WEEK_DAY_NAME },
  { "S_WEEK",           M_STAMP_OFS + M_WEEK },
  { "S_ISOWEEK",        M_STAMP_OFS + M_ISOWEEK },
  { "S_TZOFFSET",       M_STAMP_OFS + M_TZOFFSET },
  { "S_TZ",             M_STAMP_OFS + M_TZ },
  { "S_UNIXTIME",       M_STAMP_OFS + M_UNIXTIME },

  { "C_DATE",           M_CSTAMP_OFS + M_DATE },
  { "C_FULLDATE",       M_CSTAMP_OFS + M_FULLDATE },
  { "C_ISODATE",        M_CSTAMP_OFS + M_ISODATE },
  { "C_STAMP",          M_CSTAMP_OFS + M_STAMP },
  { "C_YEAR",           M_CSTAMP_OFS + M_YEAR },
  { "C_YEAR_DAY",       M_CSTAMP_OFS + M_YEAR_DAY },
  { "C_MONTH",          M_CSTAMP_OFS + M_MONTH },
  { "C_MONTH_WEEK",     M_CSTAMP_OFS + M_MONTH_WEEK },
  { "C_MONTH_ABBREV",   M_CSTAMP_OFS + M_MONTH_ABBREV },
  { "C_MONTH_NAME",     M_CSTAMP_OFS + M_MONTH_NAME },
  { "C_DAY",            M_CSTAMP_OFS + M_DAY },
  { "C_HOUR",           M_CSTAMP_OFS + M_HOUR },
  { "C_HOUR12",         M_CSTAMP_OFS + M_HOUR12 },
  { "C_MIN",            M_CSTAMP_OFS + M_MIN },
  { "C_SEC",            M_CSTAMP_OFS + M_SEC },
  { "C_MSEC",           M_CSTAMP_OFS + M_MSEC },
  { "C_USEC",           M_CSTAMP_OFS + M_USEC },
  { "C_AMPM",           M_CSTAMP_OFS + M_AMPM },
  { "C_WEEKDAY",        M_CSTAMP_OFS + M_WEEK_DAY_ABBREV }, /* deprecated */
  { "C_WEEK_DAY",       M_CSTAMP_OFS + M_WEEK_DAY },
  { "C_WEEK_DAY_ABBREV", M_CSTAMP_OFS + M_WEEK_DAY_ABBREV },
  { "C_WEEK_DAY_NAME",  M_CSTAMP_OFS + M_WEEK_DAY_NAME },
  { "C_WEEK",           M_CSTAMP_OFS + M_WEEK },
  { "C_ISOWEEK",        M_CSTAMP_OFS + M_ISOWEEK },
  { "C_TZOFFSET",       M_CSTAMP_OFS + M_TZOFFSET },
  { "C_TZ",             M_CSTAMP_OFS + M_TZ },
  { "C_UNIXTIME",       M_CSTAMP_OFS + M_UNIXTIME },

  { "P_DATE",           M_PROCESSED_OFS + M_DATE },
  { "P_FULLDATE",       M_PROCESSED_OFS + M_FULLDATE },
  { "P_ISODATE",        M_PROCESSED_OFS + M_ISODATE },
  { "P_STAMP",          M_PROCESSED_OFS + M_STAMP },
  { "P_YEAR",           M_PROCESSED_OFS + M_YEAR },
  { "P_YEAR_DAY",       M_PROCESSED_OFS + M_YEAR_DAY },
  { "P_MONTH",          M_PROCESSED_OFS + M_MONTH },
  { "P_MONTH_WEEK",     M_PROCESSED_OFS + M_MONTH_WEEK },
  { "P_MONTH_ABBREV",   M_PROCESSED_OFS + M_MONTH_ABBREV },
  { "P_MONTH_NAME",     M_PROCESSED_OFS + M_MONTH_NAME },
  { "P_DAY",            M_PROCESSED_OFS + M_DAY },
  { "P_HOUR",           M_PROCESSED_OFS + M_HOUR },
  { "P_HOUR12",         M_PROCESSED_OFS + M_HOUR12 },
  { "P_MIN",            M_PROCESSED_OFS + M_MIN },
  { "P_SEC",            M_PROCESSED_OFS + M_SEC },
  { "P_MSEC",           M_PROCESSED_OFS + M_MSEC },
  { "P_USEC",           M_PROCESSED_OFS + M_USEC },
  { "P_AMPM",           M_PROCESSED_OFS + M_AMPM },
  { "P_WEEKDAY",        M_PROCESSED_OFS + M_WEEK_DAY_ABBREV }, /* deprecated */
  { "P_WEEK_DAY",       M_PROCESSED_OFS + M_WEEK_DAY },
  { "P_WEEK_DAY_ABBREV", M_PROCESSED_OFS + M_WEEK_DAY_ABBREV },
  { "P_WEEK_DAY_NAME",  M_PROCESSED_OFS + M_WEEK_DAY_NAME },
  { "P_WEEK",           M_PROCESSED_OFS + M_WEEK },
  { "P_ISOWEEK",        M_PROCESSED_OFS + M_ISOWEEK },
  { "P_TZOFFSET",       M_PROCESSED_OFS + M_TZOFFSET },
  { "P_TZ",             M_PROCESSED_OFS + M_TZ },
  { "P_UNIXTIME",       M_PROCESSED_OFS + M_UNIXTIME },

  { "SDATA", M_SDATA },
  { "MSGHDR", M_MSGHDR },
  { "SOURCEIP", M_SOURCE_IP },
  { "SEQNUM", M_SEQNUM },
  { "CONTEXT_ID", M_CONTEXT_ID },
  { "_", M_CONTEXT_ID },
  { "RCPTID", M_RCPTID },
  { "RUNID", M_RUNID },
  { "HOSTID", M_HOSTID },
  { "UNIQID", M_UNIQID },

  /* values that have specific behaviour with older syslog-ng config versions */
  { "MSG", M_MESSAGE },
  { "MESSAGE", M_MESSAGE },
  { "HOST", M_HOST },

  /* message independent macros */
  { "LOGHOST", M_LOGHOST },
  { NULL, 0 }
};


static GTimeVal app_uptime;
static GHashTable *macro_hash;
static LogTemplateOptions template_options_for_macro_expand;

static void
_result_append_value(GString *result, const LogMessage *lm, NVHandle handle, gboolean escape)
{
  const gchar *str;
  gssize len = 0;

  str = log_msg_get_value(lm, handle, &len);
  result_append(result, str, len, escape);
}

static gboolean
_is_message_source_an_ip_address(const LogMessage *msg)
{
  if (!msg->saddr)
    return FALSE;
  if (g_sockaddr_inet_check(msg->saddr))
    return TRUE;
#if SYSLOG_NG_ENABLE_IPV6
  if (g_sockaddr_inet6_check(msg->saddr))
    return TRUE;
#endif
  return FALSE;
}

static void
log_macro_expand_date_time(GString *result, gint id, gboolean escape, const LogTemplateOptions *opts, gint tz,
                           gint32 seq_num,
                           const gchar *context_id, const LogMessage *msg)
{
  /* year, month, day */
  const UnixTime *stamp;
  UnixTime sstamp;
  guint tmp_hour;

  if (id >= M_TIME_FIRST && id <= M_TIME_LAST)
    {
      stamp = &msg->timestamps[LM_TS_STAMP];
    }
  else if (id >= M_TIME_FIRST + M_RECVD_OFS && id <= M_TIME_LAST + M_RECVD_OFS)
    {
      id -= M_RECVD_OFS;
      stamp = &msg->timestamps[LM_TS_RECVD];
    }
  else if (id >= M_TIME_FIRST + M_STAMP_OFS && id <= M_TIME_LAST + M_STAMP_OFS)
    {
      id -= M_STAMP_OFS;
      stamp = &msg->timestamps[LM_TS_STAMP];
    }
  else if (id >= M_TIME_FIRST + M_CSTAMP_OFS && id <= M_TIME_LAST + M_CSTAMP_OFS)
    {
      id -= M_CSTAMP_OFS;
      unix_time_set_now(&sstamp);
      stamp = &sstamp;
    }
  else if (id >= M_TIME_FIRST + M_PROCESSED_OFS && id <= M_TIME_LAST + M_PROCESSED_OFS)
    {
      id -= M_PROCESSED_OFS;
      stamp = &msg->timestamps[LM_TS_PROCESSED];

      if (!unix_time_is_set(stamp))
        {
          unix_time_set_now(&sstamp);
          stamp = &sstamp;
        }
    }
  else
    {
      g_assert_not_reached();
      return;
    }

  /* try to use the following zone values in order:
   *   destination specific timezone, if one is specified
   *   message specific timezone, if one is specified
   *   local timezone
   */
  WallClockTime wct;

  convert_unix_time_to_wall_clock_time_with_tz_override(stamp, &wct,
                                                        time_zone_info_get_offset(opts->time_zone_info[tz], stamp->ut_sec));
  switch (id)
    {
    case M_WEEK_DAY_ABBREV:
      g_string_append_len(result, weekday_names_abbrev[wct.wct_wday], WEEKDAY_NAME_ABBREV_LEN);
      break;
    case M_WEEK_DAY_NAME:
      g_string_append(result, weekday_names[wct.wct_wday]);
      break;
    case M_WEEK_DAY:
      format_uint32_padded(result, 0, 0, 10, wct.wct_wday + 1);
      break;
    case M_WEEK:
      format_uint32_padded(result, 2, '0', 10, (wct.wct_yday - (wct.wct_wday - 1 + 7) % 7 + 7) / 7);
      break;
    case M_ISOWEEK:
      format_uint32_padded(result, 2, '0', 10, wall_clock_time_iso_week_number(&wct));
      break;
    case M_YEAR:
      format_uint32_padded(result, 4, '0', 10, wct.wct_year + 1900);
      break;
    case M_YEAR_DAY:
      format_uint32_padded(result, 3, '0', 10, wct.wct_yday + 1);
      break;
    case M_MONTH:
      format_uint32_padded(result, 2, '0', 10, wct.wct_mon + 1);
      break;
    case M_MONTH_WEEK:
      format_uint32_padded(result, 0, 0, 10, ((wct.wct_mday / 7) +
                                              ((wct.wct_wday > 0) &&
                                               ((wct.wct_mday % 7) >= wct.wct_wday))));
      break;
    case M_MONTH_ABBREV:
      g_string_append_len(result, month_names_abbrev[wct.wct_mon], MONTH_NAME_ABBREV_LEN);
      break;
    case M_MONTH_NAME:
      g_string_append(result, month_names[wct.wct_mon]);
      break;
    case M_DAY:
      format_uint32_padded(result, 2, '0', 10, wct.wct_mday);
      break;
    case M_HOUR:
      format_uint32_padded(result, 2, '0', 10, wct.wct_hour);
      break;
    case M_HOUR12:
      if (wct.wct_hour < 12)
        tmp_hour = wct.wct_hour;
      else
        tmp_hour = wct.wct_hour - 12;

      if (tmp_hour == 0)
        tmp_hour = 12;
      format_uint32_padded(result, 2, '0', 10, tmp_hour);
      break;
    case M_MIN:
      format_uint32_padded(result, 2, '0', 10, wct.wct_min);
      break;
    case M_SEC:
      format_uint32_padded(result, 2, '0', 10, wct.wct_sec);
      break;
    case M_MSEC:
      format_uint32_padded(result, 3, '0', 10, stamp->ut_usec/1000);
      break;
    case M_USEC:
      format_uint32_padded(result, 6, '0', 10, stamp->ut_usec);
      break;
    case M_AMPM:
      g_string_append(result, wct.wct_hour < 12 ? "AM" : "PM");
      break;
    case M_DATE:
      append_format_wall_clock_time(&wct, result, TS_FMT_BSD, opts->frac_digits);
      break;
    case M_STAMP:
      if (opts->ts_format == TS_FMT_UNIX)
        append_format_unix_time(stamp, result, TS_FMT_UNIX, wct.wct_gmtoff, opts->frac_digits);
      else
        append_format_wall_clock_time(&wct, result, opts->ts_format, opts->frac_digits);
      break;
    case M_ISODATE:
      append_format_wall_clock_time(&wct, result, TS_FMT_ISO, opts->frac_digits);
      break;
    case M_FULLDATE:
      append_format_wall_clock_time(&wct, result, TS_FMT_FULL, opts->frac_digits);
      break;
    case M_UNIXTIME:
      append_format_unix_time(stamp, result, TS_FMT_UNIX, wct.wct_gmtoff, opts->frac_digits);
      break;
    case M_TZ:
    case M_TZOFFSET:
      append_format_zone_info(result, wct.wct_gmtoff);
      break;
    default:
      g_assert_not_reached();
      break;
    }
}

gboolean
log_macro_expand(GString *result, gint id, gboolean escape, const LogTemplateOptions *opts, gint tz, gint32 seq_num,
                 const gchar *context_id, const LogMessage *msg)
{
  switch (id)
    {
    case M_FACILITY:
    {
      /* facility */
      const char *n;

      n = syslog_name_lookup_name_by_value(msg->pri & LOG_FACMASK, sl_facilities);
      if (n)
        {
          g_string_append(result, n);
        }
      else
        {
          format_uint32_padded(result, 0, 0, 16, (msg->pri & LOG_FACMASK) >> 3);
        }
      break;
    }
    case M_FACILITY_NUM:
    {
      format_uint32_padded(result, 0, 0, 10, (msg->pri & LOG_FACMASK) >> 3);
      break;
    }
    case M_LEVEL:
    {
      /* level */
      const char *n;

      n = syslog_name_lookup_name_by_value(msg->pri & LOG_PRIMASK, sl_levels);
      if (n)
        {
          g_string_append(result, n);
        }
      else
        {
          format_uint32_padded(result, 0, 0, 10, msg->pri & LOG_PRIMASK);
        }

      break;
    }
    case M_LEVEL_NUM:
    {
      format_uint32_padded(result, 0, 0, 10, msg->pri & LOG_PRIMASK);
      break;
    }
    case M_TAG:
    {
      format_uint32_padded(result, 2, '0', 16, msg->pri);
      break;
    }
    case M_TAGS:
    {
      log_msg_print_tags(msg, result);
      break;
    }
    case M_BSDTAG:
    {
      format_uint32_padded(result, 0, 0, 10, (msg->pri & LOG_PRIMASK));
      g_string_append_c(result, (((msg->pri & LOG_FACMASK) >> 3) + 'A'));
      break;
    }
    case M_PRI:
    {
      format_uint32_padded(result, 0, 0, 10, msg->pri);
      break;
    }
    case M_HOST:
    {
      if (msg->flags & LF_CHAINED_HOSTNAME)
        {
          /* host */
          const gchar *p1, *p2;
          int remaining, length;
          gssize host_len;
          const gchar *host = log_msg_get_value(msg, LM_V_HOST, &host_len);

          p1 = memchr(host, '@', host_len);

          if (p1)
            p1++;
          else
            p1 = host;
          remaining = host_len - (p1 - host);
          p2 = memchr(p1, '/', remaining);
          length = p2 ? p2 - p1
                   : host_len - (p1 - host);

          result_append(result, p1, length, escape);
        }
      else
        {
          _result_append_value(result, msg, LM_V_HOST, escape);
        }
      break;
    }
    case M_SDATA:
    {
      if (escape)
        {
          GString *sdstr = g_string_sized_new(0);

          log_msg_append_format_sdata(msg, sdstr, seq_num);
          result_append(result, sdstr->str, sdstr->len, TRUE);
          g_string_free(sdstr, TRUE);
        }
      else
        {
          log_msg_append_format_sdata(msg, result, seq_num);
        }
      break;
    }
    case M_MSGHDR:
    {
      gssize len;
      const gchar *p;

      p = log_msg_get_value(msg, LM_V_LEGACY_MSGHDR, &len);
      if (len > 0)
        result_append(result, p, len, escape);
      else
        {
          /* message, complete with program name and pid */
          len = result->len;
          _result_append_value(result, msg, LM_V_PROGRAM, escape);
          if (len != result->len)
            {
              const gchar *pid = log_msg_get_value(msg, LM_V_PID, &len);
              if (len > 0)
                {
                  result_append(result, "[", 1, FALSE);
                  result_append(result, pid, len, escape);
                  result_append(result, "]", 1, FALSE);
                }
              result_append(result, ": ", 2, FALSE);
            }
        }
      break;
    }
    case M_MESSAGE:
    {
      _result_append_value(result, msg, LM_V_MESSAGE, escape);
      break;
    }
    case M_SOURCE_IP:
    {
      gchar *ip;
      gchar buf[MAX_SOCKADDR_STRING];

      if (_is_message_source_an_ip_address(msg))
        {
          g_sockaddr_format(msg->saddr, buf, sizeof(buf), GSA_ADDRESS_ONLY);
          ip = buf;
        }
      else
        {
          ip = "127.0.0.1";
        }
      result_append(result, ip, strlen(ip), escape);
      break;
    }
    case M_SEQNUM:
    {
      if (seq_num)
        {
          format_uint32_padded(result, 0, 0, 10, seq_num);
        }
      break;
    }
    case M_CONTEXT_ID:
    {
      if (context_id)
        {
          result_append(result, context_id, strlen(context_id), escape);
        }
      break;
    }

    case M_RCPTID:
    {
      rcptid_append_formatted_id(result, msg->rcptid);
      break;
    }

    case M_RUNID:
    {
      run_id_append_formatted_id(result);
      break;
    }

    case M_HOSTID:
    {
      host_id_append_formatted_id(result, msg->host_id);
      break;
    }

    case M_UNIQID:
    {
      if (msg->rcptid)
        {
          host_id_append_formatted_id(result, msg->host_id);
          g_string_append(result, "@");
          format_uint64_padded(result, 16, '0', 16, msg->rcptid);
          break;
        }
      break;
    }

    case M_LOGHOST:
    {
      const gchar *hname = opts->use_fqdn
                           ? get_local_hostname_fqdn()
                           : get_local_hostname_short();

      result_append(result, hname, -1, escape);
      break;
    }
    case M_SYSUPTIME:
    {
      GTimeVal ct;

      g_get_current_time(&ct);
      format_uint64_padded(result, 0, 0, 10, g_time_val_diff(&ct, &app_uptime) / 1000 / 10);
      break;
    }

    default:
    {
      log_macro_expand_date_time(result, id, escape, opts, tz, seq_num, context_id, msg);
      break;
    }

    }
  return TRUE;
}

gboolean
log_macro_expand_simple(GString *result, gint id, const LogMessage *msg)
{
  return log_macro_expand(result, id, FALSE, &template_options_for_macro_expand, LTZ_LOCAL, 0, NULL, msg);
}

guint
log_macro_lookup(const gchar *macro, gint len)
{
  gchar buf[256];
  gint macro_id;

  g_assert(macro_hash);
  g_strlcpy(buf, macro, MIN(sizeof(buf), len+1));
  macro_id = GPOINTER_TO_INT(g_hash_table_lookup(macro_hash, buf));
  return macro_id;
}

void
log_macros_global_init(void)
{
  gint i;

  /* init the uptime (SYSUPTIME macro) */
  g_get_current_time(&app_uptime);
  log_template_options_defaults(&template_options_for_macro_expand);

  macro_hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  for (i = 0; macros[i].name; i++)
    {
      g_hash_table_insert(macro_hash, g_strdup(macros[i].name),
                          GINT_TO_POINTER(macros[i].id));
    }
  return;
}

void
log_macros_global_deinit(void)
{
  g_hash_table_destroy(macro_hash);
  macro_hash = NULL;
}
