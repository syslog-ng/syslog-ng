/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2013 Balázs Scheidler
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

#include "template/templates.h"
#include "messages.h"
#include "logmsg.h"
#include "syslog-names.h"
#include "messages.h"
#include "hostname.h"
#include "filter/filter-expr.h"
#include "gsocket.h"
#include "plugin.h"
#include "plugin-types.h"
#include "str-format.h"
#include "rcptid.h"
#include "run-id.h"

#include <time.h>
#include <string.h>

/* macro IDs */
enum
{
  M_NONE,

  M_FACILITY,
  M_FACILITY_NUM,
  M_LEVEL,
  M_LEVEL_NUM,
  M_TAG,
  M_TAGS,
  M_BSDTAG,
  M_PRI,

  M_HOST,
  M_SDATA,

  M_MSGHDR,
  M_MESSAGE,
  M_SOURCE_IP,
  M_SEQNUM,
  M_CONTEXT_ID,

  M_LOGHOST,
  M_SYSUPTIME,
  M_RCPTID,
  M_RUNID,

  /* only touch this section if you want to add three macros, one w/o
   * prefix, and a R_ and S_ prefixed macro that relates one of the
   * timestamps of the log message. */

  M_DATE,
  M_FULLDATE,
  M_ISODATE,
  M_STAMP,
  M_YEAR,
  M_YEAR_DAY,
  M_MONTH,
  M_MONTH_WEEK,
  M_MONTH_ABBREV,
  M_MONTH_NAME,
  M_DAY,
  M_HOUR,
  M_HOUR12,
  M_MIN,
  M_SEC,
  M_USEC,
  M_MSEC,
  M_AMPM,
  M_WEEK_DAY,
  M_WEEK_DAY_ABBREV,
  M_WEEK_DAY_NAME,
  M_WEEK,
  M_TZOFFSET,
  M_TZ,
  M_UNIXTIME,
  M_TIME_FIRST = M_DATE,
  M_TIME_LAST = M_UNIXTIME,
  M_TIME_MACROS_MAX = M_UNIXTIME - M_DATE + 1,

  M_RECVD_OFS = M_TIME_MACROS_MAX,
  M_STAMP_OFS = 2 * M_TIME_MACROS_MAX,
  M_CSTAMP_OFS = 3 * M_TIME_MACROS_MAX,
};

#define M_TIME_MACROS 15

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
        { "WEEK_DAY_ABBREV",M_WEEK_DAY_ABBREV },
        { "WEEK_DAY_NAME",  M_WEEK_DAY_NAME },
        { "WEEK",           M_WEEK },
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
        { "R_WEEK_DAY_ABBREV",M_RECVD_OFS + M_WEEK_DAY_ABBREV },
        { "R_WEEK_DAY_NAME",  M_RECVD_OFS + M_WEEK_DAY_NAME },
        { "R_WEEK",           M_RECVD_OFS + M_WEEK },
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
        { "S_WEEK_DAY_ABBREV",M_STAMP_OFS + M_WEEK_DAY_ABBREV },
        { "S_WEEK_DAY_NAME",  M_STAMP_OFS + M_WEEK_DAY_NAME },
        { "S_WEEK",           M_STAMP_OFS + M_WEEK },
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
        { "C_MIN",            M_CSTAMP_OFS + M_MIN },
        { "C_SEC",            M_CSTAMP_OFS + M_SEC },
        { "C_WEEKDAY",        M_CSTAMP_OFS + M_WEEK_DAY_ABBREV }, /* deprecated */
        { "C_WEEK_DAY",       M_CSTAMP_OFS + M_WEEK_DAY },
        { "C_WEEK_DAY_ABBREV",M_CSTAMP_OFS + M_WEEK_DAY_ABBREV },
        { "C_WEEK_DAY_NAME",  M_CSTAMP_OFS + M_WEEK_DAY_NAME },
        { "C_WEEK",           M_CSTAMP_OFS + M_WEEK },
        { "C_TZOFFSET",       M_CSTAMP_OFS + M_TZOFFSET },
        { "C_TZ",             M_CSTAMP_OFS + M_TZ },
        { "C_UNIXTIME",       M_CSTAMP_OFS + M_UNIXTIME },

        { "SDATA", M_SDATA },
        { "MSGHDR", M_MSGHDR },
        { "SOURCEIP", M_SOURCE_IP },
        { "SEQNUM", M_SEQNUM },
        { "CONTEXT_ID", M_CONTEXT_ID },
        { "RCPTID", M_RCPTID },
        { "RUNID", M_RUNID },

        /* values that have specific behaviour with older syslog-ng config versions */
        { "MSG", M_MESSAGE },
        { "MESSAGE", M_MESSAGE },
        { "HOST", M_HOST },

        /* message independent macros */
        { "LOGHOST", M_LOGHOST },
        { NULL, 0 }
};

