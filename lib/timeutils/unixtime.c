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
#include "timeutils/unixtime.h"
#include "timeutils/wallclocktime.h"
#include "timeutils/cache.h"
#include "timeutils/names.h"
#include "timeutils/misc.h"

#include <stdlib.h>

void
unix_time_unset(UnixTime *self)
{
  UnixTime val = UNIX_TIME_INIT;
  *self = val;
}

void
unix_time_set_now(UnixTime *self)
{
  GTimeVal tv;

  cached_g_current_time(&tv);
  self->ut_sec = tv.tv_sec;
  self->ut_usec = tv.tv_usec;
  self->ut_gmtoff = get_local_timezone_ofs(self->ut_sec);
}

static glong
_div_round(glong n, glong d)
{
  if ((n < 0) ^ (d < 0))
    return ((n - d/2)/d);
  else
    return ((n + d/2)/d);
}

static gboolean
_binary_search(glong *haystack, gint haystack_size, glong needle)
{
  gint l = 0;
  gint h = haystack_size;
  gint m;

  while (l <= h)
    {
      m = (l + h) / 2;
      if (haystack[m] == needle)
        return TRUE;
      else if (haystack[m] > needle)
        h = m - 1;
      else if (haystack[m] < needle)
        l = m + 1;
    }
  return FALSE;
}

static gboolean
_is_gmtoff_valid(long gmtoff)
{
  /* this list was produced by this shell script:
   *
   * $ find /usr/share/zoneinfo/ -type f | xargs zdump -v > x.raw
   * $ grep '20[0-9][0-9].*gmtoff' x.raw  | awk '{ i=int(substr($16, 8)); if (i % 3600 != 0) print i;  }' | sort -n | uniq
   *
   * all of these are either 30 or 45 minutes into the hour.  Earlier we
   * also had timezones like +1040, e.g.  40 minutes into the hour, but
   * fortunately that has changed.
   *
   * Also, this is only consulted wrt real-time timestamps, so we don't need
   * historical offsets.  How often this needs to change?  Well, people do
   * all kinds of crazy stuff with timezones, but I hope this will work for
   * the foreseeable future.  If there's a bug, you can always use the
   * command above to update this list, provided you have tzdata installed.
   *
   */
  long valid_non_even_hour_gmtofs[] =
  {
    -34200,
    -16200,
    -12600,
    -9000,
    12600,
    16200,
    19800,
    20700,
    23400,
    30600,
    31500,
    34200,
    35100,
    37800,
    41400,
    45900,
    49500,
  };

  /* too far off */
  if (gmtoff < -12*3600 || gmtoff > 14 * 3600)
    return FALSE;

  /* even hours accepted */
  if ((gmtoff % 3600) == 0)
    return TRUE;

  /* non-even hours are checked using the valid arrays above */
  if (_binary_search(valid_non_even_hour_gmtofs, G_N_ELEMENTS(valid_non_even_hour_gmtofs), gmtoff))
    return TRUE;
  return FALSE;
}

static glong
_guess_recv_timezone_offset_based_on_time_difference(UnixTime *self)
{
  GTimeVal now;

  cached_g_current_time(&now);

  glong diff_in_sec = now.tv_sec - self->ut_sec;

  /* More than 24 hours means that this is definitely not a real time
   * timestamp, so we shouldn't try to auto detect timezone based on the
   * current time.
   */
  if (labs(diff_in_sec) >= 24 * 3600)
    return -1;

  glong diff_rounded_to_quarters = _div_round(diff_in_sec, 3600/4) * 3600/4;

  if (labs(now.tv_sec - self->ut_sec - diff_rounded_to_quarters) <= 30)
    {
      /* roughly quarter of an hour difference, with 30s accuracy */
      glong result = self->ut_gmtoff - diff_rounded_to_quarters;

      if (!_is_gmtoff_valid(result))
        return -1;
      return result;
    }
  return -1;
}

/* change timezone, assuming that the original timezone value has
 * incorrectly been used to parse the timestamp */
void
unix_time_fix_timezone(UnixTime *self, gint new_gmtoff)
{
  long implied_gmtoff = self->ut_gmtoff != -1 ? self->ut_gmtoff : 0;

  if (new_gmtoff != -1)
    {
      self->ut_sec -= (new_gmtoff - implied_gmtoff);
      self->ut_gmtoff = new_gmtoff;
    }
}

/* change timezone, assuming that the original timezone was correct, but we
 * want to change the timezone reference to a different one */
void
unix_time_set_timezone(UnixTime *self, gint new_gmtoff)
{
  if (new_gmtoff != -1)
    self->ut_gmtoff = new_gmtoff;
}

gboolean
unix_time_fix_timezone_assuming_the_time_matches_real_time(UnixTime *self)
{
  long target_gmtoff = _guess_recv_timezone_offset_based_on_time_difference(self);

  unix_time_fix_timezone(self, target_gmtoff);
  return target_gmtoff != -1;
}

gboolean
unix_time_eq(const UnixTime *a, const UnixTime *b)
{
  return a->ut_sec == b->ut_sec &&
         a->ut_usec == b->ut_usec &&
         a->ut_gmtoff == b->ut_gmtoff;
}
