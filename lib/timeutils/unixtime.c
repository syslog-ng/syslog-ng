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

/* change timezone, assuming that the original timezone was correct, but we
 * want to change the timezone reference to a different one */
void
unix_time_set_timezone_with_tzinfo(UnixTime *self, TimeZoneInfo *tzinfo)
{
  glong new_gmtoff = time_zone_info_get_offset(tzinfo, self->ut_sec);
  unix_time_set_timezone(self, new_gmtoff);
}

gboolean
unix_time_fix_timezone_assuming_the_time_matches_real_time(UnixTime *self)
{
  long target_gmtoff = _guess_recv_timezone_offset_based_on_time_difference(self);

  unix_time_fix_timezone(self, target_gmtoff);
  return target_gmtoff != -1;
}

void
unix_time_fix_timezone_with_tzinfo(UnixTime *self, TimeZoneInfo *tzinfo)
{
  /*
   * This function fixes the timezone (e.g.  gmtoff) value of the incoming
   * timestamp in case it was incorrectly recognized at its previous
   * parsing and converts it to the correct timezone as specified by tzinfo.
   *
   * The complexity of this function is that daylight saving thresholds are
   * specified in local time (e.g.  whichever sunday in March/October) and
   * not as an UTC timestamp.  If the current UnixTime instance in @self was
   * incorrectly parsed, then its UTC timestamp will be off by a few hours.
   * (more specifically off by the number of hours between the two
   * timezones). We have to take this inaccuracy into account.
   *
   * The basic algorithm is as follows:
   *   1) first, we look up the gmtoff value of the target timezone based on
   *      the ut_sec value in the original timestamp.  This will only be
   *      correct if we don't cross a daylight saving transition hour (of
   *      the target timezone) in any direction.  We adjust the
   *      ut_sec/ut_gmtoff pair based on the result of this lookup, by:
   *
   *            ut_sec += target_gmtoff - source_gmtoff;
   *            ut_gmtoff = target_gmtoff;
   *
   *   2) It might happen that this changed ut_sec value (as pointed out
   *      above) either moves past the DST transition hour or moves
   *      right before it, changing the target gmtoff we've guessed at point
   *      #1 above.  We detect this by doing another lookup of the target
   *      timezone and then adjusting ut_sec/ut_gmtoff again.  This handles
   *      cases properly where we don't fall INTO the transition hour at
   *      this step.
   *
   *   3) If we are _within_ the transition hour, we need a final step, as
   *      in this case the transition between the two timezones is not
   *      linear we either need to lose or add an extra hour.  This one is
   *      detected using a 3rd lookup, as we if we are in the transition
   *      hour, the 2nd step will be incorrect and the final step would
   *      return the same gmtoff as the 1st. There are two cases here:
   *
   *
   *        a) 1st_gmtoff < 2nd_gmtoff: we were in DST but then the 2nd step
   *        moved us before, so let's add an hour to ut_sec so it's again
   *        _after_ the threshold.
   *
   *        b) 1st_gmtoff > 2nd_gmtoff: we were before DST and the 2nd step
   *        moved us into ut_sec wise, but our gmtoff does not reflect that.
   *        Add an hour to ut_gmtoff.
   *
   *      This will then nicely move any second between 02:00am and 03:00am
   *      to be between 03:00am and 04:00am, just as it happens when we
   *      convert these timestamps via mktime().
   *
   */

  /* STEP 1:
   *   use the ut_sec in the incorrectly parsed timestamp to find
   *   an initial gmtoff value in target
   */

  glong fixed_gmtoff = time_zone_info_get_offset(tzinfo, self->ut_sec);
  if (fixed_gmtoff != self->ut_gmtoff)
    {
      /* adjust gmtoff to the possibly inaccurate gmtoff */
      unix_time_fix_timezone(self, fixed_gmtoff);

      /* STEP 2: check if our initial idea was correct */
      glong alt_gmtoff = time_zone_info_get_offset(tzinfo, self->ut_sec);

      if (alt_gmtoff != fixed_gmtoff)
        {
          /* if alt_gmtoff is not equal to fixed_gmtoff then initial idea
           * was wrong, we are crossing the daylight saving change hour.
           * but stamp->ut_sec should be more accurate now */

          unix_time_fix_timezone(self, alt_gmtoff);

          /* STEP 3: check if the final fix was good, e.g. are we within the transition hour */
          if (time_zone_info_get_offset(tzinfo, self->ut_sec) == fixed_gmtoff)
            {
              /* we are within the transition hour, we need to skip 1 hour in the timestamp */
              if (alt_gmtoff > fixed_gmtoff)
                {

                  /* step 2 moved ut_sec right before the transition second,
                   * while ut_gmtoff is already reflecting the changed
                   * offset, move ut_sec forward */

                  self->ut_sec += alt_gmtoff - fixed_gmtoff;
                }
              else
                {
                  /* step 2 changed ut_gmtoff to be an "old" offset. update gmtoff */
                  self->ut_gmtoff += fixed_gmtoff - alt_gmtoff;
                }
            }
        }
    }
}

gboolean
unix_time_eq(const UnixTime *a, const UnixTime *b)
{
  return a->ut_sec == b->ut_sec &&
         a->ut_usec == b->ut_usec &&
         a->ut_gmtoff == b->ut_gmtoff;
}