GHashTable *macro_hash;
GTimeVal app_uptime;

static void
result_append(GString *result, const gchar *sstr, gssize len, gboolean escape)
{
  gint i;
  const guchar *ustr = (const guchar *) sstr;

  if (len < 0)
    len = strlen(sstr);

  if (escape)
    {
      for (i = 0; i < len; i++)
        {
          if (ustr[i] == '\'' || ustr[i] == '"' || ustr[i] == '\\')
            {
              g_string_append_c(result, '\\');
              g_string_append_c(result, ustr[i]);
            }
          else if (ustr[i] < ' ')
            {
              format_uint32_padded(result, 3, '0', 8, ustr[i]);
            }
          else
            g_string_append_c(result, ustr[i]);
        }
    }
  else
    g_string_append_len(result, sstr, len);
}

static void
result_append_value(GString *result, LogMessage *lm, NVHandle handle, gboolean escape)
{
  const gchar *str;
  gssize len = 0;

  str = log_msg_get_value(lm, handle, &len);
  result_append(result, str, len, escape);
}

gboolean
log_macro_expand(GString *result, gint id, gboolean escape, const LogTemplateOptions *opts, gint tz, gint32 seq_num, const gchar *context_id, LogMessage *msg)
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
            result_append_value(result, msg, LM_V_HOST, escape);
          }
        break;
      }
    case M_SDATA:
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
    case M_MSGHDR:
      if ((msg->flags & LF_LEGACY_MSGHDR))
        {
          /* fast path for now, as most messages come from legacy devices */

          result_append_value(result, msg, LM_V_LEGACY_MSGHDR, escape);
        }
      else
        {
          /* message, complete with program name and pid */
          gssize len;

          len = result->len;
          result_append_value(result, msg, LM_V_PROGRAM, escape);
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
    case M_MESSAGE:
      if (cfg_is_config_version_older(configuration, 0x0300))
        log_macro_expand(result, M_MSGHDR, escape, opts, tz, seq_num, context_id, msg);
      result_append_value(result, msg, LM_V_MESSAGE, escape);
      break;
    case M_SOURCE_IP:
      {
        gchar *ip;

        if (msg->saddr && (g_sockaddr_inet_check(msg->saddr) ||
#if ENABLE_IPV6
            g_sockaddr_inet6_check(msg->saddr))
#else
            0)
#endif
           )
          {
            gchar buf[MAX_SOCKADDR_STRING];

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

    case M_LOGHOST:
      {
        const gchar *hname = get_local_hostname_fqdn();

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
        /* year, month, day */
        struct tm *tm, tm_storage;
        gchar buf[64];
        gint length;
        time_t t;
        LogStamp *stamp, sstamp;
        glong zone_ofs;
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
	    GTimeVal tv;

	    id -= M_CSTAMP_OFS;
	    cached_g_current_time(&tv);
	    sstamp.tv_sec = tv.tv_sec;
	    sstamp.tv_usec = tv.tv_usec;
	    sstamp.zone_offset = -1;
	    stamp = &sstamp;
	  }
	else
          {
            g_assert_not_reached();
            break;
          }

        /* try to use the following zone values in order:
         *   destination specific timezone, if one is specified
         *   message specific timezone, if one is specified
         *   local timezone
         */
        zone_ofs = (opts->time_zone_info[tz] != NULL ? time_zone_info_get_offset(opts->time_zone_info[tz], stamp->tv_sec) : stamp->zone_offset);
        if (zone_ofs == -1)
          zone_ofs = stamp->zone_offset;

        t = stamp->tv_sec + zone_ofs;

        cached_gmtime(&t, &tm_storage);
        tm  = &tm_storage;

        switch (id)
          {
          case M_WEEK_DAY_ABBREV:
            g_string_append_len(result, weekday_names_abbrev[tm->tm_wday], 3);
            break;
          case M_WEEK_DAY_NAME:
            g_string_append(result, weekday_names[tm->tm_wday]);
            break;
          case M_WEEK_DAY:
            format_uint32_padded(result, 0, 0, 10, tm->tm_wday + 1);
            break;
          case M_WEEK:
            format_uint32_padded(result, 2, '0', 10, (tm->tm_yday - (tm->tm_wday - 1 + 7) % 7 + 7) / 7);
            break;
          case M_YEAR:
            format_uint32_padded(result, 4, '0', 10, tm->tm_year + 1900);
            break;
          case M_YEAR_DAY:
            format_uint32_padded(result, 3, '0', 10, tm->tm_yday + 1);
            break;
          case M_MONTH:
            format_uint32_padded(result, 2, '0', 10, tm->tm_mon + 1);
            break;
          case M_MONTH_WEEK:
            format_uint32_padded(result, 0, 0, 10, ((tm->tm_mday / 7) + ((tm->tm_wday > 0) && ((tm->tm_mday % 7) >= tm->tm_wday))));
            break;
          case M_MONTH_ABBREV:
            g_string_append_len(result, month_names_abbrev[tm->tm_mon], 3);
            break;
          case M_MONTH_NAME:
            g_string_append(result, month_names[tm->tm_mon]);
            break;
          case M_DAY:
            format_uint32_padded(result, 2, '0', 10, tm->tm_mday);
            break;
          case M_HOUR:
            format_uint32_padded(result, 2, '0', 10, tm->tm_hour);
            break;
          case M_HOUR12:
            if (tm->tm_hour < 12)
              tmp_hour = tm->tm_hour;
            else
              tmp_hour = tm->tm_hour - 12;

            if (tmp_hour == 0)
              tmp_hour = 12;
            format_uint32_padded(result, 2, '0', 10, tmp_hour);
            break;
          case M_MIN:
            format_uint32_padded(result, 2, '0', 10, tm->tm_min);
            break;
          case M_SEC:
            format_uint32_padded(result, 2, '0', 10, tm->tm_sec);
            break;
          case M_MSEC:
            format_uint32_padded(result, 3, '0', 10, stamp->tv_usec/1000);
            break;
          case M_USEC:
            format_uint32_padded(result, 6, '0', 10, stamp->tv_usec);
            break;
          case M_AMPM:
            g_string_append(result, tm->tm_hour < 12 ? "AM" : "PM");
            break;
          case M_DATE:
          case M_STAMP:
          case M_ISODATE:
          case M_FULLDATE:
          case M_UNIXTIME:
            {
              gint format = id == M_DATE ? TS_FMT_BSD :
                            id == M_ISODATE ? TS_FMT_ISO :
                            id == M_FULLDATE ? TS_FMT_FULL :
                            id == M_UNIXTIME ? TS_FMT_UNIX :
                            opts->ts_format;

              log_stamp_append_format(stamp, result, format, zone_ofs, opts->frac_digits);
              break;
            }
          case M_TZ:
          case M_TZOFFSET:
            length = format_zone_info(buf, sizeof(buf), zone_ofs);
            g_string_append_len(result, buf, length);
            break;
          }
        break;
      }
    }
  return TRUE;
}

