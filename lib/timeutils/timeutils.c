/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#include "timeutils/timeutils.h"
#include "timeutils/cache.h"
#include "messages.h"

#include <string.h>

/**
 * get_local_timezone_ofs:
 * @when: time in UTC
 *
 * Return the zone offset (measured in seconds) of @when expressed in local
 * time. The function also takes care about daylight saving.
 **/
long
get_local_timezone_ofs(time_t when)
{
#ifdef SYSLOG_NG_HAVE_STRUCT_TM_TM_GMTOFF
  struct tm ltm;

  cached_localtime(&when, &ltm);
  return ltm.tm_gmtoff;

#else

  struct tm gtm;
  struct tm ltm;
  long tzoff;

  cached_localtime(&when, &ltm);
  cached_gmtime(&when, &gtm);

  tzoff = (ltm.tm_hour - gtm.tm_hour) * 3600;
  tzoff += (ltm.tm_min - gtm.tm_min) * 60;
  tzoff += ltm.tm_sec - gtm.tm_sec;

  if (tzoff > 0 && (ltm.tm_year < gtm.tm_year || ltm.tm_mon < gtm.tm_mon || ltm.tm_mday < gtm.tm_mday))
    tzoff -= 86400;
  else if (tzoff < 0 && (ltm.tm_year > gtm.tm_year || ltm.tm_mon > gtm.tm_mon || ltm.tm_mday > gtm.tm_mday))
    tzoff += 86400;

  return tzoff;
#endif /* SYSLOG_NG_HAVE_STRUCT_TM_TM_GMTOFF */
}

int
format_zone_info(gchar *buf, size_t buflen, glong gmtoff)
{
  return g_snprintf(buf, buflen, "%c%02ld:%02ld",
                    gmtoff < 0 ? '-' : '+',
                    (gmtoff < 0 ? -gmtoff : gmtoff) / 3600,
                    ((gmtoff < 0 ? -gmtoff : gmtoff) % 3600) / 60);
}

/**
 * check_nanosleep:
 *
 * Check if nanosleep() is accurate enough for sub-millisecond sleeping. If
 * it is not good enough, we're skipping the minor sleeps in LogReader to
 * balance the cost of returning to the main loop (e.g.  we're always going
 * to return to the main loop, instead of trying to wait for the writer).
 **/
gboolean
check_nanosleep(void)
{
  struct timespec start, stop, sleep_amount;
  glong diff;
  gint attempts;

  for (attempts = 0; attempts < 3; attempts++)
    {
      clock_gettime(CLOCK_MONOTONIC, &start);
      sleep_amount.tv_sec = 0;
      /* 0.1 msec */
      sleep_amount.tv_nsec = 1e5;

      while (nanosleep(&sleep_amount, &sleep_amount) < 0)
        ;

      clock_gettime(CLOCK_MONOTONIC, &stop);
      diff = timespec_diff_nsec(&stop, &start);
      if (diff < 5e5)
        return TRUE;
    }
  return FALSE;
}

/**
 * g_time_val_diff:
 * @t1: time value t1
 * @t2: time value t2
 *
 * Calculates the time difference between t1 and t2 in microseconds.
 * The result is positive if t1 is later than t2.
 *
 * Returns:
 * Time difference in microseconds
 */
glong
g_time_val_diff(GTimeVal *t1, GTimeVal *t2)
{
  g_assert(t1);
  g_assert(t2);
  return (t1->tv_sec - t2->tv_sec) * G_USEC_PER_SEC + (t1->tv_usec - t2->tv_usec);
}

void
timespec_add_msec(struct timespec *ts, glong msec)
{
  ts->tv_sec += msec / 1000;
  msec = msec % 1000;
  ts->tv_nsec += (glong) (msec * 1e6);
  if (ts->tv_nsec > 1e9)
    {
      ts->tv_nsec -= (glong) 1e9;
      ts->tv_sec++;
    }
}

glong
timespec_diff_msec(const struct timespec *t1, const struct timespec *t2)
{
  return (t1->tv_sec - t2->tv_sec) * 1e3 + (t1->tv_nsec - t2->tv_nsec) / 1e6;
}

glong
timespec_diff_nsec(struct timespec *t1, struct timespec *t2)
{
  return (glong)((t1->tv_sec - t2->tv_sec) * 1e9) + (t1->tv_nsec - t2->tv_nsec);
}

/* Determine (guess) the year for the month.
 *
 * It can be used for BSD logs, where year is missing.
 */
gint
determine_year_for_month(gint month, const struct tm *now)
{
  if (month == 11 && now->tm_mon == 0)
    return now->tm_year - 1;
  else if (month == 0 && now->tm_mon == 11)
    return now->tm_year + 1;
  else
    return now->tm_year;
}
