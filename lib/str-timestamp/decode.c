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
#include "str-timestamp/decode.h"
#include "str-format.h"

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

gboolean
scan_std_timestamp(const gchar **buf, gint *left, struct tm *tm)
{
  /* YYYY-MM-DD HH:MM:SS */
  if (!scan_int(buf, left, 4, &tm->tm_year) ||
      !scan_expect_char(buf, left, '-') ||
      !scan_int(buf, left, 2, &tm->tm_mon) ||
      !scan_expect_char(buf, left, '-') ||
      !scan_int(buf, left, 2, &tm->tm_mday) ||
      !scan_expect_char(buf, left, ' ') ||
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
