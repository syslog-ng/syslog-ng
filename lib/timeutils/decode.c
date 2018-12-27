/*
 * Copyright (c) 2002-2018 Balabit
 * Copyright (c) 1998-2018 Balázs Scheidler
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
#include "timeutils/decode.h"
#include "str-format.h"
#include "timeutils.h"

#include <ctype.h>
#include <string.h>

gboolean
scan_day_abbrev(const gchar **buf, gint *left, gint *wday)
{
  *wday = -1;

  if (*left < 3)
    return FALSE;

  switch (**buf)
    {
    case 'S':
      if (strncasecmp(*buf, "Sun", 3) == 0)
        *wday = 0;
      else if (strncasecmp(*buf, "Sat", 3) == 0)
        *wday = 6;
      break;
    case 'M':
      if (strncasecmp(*buf, "Mon", 3) == 0)
        *wday = 1;
      break;
    case 'T':
      if (strncasecmp(*buf, "Tue", 3) == 0)
        *wday = 2;
      else if (strncasecmp(*buf, "Thu", 3) == 0)
        *wday = 4;
      break;
    case 'W':
      if (strncasecmp(*buf, "Wed", 3) == 0)
        *wday = 3;
      break;
    case 'F':
      if (strncasecmp(*buf, "Fri", 3) == 0)
        *wday = 5;
      break;
    default:
      return FALSE;
    }

  (*buf) += 3;
  (*left) -= 3;
  return TRUE;
}

gboolean
scan_month_abbrev(const gchar **buf, gint *left, gint *mon)
{
  *mon = -1;

  if (*left < 3)
    return FALSE;

  switch (**buf)
    {
    case 'J':
      if (strncasecmp(*buf, "Jan", 3) == 0)
        *mon = 0;
      else if (strncasecmp(*buf, "Jun", 3) == 0)
        *mon = 5;
      else if (strncasecmp(*buf, "Jul", 3) == 0)
        *mon = 6;
      break;
    case 'F':
      if (strncasecmp(*buf, "Feb", 3) == 0)
        *mon = 1;
      break;
    case 'M':
      if (strncasecmp(*buf, "Mar", 3) == 0)
        *mon = 2;
      else if (strncasecmp(*buf, "May", 3) == 0)
        *mon = 4;
      break;
    case 'A':
      if (strncasecmp(*buf, "Apr", 3) == 0)
        *mon = 3;
      else if (strncasecmp(*buf, "Aug", 3) == 0)
        *mon = 7;
      break;
    case 'S':
      if (strncasecmp(*buf, "Sep", 3) == 0)
        *mon = 8;
      break;
    case 'O':
      if (strncasecmp(*buf, "Oct", 3) == 0)
        *mon = 9;
      break;
    case 'N':
      if (strncasecmp(*buf, "Nov", 3) == 0)
        *mon = 10;
      break;
    case 'D':
      if (strncasecmp(*buf, "Dec", 3) == 0)
        *mon = 11;
      break;
    default:
      return FALSE;
    }

  (*buf) += 3;
  (*left) -= 3;
  return TRUE;
}


/* this function parses the date/time portion of an ISODATE */
gboolean
scan_iso_timestamp(const gchar **buf, gint *left, struct tm *tm)
{
  /* YYYY-MM-DDTHH:MM:SS */
  if (!scan_int(buf, left, 4, &tm->tm_year) ||
      !scan_expect_char(buf, left, '-') ||
      !scan_int(buf, left, 2, &tm->tm_mon) ||
      !scan_expect_char(buf, left, '-') ||
      !scan_int(buf, left, 2, &tm->tm_mday) ||
      !scan_expect_char(buf, left, 'T') ||
      !scan_int(buf, left, 2, &tm->tm_hour) ||
      !scan_expect_char(buf, left, ':') ||
      !scan_int(buf, left, 2, &tm->tm_min) ||
      !scan_expect_char(buf, left, ':') ||
      !scan_int(buf, left, 2, &tm->tm_sec))
    return FALSE;
  tm->tm_year -= 1900;
  tm->tm_mon -= 1;
  return TRUE;
}

