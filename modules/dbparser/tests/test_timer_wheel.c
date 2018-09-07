/*
 * Copyright (c) 2010, 2018 Balabit
 * Copyright (c) 2010, 2015 Balázs Scheidler <balazs.scheidler@balabit.com>
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

#include "timerwheel.h"


#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include <criterion/criterion.h>

#define NUM_TIMERS 10000

gint num_callbacks;
guint64 prev_now;

void
timer_callback(TimerWheel *self, guint64 now, gpointer user_data)
{
  guint64 expires = *(guint64 *) user_data;

  cr_assert_eq(now, expires, "Expected time is not matching current time in callback, "
               "now=%" G_GUINT64_FORMAT ", expires=%" G_GUINT64_FORMAT "\n",
               now, expires);

  cr_expect_leq(prev_now, now, "Callback current time is not monotonically increasing, "
                "prev_now=%" G_GUINT64_FORMAT ", now=%" G_GUINT64_FORMAT "\n",
                prev_now, now);

  prev_now = now;
  num_callbacks++;
}

#define ASSOC_DATA_STRING "timerwheel associated data, check whether it's freed"

static void
_test_assoc_data(TimerWheel *wheel)
{
  timer_wheel_set_associated_data(wheel, g_strdup(ASSOC_DATA_STRING), (GDestroyNotify) g_free);
  cr_expect_str_eq(timer_wheel_get_associated_data(wheel), ASSOC_DATA_STRING,
                   "Associated data mismatch, found=%s, expected=%s",
                   (gchar *) timer_wheel_get_associated_data(wheel), ASSOC_DATA_STRING);
}

void
test_wheel(gint seed)
{
  TimerWheel *wheel;
  gint i;
  guint64 latest = 0;
  gint expected_callbacks;

  prev_now = 0;
  num_callbacks = 0;
  expected_callbacks = 0;
  srand(seed);
  wheel = timer_wheel_new();
  _test_assoc_data(wheel);
  timer_wheel_set_time(wheel, 1);
  for (i = 0; i < NUM_TIMERS; i++)
    {
      guint64 expires;
      TWEntry *timer1, *timer2, *timer3;
      gint r;

      expires = rand() & ((1 << 24) - 1);
      if (expires <= 1)
        expires = 1;

      if (expires > latest)
        latest = expires;

      timer1 = timer_wheel_add_timer(wheel, expires - 1, timer_callback,
                                     g_memdup(&expires, sizeof(expires)), (GDestroyNotify) g_free);
      timer2 = timer_wheel_add_timer(wheel, expires - 1, timer_callback,
                                     g_memdup(&expires, sizeof(expires)), (GDestroyNotify) g_free);
      timer3 = timer_wheel_add_timer(wheel, expires - 1, timer_callback,
                                     g_memdup(&expires, sizeof(expires)), (GDestroyNotify) g_free);
      expected_callbacks += 3;
      r = rand() & 0xFF;
      if (r < 64)
        {
          /* delete the timer with 25% chance */
          timer_wheel_del_timer(wheel, timer1);
          expected_callbacks--;
        }
      else if (r < 128)
        {
          /* delete the timer with 25% chance */
          timer_wheel_del_timer(wheel, timer2);
          expected_callbacks--;
        }
      else if (r < 192)
        {
          /* delete the timer with 25% chance */
          timer_wheel_del_timer(wheel, timer3);
          expected_callbacks--;
        }
    }
  timer_wheel_set_time(wheel, latest + 1);
  cr_assert_eq(num_callbacks, expected_callbacks, "Error: not enough callbacks received, "
               "num_callbacks=%d, expected=%d\n",
               num_callbacks, expected_callbacks);

  timer_wheel_free(wheel);
}

Test(dbparser, test_timer_wheel_const)
{
  test_wheel(1234567890);
}

Test(dbparser, test_timer_wheel_current_time)
{
  test_wheel(time(NULL));
}
