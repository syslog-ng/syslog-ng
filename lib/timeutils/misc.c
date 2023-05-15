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

#include "compat/time.h"
#include "timeutils/misc.h"
#include "timeutils/cache.h"
#include "messages.h"

#include <errno.h>
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
        {
          if (errno != EINTR)
            return FALSE;
        }

      clock_gettime(CLOCK_MONOTONIC, &stop);
      diff = timespec_diff_nsec(&stop, &start);
      if (diff < 5e5)
        return TRUE;
    }
  return FALSE;
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

void
timespec_add_usec(struct timespec *ts, glong usec)
{
  ts->tv_sec += usec / 1000000;
  usec = usec % 1000000;
  ts->tv_nsec += (glong) (usec * 1000);
  if (ts->tv_nsec > 1e9)
    {
      ts->tv_nsec -= (glong) 1e9;
      ts->tv_sec++;
    }
}

glong
timespec_diff_msec(const struct timespec *t1, const struct timespec *t2)
{
  return ((t1->tv_sec - t2->tv_sec) * 1000 + (t1->tv_nsec - t2->tv_nsec) / 1000000);
}

glong
timespec_diff_usec(const struct timespec *t1, const struct timespec *t2)
{
  return ((t1->tv_sec - t2->tv_sec) * 1e6 + (t1->tv_nsec - t2->tv_nsec) / 1000);
}

glong
timespec_diff_nsec(struct timespec *t1, struct timespec *t2)
{
  return (glong)((t1->tv_sec - t2->tv_sec) * 1e9) + (t1->tv_nsec - t2->tv_nsec);
}
