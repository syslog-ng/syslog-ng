/*
 * Copyright (c) 2002-2018 Balabit
 * Copyright (c) 1998-2018 Balázs Scheidler
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

#include "syslog-timestamp.h"
#include "timeutils.h"
#include "str-format.h"

#include <ctype.h>

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
log_msg_parse_rfc3164_date_unnormalized(LogStamp *stamp, const guchar **data, gint *length, struct tm *tm)
{
  GTimeVal now;
  const guchar *src = *data;
  gint left = *length;

  cached_g_current_time(&now);

  /* If the next chars look like a date, then read them as a date. */
  if (__is_iso_stamp((const gchar *)src, left))
    {
      if (!__parse_iso_stamp(&now, stamp, tm, &src, &left))
        return FALSE;
    }
  else
    {
      glong usec = 0;
      if (!__parse_bsd_timestamp(&src, &left, &now, tm, &usec))
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
  *data = src;
  *length = left;
  return TRUE;
}

gboolean
log_msg_parse_rfc5424_date_unnormalized(LogStamp *stamp, const guchar **data, gint *length, struct tm *tm)
{
  GTimeVal now;
  const guchar *src = *data;
  gint left = *length;

  cached_g_current_time(&now);

  if (!__parse_iso_stamp(&now, stamp, tm, &src, &left))
    return FALSE;

  *data = src;
  *length = left;
  return TRUE;
}