guint
log_macro_lookup(gchar *macro, gint len)
{
  gchar buf[256];
  gint macro_id;

  g_assert(macro_hash);
  g_strlcpy(buf, macro, MIN(sizeof(buf), len+1));
  macro_id = GPOINTER_TO_INT(g_hash_table_lookup(macro_hash, buf));

  if (cfg_is_config_version_older(configuration, 0x0300) && (macro_id == M_MESSAGE))
    {
      static gboolean msg_macro_warning = FALSE;

      if (!msg_macro_warning)
        {
          msg_warning("WARNING: template: the meaning of the $MSG/$MESSAGE macros has changed from " VERSION_3_0 ", please prepend a $MSGHDR when upgrading to " VERSION_3_0 " config format", NULL);
          msg_macro_warning = TRUE;
        }
    }
  return macro_id;
}

typedef struct
{
  LogTemplate *template;
  GList *result;
  gchar *cursor;
  GString *text;
  gint msg_ref;
} LogTemplateCompiler;

enum
{
  LTE_MACRO,
  LTE_VALUE,
  LTE_FUNC
};

typedef struct _LogTemplateElem
{
  gsize text_len;
  gchar *text;
  gchar *default_value;
  guint16 msg_ref;
  guint8 type;
  union
  {
    guint macro;
    NVHandle value_handle;
    struct
    {
      LogTemplateFunction *ops;
      gpointer state;
    } func;
  };
} LogTemplateElem;


/* simple template functions which take templates as arguments */

gboolean
tf_simple_func_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent, gint argc, gchar *argv[], GError **error)
{
  TFSimpleFuncState *state = (TFSimpleFuncState *) s;
  gint i;

  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
  state->argv = g_malloc(sizeof(LogTemplate *) * (argc - 1));

  /* NOTE: the argv argument con tains the function name as argv[0],
   * but the LogTemplate array doesn't. Thus the index is shifted by
   * one. */
  for (i = 0; i < argc - 1; i++)
    {
      state->argv[i] = log_template_new(parent->cfg, NULL);
      log_template_set_escape(state->argv[i], parent->escape);
      if (!log_template_compile(state->argv[i], argv[i + 1], error))
        goto error;
    }
  state->argc = argc - 1;
  return TRUE;
 error:
  return FALSE;
}

void
tf_simple_func_eval(LogTemplateFunction *self, gpointer s, const LogTemplateInvokeArgs *args)
{
  TFSimpleFuncState *state = (TFSimpleFuncState *) s;
  gint i;

  for (i = 0; i < state->argc; i++)
    {
      GString **arg;

      if (args->bufs->len <= i)
        g_ptr_array_add(args->bufs, g_string_sized_new(256));

      arg = (GString **) &g_ptr_array_index(args->bufs, i);
      g_string_truncate(*arg, 0);

      log_template_append_format_recursive(state->argv[i], args, *arg);
    }
}