gboolean
scan_pix_timestamp(const gchar **buf, gint *left, struct tm *tm)
{
  /* PIX/ASA timestamp, expected format: MMM DD YYYY HH:MM:SS */
  if (!scan_month_abbrev(buf, left, &tm->tm_mon) ||
      !scan_expect_char(buf, left, ' ') ||
      !scan_int(buf, left, 2, &tm->tm_mday) ||
      !scan_expect_char(buf, left, ' ') ||
      !scan_int(buf, left, 4, &tm->tm_year) ||
      !scan_expect_char(buf, left, ' ') ||
      !scan_int(buf, left, 2, &tm->tm_hour) ||
      !scan_expect_char(buf, left, ':') ||
      !scan_int(buf, left, 2, &tm->tm_min) ||
      !scan_expect_char(buf, left, ':') ||
      !scan_int(buf, left, 2, &tm->tm_sec))
    return FALSE;
  tm->tm_year -= 1900;
  return TRUE;
}

gboolean
scan_linksys_timestamp(const gchar **buf, gint *left, struct tm *tm)
{
  /* LinkSys timestamp, expected format: MMM DD HH:MM:SS YYYY */

  if (!scan_month_abbrev(buf, left, &tm->tm_mon) ||
      !scan_expect_char(buf, left, ' ') ||
      !scan_int(buf, left, 2, &tm->tm_mday) ||
      !scan_expect_char(buf, left, ' ') ||
      !scan_int(buf, left, 2, &tm->tm_hour) ||
      !scan_expect_char(buf, left, ':') ||
      !scan_int(buf, left, 2, &tm->tm_min) ||
      !scan_expect_char(buf, left, ':') ||
      !scan_int(buf, left, 2, &tm->tm_sec) ||
      !scan_expect_char(buf, left, ' ') ||
      !scan_int(buf, left, 4, &tm->tm_year))
    return FALSE;
  tm->tm_year -= 1900;
  return TRUE;
}

gboolean
scan_bsd_timestamp(const gchar **buf, gint *left, struct tm *tm)
{
  /* RFC 3164 timestamp, expected format: MMM DD HH:MM:SS ... */
  if (!scan_month_abbrev(buf, left, &tm->tm_mon) ||
      !scan_expect_char(buf, left, ' ') ||
      !scan_int(buf, left, 2, &tm->tm_mday) ||
      !scan_expect_char(buf, left, ' ') ||
      !scan_int(buf, left, 2, &tm->tm_hour) ||
      !scan_expect_char(buf, left, ':') ||
      !scan_int(buf, left, 2, &tm->tm_min) ||
      !scan_expect_char(buf, left, ':') ||
      !scan_int(buf, left, 2, &tm->tm_sec))
    return FALSE;
  return TRUE;
}


/*******************************************************************************
 * Timezone conversions
 *******************************************************************************/

static inline void
__determine_recv_timezone_offset(LogStamp *const timestamp, glong const recv_timezone_ofs)
{
  if (!log_stamp_is_timezone_set(timestamp))
    timestamp->zone_offset = recv_timezone_ofs;

  if (!log_stamp_is_timezone_set(timestamp))
    timestamp->zone_offset = get_local_timezone_ofs(timestamp->tv_sec);
}

static void
__fixup_hour_in_struct_tm_within_transition_periods(LogStamp *stamp, struct tm *tm, glong recv_timezone_ofs)
{
  /* save the tm_hour value as received from the client */
  gint unnormalized_hour = tm->tm_hour;

  /* NOTE: mktime() returns the time assuming that the timestamp we
   * received was in local time. */

  /* tell cached_mktime() that we have no clue whether Daylight Saving is enabled or not */
  tm->tm_isdst = -1;
  stamp->tv_sec = cached_mktime(tm);

  /* We need to determine the timezone we want to assume the message was
   * received from.  This depends on the recv-time-zone() setting and the
   * the tv_sec value as converted by mktime() above. */
  __determine_recv_timezone_offset(stamp, recv_timezone_ofs);

  /* save the tm_hour as adjusted by mktime() */
  gint normalized_hour = tm->tm_hour;

  /* fix up the tv_sec value by transposing it into the target timezone:
   *
   * First we add the local time zone offset then subtract the target time
   * zone offset.  This is not trivial however, as we have to determine
   * exactly what the local timezone offset is at the current second, as
   * used by mktime(). It is composed of these values:
   *
   *  1) get_local_timezone_ofs()
   *  2) then in transition periods, mktime() will change tm->tm_hour
   *     according to its understanding (e.g.  sprint time, 02:01 is changed
   *     to 03:01), which is an additional factor that needs to be taken care
   *     of.  This is the (normalized_hour - unnormalized_hour) part below
   *
   */
  stamp->tv_sec = stamp->tv_sec
                  /* these two components are the zone offset as used by mktime() */
                  + get_local_timezone_ofs(stamp->tv_sec)
                  - (normalized_hour - unnormalized_hour) * 3600
                  /* this is the zone offset value we want to be */
                  - stamp->zone_offset;
}

