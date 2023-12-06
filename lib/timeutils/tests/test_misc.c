/*
 * Copyright (c) 2002-2019 Balabit
 * Copyright (c) 1998-2019 Balázs Scheidler
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
#include <criterion/criterion.h>
#include "timeutils/misc.h"

#include "apphook.h"

Test(misc, timespec_diff_msec_returns_the_timespec_difference_in_msec)
{
  struct timespec ts1;
  struct timespec ts2;

  ts1 = ((struct timespec)
  {
    .tv_sec = 1, .tv_nsec = MSEC_TO_NSEC(100)
  });
  ts2 = ((struct timespec)
  {
    .tv_sec = 1, .tv_nsec = MSEC_TO_NSEC(200)
  });

  cr_assert(timespec_diff_msec(&ts2, &ts1) == 100);
  cr_assert(timespec_diff_usec(&ts2, &ts1) == MSEC_TO_USEC(100));
  cr_assert(timespec_diff_nsec(&ts2, &ts1) == MSEC_TO_NSEC(100));

  ts1 = ((struct timespec)
  {
    .tv_sec = 0, .tv_nsec = MSEC_TO_NSEC(900)
  });
  ts2 = ((struct timespec)
  {
    .tv_sec = 1, .tv_nsec = MSEC_TO_NSEC(100)
  });

  cr_assert(timespec_diff_msec(&ts2, &ts1) == 200);
  cr_assert(timespec_diff_usec(&ts2, &ts1) == MSEC_TO_USEC(200));
  cr_assert(timespec_diff_nsec(&ts2, &ts1) == MSEC_TO_NSEC(200));

  ts1 = ((struct timespec)
  {
    .tv_sec = 1, .tv_nsec = MSEC_TO_NSEC(100)
  });
  ts2 = ((struct timespec)
  {
    .tv_sec = 0, .tv_nsec = MSEC_TO_NSEC(900)
  });

  cr_assert(timespec_diff_msec(&ts2, &ts1) == -200);
  cr_assert(timespec_diff_usec(&ts2, &ts1) == MSEC_TO_USEC(-200));
  cr_assert(timespec_diff_nsec(&ts2, &ts1) == MSEC_TO_NSEC(-200));
}

Test(misc, timespec_add_msec_adds_msec_to_timespec)
{
  struct timespec ts1;
  struct timespec ts2;

  ts1 = ((struct timespec)
  {
    .tv_sec = 1, .tv_nsec = MSEC_TO_NSEC(100)
  });
  ts2 = ts1;

  timespec_add_msec(&ts2, 100);

  cr_assert(timespec_diff_msec(&ts2, &ts1) == 100);
  cr_assert(timespec_diff_usec(&ts2, &ts1) == MSEC_TO_USEC(100));
  cr_assert(timespec_diff_nsec(&ts2, &ts1) == MSEC_TO_NSEC(100));

  ts1 = ((struct timespec)
  {
    .tv_sec = 1, .tv_nsec = MSEC_TO_NSEC(100)
  });
  ts2 = ts1;

  timespec_add_msec(&ts2, 900);

  cr_assert(timespec_diff_msec(&ts2, &ts1) == 900);
  cr_assert(timespec_diff_usec(&ts2, &ts1) == MSEC_TO_USEC(900));
  cr_assert(timespec_diff_nsec(&ts2, &ts1) == MSEC_TO_NSEC(900));
  cr_assert(ts2.tv_nsec == 0, "%ld", ts2.tv_nsec);
  cr_assert(ts2.tv_sec == 2, "%ld", ts2.tv_sec);

  timespec_add_msec(&ts2, -1);

  cr_assert(timespec_diff_msec(&ts2, &ts1) == 899);
  cr_assert(timespec_diff_usec(&ts2, &ts1) == MSEC_TO_USEC(899));
  cr_assert(timespec_diff_nsec(&ts2, &ts1) == MSEC_TO_NSEC(899));
  cr_assert(ts2.tv_nsec == MSEC_TO_NSEC(999), "%ld", ts2.tv_nsec);
  cr_assert(ts2.tv_sec == 1, "%ld", ts2.tv_sec);

  timespec_add_msec(&ts2, 2);

  cr_assert(timespec_diff_msec(&ts2, &ts1) == 901);
  cr_assert(timespec_diff_usec(&ts2, &ts1) == MSEC_TO_USEC(901));
  cr_assert(timespec_diff_nsec(&ts2, &ts1) == MSEC_TO_NSEC(901));
  cr_assert(ts2.tv_nsec == MSEC_TO_NSEC(1), "%ld", ts2.tv_nsec);
  cr_assert(ts2.tv_sec == 2, "%ld", ts2.tv_sec);
}

Test(misc, timespec_add_usec_adds_usec_to_timespec)
{
  struct timespec ts1;
  struct timespec ts2;

  ts1 = ((struct timespec)
  {
    .tv_sec = 1, .tv_nsec = USEC_TO_NSEC(100000)
  });
  ts2 = ts1;

  timespec_add_usec(&ts2, MSEC_TO_USEC(100));

  cr_assert(timespec_diff_msec(&ts2, &ts1) == 100);
  cr_assert(timespec_diff_usec(&ts2, &ts1) == MSEC_TO_USEC(100));
  cr_assert(timespec_diff_nsec(&ts2, &ts1) == MSEC_TO_NSEC(100));

  ts1 = ((struct timespec)
  {
    .tv_sec = 1, .tv_nsec = MSEC_TO_NSEC(100)
  });
  ts2 = ts1;

  timespec_add_usec(&ts2, MSEC_TO_USEC(900));

  cr_assert(timespec_diff_msec(&ts2, &ts1) == 900);
  cr_assert(timespec_diff_usec(&ts2, &ts1) == MSEC_TO_USEC(900));
  cr_assert(timespec_diff_nsec(&ts2, &ts1) == MSEC_TO_NSEC(900));
  cr_assert(ts2.tv_nsec == 0, "%ld", ts2.tv_nsec);
  cr_assert(ts2.tv_sec == 2, "%ld", ts2.tv_sec);

  timespec_add_usec(&ts2, MSEC_TO_USEC(-1));

  cr_assert(timespec_diff_msec(&ts2, &ts1) == 899);
  cr_assert(timespec_diff_usec(&ts2, &ts1) == MSEC_TO_USEC(899));
  cr_assert(timespec_diff_nsec(&ts2, &ts1) == MSEC_TO_NSEC(899));
  cr_assert(ts2.tv_nsec == MSEC_TO_NSEC(999), "%ld", ts2.tv_nsec);
  cr_assert(ts2.tv_sec == 1, "%ld", ts2.tv_sec);

  timespec_add_usec(&ts2, MSEC_TO_USEC(2));

  cr_assert(timespec_diff_msec(&ts2, &ts1) == 901);
  cr_assert(timespec_diff_usec(&ts2, &ts1) == MSEC_TO_USEC(901));
  cr_assert(timespec_diff_nsec(&ts2, &ts1) == MSEC_TO_NSEC(901));
  cr_assert(ts2.tv_nsec == MSEC_TO_NSEC(1), "%ld", ts2.tv_nsec);
  cr_assert(ts2.tv_sec == 2, "%ld", ts2.tv_sec);
}


static void
setup(void)
{
  setenv("TZ", "CET", TRUE);
  tzset();
  app_startup();
}

static void
teardown(void)
{
  app_shutdown();
}

TestSuite(unixtime, .init = setup, .fini = teardown);