void
tf_simple_func_call(LogTemplateFunction *self, gpointer s, const LogTemplateInvokeArgs *args, GString *result)
{
  TFSimpleFunc simple_func = (TFSimpleFunc) self->arg;
  TFSimpleFuncState *state = (TFSimpleFuncState *) s;

  simple_func(args->messages[args->num_messages-1], state->argc, (GString **) args->bufs->pdata, result);
}

void
tf_simple_func_free_state(gpointer s)
{
  TFSimpleFuncState *state = (TFSimpleFuncState *) s;
  gint i;

  for (i = 0; i < state->argc; i++)
    {
      if (state->argv[i])
        log_template_unref(state->argv[i]);
    }
  g_free(state->argv);
}

static void
log_template_add_macro_elem(LogTemplateCompiler *self, guint macro, gchar *default_value)
{
  LogTemplateElem *e;

  e = g_new0(LogTemplateElem, 1);
  e->type = LTE_MACRO;
  e->text_len = self->text ? self->text->len : 0;
  e->text = self->text ? g_strndup(self->text->str, self->text->len) : NULL;
  e->macro = macro;
  e->default_value = default_value;
  e->msg_ref = self->msg_ref;
  self->result = g_list_prepend(self->result, e);
}

static void
log_template_add_value_elem(LogTemplateCompiler *self, gchar *value_name, gsize value_name_len, gchar *default_value)
{
  LogTemplateElem *e;
  gchar *dup;

  e = g_new0(LogTemplateElem, 1);
  e->type = LTE_VALUE;
  e->text_len = self->text ? self->text->len : 0;
  e->text = self->text ? g_strndup(self->text->str, self->text->len) : NULL;
  /* value_name is not NUL terminated */
  dup = g_strndup(value_name, value_name_len);
  e->value_handle = log_msg_get_value_handle(dup);
  g_free(dup);
  e->default_value = default_value;
  e->msg_ref = self->msg_ref;
  self->result = g_list_prepend(self->result, e);
}


/* NOTE: this steals argv if successful */
static gboolean
log_template_add_func_elem(LogTemplateCompiler *self, gint argc, gchar *argv[], GError **error)
{
  LogTemplateElem *e;
  Plugin *p;
  gchar *argv_copy[argc + 1];

  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  if (argc == 0)
    return TRUE;

  e = g_malloc0(sizeof(LogTemplateElem) + (argc - 1) * sizeof(LogTemplate *));
  e->type = LTE_FUNC;
  e->text_len = self->text ? self->text->len : 0;
  e->text = self->text ? g_strndup(self->text->str, self->text->len) : NULL;
  e->msg_ref = self->msg_ref;

  p = plugin_find(self->template->cfg, LL_CONTEXT_TEMPLATE_FUNC, argv[0]);
  if (!p)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE, "Unknown template function %s", argv[0]);
      goto error;
    }
  else
    {
      e->func.ops = plugin_construct(p, self->template->cfg, LL_CONTEXT_TEMPLATE_FUNC, argv[0]);
    }

  e->func.state = g_malloc0(e->func.ops->size_of_state);

  /* prepare may modify the argv array: remove and rearrange elements */
  memcpy(argv_copy, argv, (argc + 1) * sizeof(argv[0]));
  if (!e->func.ops->prepare(e->func.ops, e->func.state, self->template, argc, argv_copy, error))
    {
      e->func.ops->free_state(e->func.state);
      g_free(e->func.state);
      goto error;
    }
  g_strfreev(argv);
  self->result = g_list_prepend(self->result, e);
  return TRUE;

 error:
  if (e->text)
    g_free(e->text);
  g_free(e);
  return FALSE;
}

static void
log_template_elem_free(LogTemplateElem *e)
{
  switch (e->type)
    {
    case LTE_FUNC:
      if (e->func.state)
        {
          e->func.ops->free_state(e->func.state);
          g_free(e->func.state);
        }
      break;
    }
  if (e->default_value)
    g_free(e->default_value);
  if (e->text)
    g_free(e->text);
  g_free(e);
}

static void
log_template_elem_free_list(GList *el)
{
  for (; el; el = el->next)
    {
      log_template_elem_free((LogTemplateElem *) el->data);
    }
  g_list_free(el);
}

