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
  ts->tv_nsec += (glong) (msec * 1000000);
  if (ts->tv_nsec >= 1000000000)
    {
      ts->tv_nsec -= (glong) 1000000000;
      ts->tv_sec++;
    }
  else if (ts->tv_nsec < 0)
    {
      ts->tv_nsec += (glong) 1000000000;
      ts->tv_sec--;
    }
}

void
timespec_add_usec(struct timespec *ts, glong usec)
{
  ts->tv_sec += usec / 1000000;
  usec = usec % 1000000;
  ts->tv_nsec += (glong) (usec * 1000);
  if (ts->tv_nsec >= 1000000000)
    {
      ts->tv_nsec -= (glong) 1000000000;
      ts->tv_sec++;
    }
  else if (ts->tv_nsec < 0)
    {
      ts->tv_nsec += (glong) 1000000000;
      ts->tv_sec--;
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
  return ((t1->tv_sec - t2->tv_sec) * 1000000 + (t1->tv_nsec - t2->tv_nsec) / 1000);
}

glong
timespec_diff_nsec(struct timespec *t1, struct timespec *t2)
{
  return (glong)((t1->tv_sec - t2->tv_sec) * 1000000000) + (t1->tv_nsec - t2->tv_nsec);
}
