/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2013 Balázs Scheidler
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
#include "stats-timer.h"
#include "stats.h"

#include <iv.h>

static struct iv_timer stats_timer;

static void
stats_timer_rearm(gint stats_freq)
{
  stats_timer.cookie = GINT_TO_POINTER(stats_freq);
  if (stats_freq > 0)
    {
      /* arm the timer */
      iv_validate_now();
      stats_timer.expires = iv_now;
      timespec_add_msec(&stats_timer.expires, stats_freq * 1000);
      iv_timer_register(&stats_timer);
    }
}

static void
stats_timer_elapsed(gpointer st)
{
  gint stats_freq = GPOINTER_TO_INT(st);

  stats_generate_log();
  stats_timer_rearm(stats_freq);
}

void
stats_timer_reinit(gint stats_freq)
{
  IV_TIMER_INIT(&stats_timer);
  stats_timer.handler = stats_timer_elapsed;

  if (iv_timer_registered(&stats_timer))
    iv_timer_unregister(&stats_timer);

  stats_timer_rearm(stats_freq);
}