static void
parse_msg_ref(LogTemplateCompiler *self)
{
  self->msg_ref = 0;
  if ((*self->cursor) == '@')
    {
      self->cursor++;
      if ((*self->cursor) >= '0' && (*self->cursor) <= '9')
        {
          /* syntax: ${name}@1 to denote the log message index in the correllation state */
          while ((*self->cursor) >= '0' && (*self->cursor) <= '9')
            {
              self->msg_ref += self->msg_ref * 10 + ((*self->cursor) - '0');
              self->cursor++;
            }
          self->msg_ref += 1;
        }
      else
        {
          if ((*self->cursor) != '@')
            {
              msg_warning("Non-numeric correlation state ID found, assuming a literal '@' character. To avoid confusion when using a literal '@' after a macro or template function, write '@@' in the template.",
                          evt_tag_str("Template", self->template->template),
                          NULL);
              self->cursor--;
            }
          self->msg_ref = 0;
        }
    }
}

static void
log_template_compiler_fill_compile_error(GError **error, const gchar *error_info, gint error_pos)
{
  g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE, "%s, error_pos='%d'", error_info, error_pos);
}

static void
log_template_compiler_append_and_increment(LogTemplateCompiler *self, GString *text)
{
  g_string_append_c(text, *self->cursor);
  self->cursor++;
}

static gint
log_template_compiler_get_macro_length(gchar *start, gchar *end, gchar **token)
{
  gint result = 0;
  gchar *colon = memchr(start, ':', end - start - 1);
  if (colon)
    {
      result = colon - start;
      *token = colon < end ? colon + 1 : NULL;
    }
  else
    {
      result = end - start - 1;
      *token = NULL;
    }
  return result;
}


static gchar *
log_template_compiler_get_default_value(LogTemplateCompiler *self, gchar *token)
{
  g_assert(token);
  if (*token != '-')
    {
      return NULL;
    }
  return g_strndup(token + 1, self->cursor - token - 2);
}

static inline gboolean
is_macro_name(gchar c)
{
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c == '_') || (c >= '0' && c <= '9');
}

#define STEP_BY_TRUE(p, x) while(x) p++;

static void
log_template_compiler_add_elem(LogTemplateCompiler *self, gchar *start, gint macro_len, gchar *default_value)
{
  gint macro = log_macro_lookup(start, macro_len);
  if (macro == M_NONE)
    {
      log_template_add_value_elem(self, start, macro_len, default_value);
    }
  else
    {
      log_template_add_macro_elem(self, macro, default_value);
    }
}

static gboolean
log_template_compiler_process_braced_template(LogTemplateCompiler *self, GError **error)
{
  gint macro_len;
  gchar *default_value = NULL;
  gchar *start;
  gchar *end;
  gchar *token;

  self->cursor++;
  start = self->cursor;
  end = strchr(self->cursor, '}');

  if (!end)
    {
      log_template_compiler_fill_compile_error(error, "Invalid macro, '}' is missing", strlen(self->template->template));
      return FALSE;
    }

  self->cursor = end + 1;
  macro_len = log_template_compiler_get_macro_length(start, self->cursor, &token);
  if (token)
    {
      default_value = log_template_compiler_get_default_value(self, token);
      if (!default_value)
        {
          log_template_compiler_fill_compile_error(error, "Unknown substitution function", token - self->template->template);
          return FALSE;
        }
    }
  parse_msg_ref(self);
  log_template_compiler_add_elem(self, start, macro_len, default_value);
  return TRUE;
}

static gboolean
log_template_compiler_add_quoted_string(LogTemplateCompiler *self, gboolean is_top_level, GString *result)
{
  gchar *quote = self->cursor;
  gchar *end_of_quote = strchr(quote + 1, *quote);
  if (!end_of_quote)
    {
      return FALSE;
    }
  self->cursor = end_of_quote + 1;
  if (is_top_level)
    {
      /* skip the quote in top-level and don't skip in expressions enclosed in parens */
      quote++;
    }
  else
    {
      end_of_quote++;
    }
  g_string_append_len(result, quote, end_of_quote - quote);
  return TRUE;
}

static gboolean
log_template_compiler_process_arg_list(LogTemplateCompiler *self, GPtrArray *result)
{
  GString *arg_buf = g_string_sized_new(32);
  gint parens = 1;
  self->cursor++;

  while (*self->cursor && *self->cursor == ' ')
    self->cursor++;

  while(*self->cursor)
    {
      if (*self->cursor == '\\')
        {
          self->cursor++;
        }
      else if (*self->cursor == '(')
        {
          parens++;
        }
      else if (*self->cursor == ')')
        {
          parens--;
          if (parens == 0)
            {
              break;
            }
        }
      else if (*self->cursor == '"' || *self->cursor == '\'')
        {
          if (!log_template_compiler_add_quoted_string(self, parens == 1, arg_buf))
            {
              g_ptr_array_add(result, NULL);
              g_string_free(arg_buf, TRUE);
              return FALSE;
            }
          continue;
        }
      else if (parens == 1 && (*self->cursor == ' ' || *self->cursor == '\t'))
        {
          g_ptr_array_add(result, g_strndup(arg_buf->str, arg_buf->len));
          g_string_truncate(arg_buf, 0);
          while (*self->cursor && (*self->cursor == ' ' || *self->cursor == '\t'))
            self->cursor++;
          continue;
        }
      log_template_compiler_append_and_increment(self, arg_buf);
    }
  if (arg_buf->len > 0)
    {
      g_ptr_array_add(result, g_strndup(arg_buf->str, arg_buf->len));
    }
  g_ptr_array_add(result, NULL);
  g_string_free(arg_buf, TRUE);
  return *self->cursor == ')';
}