/*******************************************************************************
 * Parse ISO timestamp
 *******************************************************************************/

static guint32
__parse_usec(const guchar **data, gint *length)
{
  guint32 usec = 0;
  const guchar *src = *data;
  if (*length > 0 && *src == '.')
    {
      gulong frac = 0;
      gint div = 1;
      /* process second fractions */

      src++;
      (*length)--;
      while (*length > 0 && div < 10e5 && isdigit(*src))
        {
          frac = 10 * frac + (*src) - '0';
          div = div * 10;
          src++;
          (*length)--;
        }
      while (isdigit(*src))
        {
          src++;
          (*length)--;
        }
      usec = frac * (1000000 / div);
    }
  *data = src;
  return usec;
}

static gboolean
__has_iso_timezone(const guchar *src, gint length)
{
  return (length >= 5) &&
         (*src == '+' || *src == '-') &&
         isdigit(*(src+1)) &&
         isdigit(*(src+2)) &&
         *(src+3) == ':' &&
         isdigit(*(src+4)) &&
         isdigit(*(src+5)) &&
         !isdigit(*(src+6));
}

static guint32
__parse_iso_timezone(const guchar **data, gint *length)
{
  gint hours, mins;
  const guchar *src = *data;
  guint32 tz = 0;
  /* timezone offset */
  gint sign = *src == '-' ? -1 : 1;

  hours = (*(src + 1) - '0') * 10 + *(src + 2) - '0';
  mins = (*(src + 4) - '0') * 10 + *(src + 5) - '0';
  tz = sign * (hours * 3600 + mins * 60);
  src += 6;
  (*length) -= 6;
  *data = src;
  return tz;
}

static gboolean
__is_iso_stamp(const gchar *stamp, gint length)
{
  return (length >= 19
          && stamp[4] == '-'
          && stamp[7] == '-'
          && stamp[10] == 'T'
          && stamp[13] == ':'
          && stamp[16] == ':'
         );
}

static gboolean
__parse_iso_stamp(const GTimeVal *now, LogStamp *stamp, struct tm *tm, const guchar **data, gint *length)
{
  /* RFC3339 timestamp, expected format: YYYY-MM-DDTHH:MM:SS[.frac]<+/->ZZ:ZZ */
  time_t now_tv_sec = (time_t) now->tv_sec;
  const guchar *src = *data;

  stamp->tv_usec = 0;

  /* NOTE: we initialize various unportable fields in tm using a
   * localtime call, as the value of tm_gmtoff does matter but it does
   * not exist on all platforms and 0 initializing it causes trouble on
   * time-zone barriers */

  cached_localtime(&now_tv_sec, tm);
  if (!scan_iso_timestamp((const gchar **) &src, length, tm))
    {
      return FALSE;
    }

  stamp->tv_usec = __parse_usec(&src, length);

  if (*length > 0 && *src == 'Z')
    {
      /* Z is special, it means UTC */
      stamp->zone_offset = 0;
      src++;
      (*length)--;
    }
  else if (__has_iso_timezone(src, *length))
    {
      stamp->zone_offset = __parse_iso_timezone(&src, length);
    }
  else
    {
      stamp->zone_offset = -1;
    }

  *data = src;
  return TRUE;
}

/*******************************************************************************
 * Parse BSD timestamp
 *******************************************************************************/

static gboolean
__is_bsd_rfc_3164(const guchar *src, guint32 left)
{
  return left >= 15 && src[3] == ' ' && src[6] == ' ' && src[9] == ':' && src[12] == ':';
}