static gboolean
log_template_compiler_process_template_function(LogTemplateCompiler *self, GError **error)
{
  GPtrArray *strv = g_ptr_array_new();

  if (!log_template_compiler_process_arg_list(self, strv))
    {
      log_template_compiler_fill_compile_error(error, "Invalid template function reference, missing function name or inbalanced '('", self->cursor - self->template->template);
      goto error;
    }
  self->cursor++;

  parse_msg_ref(self);
  if (!log_template_add_func_elem(self, strv->len - 1, (gchar **) strv->pdata, error))
    {
      goto error;
    }
  g_ptr_array_free(strv, FALSE);
  return TRUE;
error:
  g_strfreev((gchar **)strv->pdata);
  g_ptr_array_free(strv, FALSE);
  return FALSE;
}

static void
log_template_compiler_process_unbraced_template(LogTemplateCompiler *self)
{
  gchar *start = self->cursor;
  gint macro_len;
  do
    {
      self->cursor++;
    }
  while (is_macro_name(*self->cursor));
  macro_len = self->cursor - start;
  log_template_compiler_add_elem(self, start, macro_len, NULL);
}

static gboolean
log_template_compiler_process_value(LogTemplateCompiler *self, GError **error)
{
  gboolean finished = FALSE;
  gchar p;
  self->cursor++;
  p = *self->cursor;
  /* macro reference */
  if (p == '{')
    {
      if (!log_template_compiler_process_braced_template(self, error))
        {
          return FALSE;
        }
      finished = TRUE;
    }
  /* template function */
  else if (p == '(')
    {
      if (!log_template_compiler_process_template_function(self, error))
        {
          return FALSE;
        }
      finished = TRUE;
    }
  /* unbraced macro */
  else if (is_macro_name(p))
    {
      log_template_compiler_process_unbraced_template(self);
      finished = TRUE;
    }
  /* escaped value with dollar */
  else
    {
      if (p != '$')
        {
          g_string_append_c(self->text, '$');
        }
      if (p)
        {
          log_template_compiler_append_and_increment(self, self->text);
        }
    }
  if (finished)
    {
      g_string_truncate(self->text, 0);
    }
  return TRUE;
}

gboolean
log_template_compiler_process_token(LogTemplateCompiler *self, GError **error)
{
  self->msg_ref = 0;
  if (*self->cursor == '$')
    {
      return log_template_compiler_process_value(self, error);
    }
  if (*self->cursor == '\\')
    {
      if (cfg_is_config_version_older(self->template->cfg, 0x305))
        {
          msg_warning("Template escaping changed in version 3.5. Use '$$' to specify a literal dollar sign instead of '\\$' and remove the escaping of the backslash character when you upgrade your configuration",
                      evt_tag_str("Template", self->template->template),
                      NULL);
          self->cursor++;
        }

    }
  if (*self->cursor)
    {
      log_template_compiler_append_and_increment(self, self->text);
    }
  return TRUE;
}

static void
log_template_compiler_free_result(LogTemplateCompiler *self)
{
  log_template_elem_free_list(self->result);
  self->result = NULL;
}

static gboolean
log_template_compiler_compile(LogTemplateCompiler *self, GList **compiled_template, GError **error)
{
  gboolean result = FALSE;

  while (*self->cursor)
    {
      if (!log_template_compiler_process_token(self, error))
        {
          log_template_compiler_free_result(self);
          g_string_sprintf(self->text, "error in template: %s", self->template->template);
          log_template_add_macro_elem(self, M_NONE, NULL);
          goto error;
        }
    }
  if (self->text->len)
    {
      log_template_add_macro_elem(self, M_NONE, NULL);
    }
  result = TRUE;
 error:
  *compiled_template = g_list_reverse(self->result);
  self->result = NULL;
  return result;
}

static void
log_template_compiler_init(LogTemplateCompiler *self, LogTemplate *template)
{
  memset(self, 0, sizeof(*self));

  self->template = log_template_ref(template);
  self->cursor = self->template->template;
  self->text = g_string_sized_new(32);
}

static void
log_template_compiler_clear(LogTemplateCompiler *self)
{
  log_template_unref(self->template);
  g_string_free(self->text, TRUE);
}

static void
log_template_reset_compiled(LogTemplate *self)
{
  log_template_elem_free_list(self->compiled_template);
  self->compiled_template = NULL;
}

gboolean
log_template_compile(LogTemplate *self, const gchar *template, GError **error)
{
  LogTemplateCompiler compiler;
  gboolean result;

  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  log_template_reset_compiled(self);
  if (self->template)
    g_free(self->template);
  self->template = g_strdup(template);

  log_template_compiler_init(&compiler, self);
  result = log_template_compiler_compile(&compiler, &self->compiled_template, error);
  log_template_compiler_clear(&compiler);
  return result;
}

void
log_template_set_escape(LogTemplate *self, gboolean enable)
{
  self->escape = enable;
}

gboolean
log_template_set_type_hint(LogTemplate *self, const gchar *type_hint, GError **error)
{
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  return type_hint_parse(type_hint, &self->type_hint, error);
}


void
log_template_append_format_with_context(LogTemplate *self, LogMessage **messages, gint num_messages, const LogTemplateOptions *opts, gint tz, gint32 seq_num, const gchar *context_id, GString *result)
{
  GList *p;
  LogTemplateElem *e;

  if (!opts)
    opts = &self->cfg->template_options;

  for (p = self->compiled_template; p; p = g_list_next(p))
    {
      gint msg_ndx;

      e = (LogTemplateElem *) p->data;
      if (e->text)
        {
          g_string_append_len(result, e->text, e->text_len);
        }

      /* NOTE: msg_ref is 1 larger than the index specified by the user in
       * order to make it distinguishable from the zero value.  Therefore
       * the '>' instead of '>='
       *
       * msg_ref == 0 means that the user didn't specify msg_ref
       * msg_ref >= 1 means that the user supplied the given msg_ref, 1 is equal to @0 */
      if (e->msg_ref > num_messages)
        continue;
      msg_ndx = num_messages - e->msg_ref;

      /* value and macro can't understand a context, assume that no msg_ref means @0 */
      if (e->msg_ref == 0)
        msg_ndx--;

      switch (e->type)
        {
        case LTE_VALUE:
          {
            const gchar *value = NULL;
            gssize value_len = -1;

            value = log_msg_get_value(messages[msg_ndx], e->value_handle, &value_len);
            if (value && value[0])
              result_append(result, value, value_len, self->escape);
            else if (e->default_value)
              result_append(result, e->default_value, -1, self->escape);
            break;
          }
        case LTE_MACRO:
          {
            gint len = result->len;

            if (e->macro)
              {
                log_macro_expand(result, e->macro, self->escape, opts ? opts : &self->cfg->template_options, tz, seq_num, context_id, messages[msg_ndx]);
                if (len == result->len && e->default_value)
                  g_string_append(result, e->default_value);
              }
            break;
          }
        case LTE_FUNC:
          {
            g_static_mutex_lock(&self->arg_lock);
            if (!self->arg_bufs)
              self->arg_bufs = g_ptr_array_sized_new(0);

            if (1)
              {
                LogTemplateInvokeArgs args =
                  {
                    self->arg_bufs,
                    e->msg_ref ? &messages[msg_ndx] : messages,
                    e->msg_ref ? 1 : num_messages,
                    opts,
                    tz,
                    seq_num,
                    context_id
                  };


                /* if a function call is called with an msg_ref, we only
                 * pass that given logmsg to argument resolution, otherwise
                 * we pass the whole set so the arguments can individually
                 * specify which message they want to resolve from
                 */
                if (e->func.ops->eval)
                  e->func.ops->eval(e->func.ops, e->func.state, &args);
                e->func.ops->call(e->func.ops, e->func.state, &args, result);
              }
            g_static_mutex_unlock(&self->arg_lock);
            break;
          }
        }
    }
}

void
log_template_append_format_recursive(LogTemplate *self, const LogTemplateInvokeArgs *args, GString *result)
{
  log_template_append_format_with_context(self,
                                          args->messages, args->num_messages,
                                          args->opts, args->tz, args->seq_num, args->context_id, result);
}

void
log_template_format_with_context(LogTemplate *self, LogMessage **messages, gint num_messages, const LogTemplateOptions *opts, gint tz, gint32 seq_num, const gchar *context_id, GString *result)
{
  g_string_truncate(result, 0);
  log_template_append_format_with_context(self, messages, num_messages, opts, tz, seq_num, context_id, result);
}

void
log_template_append_format(LogTemplate *self, LogMessage *lm, const LogTemplateOptions *opts, gint tz, gint32 seq_num, const gchar *context_id, GString *result)
{
  log_template_append_format_with_context(self, &lm, 1, opts, tz, seq_num, context_id, result);
}

void
log_template_format(LogTemplate *self, LogMessage *lm, const LogTemplateOptions *opts, gint tz, gint32 seq_num, const gchar *context_id, GString *result)
{
  g_string_truncate(result, 0);
  log_template_append_format(self, lm, opts, tz, seq_num, context_id, result);
}