static gboolean
__is_bsd_linksys(const guchar *src, guint32 left)
{
  return (left >= 21
          && __is_bsd_rfc_3164(src, left)
          && src[15] == ' '
          && isdigit(src[16])
          && isdigit(src[17])
          && isdigit(src[18])
          && isdigit(src[19])
          && isspace(src[20])
         );
}

static gboolean
__is_bsd_pix_or_asa(const guchar *src, guint32 left)
{
  return (left >= 21
          && src[3] == ' '
          && src[6] == ' '
          && src[11] == ' '
          && src[14] == ':'
          && src[17] == ':'
          && (src[20] == ':' || src[20] == ' ')
          && isdigit(src[7])
          && isdigit(src[8])
          && isdigit(src[9])
          && isdigit(src[10])
         );
}

static gboolean
__parse_bsd_timestamp(const guchar **data, gint *length, const GTimeVal *now, struct tm *tm, glong *usec)
{
  gint left = *length;
  const guchar *src = *data;
  time_t now_tv_sec = (time_t) now->tv_sec;
  struct tm local_time;
  cached_localtime(&now_tv_sec, tm);
  cached_localtime(&now_tv_sec, &local_time);

  if (__is_bsd_pix_or_asa(src, left))
    {
      if (!scan_pix_timestamp((const gchar **) &src, &left, tm))
        return FALSE;

      if (*src == ':')
        {
          src++;
          left--;
        }
    }
  else if (__is_bsd_linksys(src, left))
    {
      if (!scan_linksys_timestamp((const gchar **) &src, &left, tm))
        return FALSE;
    }
  else if (__is_bsd_rfc_3164(src, left))
    {
      if (!scan_bsd_timestamp((const gchar **) &src, &left, tm))
        return FALSE;

      *usec = __parse_usec(&src, &left);

      tm->tm_year = determine_year_for_month(tm->tm_mon, &local_time);
    }
  else
    {
      return FALSE;
    }
  *data = src;
  *length = left;
  return TRUE;
}

gboolean
scan_rfc3164_timestamp(const guchar **data, gint *length,
                       LogStamp *stamp,
                       gboolean ignore_result, glong recv_timezone_ofs)
{
  GTimeVal now;
  const guchar *src = *data;
  gint left = *length;
  struct tm tm;

  cached_g_current_time(&now);

  /* If the next chars look like a date, then read them as a date. */
  if (__is_iso_stamp((const gchar *)src, left))
    {
      if (!__parse_iso_stamp(&now, stamp, &tm, &src, &left))
        return FALSE;
    }
  else
    {
      glong usec = 0;
      if (!__parse_bsd_timestamp(&src, &left, &now, &tm, &usec))
        return FALSE;

      stamp->tv_usec = usec;
    }

  /* we might have a closing colon at the end of the timestamp, "Cisco" I am
   * looking at you, skip that as well, so we can reliably detect IPv6
   * addresses as hostnames, which would be using ":" as well. */

  if (*src == ':')
    {
      ++src;
      --left;
    }

  if (!ignore_result)
    __fixup_hour_in_struct_tm_within_transition_periods(stamp, &tm, recv_timezone_ofs);

  *data = src;
  *length = left;
  return TRUE;
}

gboolean
scan_rfc5424_timestamp(const guchar **data, gint *length,
                       LogStamp *stamp,
                       gboolean ignore_result, glong recv_timezone_ofs)
{
  GTimeVal now;
  const guchar *src = *data;
  gint left = *length;
  struct tm tm;

  cached_g_current_time(&now);

  if (G_UNLIKELY(left >= 1 && src[0] == '-'))
    {
      stamp->tv_sec = now.tv_sec;
      stamp->tv_usec = now.tv_usec;
      stamp->zone_offset = get_local_timezone_ofs(now.tv_sec);
      src++;
      left--;
    }
  else if (__parse_iso_stamp(&now, stamp, &tm, &src, &left))
    {
      if (!ignore_result)
        __fixup_hour_in_struct_tm_within_transition_periods(stamp, &tm, recv_timezone_ofs);
    }
  else
    return FALSE;

  *data = src;
  *length = left;
  return TRUE;
}