LogTemplate *
log_template_new(GlobalConfig *cfg, gchar *name)
{
  LogTemplate *self = g_new0(LogTemplate, 1);

  self->name = g_strdup(name);
  self->ref_cnt = 1;
  self->cfg = cfg;
  g_static_mutex_init(&self->arg_lock);
  if (cfg_is_config_version_older(cfg, 0x0300))
    {
      static gboolean warn_written = FALSE;

      if (!warn_written)
        {
          msg_warning("WARNING: template: the default value for template-escape has changed to 'no' from " VERSION_3_0 ", please update your configuration file accordingly",
                      NULL);
          warn_written = TRUE;
        }
      self->escape = TRUE;
    }
  return self;
}

static void
log_template_free(LogTemplate *self)
{
  if (self->arg_bufs)
    {
      gint i;

      for (i = 0; i < self->arg_bufs->len; i++)
        g_string_free(g_ptr_array_index(self->arg_bufs, i), TRUE);
      g_ptr_array_free(self->arg_bufs, TRUE);
    }
  log_template_reset_compiled(self);
  g_free(self->name);
  g_free(self->template);
  g_static_mutex_free(&self->arg_lock);
  g_free(self);
}

LogTemplate *
log_template_ref(LogTemplate *s)
{
  if (s)
    {
      g_assert(s->ref_cnt > 0);
      s->ref_cnt++;
    }
  return s;
}

void
log_template_unref(LogTemplate *s)
{
  if (s)
    {
      g_assert(s->ref_cnt > 0);
      if (--s->ref_cnt == 0)
        log_template_free(s);
    }
}

/* NOTE: _init needs to be idempotent when called multiple times w/o invoking _destroy */
void
log_template_options_init(LogTemplateOptions *options, GlobalConfig *cfg)
{
  gint i;

  if (options->initialized)
    return;
  if (options->ts_format == -1)
    options->ts_format = cfg->template_options.ts_format;
  for (i = 0; i < LTZ_MAX; i++)
    {
      if (options->time_zone[i] == NULL)
        options->time_zone[i] = g_strdup(cfg->template_options.time_zone[i]);
      if (options->time_zone_info[i] == NULL)
        options->time_zone_info[i] = time_zone_info_new(options->time_zone[i]);
    }

  if (options->frac_digits == -1)
    options->frac_digits = cfg->template_options.frac_digits;
  if (options->on_error == -1)
    options->on_error = cfg->template_options.on_error;
  options->initialized = TRUE;
}

void
log_template_options_destroy(LogTemplateOptions *options)
{
  gint i;

  for (i = 0; i < LTZ_MAX; i++)
    {
      if (options->time_zone[i])
        g_free(options->time_zone[i]);
      if (options->time_zone_info[i])
        time_zone_info_free(options->time_zone_info[i]);
    }
  options->initialized = FALSE;
}

void
log_template_options_defaults(LogTemplateOptions *options)
{
  memset(options, 0, sizeof(LogTemplateOptions));
  options->frac_digits = -1;
  options->ts_format = -1;
  options->on_error = -1;
}

GQuark
log_template_error_quark()
{
  return g_quark_from_static_string("log-template-error-quark");
}

void
log_template_global_init(void)
{
  gint i;

  /* init the uptime (SYSUPTIME macro) */
  g_get_current_time(&app_uptime);

  macro_hash = g_hash_table_new(g_str_hash, g_str_equal);
  for (i = 0; macros[i].name; i++)
    {
      g_hash_table_insert(macro_hash, macros[i].name,
                          GINT_TO_POINTER(macros[i].id));
    }
  return;
}

void
log_template_global_deinit(void)
{
  g_hash_table_destroy(macro_hash);
  macro_hash = NULL;
}

gboolean
log_template_on_error_parse(const gchar *strictness, gint *out)
{
  const gchar *p = strictness;
  gboolean silently = FALSE;

  if (!strictness)
    {
      *out = ON_ERROR_DROP_MESSAGE;
      return TRUE;
    }

  if (strncmp(strictness, "silently-", strlen("silently-")) == 0)
    {
      silently = TRUE;
      p = strictness + strlen("silently-");
    }

  if (strcmp(p, "drop-message") == 0)
    *out = ON_ERROR_DROP_MESSAGE;
  else if (strcmp(p, "drop-property") == 0)
    *out = ON_ERROR_DROP_PROPERTY;
  else if (strcmp(p, "fallback-to-string") == 0)
    *out = ON_ERROR_FALLBACK_TO_STRING;
  else
    return FALSE;

  if (silently)
    *out |= ON_ERROR_SILENT;

  return TRUE;
}

void
log_template_options_set_on_error(LogTemplateOptions *options, gint on_error)
{
  options->on_error = on_error;
}
